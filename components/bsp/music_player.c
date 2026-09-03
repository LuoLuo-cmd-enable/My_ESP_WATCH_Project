/*
 * music_player.c - 音乐播放核心（WAV 分块播放）
 *
 * 架构：独立任务循环 → audio_amp_play_pcm() 分块输出
 *      播放中按块统计 RMS 电平（波形用）、推进进度、支持暂停/跳转
 *      WAV 头解析 PCM16 单/双声道（44 字节标准头）
 */
#include "music_player.h"
#include "audio_amp.h"
#include "power_sleep.h"

/* miniimp3 单头文件库：在此处展开实现（仅本文件） */
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "mp3dec/minimp3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

#define TAG "MUSIC"

#define PLAYER_TASK_STACK    32768 /* 任务栈(PSRAM)：miniimp3 解码内部 scratch 需 ~16KB */
#define PLAYER_CHUNK_SAMPLES  1024  /* 增大分块，降低 SD 读取延迟造成的 I2S 欠载 */
#define PLAYER_LEVEL_WINDOW   8     /* 电平统计块数 */
#define MP3_IN_BUF_SIZE       65536 /* MP3 输入缓冲，减少 SD 读取频率 */
#define MP3_PCM_SAMPLES       1152  /* MP3 每帧最大样本数 */

/* WAV 头（44 字节标准 PCM 头） */
#pragma pack(push, 1)
typedef struct {
    char     riff[4];
    uint32_t file_size;
    char     wave[4];
    char     fmt[4];
    /* 一般是16字节 */
    uint32_t fmt_size;
    uint16_t audio_fmt;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    /*-------------*/
    char     data[4];
    uint32_t data_size;
} wav_header_t;
#pragma pack(pop)

/* ---- 播放状态（跨任务共享） ---- */
static volatile music_state_t s_state = MUSIC_STATE_STOP;
static volatile uint8_t  s_level = 0;
static volatile uint32_t s_pos_ms = 0;
static volatile uint32_t s_total_ms = 0;
static volatile bool     s_seek_req = false;
static volatile uint32_t s_seek_ms = 0;

static char s_music_name[64] = {0};
static char s_music_path[512] = {0};
static volatile bool s_switch_req = false;   /* 换歌请求：立即生效 */

static TaskHandle_t s_player_task = NULL;
static bool s_sleep_blocked = false;          /* 播放期间禁止睡眠 */

/* ---- 内部：解析 WAV 头，返回 data 偏移；失败返回 -1 ---- */
static long wav_parse_header(FILE *fp, wav_header_t *hdr)
{
    if (fp == NULL || hdr == NULL) return -1;
    if (fread(hdr, 1, sizeof(wav_header_t), fp) != sizeof(wav_header_t)) return -1;
    if (memcmp(hdr->riff, "RIFF", 4) != 0 || memcmp(hdr->wave, "WAVE", 4) != 0 ||
        memcmp(hdr->fmt, "fmt ", 4) != 0 || hdr->audio_fmt != 1) {
        return -1;
    }
    /* 有些文件 fmt 块大于 16，data 偏移需按块找 */
    if (hdr->fmt_size > 16) {
        fseek(fp, hdr->fmt_size - 16, SEEK_CUR);
        char chunk[4];
        uint32_t size = 0;
        for (int i = 0; i < 8; i++) {
            if (fread(chunk, 1, 4, fp) != 4) return -1;
            if (fread(&size, 4, 1, fp) != 1) return -1;
            if (memcmp(chunk, "data", 4) == 0) break;
            fseek(fp, size, SEEK_CUR);
        }
        if (memcmp(chunk, "data", 4) != 0) return -1;
        hdr->data_size = size;
    }
    return ftell(fp);
}

/* ================================================================
 * 播放辅助函数
 * ================================================================ */

/* 读取一段 PCM 数据，返回实际读到的 int16 样本数 */
static size_t read_pcm_chunk(FILE *fp, int16_t *chunk, size_t max_samples, uint16_t channels)
{
    if (channels == 1) {
        return fread(chunk, sizeof(int16_t), max_samples, fp);
    }

    /* I2S is configured for mono, so downmix interleaved stereo PCM. */
    int16_t stereo[PLAYER_CHUNK_SAMPLES * 2];
    size_t frames = max_samples / 2;
    if (frames > PLAYER_CHUNK_SAMPLES) frames = PLAYER_CHUNK_SAMPLES;
    size_t got = fread(stereo, sizeof(int16_t) * 2, frames, fp);
    for (size_t i = 0; i < got; i++) {
        chunk[i] = (int16_t)(((int32_t)stereo[i * 2] + stereo[i * 2 + 1]) / 2);
    }
    return got;
}

/* 统计一段数据的电平（0~100，绝对值平均近似 RMS） */
static uint8_t calc_level(const int16_t *data, size_t samples)
{
    if (samples == 0) return 0;
    int64_t acc = 0;
    for (size_t i = 0; i < samples; i++) {
        int32_t v = data[i];
        if (v < 0) v = -v;
        acc += v;
    }
    uint32_t lv = (uint32_t)(acc / samples / 327);
    return (uint8_t)(lv > 100 ? 100 : lv);
}

/* 处理跳转请求：定位到 data 区指定毫秒位置 */
static void handle_seek(FILE *fp, long data_offset, uint32_t bytes_per_sec,
                        uint32_t total_bytes)
{
    if (!s_seek_req) return;
    s_seek_req = false;

    long byte_off = (long)((uint64_t)s_seek_ms * bytes_per_sec / 1000);
    byte_off &= ~1L;                     /* 2 字节对齐 */
    if (byte_off > (long)total_bytes) byte_off = (long)total_bytes;
    if (byte_off < 0) byte_off = 0;

    if (fseek(fp, data_offset + byte_off, SEEK_SET) == 0) {
        s_pos_ms = (uint32_t)((uint64_t)byte_off * 1000 / bytes_per_sec);
    }
}

/* 判断文件是否为 MP3（按扩展名）/sdcard/music/xxx.mp3 */
static bool is_mp3_path(const char *path)
{
    if (path == NULL) return false;
    const char *ext = strrchr(path, '.');
    if (ext == NULL) return false;
    return (strcasecmp(ext, ".mp3") == 0);
}

/* ================================================================
 * MP3 播放循环（miniimp3 流式解码 → audio_amp 输出）
 * 流式：维护输入缓冲，解码完从文件补充，支持任意大小 MP3
 * ================================================================ */
static void play_mp3_loop(FILE *fp)
{
    /* 解码器状态（跨帧记忆）6.7KB */
    mp3dec_t *dec = heap_caps_malloc(sizeof(mp3dec_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (dec == NULL) dec = malloc(sizeof(mp3dec_t));
    if (dec == NULL) {
        ESP_LOGE(TAG, "mp3 dec alloc failed");
        return;
    }
    mp3dec_init(dec);

    /* 解码输出 PCM 4.6KB双声道需要预留1152(每帧的样本数)*2(通道数量)*2立体声需要降混 */
    int16_t *pcm = heap_caps_malloc(MP3_PCM_SAMPLES * 2 * sizeof(int16_t),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pcm == NULL) pcm = malloc(MP3_PCM_SAMPLES * 2 * sizeof(int16_t));
    if (pcm == NULL) {
        ESP_LOGE(TAG, "mp3 pcm alloc failed");
        free(dec);
        return;
    }

    /* 压缩数据输入缓冲 64KB */
    uint8_t *in_buf = heap_caps_malloc(MP3_IN_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (in_buf == NULL) in_buf = malloc(MP3_IN_BUF_SIZE);
    if (in_buf == NULL) {
        ESP_LOGE(TAG, "mp3 inbuf alloc failed");
        free(dec);
        free(pcm);
        return;
    }
    uint32_t level_acc = 0;         /* 8帧的电平累加值 */
    uint32_t level_cnt = 0;         /* 已累加帧数，到8就刷新 s_level */
    bool muted = false;             /* 暂停时 I2S 是否已关断 */
    bool sr_inited = false;         /* I2S 是否已按首帧采样率配好 */
    uint32_t mp3_sample_rate = 0;   /* 记录采样率，恢复播放时重新 init 用 */

    /* 初始填充输入缓冲 */
    ESP_LOGD(TAG, "mp3: read head...");
    size_t buf_len = fread(in_buf, 1, MP3_IN_BUF_SIZE, fp);         /* in_buf 中有效数据的字节数 */
    ESP_LOGD(TAG, "mp3: head read %u bytes", (unsigned)buf_len);
    size_t buf_pos = 0;                                             /* 已解码消费到的位置（光标） */
    uint32_t frame_cnt = 0;                                         /* 已解帧数，每200帧打印日志 */
    int decode_fail_cnt = 0;                                        /* 连续解码失败计数 */
    uint32_t last_progress = xTaskGetTickCount();                   /* 无进展看门狗 */
    bool watchdog_armed = true;                                     /* 看门狗是否启用 */

    while (buf_len > 0 && s_state != MUSIC_STATE_STOP) {
        /* 换歌 */
        if (s_switch_req) {
            s_switch_req = false; 
            break; 
        }
        /* 暂停 */
        if (s_state == MUSIC_STATE_PAUSED) {
            last_progress = xTaskGetTickCount();   /* 暂停不算卡死 */
            if (!muted) { 
                audio_amp_deinit(); 
                muted = true; 
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (muted) {
            if (mp3_sample_rate != 0) {
                audio_amp_init(mp3_sample_rate);
            }
            muted = false;
        }

        /* 无进展看门狗：3 秒没推进（解码死循环/写 I2S 卡死）则放弃 */
        if (watchdog_armed &&
            (xTaskGetTickCount() - last_progress) > pdMS_TO_TICKS(3000)) {
            ESP_LOGE(TAG, "mp3: watchdog timeout (frame=%lu pos_ms=%lu buf=%u/%u)",
                     (unsigned long)frame_cnt, (unsigned long)s_pos_ms,
                     (unsigned)buf_len, (unsigned)buf_pos);
            break;
        }

        /* 解码一帧。miniimp3 三种返回：
         *  samples > 0            : 成功解出一帧
         *  samples<=0,fb>0        : 跳过垃圾字节(ID3/填充)，需推进
         *  samples<=0,fb<=0       : 缓冲不足/文件结束，需补充 */
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(dec, in_buf + buf_pos, (int)(buf_len - buf_pos),
                                          pcm, &info);
        if (samples > 0) {
            decode_fail_cnt = 0;
            buf_pos += info.frame_bytes;
        } else if (info.frame_bytes > 0) {
            /* 跳过垃圾字节后继续（关键：必须推进 buf_pos，否则死循环） */
            buf_pos += info.frame_bytes;
            continue;
        } else {
            /* 缓冲不足：移位 + 补充 */
            decode_fail_cnt++;
            if (decode_fail_cnt > 100) {
                ESP_LOGE(TAG, "mp3: too many decode fails, give up");
                break;
            }
            if (buf_pos > 0) {
                memmove(in_buf, in_buf + buf_pos, buf_len - buf_pos);
                buf_len -= buf_pos;
                buf_pos = 0;
            }
            size_t more = fread(in_buf + buf_len, 1, MP3_IN_BUF_SIZE - buf_len, fp);
            if (more == 0) break;   /* 文件读完 */
            buf_len += more;
            continue;
        }

        /* 首次获取采样率 */
        if (!sr_inited && info.hz > 0) {
            ESP_LOGD(TAG, "mp3: first frame hz=%d ch=%d bitrate=%d",
                     info.hz, info.channels, info.bitrate_kbps);
            audio_amp_init((uint32_t)info.hz);
            mp3_sample_rate = (uint32_t)info.hz;
            sr_inited = true;
            /* 总时长估算（CBR 假设：字节数 / 码率），供进度条 */
            long cur = ftell(fp);
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, cur, SEEK_SET);
            if (info.bitrate_kbps > 0 && fsize > 0) {
                s_total_ms = (uint32_t)((uint64_t)fsize * 8000 / (info.bitrate_kbps * 1000));
            }
        }
        if (!sr_inited) continue;   /* 还没拿到采样率，跳过本帧 */

        int dec_samples = samples;   /* 每声道样本数（时间计算用） */
        if (info.channels == 1) {
            if (audio_amp_play_pcm(pcm, (uint32_t)samples * 2) != ESP_OK) {
                ESP_LOGE(TAG, "mp3 PCM queue failed");
                break;
            }
        } else {
            for (int i = 0; i < samples; i++) {
                pcm[i] = (int16_t)(((int32_t)pcm[i * 2] + pcm[i * 2 + 1]) / 2);
            }
            if (audio_amp_play_pcm(pcm, (uint32_t)samples * 2) != ESP_OK) {
                ESP_LOGE(TAG, "mp3 PCM queue failed");
                break;
            }
        }
        last_progress = xTaskGetTickCount();   /* 有进展，刷新看门狗 */

        /* 诊断：每 200 帧打印一次进度（DEBUG 级） */
        if ((++frame_cnt % 200) == 0) {
            ESP_LOGD(TAG, "mp3 frame=%lu pos_ms=%lu buf_len=%u pos=%u",
                     (unsigned long)frame_cnt, (unsigned long)s_pos_ms,
                     (unsigned)buf_len, (unsigned)buf_pos);
        }

        /* 电平统计 */
        level_acc += calc_level(pcm, (size_t)samples * 2);
        if (++level_cnt >= PLAYER_LEVEL_WINDOW) {
            s_level = (uint8_t)(level_acc / level_cnt);
            level_acc = 0;
            level_cnt = 0;
        }
        s_pos_ms += (uint32_t)(dec_samples * 1000 / (info.hz ? (uint32_t)info.hz : 44100));
    }

    free(in_buf);
    free(pcm);
    free(dec);
}

/* ---- 播放任务：打开文件 → 循环分块播放 → 统计电平/进度 ---- */
static void music_player_task(void *arg)
{
    (void)arg;

    while(1){
        /* 无歌可播（已 stop/关闭）：等待 play 设置路径 */
        if (s_music_path[0] == '\0') {
            vTaskSuspend(NULL);
            continue;
        }
        /* 这里的s_music_path是fullpath */
        FILE *fp = fopen(s_music_path, "rb");
        if (fp == NULL) {
            ESP_LOGE(TAG, "open %s failed", s_music_path);
            s_state = MUSIC_STATE_STOP;
            s_music_path[0] = '\0';
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* MP3：走 miniimp3 解码播放 */
        if (is_mp3_path(s_music_path)) {
            ESP_LOGI(TAG, "play mp3: %s", s_music_name);
            s_pos_ms = 0;
            s_level = 0;
            s_total_ms = 0;   /* 首帧后按码率估算 */
            s_state = MUSIC_STATE_PLAYING;
            play_mp3_loop(fp);
            fclose(fp);
            s_level = 0;
            if (s_state == MUSIC_STATE_STOP || s_music_path[0] == '\0' || s_switch_req) {
                s_switch_req = false;
                continue;
            }
            ESP_LOGI(TAG, "end of file, loop: %s", s_music_name);
            s_state = MUSIC_STATE_PLAYING;
            continue;   /* 重新 fopen 循环播放 */
        }

        /* 解析 WAV 头 */
        wav_header_t hdr;
        long data_offset = wav_parse_header(fp, &hdr);
        if (data_offset < 0 || hdr.bits_per_sample != 16 ||
            (hdr.channels != 1 && hdr.channels != 2)) {
            ESP_LOGE(TAG, "unsupported wav (need PCM16 mono/stereo): %s", s_music_path);
            fclose(fp);
            s_state = MUSIC_STATE_STOP;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        fseek(fp, data_offset, SEEK_SET);

        /* data_size 兜底：声明值可能不准，用文件实际剩余 */
        {
            long cur = ftell(fp);
            fseek(fp, 0, SEEK_END);
            long file_end = ftell(fp);
            fseek(fp, cur, SEEK_SET);
            long remain = file_end - cur;
            if (remain > 0) hdr.data_size = (uint32_t)remain;
        }
        ESP_LOGI(TAG, "wav: fmt=%u ch=%u sr=%lu off=%ld size=%lu",
                 hdr.bits_per_sample, hdr.channels,
                 (unsigned long)hdr.sample_rate, data_offset,
                 (unsigned long)hdr.data_size);

        /* 初始化播放 */
        audio_amp_init(hdr.sample_rate);
        uint32_t bytes_per_sec = hdr.sample_rate * hdr.channels * 2;
        s_total_ms = (bytes_per_sec > 0)
                     ? (uint32_t)((uint64_t)hdr.data_size * 1000 / bytes_per_sec)
                     : 0;
        s_pos_ms = 0;
        s_level = 0;
        s_state = MUSIC_STATE_PLAYING;
        ESP_LOGI(TAG, "play %s (%lu ms)", s_music_name, (unsigned long)s_total_ms);

        /* 分块播放主循环 */
        int16_t chunk[PLAYER_CHUNK_SAMPLES * 2];
        uint32_t level_acc = 0;
        uint32_t level_cnt = 0;
        bool muted = false;

        while (s_state != MUSIC_STATE_STOP) {
            /* 换歌请求：立即退出当前播放（切歌） */
            if (s_switch_req) {
                s_switch_req = false;
                break;
            }
            /* 暂停：关断 I2S 真静音，恢复时重建 */
            if (s_state == MUSIC_STATE_PAUSED) {
                if (!muted) {
                    audio_amp_deinit();
                    muted = true;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            if (muted) {
                audio_amp_init(hdr.sample_rate);
                muted = false;
            }

            /* 跳转 */
            handle_seek(fp, data_offset, bytes_per_sec, hdr.data_size);

            /* 读一块并播放 */
            size_t got = read_pcm_chunk(fp, chunk, PLAYER_CHUNK_SAMPLES * 2, hdr.channels);
            if (got == 0) break;   /* 文件读完 */

            audio_amp_play_pcm(chunk, (uint32_t)got * 2);

            /* 电平统计（每 PLAYER_LEVEL_WINDOW 块更新一次） */
            level_acc += calc_level(chunk, got);
            if (++level_cnt >= PLAYER_LEVEL_WINDOW) {
                s_level = (uint8_t)(level_acc / level_cnt);
                level_acc = 0;
                level_cnt = 0;
            }

            /* 推进进度 */
            s_pos_ms += (uint32_t)((uint64_t)got * 1000 / bytes_per_sec);
            if (s_pos_ms > s_total_ms) s_pos_ms = s_total_ms;
        }

        /* 播放结束：循环重播或等待 */
        fclose(fp);
        s_level = 0;
        if (s_state == MUSIC_STATE_STOP || s_music_path[0] == '\0') {
            continue;   /* 已停止/关闭：回外层等待 */
        }
        if (s_switch_req) {
            s_switch_req = false;   /* 换歌：直接进外层重新 fopen */
            continue;
        }
        ESP_LOGI(TAG, "end of file, loop: %s", s_music_name);
        s_state = MUSIC_STATE_PLAYING;   /* 循环重播 */
    }
}

void music_player_init(void)
{
    if (s_player_task != NULL) return;
    /* 栈放 PSRAM：miniimp3 解码内部需 ~16KB 栈（scratch），内部 RAM 紧张 */
    static StackType_t *s_player_stack = NULL;
    static StaticTask_t s_player_tcb;
    if (s_player_stack == NULL) {
        s_player_stack = heap_caps_malloc(PLAYER_TASK_STACK * sizeof(StackType_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_player_stack == NULL) {
        ESP_LOGE(TAG, "player stack alloc failed");
        return;
    }
    s_player_task = xTaskCreateStaticPinnedToCore(
        music_player_task, "music_player", PLAYER_TASK_STACK, NULL, 4,
        s_player_stack, &s_player_tcb, 0);
    ESP_LOGI(TAG, "music player ready, stack=%u", (unsigned)PLAYER_TASK_STACK);
}

esp_err_t music_player_play(const char *path)
{
    if (path == NULL || path[0] == '\0') return ESP_FAIL;

    /* 只在"已有歌在播"时才置切歌标志（否则首次播放被误判中断） */
    bool was_playing = (s_state == MUSIC_STATE_PLAYING || s_state == MUSIC_STATE_PAUSED);

    /* 更新路径并请求立即切歌（播放任务检测后退出旧歌循环） */
    snprintf(s_music_path, sizeof(s_music_path), "%s", path);
    const char *slash = strrchr(path, '/');
    snprintf(s_music_name, sizeof(s_music_name), "%s", slash ? slash + 1 : path);

    s_seek_req = false;
    s_pos_ms = 0;
    s_level = 0;
    s_state = MUSIC_STATE_PLAYING;   /* 明确进入播放态 */
    s_switch_req = was_playing;

    /* 播放期间禁止系统睡眠（否则 sleep 会关 I2S 导致卡死/无声） */
    if (!s_sleep_blocked) {
        power_sleep_block();
        s_sleep_blocked = true;
    }

    /* 唤醒任务（任务循环里会自动 fopen 播放新歌） */
    if (s_player_task != NULL) {
        vTaskResume(s_player_task);
    }
    return ESP_OK;
}

void music_player_toggle_pause(void)
{
    if (s_state == MUSIC_STATE_PLAYING) {
        s_state = MUSIC_STATE_PAUSED;
        s_level = 0;
    } else if (s_state == MUSIC_STATE_PAUSED) {
        s_state = MUSIC_STATE_PLAYING;
    }
}

void music_player_seek(uint32_t pos_ms)
{
    if (pos_ms > s_total_ms) pos_ms = s_total_ms;
    s_seek_req = true;
    s_seek_ms = pos_ms;
}

void music_player_stop(void)
{
    s_state = MUSIC_STATE_STOP;
    s_level = 0;
    s_switch_req = false;
    s_music_path[0] = '\0';   /* 清空路径：播放任务回外层挂起 */
    audio_amp_deinit();       /* 关断 I2S */

    /* 恢复允许睡眠 */
    if (s_sleep_blocked) {
        power_sleep_unblock();
        s_sleep_blocked = false;
    }
}

music_state_t music_player_get_state(void) { return s_state; }
uint8_t music_player_get_level(void)       { return s_level; }
uint32_t music_player_get_pos_ms(void)     { return s_pos_ms; }
uint32_t music_player_get_total_ms(void)   { return s_total_ms; }
const char *music_player_get_name(void)    { return s_music_name; }


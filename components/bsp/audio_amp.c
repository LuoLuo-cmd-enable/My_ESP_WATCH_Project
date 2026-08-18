/*
 * audio_amp.c - MAX98357A I2S 数字功放驱动
 * 引脚: BCLK=GPIO47, WS(LRC)=GPIO48, DIN=GPIO21
 * MAX98357A: 无需使能脚，I2S 有数据即出声
 * power_sleep文件下需要修改相应的休眠设置
 */
#include "audio_amp.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#define TAG "AUDIO_AMP"

static i2s_chan_handle_t s_tx_chan = NULL;

esp_err_t audio_amp_init(uint32_t sample_rate)
{
    if (s_tx_chan != NULL) {
        return ESP_OK;   /* 已初始化 */
    }

    /* 标准 I2S 配置（MAX98357A 要求 I2S 标准格式） */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   /* MAX98357A 不需要 MCLK */
            .bclk = AUDIO_BCLK_GPIO,
            .ws   = AUDIO_WS_GPIO,
            .dout = AUDIO_DIN_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MAX98357A ready, sample_rate=%lu", (unsigned long)sample_rate);
    return ESP_OK;
}

esp_err_t audio_amp_play_pcm(const int16_t *data, uint32_t len)
{
    if (s_tx_chan == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 单声道 → 双声道扩展（MAX98357A 双声道模式） */
    uint32_t written = 0;
    while (written < len) {
        size_t bytes = 0;
        esp_err_t ret = i2s_channel_write(s_tx_chan,
                                          (const char *)data + written,
                                          len - written,
                                          &bytes, portMAX_DELAY);
        if (ret != ESP_OK) {
            return ret;
        }
        if (bytes == 0) {
            break;
        }
        written += bytes;
    }
    return ESP_OK;
}

/* WAV 头（44 字节标准 PCM 头） */
#pragma pack(push, 1)
typedef struct {
    char     riff[4];      /* "RIFF" */
    uint32_t file_size;
    char     wave[4];      /* "WAVE" */
    char     fmt[4];       /* "fmt " */
    uint32_t fmt_size;     /* 16 */
    uint16_t audio_fmt;    /* 1 = PCM */
    uint16_t channels;     /* 1/2 */
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data[4];      /* "data" */
    uint32_t data_size;
} wav_header_t;
#pragma pack(pop)

esp_err_t audio_amp_play_wav(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s failed", path);
        return ESP_FAIL;
    }

    wav_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        ESP_LOGE(TAG, "read wav header failed");
        fclose(fp);
        return ESP_FAIL;
    }

    /* 校验 WAV 头 */
    if (memcmp(hdr.riff, "RIFF", 4) != 0 || memcmp(hdr.wave, "WAVE", 4) != 0 ||
        hdr.audio_fmt != 1) {
        ESP_LOGE(TAG, "not a PCM wav file");
        fclose(fp);
        return ESP_FAIL;
    }

    esp_err_t ret = audio_amp_init(hdr.sample_rate);
    if (ret != ESP_OK) {
        fclose(fp);
        return ret;
    }

    /* 流式播放 data 段 */
    uint8_t buf[2048];
    uint32_t remain = hdr.data_size;
    while (remain > 0) {
        uint32_t to_read = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        size_t got = fread(buf, 1, to_read, fp);
        if (got == 0) {
            break;
        }
        uint32_t written = 0;
        while (written < got) {
            size_t bytes = 0;
            if (i2s_channel_write(s_tx_chan, (const char *)buf + written,
                                  got - written, &bytes, pdMS_TO_TICKS(100)) != ESP_OK) {
                break;
            }
            written += bytes;
        }
        remain -= got;
    }

    fclose(fp);
    ESP_LOGI(TAG, "wav play done: %s", path);
    return ESP_OK;
}

void audio_amp_deinit(void)
{
    if (s_tx_chan != NULL) {
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        ESP_LOGI(TAG, "audio amp deinit");
    }
}

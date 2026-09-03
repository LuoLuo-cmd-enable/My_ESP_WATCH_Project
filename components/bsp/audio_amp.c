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
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

#define TAG "AUDIO_AMP"

static i2s_chan_handle_t s_tx_chan = NULL;
static uint32_t s_cur_sample_rate = 0;   /* 当前 I2S 采样率 */

#define AUDIO_RING_BLOCK_SIZE 256
#define AUDIO_RING_BLOCK_COUNT 128
#define AUDIO_OUTPUT_VOLUME_PERCENT 20

static uint8_t *s_pcm_ring = NULL;
static uint32_t s_ring_read = 0;
static uint32_t s_ring_write = 0;
static SemaphoreHandle_t s_ring_data = NULL;
static SemaphoreHandle_t s_ring_space = NULL;
static SemaphoreHandle_t s_output_done = NULL;
static TaskHandle_t s_output_task = NULL;
static volatile bool s_output_stop = false;
static SemaphoreHandle_t s_api_mutex = NULL;

static void audio_amp_deinit_locked(void);

static void audio_amp_lock(void)
{
    if (s_api_mutex == NULL) {
        s_api_mutex = xSemaphoreCreateMutex();
    }
    if (s_api_mutex != NULL) {
        xSemaphoreTake(s_api_mutex, portMAX_DELAY);
    }
}

static void audio_amp_unlock(void)
{
    if (s_api_mutex != NULL) {
        xSemaphoreGive(s_api_mutex);
    }
}

static void audio_output_task(void *arg)
{
    (void)arg;
    uint8_t block[AUDIO_RING_BLOCK_SIZE];

    while (!s_output_stop) {
        if (xSemaphoreTake(s_ring_data, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (s_output_stop) break;

        memcpy(block, s_pcm_ring + s_ring_read * AUDIO_RING_BLOCK_SIZE,
               AUDIO_RING_BLOCK_SIZE);
        s_ring_read = (s_ring_read + 1) % AUDIO_RING_BLOCK_COUNT;

        size_t written = 0;
        while (written < AUDIO_RING_BLOCK_SIZE && !s_output_stop) {
            size_t bytes = 0;
            esp_err_t ret = i2s_channel_write(s_tx_chan, block + written,
                                              AUDIO_RING_BLOCK_SIZE - written,
                                              &bytes, pdMS_TO_TICKS(300));
            if (ret != ESP_OK || bytes == 0) {
                ESP_LOGE(TAG, "I2S output failed: %s",
                         esp_err_to_name(ret));
                break;
            }
            written += bytes;
        }
        xSemaphoreGive(s_ring_space);
    }

    xSemaphoreGive(s_output_done);
    vTaskDelete(NULL);
}

static void audio_ring_reset(void)
{
    s_ring_read = 0;
    s_ring_write = 0;
    /* 待写入空位清零 */
    while (xSemaphoreTake(s_ring_data, 0) == pdTRUE);
    while (xSemaphoreGive(s_ring_space) == pdTRUE);
}

esp_err_t audio_amp_init(uint32_t sample_rate)
{
    audio_amp_lock();
    /* 已初始化且采样率相同：直接返回 */
    if (s_tx_chan != NULL && s_cur_sample_rate == sample_rate) {
        audio_amp_unlock();
        return ESP_OK;
    }
    /* 采样率变了：先关闭旧输出任务再重建。 */
    if (s_tx_chan != NULL) {
        audio_amp_deinit_locked();
    }

    /* 标准 I2S 配置（MAX98357A 要求 I2S 标准格式） */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 512;
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        audio_amp_unlock();
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
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
        audio_amp_unlock();
        return ret;
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        audio_amp_unlock();
        return ret;
    }

    if (s_pcm_ring == NULL) {
        s_pcm_ring = heap_caps_malloc(AUDIO_RING_BLOCK_SIZE * AUDIO_RING_BLOCK_COUNT,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_pcm_ring == NULL) {
            ESP_LOGE(TAG, "PCM ring allocation failed");
            i2s_channel_disable(s_tx_chan);
            i2s_del_channel(s_tx_chan);
            s_tx_chan = NULL;
            audio_amp_unlock();
            return ESP_ERR_NO_MEM;
        }
    }
    s_ring_data = xSemaphoreCreateCounting(AUDIO_RING_BLOCK_COUNT, 0);
    s_ring_space = xSemaphoreCreateCounting(AUDIO_RING_BLOCK_COUNT,
                                             AUDIO_RING_BLOCK_COUNT);
    s_output_done = xSemaphoreCreateBinary();
    if (s_ring_data == NULL || s_ring_space == NULL || s_output_done == NULL) {
        ESP_LOGE(TAG, "PCM ring semaphore allocation failed");
        if (s_ring_data) vSemaphoreDelete(s_ring_data);
        if (s_ring_space) vSemaphoreDelete(s_ring_space);
        if (s_output_done) vSemaphoreDelete(s_output_done);
        s_ring_data = NULL;
        s_ring_space = NULL;
        s_output_done = NULL;
        heap_caps_free(s_pcm_ring);
        s_pcm_ring = NULL;
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        audio_amp_unlock();
        return ESP_ERR_NO_MEM;
    }
    audio_ring_reset();
    s_output_stop = false;
    if (xTaskCreatePinnedToCore(audio_output_task, "audio_output", 4096,
                                NULL, 5, &s_output_task, 0) != pdPASS) {
        ESP_LOGE(TAG, "audio output task creation failed");
        vSemaphoreDelete(s_ring_data);
        vSemaphoreDelete(s_ring_space);
        vSemaphoreDelete(s_output_done);
        s_ring_data = NULL;
        s_ring_space = NULL;
        s_output_done = NULL;
        heap_caps_free(s_pcm_ring);
        s_pcm_ring = NULL;
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        audio_amp_unlock();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "MAX98357A ready, sample_rate=%lu", (unsigned long)sample_rate);
    s_cur_sample_rate = sample_rate;
    audio_amp_unlock();
    return ESP_OK;
}

esp_err_t audio_amp_play_pcm(const int16_t *data, uint32_t len)
{
    audio_amp_lock();
    if (s_tx_chan == NULL || data == NULL || len == 0 || (len & 1U) != 0) {
        audio_amp_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t *buffer = (const uint8_t *)data;
    uint32_t queued = 0;
    int16_t scaled_block[AUDIO_RING_BLOCK_SIZE / sizeof(int16_t)];
    while (queued < len) {
        if (xSemaphoreTake(s_ring_space, pdMS_TO_TICKS(300)) != pdTRUE) {
            audio_amp_unlock();
            return ESP_ERR_TIMEOUT;
        }
        uint32_t bytes = len - queued;
        if (bytes > AUDIO_RING_BLOCK_SIZE) bytes = AUDIO_RING_BLOCK_SIZE;
        bytes &= ~1U;
        for (uint32_t i = 0; i < bytes / sizeof(int16_t); i++) {
            int32_t sample = ((const int16_t *)(buffer + queued))[i];
            scaled_block[i] = (int16_t)((sample * AUDIO_OUTPUT_VOLUME_PERCENT) / 100);
        }
        memcpy(s_pcm_ring + s_ring_write * AUDIO_RING_BLOCK_SIZE,
             scaled_block, bytes);
        if (bytes < AUDIO_RING_BLOCK_SIZE) {
            memset(s_pcm_ring + s_ring_write * AUDIO_RING_BLOCK_SIZE + bytes,
                   0, AUDIO_RING_BLOCK_SIZE - bytes);
        }
        s_ring_write = (s_ring_write + 1) % AUDIO_RING_BLOCK_COUNT;
        xSemaphoreGive(s_ring_data);
        queued += bytes;
    }
    audio_amp_unlock();
    return ESP_OK;
}

static void audio_amp_deinit_locked(void)
{
    if (s_tx_chan != NULL) {
        s_output_stop = true;
        if (s_ring_data != NULL) xSemaphoreGive(s_ring_data);
        if (s_output_task != NULL && s_output_done != NULL) {
            xSemaphoreTake(s_output_done, pdMS_TO_TICKS(1000));
            s_output_task = NULL;
        }
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        if (s_ring_data != NULL) vSemaphoreDelete(s_ring_data);
        if (s_ring_space != NULL) vSemaphoreDelete(s_ring_space);
        if (s_output_done != NULL) vSemaphoreDelete(s_output_done);
        s_ring_data = NULL;
        s_ring_space = NULL;
        s_output_done = NULL;
        if (s_pcm_ring != NULL) {
            heap_caps_free(s_pcm_ring);
            s_pcm_ring = NULL;
        }
        s_cur_sample_rate = 0;
        ESP_LOGI(TAG, "audio amp deinit");
    }
}

void audio_amp_deinit(void)
{
    audio_amp_lock();
    audio_amp_deinit_locked();
    audio_amp_unlock();
}

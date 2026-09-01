/*
 * audio_amp.h - MAX98357A I2S 数字功放驱动
 * 引脚: BCLK=GPIO47, WS(LRC)=GPIO48, DIN=GPIO21
 */
#ifndef _AUDIO_AMP_H_
#define _AUDIO_AMP_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* 引脚定义 */
#define AUDIO_BCLK_GPIO   GPIO_NUM_47
#define AUDIO_WS_GPIO     GPIO_NUM_48
#define AUDIO_DIN_GPIO    GPIO_NUM_21

/* 常用采样率 */
#define AUDIO_SAMPLE_RATE_8K   8000
#define AUDIO_SAMPLE_RATE_16K  16000
#define AUDIO_SAMPLE_RATE_22K  22050
#define AUDIO_SAMPLE_RATE_44K  44100

/**
 * @brief 初始化 I2S + MAX98357A
 * @param sample_rate 采样率（如 AUDIO_SAMPLE_RATE_44K）
 * @return esp_err_t
 */
esp_err_t audio_amp_init(uint32_t sample_rate);

/**
 * @brief 播放一段 PCM16 数据（阻塞，播放完返回）
 * @param data  PCM 数据指针（16bit 有符号）
 * @param len   数据字节数（非样本数）
 * @return esp_err_t
 */
esp_err_t audio_amp_play_pcm(const int16_t *data, uint32_t len);

/**
 * @brief 关断 I2S（低功耗：进睡眠前调用）
 */
void audio_amp_deinit(void);

#endif /* _AUDIO_AMP_H_ */

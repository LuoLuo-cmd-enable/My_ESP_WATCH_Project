/*
 * music_player.h - 音乐播放核心（WAV/MP3 分块播放）
 *
 * 依赖 audio_amp（MAX98357A I2S 功放）做底层 PCM 输出，
 * 本模块负责：打开文件 / 分块读取 / 电平统计 / 暂停切换 / 进度 / 循环。
 * 波形显示由 UI 层轮询 music_player_get_level()。
 */
#ifndef _MUSIC_PLAYER_H_
#define _MUSIC_PLAYER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* 波形柱最大数量（UI 层用） */
#define MUSIC_WAVE_BARS  24

typedef enum {
    MUSIC_STATE_STOP = 0,   /* 未播放 */
    MUSIC_STATE_PLAYING,    /* 播放中 */
    MUSIC_STATE_PAUSED,     /* 已暂停 */
} music_state_t;

/**
 * @brief 初始化音乐播放器（内部启动播放任务）
 */
void music_player_init(void);

/**
 * @brief 播放指定 WAV 文件（可随时换歌）
 * @param path 完整路径 /sdcard/music/xxx.wav
 */
esp_err_t music_player_play(const char *path);

/**
 * @brief 播放/暂停切换
 */
void music_player_toggle_pause(void);

/**
 * @brief 跳到指定位置（毫秒）
 */
void music_player_seek(uint32_t pos_ms);

/**
 * @brief 停止播放
 */
void music_player_stop(void);

/**
 * @brief 获取播放状态
 */
music_state_t music_player_get_state(void);

/**
 * @brief 获取当前电平（0~100，波形柱高度用）
 */
uint8_t music_player_get_level(void);

/**
 * @brief 获取播放位置（毫秒）
 */
uint32_t music_player_get_pos_ms(void);

/**
 * @brief 获取总时长（毫秒）
 */
uint32_t music_player_get_total_ms(void);

/**
 * @brief 获取当前文件名（无路径）
 */
const char *music_player_get_name(void);

#endif /* _MUSIC_PLAYER_H_ */

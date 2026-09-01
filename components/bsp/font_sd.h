/*
 * font_sd.h - SD 卡动态字库适配
 *
 * 从 SD 卡 /font/Font_20.bin 加载 LVGL 官网生成的中文字体（覆盖 0x4E00-0x9FA5），
 * 未覆盖的字符（ASCII/标点等）自动回退到内置 songti_font_16。
 *
 * 依赖：img_display.c 注册的 'S' 盘符 LVGL 文件系统驱动（映射 /sdcard）
 */
#ifndef _FONT_SD_H_
#define _FONT_SD_H_

#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief 初始化 SD 字库（需在 LVGL 初始化 + my_fs_init 之后调用）
 * @note 加载失败时自动回退到内置 songti_font_16，不影响运行
 */
esp_err_t font_sd_init(void);

/**
 * @brief 获取 SD 字库字体指针（未加载成功时返回内置 songti_font_16）
 */
lv_font_t *font_sd_get(void);

/**
 * @brief SD 字库是否已完全加载完成
 * @return true=已就绪（或加载失败已回退，可正常显示） false=仍在加载中
 */
bool font_sd_is_ready(void);

#endif /* _FONT_SD_H_ */

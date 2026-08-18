#ifndef __BATTERY_MANAGEMENT
#define __BATTERY_MANAGEMENT

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* 硬件引脚 */
#define BATTERY_ADC_CHANNEL   ADC_CHANNEL_3   /* GPIO4 = ADC1_CH3 */
#define BATTERY_ADC_GPIO      GPIO_NUM_4

/* 分压与校准 */
#define BATTERY_DIVIDER_RATIO  2.0f            /* 两个10K分压 = 1/2，反推 = 2 */
#define BATTERY_ADC_ATTEN      ADC_ATTEN_DB_12 /* 0~3.3V 量程 */

/* 锂电池电压阈值（3.7V 标称，4.2V 满电，3.0V 截止） */
#define BATTERY_VOLTAGE_FULL      4.20f   /* 满电 */
#define BATTERY_VOLTAGE_EMPTY     3.00f   /* 截止 */

/**
 * @brief 电池电量等级（LVGL 显示用）
 */
typedef enum {
    BATTERY_LEVEL_0 = 0,    /* 0~20% */
    BATTERY_LEVEL_1,        /* 20~40% */
    BATTERY_LEVEL_2,        /* 40~60% */
    BATTERY_LEVEL_3,        /* 60~80% */
    BATTERY_LEVEL_4,        /* 80~100% */
    BATTERY_LEVEL_MAX
} battery_level_t;

/**
 * @brief 初始化 ADC（GPIO4 分压采样）
 * @return esp_err_t
 */
esp_err_t battery_init(void);

/**
 * @brief 读取电池电压（mV）
 * @return 电压值（毫伏），失败返回 0
 */
uint32_t battery_get_voltage_mv(void);

/**
 * @brief 读取电池电压（V，浮点）
 * @return 电压值（伏），失败返回 0
 */
float battery_get_voltage_v(void);

/**
 * @brief 计算电池电量百分比（0~100）
 * @return 百分比（按电压线性映射，带迟滞防抖）
 */
uint8_t battery_get_percent(void);

/**
 * @brief 获取电量等级（0~4，LVGL 图标用）
 * @return battery_level_t
 */
battery_level_t battery_get_level(void);

#endif /* __BATTERY_MANAGEMENT */






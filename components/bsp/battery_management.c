/*
 * battery_management.c - 电池电量管理（ADC 采样）
 *
 * 硬件：GPIO4 = ADC1_CH3
 *       电池 -- 10K -- GPIO4 -- 10K -- GND （分压 1/2）
 * 满电 4.2V → ADC 2.1V，截止 3.0V → ADC 1.5V
 */
#include "battery_management.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "BATTERY"

#define ADC_SAMPLE_COUNT    8      /* 采样次数取平均，防抖动 */

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_battery_inited = false;

/* 电量百分比平滑：一阶低通滤波（EMA） */
static float s_ema_percent = -1.0f;
#define EMA_ALPHA   0.2f        /* 平滑系数：越大响应越快、越抖；越小越平滑、越滞后 */

/* 原迟滞参数 
 static int s_last_percent = -1; 
 #define HYSTERESIS_STEP   5 */

esp_err_t battery_init(void)
{
    if (s_battery_inited) {
        return ESP_OK;
    }

    /* 配置 ADC1 单次采样 */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc oneshot new unit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,           /* 12dB = 20*log10(vin/vout) */
        .bitwidth = ADC_BITWIDTH_12,          /* 12bit: 0~4095 */
    };
    ret = adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc config channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 创建校准句柄（曲线拟合方案，ESP32-S3 出厂带校准 eFuse） */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "adc cali create failed: %s (will use raw approx)", esp_err_to_name(ret));
        s_cali_handle = NULL;   /* 校准失败不阻塞，退回近似换算 */
    }

    s_battery_inited = true;
    ESP_LOGI(TAG, "battery adc init ok (GPIO4, channel 3)");
    return ESP_OK;
}

uint32_t battery_get_voltage_mv(void)
{
    if (!s_battery_inited || s_adc_handle == NULL) {
        return 0;
    }

    /* 多次采样取平均 */
    int32_t sum_raw = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw) == ESP_OK) {
            sum_raw += raw;
        }
        vTaskDelay(pdMS_TO_TICKS(2));   /* 采样间隔，稳定读数 */
    }
    int avg_raw = sum_raw / ADC_SAMPLE_COUNT;

    int adc_mv = 0;
    if (s_cali_handle != NULL) {
        /* 校准方案：原始值 → 电压（mV），精度 ±10mV */
        if (adc_cali_raw_to_voltage(s_cali_handle, avg_raw, &adc_mv) != ESP_OK) {
            adc_mv = 0;
        }
    } else {
        /* 无校准退回：12bit 近似线性，量程约 0~2500mV */
        adc_mv = (avg_raw * 2500) / 4095;
    }

    /* 乘分压比反推电池电压 */
    uint32_t battery_mv = (uint32_t)(adc_mv * BATTERY_DIVIDER_RATIO);
    return battery_mv;
}

float battery_get_voltage_v(void)
{
    return battery_get_voltage_mv() / 1000.0f;
}

uint8_t battery_get_percent(void)
{
    float volt = battery_get_voltage_v();
    if (volt <= 0) {
        return 0;
    }

    /* 线性映射：3.0V=0%，4.2V=100% */
    int percent = (int)((volt - BATTERY_VOLTAGE_EMPTY) /
                        (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100.0f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    /* 一阶低通滤波（EMA）：平滑百分比，防 UI 抖动
       y[n] = α·x[n] + (1-α)·y[n-1] */
    if (s_ema_percent < 0.0f) {
        s_ema_percent = (float)percent;                     /* 首次直接采用 */
    } else {
        s_ema_percent = EMA_ALPHA * percent +
                        (1.0f - EMA_ALPHA) * s_ema_percent; /* 指数平滑 */
    }
    return (uint8_t)(s_ema_percent + 0.5f);                 /* 四舍五入 */

    /* 原迟滞方案
    if (s_last_percent >= 0) {
        if (percent >= s_last_percent - HYSTERESIS_STEP &&
            percent <= s_last_percent + HYSTERESIS_STEP) {
            return (uint8_t)s_last_percent;
        }
    }
    s_last_percent = percent;
    return (uint8_t)percent; */
}











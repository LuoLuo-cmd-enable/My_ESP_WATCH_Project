/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include "gui_guider.h"
#include "custom.h"
#include "arc_menu.h"
#include "ui_transition.h"

#include <stdio.h>
#include "lvgl.h"
#include "lvgl_display.h"
#include "SD_card.h"
#include "esp_log.h"
#include "esp_err.h"
#include "novel_progress.h"
#include "img_display.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "onenet_ota.h"
#include "local_ota.h"
#include "st7789_driver.h"
#include "rtc_time_service.h"
#include "get_weather.h"
#include "../game/2048/lv_100ask_2048.h"
#include "../game/memory_game/lv_100ask_memory_game.h"
#include "../game/snake/lv_100ask_snake.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

#define TAG     "EVENT_INIT"

#define MOUNT_POINT "/sdcard"   // SD卡挂载点（用于在文件列表中查找完整路径）

// 全局OTA运行标志：true表示正在OTA升级，用于防止重复触发升级
static volatile bool g_is_ota_running = false;

extern lv_ui guider_ui;   // GUI Guider 生成的全局UI结构体（所有屏幕控件的指针）

extern FILE *fp;                     // 小说文件句柄（由 SD_card.c 定义，这里引用）
extern long g_file_offset;           // 小说当前阅读偏移量（翻页位置）
char full_path[512] = {0};           // 通用文件完整路径缓冲区（/sdcard/xxx）
char img_full_path[512] = {0};       // 图片文件完整路径缓冲区（图片浏览用）
extern char display_buf[1200];       // 小说分页显示缓冲区（由 SD_card.c 定义）
extern SemaphoreHandle_t sntp_trigger_sem;   // NTP同步触发信号量（由 ntp_time.c 定义）

/* ===== 快捷面板（时钟屏向上滑呼出）控件指针 ===== */
static lv_obj_t *s_screen_quick = NULL;            // 快捷面板根对象
static lv_obj_t *s_quick_wifi_btn = NULL;          // WiFi开关按钮
static lv_obj_t *s_quick_wifi_btn_label = NULL;    // WiFi按钮文字
static lv_obj_t *s_quick_time_sync_btn = NULL;     // 时间同步按钮
static lv_obj_t *s_quick_time_sync_btn_label = NULL; // 时间同步按钮文字
static lv_obj_t *s_quick_wifi_status_label = NULL; // WiFi状态标签
static lv_obj_t *s_quick_time_sync_status_label = NULL; // 时间同步状态标签
static lv_obj_t *s_quick_weather_btn = NULL;       // 天气按钮
static lv_obj_t *s_quick_weather_label = NULL;     // 天气内容标签
static lv_obj_t *s_quick_weather_status_label = NULL; // 天气状态标签
static lv_obj_t *s_quick_bri_slider = NULL;        // 亮度调节滑条
static lv_obj_t *s_quick_bri_label = NULL;         // 亮度百分比标签

/* ===== WiFi 开关状态变量 ===== */
static bool s_quick_wifi_enabled = false;      // WiFi 是否已请求开启
static bool s_quick_wifi_connected = false;    // WiFi 是否已连接成功
static bool s_quick_wifi_busy = false;         // 正在执行开关操作（防重复点击）
static uint32_t s_quick_wifi_busy_tick = 0;    // 忙碌开始的系统tick，用于超时判断
static uint32_t s_quick_wifi_busy_timeout_ms = 2500; // 忙碌超时时间 2.5 秒
static lv_timer_t *s_quick_wifi_watchdog_timer = NULL; // 忙碌状态看门狗定时器
static QueueHandle_t s_quick_wifi_cmd_queue = NULL;    // WiFi命令队列（任务间通信）
static TaskHandle_t s_quick_wifi_worker_task_handle = NULL; // WiFi工作任务句柄

// WiFi 开关命令结构体（通过队列发给工作任务）
typedef struct {
    bool enable;   // true=开WiFi, false=关WiFi
} quick_wifi_cmd_t;

/**
 * @brief 更新快捷面板上的亮度百分比标签文字
 * @param value 当前亮度值(10~100)
 */
static void quick_update_brightness_label(int value)
{
    if (s_quick_bri_label != NULL && lv_obj_is_valid(s_quick_bri_label)) {
        lv_label_set_text_fmt(s_quick_bri_label, "%d%%", value);
    }
}

/**
 * @brief 刷新快捷面板 WiFi 按钮和状态标签的显示
 *
 * 根据 s_quick_wifi_enabled / connected / busy 三个状态
 * 设置按钮背景色、禁用状态、以及状态文字（Off/Connected/Connecting...）
 */
static void quick_refresh_wifi_ui(void)
{
    if (s_quick_wifi_btn != NULL && lv_obj_is_valid(s_quick_wifi_btn)) {
        uint32_t color = s_quick_wifi_enabled ? 0x2F80ED : 0x8E8E8E;
        uint32_t pressed_color = s_quick_wifi_enabled ? 0x2469C8 : 0x7A7A7A;
        lv_obj_set_style_bg_color(s_quick_wifi_btn, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_quick_wifi_btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
        if (s_quick_wifi_busy) lv_obj_add_state(s_quick_wifi_btn, LV_STATE_DISABLED);
        else lv_obj_clear_state(s_quick_wifi_btn, LV_STATE_DISABLED);
    }
    if (s_quick_wifi_btn_label != NULL && lv_obj_is_valid(s_quick_wifi_btn_label)) {
        lv_label_set_text(s_quick_wifi_btn_label, "wifi");
        lv_obj_set_style_text_color(s_quick_wifi_btn_label, lv_color_hex(0xFFFFFF), 0);
    }
    if (s_quick_wifi_status_label != NULL && lv_obj_is_valid(s_quick_wifi_status_label)) {
        if (s_quick_wifi_busy) {
            lv_label_set_text(s_quick_wifi_status_label, "Switching...");
            lv_obj_set_style_text_color(s_quick_wifi_status_label, lv_color_hex(0xC8CDD6), 0);
        } else if (!s_quick_wifi_enabled) {
            lv_label_set_text(s_quick_wifi_status_label, "Off");
            lv_obj_set_style_text_color(s_quick_wifi_status_label, lv_color_hex(0x9A9A9A), 0);
        } else if (s_quick_wifi_connected) {
            lv_label_set_text(s_quick_wifi_status_label, "Connected");
            lv_obj_set_style_text_color(s_quick_wifi_status_label, lv_color_hex(0x33CC66), 0);
        } else {
            lv_label_set_text(s_quick_wifi_status_label, "Connecting...");
            lv_obj_set_style_text_color(s_quick_wifi_status_label, lv_color_hex(0xF0B429), 0);
        }
    }
}

/**
 * @brief 设置 WiFi 忙碌状态（防止开关操作期间重复点击）
 * @param busy       true=进入忙碌, false=结束忙碌
 * @param timeout_ms 忙碌超时时间，0 表示使用默认 2500ms
 */
static void quick_set_wifi_busy(bool busy, uint32_t timeout_ms)
{
    s_quick_wifi_busy = busy;
    if (busy) {
        s_quick_wifi_busy_tick = lv_tick_get();
        s_quick_wifi_busy_timeout_ms = (timeout_ms > 0U) ? timeout_ms : 2500U;
    }
    quick_refresh_wifi_ui();
}

/**
 * @brief WiFi 工作任务：阻塞等待命令队列，执行开/关 WiFi
 *
 * 为什么用独立任务？wifi_manager_start/stop 内部可能涉及 WiFi 驱动同步等待，
 * 放任务里执行可避免阻塞 LVGL 渲染线程。
 */
static void quick_wifi_worker_task(void *param)
{
    LV_UNUSED(param);
    quick_wifi_cmd_t cmd = {0};
    while (1) {
        if (xQueueReceive(s_quick_wifi_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            if (cmd.enable) wifi_manager_start();
            else wifi_manager_stop();
        }
    }
}

/**
 * @brief 初始化 WiFi 工作任务和命令队列（只执行一次）
 * @return true=初始化成功 false=失败
 */
static bool quick_wifi_worker_init_once(void)
{
    if (s_quick_wifi_cmd_queue == NULL) {
        s_quick_wifi_cmd_queue = xQueueCreate(4, sizeof(quick_wifi_cmd_t));
        if (s_quick_wifi_cmd_queue == NULL) {
            ESP_LOGE(TAG, "quick wifi queue create failed");
            return false;
        }
    }
    if (s_quick_wifi_worker_task_handle == NULL) {
        if (xTaskCreate(quick_wifi_worker_task, "quick_wifi_task", 3072, NULL, 4,
                        &s_quick_wifi_worker_task_handle) != pdPASS) {
            ESP_LOGE(TAG, "quick wifi worker create failed");
            s_quick_wifi_worker_task_handle = NULL;
            return false;
        }
    }
    return true;
}

/**
 * @brief 向 WiFi 工作任务投递一条开关命令
 * @param enable true=开WiFi false=关WiFi
 * @return true=投递成功 false=队列满/未初始化
 */
static bool quick_wifi_post_set(bool enable)
{
    quick_wifi_cmd_t cmd = {
        .enable = enable
    };
    if (!quick_wifi_worker_init_once()) return false;
    if (xQueueSend(s_quick_wifi_cmd_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "quick wifi queue full");
        return false;
    }
    return true;
}

/**
 * @brief WiFi 忙碌状态看门狗：若开关操作超时仍未结束，强制清除忙碌状态
 *
 * 每 200ms 由 LVGL 定时器调用一次，防止 WiFi 驱动卡死导致按钮永远禁用。
 */
static void quick_wifi_watchdog_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!s_quick_wifi_busy) return;
    if (lv_tick_elaps(s_quick_wifi_busy_tick) >= s_quick_wifi_busy_timeout_ms) {
        quick_set_wifi_busy(false, 0);
    }
}

/**
 * @brief 设置 WiFi 连接状态（由 lvgl_display.c 的 wifi_state_handler 回调调用）
 * @param connected true=已连接 false=未连接
 */
void wifi_quick_set_state(bool connected)
{
    s_quick_wifi_connected = connected;
    if (connected) {
        s_quick_wifi_enabled = true;
    }
    quick_set_wifi_busy(false, 0);
}

/**
 * @brief 更新快捷面板的 NTP 时间同步状态文字
 * @param text      显示文字
 * @param color_hex 文字颜色（0xRRGGBB）
 */
void quick_time_sync_set_status(const char *text, uint32_t color_hex)
{
    if (s_quick_time_sync_status_label == NULL || !lv_obj_is_valid(s_quick_time_sync_status_label)) {
        return;
    }
    lv_label_set_text(s_quick_time_sync_status_label, (text != NULL) ? text : "");
    lv_obj_set_style_text_color(s_quick_time_sync_status_label, lv_color_hex(color_hex), 0);
}

/**
 * @brief 更新天气状态文字（同时更新快捷面板标签和天气屏标签）
 * @param text      显示文字
 * @param color_hex 文字颜色
 */
void weather_ui_set_status(const char *text, uint32_t color_hex)
{
    if (s_quick_weather_status_label != NULL && lv_obj_is_valid(s_quick_weather_status_label)) {
        lv_label_set_text(s_quick_weather_status_label, (text != NULL) ? text : "");
        lv_obj_set_style_text_color(s_quick_weather_status_label, lv_color_hex(color_hex), 0);
    }

    if (guider_ui.screen_weather_label_status != NULL && lv_obj_is_valid(guider_ui.screen_weather_label_status)) {
        lv_label_set_text(guider_ui.screen_weather_label_status, (text != NULL) ? text : "");
        lv_obj_set_style_text_color(guider_ui.screen_weather_label_status, lv_color_hex(color_hex), 0);
    }
}

/**
 * @brief 将时间戳格式化为 "HH:MM" 字符串
 * @param buf  输出缓冲区
 * @param cap  缓冲区大小
 * @param t    时间戳（秒）
 */
static void weather_format_hhmm(char *buf, size_t cap, time_t t)
{
    if (buf == NULL || cap == 0) return;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    snprintf(buf, cap, "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
}

/**
 * @brief 从天气快照刷新所有天气 UI（快捷面板标签 + 天气屏所有标签）
 *
 * 数据无效时显示 "--"，并根据状态码显示不同颜色（Syncing/No WiFi/HTTP fail/OK）
 */
void weather_ui_refresh_from_snapshot(void)
{
    weather_snapshot_t snap;
    bool ok = weather_get_snapshot(&snap);

    const char *condition = (ok && snap.valid && snap.condition[0] != '\0') ? snap.condition : "--";
    const char *city = (ok && snap.valid && snap.city[0] != '\0') ? snap.city : "--";

    if (s_quick_weather_label != NULL && lv_obj_is_valid(s_quick_weather_label)) {
        lv_label_set_text(s_quick_weather_label, condition);
    }

    if (guider_ui.screen_weather_label_city != NULL && lv_obj_is_valid(guider_ui.screen_weather_label_city)) {
        lv_label_set_text_fmt(guider_ui.screen_weather_label_city, "City: %s", city);
    }
    if (guider_ui.screen_weather_label_condition != NULL && lv_obj_is_valid(guider_ui.screen_weather_label_condition)) {
        lv_label_set_text_fmt(guider_ui.screen_weather_label_condition, "Weather: %s", condition);
    }

    if (guider_ui.screen_weather_label_temp != NULL && lv_obj_is_valid(guider_ui.screen_weather_label_temp)) {
        if (ok && snap.valid) {
            lv_label_set_text_fmt(guider_ui.screen_weather_label_temp, "Temp: %d~%d C", (int)snap.low, (int)snap.high);
        } else {
            lv_label_set_text(guider_ui.screen_weather_label_temp, "Temp: --");
        }
    }
    if (guider_ui.screen_weather_label_humidity != NULL && lv_obj_is_valid(guider_ui.screen_weather_label_humidity)) {
        if (ok && snap.valid) {
            lv_label_set_text_fmt(guider_ui.screen_weather_label_humidity, "Humidity: %u%%", (unsigned)snap.humidity);
        } else {
            lv_label_set_text(guider_ui.screen_weather_label_humidity, "Humidity: --");
        }
    }
    if (guider_ui.screen_weather_label_update != NULL && lv_obj_is_valid(guider_ui.screen_weather_label_update)) {
        if (ok && snap.valid && snap.last_update != 0) {
            char hhmm[16];
            weather_format_hhmm(hhmm, sizeof(hhmm), snap.last_update);
            lv_label_set_text_fmt(guider_ui.screen_weather_label_update, "Updated: %s", hhmm);
        } else {
            lv_label_set_text(guider_ui.screen_weather_label_update, "Updated: --");
        }
    }

    if (!ok) {
        weather_ui_set_status("No data", 0x9A9A9A);
        return;
    }

    switch (snap.status) {
        case WEATHER_SYNCING:
            weather_ui_set_status("Syncing...", 0xF0B429);
            break;
        case WEATHER_NO_WIFI:
            weather_ui_set_status("No WiFi", 0x9A9A9A);
            break;
        case WEATHER_HTTP_FAIL:
            weather_ui_set_status("HTTP fail", 0xFF3333);
            break;
        case WEATHER_PARSE_FAIL:
            weather_ui_set_status("Parse fail", 0xFF3333);
            break;
        case WEATHER_OK:
        default:
            if (snap.valid && snap.last_update != 0) {
                char hhmm[16];
                weather_format_hhmm(hhmm, sizeof(hhmm), snap.last_update);
                char status[32];
                snprintf(status, sizeof(status), "OK %s", hhmm);
                weather_ui_set_status(status, 0x33CC66);
            } else {
                weather_ui_set_status("OK", 0x33CC66);
            }
            break;
    }
}

/**
 * @brief 加载时钟屏（若未创建则先创建）
 */
static void quick_screen_load_clock(void)
{
    if (guider_ui.clock_screen == NULL || !lv_obj_is_valid(guider_ui.clock_screen)) {
        setup_scr_clock_screen(&guider_ui);
    }
    lv_scr_load_anim(guider_ui.clock_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/**
 * @brief 打开 WiFi 设置屏（若未创建则先创建）
 */
static void quick_screen_open_wifi_setting(void)
{
    if (guider_ui.screen_wifi_set == NULL || !lv_obj_is_valid(guider_ui.screen_wifi_set)) {
        setup_scr_screen_wifi_set(&guider_ui);
    }
    lv_scr_load_anim(guider_ui.screen_wifi_set, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/**
 * @brief 打开时间设置屏（若未创建则先创建）
 */
static void quick_screen_open_time_setting(void)
{
    if (guider_ui.screen_time_set == NULL || !lv_obj_is_valid(guider_ui.screen_time_set)) {
        setup_scr_screen_time_set(&guider_ui);
    }
    lv_scr_load_anim(guider_ui.screen_time_set, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/**
 * @brief 打开天气屏，并刷新天气数据（若未创建则先创建）
 */
static void quick_screen_open_weather_screen(void)
{
    if (guider_ui.screen_weather == NULL || !lv_obj_is_valid(guider_ui.screen_weather)) {
        setup_scr_screen_weather(&guider_ui);
    }
    weather_ui_refresh_from_snapshot();
    lv_scr_load_anim(guider_ui.screen_weather, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/**
 * @brief 快捷面板手势事件：向上滑动 → 返回时钟屏
 */
static void quick_screen_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) indev = lv_indev_get_act();
    if (indev != NULL && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        quick_screen_load_clock();
    }
}

/**
 * @brief 亮度滑条事件：拖动时实时调节背光亮度（范围限制 10~100%）
 */
static void quick_brightness_slider_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    int val = lv_slider_get_value(lv_event_get_target(e));
    if (val < 10) val = 10;
    if (val > 100) val = 100;
    st7789_lcd_set_brightness((uint8_t)val);
    quick_update_brightness_label(val);
}

/**
 * @brief 快捷面板 WiFi 按钮事件
 *
 * 短按：切换 WiFi 开/关（投递命令给工作任务，期间禁用按钮防重复操作）
 * 长按：进入 WiFi 设置屏
 */
static void quick_wifi_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        if (s_quick_wifi_busy) return;

        bool prev_enabled = s_quick_wifi_enabled;
        bool prev_connected = s_quick_wifi_connected;
        bool next_enabled = !s_quick_wifi_enabled;

        s_quick_wifi_enabled = next_enabled;
        if (!next_enabled) {
            s_quick_wifi_connected = false;
        }
        quick_set_wifi_busy(true, 2500);

        if (!quick_wifi_post_set(next_enabled)) {
            s_quick_wifi_enabled = prev_enabled;
            s_quick_wifi_connected = prev_connected;
            quick_set_wifi_busy(false, 0);
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        quick_screen_open_wifi_setting();
    }
}

/**
 * @brief 快捷面板时间同步按钮事件
 *
 * 短按：通过信号量触发 NTP 同步
 * 长按：进入时间设置屏
 */
static void quick_time_sync_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        quick_time_sync_set_status("Syncing...", 0xF0B429);
        if (sntp_trigger_sem != NULL) {
            xSemaphoreGive(sntp_trigger_sem);
            ESP_LOGI(TAG, "quick panel ntp sync trigger sent");
        } else {
            quick_time_sync_set_status("NTP unavailable", 0xFF3333);
            ESP_LOGW(TAG, "quick panel ntp trigger unavailable");
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        quick_screen_open_time_setting();
    }
}

/**
 * @brief 快捷面板天气按钮事件
 *
 * 短按：重新请求天气同步
 * 长按：打开天气屏
 */
static void quick_weather_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        weather_ui_set_status("Syncing...", 0xF0B429);
        weather_request_sync();
    } else if (code == LV_EVENT_LONG_PRESSED) {
        quick_screen_open_weather_screen();
    }
}

/**
 * @brief 打开快捷面板：首次进入时动态创建全部控件
 *
 * 布局：顶部两个按钮(WiFi/时间同步) + 亮度滑条 + 天气按钮
 * 之后再次进入只刷新数值，不重建控件。
 */
static void open_clock_quick_screen(void)
{
    int cur_bri = (int)st7789_lcd_get_brightness();
    if (cur_bri < 10 || cur_bri > 100) cur_bri = 30;

    if (s_screen_quick == NULL || !lv_obj_is_valid(s_screen_quick)) {
        const int quick_btn_w = 96;
        const int quick_btn_h = 38;
        s_screen_quick = lv_obj_create(NULL);
        lv_obj_set_size(s_screen_quick, 240, 284);
        lv_obj_clear_flag(s_screen_quick, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(s_screen_quick, lv_color_hex(0x0B1220), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_screen_quick, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(s_screen_quick, quick_screen_event_handler, LV_EVENT_GESTURE, NULL);

        // WiFi 开关按钮（左上）
        s_quick_wifi_btn = lv_btn_create(s_screen_quick);
        lv_obj_set_size(s_quick_wifi_btn, quick_btn_w, quick_btn_h);
        lv_obj_align(s_quick_wifi_btn, LV_ALIGN_TOP_MID, -60, 14);
        lv_obj_set_style_radius(s_quick_wifi_btn, 12, 0);
        lv_obj_set_style_border_width(s_quick_wifi_btn, 0, 0);
        lv_obj_add_event_cb(s_quick_wifi_btn, quick_wifi_btn_event_cb, LV_EVENT_ALL, NULL);

        s_quick_wifi_btn_label = lv_label_create(s_quick_wifi_btn);
        lv_obj_set_style_text_font(s_quick_wifi_btn_label, &songti_font_16, 0);
        lv_obj_center(s_quick_wifi_btn_label);

        // 时间同步按钮（右上）
        s_quick_time_sync_btn = lv_btn_create(s_screen_quick);
        lv_obj_set_size(s_quick_time_sync_btn, quick_btn_w, quick_btn_h);
        lv_obj_align(s_quick_time_sync_btn, LV_ALIGN_TOP_MID, 60, 14);
        lv_obj_set_style_radius(s_quick_time_sync_btn, 12, 0);
        lv_obj_set_style_border_width(s_quick_time_sync_btn, 0, 0);
        lv_obj_set_style_bg_color(s_quick_time_sync_btn, lv_color_hex(0x4CAF50), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_quick_time_sync_btn, lv_color_hex(0x3D9142), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(s_quick_time_sync_btn, quick_time_sync_btn_event_cb, LV_EVENT_ALL, NULL);

        s_quick_time_sync_btn_label = lv_label_create(s_quick_time_sync_btn);
        lv_label_set_text(s_quick_time_sync_btn_label, "sync");
        lv_obj_set_style_text_font(s_quick_time_sync_btn_label, &songti_font_16, 0);
        lv_obj_set_style_text_color(s_quick_time_sync_btn_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(s_quick_time_sync_btn_label);

        // WiFi 状态标签（WiFi按钮下方）
        s_quick_wifi_status_label = lv_label_create(s_screen_quick);
        lv_obj_set_width(s_quick_wifi_status_label, quick_btn_w);
        lv_obj_align_to(s_quick_wifi_status_label, s_quick_wifi_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
        lv_obj_set_style_text_font(s_quick_wifi_status_label, &songti_font_16, 0);
        lv_obj_set_style_text_align(s_quick_wifi_status_label, LV_TEXT_ALIGN_CENTER, 0);

        // 时间同步状态标签（同步按钮下方）
        s_quick_time_sync_status_label = lv_label_create(s_screen_quick);
        lv_obj_set_width(s_quick_time_sync_status_label, quick_btn_w);
        lv_obj_align_to(s_quick_time_sync_status_label, s_quick_time_sync_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
        lv_obj_set_style_text_font(s_quick_time_sync_status_label, &songti_font_16, 0);
        lv_obj_set_style_text_align(s_quick_time_sync_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(s_quick_time_sync_status_label, "");
        lv_obj_set_style_text_color(s_quick_time_sync_status_label, lv_color_hex(0xC8CDD6), 0);

        // 亮度滑条（中上区域）
        s_quick_bri_slider = lv_slider_create(s_screen_quick);
        lv_slider_set_range(s_quick_bri_slider, 10, 100);
        lv_obj_set_size(s_quick_bri_slider, 168, 12);
        lv_obj_align(s_quick_bri_slider, LV_ALIGN_TOP_MID, 0, 76);
        lv_obj_add_event_cb(s_quick_bri_slider, quick_brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // 亮度百分比标签（滑条右侧）
        s_quick_bri_label = lv_label_create(s_screen_quick);
        lv_obj_set_style_text_font(s_quick_bri_label, &songti_font_16, 0);
        lv_obj_align_to(s_quick_bri_label, s_quick_bri_slider, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

        // 天气按钮（滑条下方）
        s_quick_weather_btn = lv_btn_create(s_screen_quick);
        lv_obj_set_size(s_quick_weather_btn, 200, 44);
        lv_obj_align_to(s_quick_weather_btn, s_quick_bri_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 18);
        lv_obj_set_style_radius(s_quick_weather_btn, 10, 0);
        lv_obj_set_style_border_width(s_quick_weather_btn, 0, 0);
        lv_obj_set_style_bg_color(s_quick_weather_btn, lv_color_hex(0x2D3748), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_quick_weather_btn, lv_color_hex(0x1F2937), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(s_quick_weather_btn, quick_weather_btn_event_cb, LV_EVENT_ALL, NULL);

        // 天气内容标签（天气按钮内部居中）
        s_quick_weather_label = lv_label_create(s_quick_weather_btn);
        lv_label_set_text(s_quick_weather_label, "--");
        lv_obj_set_style_text_font(s_quick_weather_label, &songti_font_16, 0);
        lv_obj_set_style_text_color(s_quick_weather_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(s_quick_weather_label);

        // 天气状态标签（天气按钮下方）
        s_quick_weather_status_label = lv_label_create(s_screen_quick);
        lv_obj_set_width(s_quick_weather_status_label, 200);
        lv_obj_align_to(s_quick_weather_status_label, s_quick_weather_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
        lv_obj_set_style_text_font(s_quick_weather_status_label, &songti_font_16, 0);
        lv_obj_set_style_text_align(s_quick_weather_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(s_quick_weather_status_label, "");
        lv_obj_set_style_text_color(s_quick_weather_status_label, lv_color_hex(0xC8CDD6), 0);
    }

    // 创建 WiFi 忙碌看门狗定时器（每 200ms 检查一次）
    if (s_quick_wifi_watchdog_timer == NULL) {
        s_quick_wifi_watchdog_timer = lv_timer_create(quick_wifi_watchdog_cb, 200, NULL);
    }

    // 每次进入刷新：亮度滑条位置、亮度文字、WiFi状态、天气
    if (s_quick_bri_slider != NULL && lv_obj_is_valid(s_quick_bri_slider)) {
        lv_slider_set_value(s_quick_bri_slider, cur_bri, LV_ANIM_OFF);
    }
    (void)quick_wifi_worker_init_once();
    quick_update_brightness_label(cur_bri);
    quick_refresh_wifi_ui();
    weather_ui_refresh_from_snapshot();
    lv_scr_load_anim(s_screen_quick, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/* 手势切屏时，将缩放动画中心设置为手势触摸点 */
static void set_gesture_center(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) indev = lv_indev_get_act();
    if (indev != NULL) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        ui_transition_set_center(p.x, p.y);
    }
}

/**
 * @brief 时钟屏手势事件处理
 *
 * 上滑：打开主菜单（若弧形菜单未打开）
 * 下滑：打开快捷面板
 * 右滑：打开弧形菜单
 * 左滑：关闭弧形菜单
 */
static void clock_screen_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_TOP:
        {
            if (arc_menu_is_open()) {
                break; /* 弧形菜单打开时：不响应该手势，交给菜单自己处理 */
            }
            if (ui_transition_is_busy()) {
                break; /* 切屏动画进行中，防冒泡重复触发 */
            }
            /* 非阻塞切屏：跳过 wait_release，避免唤醒时死锁 */
            set_gesture_center(e);
            ui_load_scr_with_zoom(&guider_ui, &guider_ui.menu_screen, guider_ui.menu_screen_del, &guider_ui.clock_screen_del, setup_scr_menu_screen, LV_SCR_LOAD_ANIM_NONE, 300, 0, false, false);
            break;
        }
        case LV_DIR_BOTTOM:
        {
            if (arc_menu_is_open()) {
                break; /* 弧形菜单打开时：交给菜单处理垂直拖动 */
            }
            open_clock_quick_screen();
            break;
        }
        case LV_DIR_RIGHT:
        {
            // 右滑：打开弧形快捷菜单
            arc_menu_open();
            break;
        }
        case LV_DIR_LEFT:
        {
            if (arc_menu_is_open()) {
                arc_menu_close();
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 时钟屏内容容器手势事件（与时钟屏手势逻辑相同，作用于内部容器）
 *
 * 主要为了让容器区域（时间/日期显示区）也能响应滑动操作，
 * 逻辑与 clock_screen_event_handler 完全一致。
 */
static void clock_screen_cont_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            arc_menu_open();
        } else if (dir == LV_DIR_LEFT) {
            if (arc_menu_is_open()) {
                arc_menu_close();
            }
        } else if (dir == LV_DIR_TOP) {
            if (arc_menu_is_open()) {
                break;
            }
            if (ui_transition_is_busy()) {
                break; /* 切屏动画进行中，防冒泡重复触发 */
            }
            /* 非阻塞切屏：跳过 wait_release，避免唤醒时死锁 */
            set_gesture_center(e);
            ui_load_scr_with_zoom(&guider_ui, &guider_ui.menu_screen, guider_ui.menu_screen_del, &guider_ui.clock_screen_del, setup_scr_menu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, false);
        } else if (dir == LV_DIR_BOTTOM) {
            if (arc_menu_is_open()) {
                break; /* 弧形菜单打开时：交给菜单处理垂直拖动 */
            }
            open_clock_quick_screen();
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 点击时钟屏时间文字时循环切换字体颜色（6 种颜色）
 *
 * 仅在点击目标为时间标签、且弧形菜单未打开时生效。
 */
void clock_screen_color_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    if (arc_menu_is_open()) return;
    if (lv_event_get_target(e) != guider_ui.clock_screen_label_time) return;

    // 可循环切换的颜色列表（主题色）
    static const uint32_t colors[] = {
        0xF5F0E1,  // 米白
        0xFF6B6B,  // 珊瑚红
        0x4ECDC4,  // 青绿
        0xFFE66D,  // 金黄
        0x95E1D3,  // 薄荷绿
        0x000000,  // 纯黑
    };
    static uint8_t color_idx = 0;

    color_idx = (color_idx + 1) % (sizeof(colors) / sizeof(colors[0]));

    lv_obj_set_style_text_color(guider_ui.clock_screen_label_time, lv_color_hex(colors[color_idx]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(guider_ui.clock_screen_label_date, lv_color_hex(colors[color_idx]), LV_PART_MAIN | LV_STATE_DEFAULT);
}

/**
 * @brief 初始化时钟屏事件绑定
 *
 * - 取消滚动标志（时钟屏不可滚动）
 * - 时间/日期标签设为可点击（支持点击换色）且手势事件可冒泡
 * - 绑定时钟屏及容器的手势回调
 */
void events_init_clock_screen (lv_ui *ui)
{
    lv_obj_clear_flag(ui->clock_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui->clock_screen_cont_2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(ui->clock_screen_label_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui->clock_screen_label_date, LV_OBJ_FLAG_CLICKABLE);

    //注册冒泡flag使其能向上传导
    lv_obj_add_flag(ui->clock_screen_cont_2, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(ui->clock_screen_label_time, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(ui->clock_screen_label_date, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_add_event_cb(ui->clock_screen, clock_screen_event_handler, LV_EVENT_GESTURE, ui);
    lv_obj_add_event_cb(ui->clock_screen_cont_2, clock_screen_cont_2_event_handler, LV_EVENT_GESTURE, ui);
}

/**
 * @brief 主菜单屏手势事件：下滑返回时钟屏
 */
static void menu_screen_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_BOTTOM:
        {
            // 下滑返回时钟屏（非阻塞切屏，避免唤醒时死锁）
            set_gesture_center(e);
            ui_load_scr_with_zoom(&guider_ui, &guider_ui.clock_screen, guider_ui.clock_screen_del, &guider_ui.menu_screen_del, setup_scr_clock_screen, LV_SCR_LOAD_ANIM_NONE, 300, 0, false, false);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 主菜单第 1 项（小说）点击：切换到小说列表屏
 */
void menu_screen_list_1_item0_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        set_gesture_center(e);
        ui_load_scr_with_zoom(&guider_ui, &guider_ui.novel_list, guider_ui.novel_list_del, &guider_ui.menu_screen_del, setup_scr_novel_list, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 主菜单第 3 项（设置）点击：切换到设置屏
 *
 * 先确认当前活动屏是主菜单，防止切屏动画期间重复触发。
 */
void menu_screen_list_1_item2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        // 仅当主菜单为当前活动屏时才响应，防止切屏动画期间重复触发
        if (lv_scr_act() != guider_ui.menu_screen) return;
        set_gesture_center(e);
        ui_load_scr_with_zoom(&guider_ui, &guider_ui.setting_screen, guider_ui.setting_screen_del, &guider_ui.menu_screen_del, setup_scr_setting_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 主菜单第 2 项（图片浏览）点击：切换到图片列表屏
 */
void menu_screen_list_1_item1_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        set_gesture_center(e);
        ui_load_scr_with_zoom(&guider_ui,
            &guider_ui.screen_img_list, guider_ui.screen_img_list_del,
            &guider_ui.menu_screen_del, setup_scr_screen_img_list,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief 主菜单第 4 项（视频）点击：切换到视频列表屏
 */
void menu_screen_list_1_item3_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        set_gesture_center(e);
        ui_load_scr_with_zoom(&guider_ui,
            &guider_ui.video_list, guider_ui.video_list_del,
            &guider_ui.menu_screen_del, setup_scr_video_list,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief 主菜单第 5 项（游戏）点击：切换到游戏列表屏
 */
void menu_screen_list_1_item4_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        set_gesture_center(e);
        ui_load_scr_with_zoom(&guider_ui,
            &guider_ui.screen_game, guider_ui.screen_game_del,
            &guider_ui.menu_screen_del, setup_scr_screen_game,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief 初始化主菜单屏事件绑定（手势 + 5 个列表项点击 + 可滑动菜单）
 */
void events_init_menu_screen (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->menu_screen, menu_screen_event_handler, LV_EVENT_ALL, ui);

    // 创建可滑动菜单（custom 模块提供的横向滑动列表）
    create_swipeable_menu(ui);
}

/**
 * @brief 游戏列表屏手势：右滑返回主菜单
 */
static void screen_game_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.menu_screen, guider_ui.menu_screen_del,
                &guider_ui.screen_game_del, setup_scr_menu_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

/* ===== 2048 游戏相关控件指针 ===== */
static lv_obj_t *s_screen_2048 = NULL;      // 2048 屏幕根对象
static lv_obj_t *s_obj_2048 = NULL;         // 2048 游戏组件
static lv_obj_t *s_label_2048_score = NULL; // 2048 分数标签

/**
 * @brief 更新 2048 分数标签（根据游戏状态显示 SCORE / YOU WIN / GAME OVER）
 */
static void screen_2048_update_score_text(void)
{
    if (s_label_2048_score == NULL || s_obj_2048 == NULL) {
        return;
    }

    if (lv_100ask_2048_get_best_tile(s_obj_2048) >= 2048) {
        lv_label_set_text(s_label_2048_score, "YOU WIN!");
        return;
    }

    if (lv_100ask_2048_get_status(s_obj_2048)) {
        lv_label_set_text(s_label_2048_score, "GAME OVER");
        return;
    }

    lv_label_set_text_fmt(s_label_2048_score, "SCORE: %d", lv_100ask_2048_get_score(s_obj_2048));
}

/**
 * @brief 2048 数值变化事件（每次合并后刷新分数）
 */
static void screen_2048_value_changed_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    screen_2048_update_score_text();
}

/**
 * @brief 2048 "New" 按钮点击：开始新游戏并刷新分数
 */
static void screen_2048_new_game_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s_obj_2048 == NULL) {
        return;
    }
    lv_100ask_2048_set_new_game(s_obj_2048);
    screen_2048_update_score_text();
}

/**
 * @brief 2048 返回游戏列表屏
 */
static void screen_2048_back_to_game_list(void)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (indev != NULL) {
        /* non-blocking: skip wait_release to avoid wakeup deadlock */
    }

    if (guider_ui.screen_game != NULL) {
        lv_scr_load_anim(guider_ui.screen_game, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

/**
 * @brief 2048 "Back" 按钮点击：返回游戏列表
 */
static void screen_2048_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    screen_2048_back_to_game_list();
}

/**
 * @brief 创建并打开 2048 游戏屏（含 Back/New 按钮、分数标签、游戏组件）
 */
static void open_game_2048_screen(void)
{
    s_screen_2048 = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen_2048, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen_2048, lv_color_hex(0xF6F0E8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_screen_2048, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 返回按钮（左上角）
    lv_obj_t *btn_back = lv_btn_create(s_screen_2048);
    lv_obj_set_size(btn_back, 62, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 16, 8);
    lv_obj_add_event_cb(btn_back, screen_2048_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_center(label_back);

    // 新游戏按钮（右上角）
    lv_obj_t *btn_new_game = lv_btn_create(s_screen_2048);
    lv_obj_set_size(btn_new_game, 78, 30);
    lv_obj_align(btn_new_game, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_add_event_cb(btn_new_game, screen_2048_new_game_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_new = lv_label_create(btn_new_game);
    lv_label_set_text(label_new, "New");
    lv_obj_center(label_new);

    // 分数标签（顶部居中）
    s_label_2048_score = lv_label_create(s_screen_2048);
    lv_obj_align(s_label_2048_score, LV_ALIGN_TOP_MID, 0, 16);

    // 2048 游戏组件（底部 220x220）
    s_obj_2048 = lv_100ask_2048_create(s_screen_2048);
    lv_obj_set_size(s_obj_2048, 220, 220);
    lv_obj_align(s_obj_2048, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_event_cb(s_obj_2048, screen_2048_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    screen_2048_update_score_text();
    lv_scr_load_anim(s_screen_2048, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/* ===== 记忆游戏控件指针 ===== */
static lv_obj_t *s_screen_memory = NULL;
static lv_obj_t *s_obj_memory = NULL;
/* ===== 贪吃蛇控件指针 ===== */
static lv_obj_t *s_screen_snake = NULL; // 贪吃蛇屏根对象
static lv_obj_t *s_obj_snake = NULL;    // 贪吃蛇组件
static lv_obj_t *s_label_snake_score = NULL; // 贪吃蛇分数标签
static lv_point_t s_snake_touch_anchor = {0, 0};// 贪吃蛇触摸起点
static bool s_snake_touch_active = false; // 贪吃蛇触摸进行中
/* ===== Flappy Bird 控件指针 ===== */
static lv_obj_t *s_flappy_screen = NULL; // Flappy屏根对象
static lv_obj_t *s_flappy_back_btn = NULL;// Flappy返回按钮
static lv_obj_t *s_flappy_bird = NULL; // Flappy小鸟
static lv_obj_t *s_flappy_pipe_top = NULL; // Flappy上方水管
static lv_obj_t *s_flappy_pipe_bottom = NULL; // Flappy下方水管
static lv_obj_t *s_flappy_score_label = NULL; // Flappy分数标签
static lv_timer_t *s_flappy_timer = NULL; // Flappy物理刷新定时器
static int16_t s_flappy_bird_y = 120;      // 小鸟Y坐标
static int16_t s_flappy_bird_vy = 0;       // 小鸟Y速度
static int16_t s_flappy_pipe_x = 240;      // 水管X坐标
static int16_t s_flappy_gap_y = 90;        // 水管缺口Y
static uint16_t s_flappy_score = 0;        // Flappy得分
static bool s_flappy_alive = true;         // 小鸟存活
static bool s_flappy_pipe_passed = false;  // 是否已过当前水管

/**
 * @brief 记忆游戏返回游戏列表屏
 */
static void screen_memory_back_to_game_list(void)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (indev != NULL) {
        /* non-blocking: skip wait_release to avoid wakeup deadlock */
    }

    if (guider_ui.screen_game != NULL) {
        lv_scr_load_anim(guider_ui.screen_game, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

/**
 * @brief 记忆游戏 "Back" 按钮点击：返回游戏列表
 */
static void screen_memory_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    screen_memory_back_to_game_list();
}

/**
 * @brief 记忆游戏 "New" 按钮点击：重置 4x4 牌局
 */
static void screen_memory_new_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s_obj_memory == NULL) {
        return;
    }
    lv_100ask_memory_game_set_map(s_obj_memory, 4, 4);
}

/**
 * @brief 创建并打开记忆游戏屏（Back/New 按钮 + 4x4 翻牌区）
 */
static void open_game_memory_screen(void)
{
    s_screen_memory = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen_memory, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen_memory, lv_color_hex(0xF1F6FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_screen_memory, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 返回按钮
    lv_obj_t *btn_back = lv_btn_create(s_screen_memory);
    lv_obj_set_size(btn_back, 62, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 16, 8);
    lv_obj_add_event_cb(btn_back, screen_memory_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_center(label_back);

    // 新游戏按钮
    lv_obj_t *btn_new = lv_btn_create(s_screen_memory);
    lv_obj_set_size(btn_new, 78, 30);
    lv_obj_align(btn_new, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_add_event_cb(btn_new, screen_memory_new_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_new = lv_label_create(btn_new);
    lv_label_set_text(label_new, "New");
    lv_obj_center(label_new);

    // 标题
    lv_obj_t *title = lv_label_create(s_screen_memory);
    lv_label_set_text(title, "Memory Game");
    lv_obj_set_style_text_font(title, &songti_font_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    // 4x4 翻牌游戏组件
    s_obj_memory = lv_100ask_memory_game_create(s_screen_memory);
    lv_obj_set_size(s_obj_memory, 220, 220);
    lv_obj_align(s_obj_memory, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_100ask_memory_game_set_map(s_obj_memory, 4, 4);

    lv_scr_load_anim(s_screen_memory, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}


/**
 * @brief 更新贪吃蛇分数标签（游戏结束时显示最终得分）
 */
static void screen_snake_update_score_text(void)
{
    if (s_label_snake_score == NULL || s_obj_snake == NULL) {
        return;
    }

    if (lv_100ask_snake_is_over(s_obj_snake)) {
        lv_label_set_text_fmt(s_label_snake_score, "GAME OVER  SCORE:%d", lv_100ask_snake_get_score(s_obj_snake));
        return;
    }

    lv_label_set_text_fmt(s_label_snake_score, "SCORE: %d", lv_100ask_snake_get_score(s_obj_snake));
}

/**
 * @brief 贪吃蛇返回游戏列表屏
 */
static void screen_snake_back_to_game_list(void)
{
    if (guider_ui.screen_game != NULL) {
        lv_scr_load_anim(guider_ui.screen_game, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

/**
 * @brief 贪吃蛇 "Back" 按钮点击：返回游戏列表
 */
static void screen_snake_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    screen_snake_back_to_game_list();
}

/**
 * @brief 贪吃蛇 "New" 按钮点击：开始新游戏并刷新分数
 */
static void screen_snake_new_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s_obj_snake == NULL) {
        return;
    }
    lv_100ask_snake_new_game(s_obj_snake);
    screen_snake_update_score_text();
}

/**
 * @brief 贪吃蛇分数变化事件（吃到食物后刷新）
 */
static void screen_snake_value_changed_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    screen_snake_update_score_text();
}

/**
 * @brief 贪吃蛇手势/触摸控制：支持滑动方向和拖拽方向切换蛇头朝向
 *
 * 处理的事件：PRESSED(记录起点) / RELEASED / PRESSING(拖拽定向) / GESTURE(滑动定向)
 */
static void screen_snake_gesture_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev;

    if (s_obj_snake == NULL) {
        return;
    }

    indev = lv_event_get_indev(e);
    if (indev == NULL) {
        indev = lv_indev_get_act();
    }
    if (indev == NULL) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        // 按下：记录触摸起点，进入拖拽模式
        lv_indev_get_point(indev, &s_snake_touch_anchor);
        s_snake_touch_active = true;
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // 松开或丢失按压：退出拖拽模式
        s_snake_touch_active = false;
        return;
    }

    if (code == LV_EVENT_PRESSING && s_snake_touch_active) {
        // 拖拽中：根据位移矢量决定蛇头方向
        lv_point_t vect;
        lv_point_t p;
        lv_indev_get_vect(indev, &vect);
        lv_indev_get_point(indev, &p);

        if (LV_ABS(vect.x) >= 2 || LV_ABS(vect.y) >= 2) {
            if (LV_ABS(vect.x) >= LV_ABS(vect.y)) {
                lv_100ask_snake_set_dir(s_obj_snake, (vect.x > 0) ? LV_DIR_RIGHT : LV_DIR_LEFT);
            } else {
                lv_100ask_snake_set_dir(s_obj_snake, (vect.y > 0) ? LV_DIR_BOTTOM : LV_DIR_TOP);
            }
            s_snake_touch_anchor = p;
            return;
        }

        {
            // 相对起点的累计位移判断（阈值 4px）
            int dx = p.x - s_snake_touch_anchor.x;
            int dy = p.y - s_snake_touch_anchor.y;
            if (LV_ABS(dx) >= 4 || LV_ABS(dy) >= 4) {
                if (LV_ABS(dx) >= LV_ABS(dy)) {
                    lv_100ask_snake_set_dir(s_obj_snake, (dx > 0) ? LV_DIR_RIGHT : LV_DIR_LEFT);
                } else {
                    lv_100ask_snake_set_dir(s_obj_snake, (dy > 0) ? LV_DIR_BOTTOM : LV_DIR_TOP);
                }
                s_snake_touch_anchor = p;
            }
        }
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        // 滑动手势：直接按手势方向转向
        lv_dir_t dir = lv_indev_get_gesture_dir(indev);
        if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT || dir == LV_DIR_TOP || dir == LV_DIR_BOTTOM) {
            lv_100ask_snake_set_dir(s_obj_snake, dir);
        }
    }
}

/**
 * @brief 创建并打开贪吃蛇屏（Back/New 按钮 + 分数 + 游戏区，支持手势控制）
 */
static void open_game_snake_screen(void)
{
    s_screen_snake = lv_obj_create(NULL);
    lv_obj_add_flag(s_screen_snake, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_screen_snake, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(s_screen_snake, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(s_screen_snake, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen_snake, lv_color_hex(0xEEF7E7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_screen_snake, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 返回按钮
    lv_obj_t *btn_back = lv_btn_create(s_screen_snake);
    lv_obj_set_size(btn_back, 62, 36);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 16, 8);
    lv_obj_add_event_cb(btn_back, screen_snake_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_center(label_back);

    // 新游戏按钮
    lv_obj_t *btn_new = lv_btn_create(s_screen_snake);
    lv_obj_set_size(btn_new, 78, 30);
    lv_obj_align(btn_new, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_add_event_cb(btn_new, screen_snake_new_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_new = lv_label_create(btn_new);
    lv_label_set_text(label_new, "New");
    lv_obj_center(label_new);

    // 标题（贪吃蛇）
    lv_obj_t *title = lv_label_create(s_screen_snake);
    lv_label_set_text(title, "\xE8\xB4\xAA\xE5\x90\x83\xE8\x9B\x87");
    lv_obj_set_style_text_font(title, &songti_font_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    // 分数标签
    s_label_snake_score = lv_label_create(s_screen_snake);
    lv_obj_align(s_label_snake_score, LV_ALIGN_TOP_MID, 0, 40);

    // 贪吃蛇游戏组件（底部 224x224）
    s_obj_snake = lv_100ask_snake_create(s_screen_snake);
    lv_obj_add_flag(s_obj_snake, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_obj_snake, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(s_obj_snake, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(s_obj_snake, 224, 224);
    lv_obj_align(s_obj_snake, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_add_event_cb(s_obj_snake, screen_snake_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_obj_snake, screen_snake_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_obj_snake, screen_snake_gesture_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_obj_snake, screen_snake_gesture_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_obj_snake, screen_snake_gesture_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(s_obj_snake, screen_snake_gesture_cb, LV_EVENT_PRESSING, NULL);

    // 整屏也响应手势/触摸（事件冒泡到游戏组件）
    lv_obj_add_event_cb(s_screen_snake, screen_snake_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_screen_snake, screen_snake_gesture_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_screen_snake, screen_snake_gesture_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_screen_snake, screen_snake_gesture_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(s_screen_snake, screen_snake_gesture_cb, LV_EVENT_PRESSING, NULL);

    screen_snake_update_score_text();
    lv_scr_load_anim(s_screen_snake, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}


/**
 * @brief 刷新 Flappy Bird 画面（小鸟位置、上下水管、分数）
 */
static void flappy_refresh_ui(void)
{
    if (s_flappy_bird != NULL) {
        lv_obj_set_pos(s_flappy_bird, 52, s_flappy_bird_y);
    }

    if (s_flappy_pipe_top != NULL) {
        lv_obj_set_pos(s_flappy_pipe_top, s_flappy_pipe_x, 0);
        lv_obj_set_size(s_flappy_pipe_top, 28, s_flappy_gap_y);
    }

    if (s_flappy_pipe_bottom != NULL) {
        lv_coord_t bottom_y = s_flappy_gap_y + 90;
        lv_coord_t bottom_h = 284 - bottom_y;
        if (bottom_h < 1) bottom_h = 1;
        lv_obj_set_pos(s_flappy_pipe_bottom, s_flappy_pipe_x, bottom_y);
        lv_obj_set_size(s_flappy_pipe_bottom, 28, bottom_h);
    }

    if (s_flappy_score_label != NULL) {
        if (s_flappy_alive) {
            lv_label_set_text_fmt(s_flappy_score_label, "Score: %u", s_flappy_score);
        } else {
            lv_label_set_text_fmt(s_flappy_score_label, "Game Over  Score: %u", s_flappy_score);
        }
    }
}

/**
 * @brief 重置 Flappy Bird 游戏（小鸟复位、水管位置随机、分数清零）
 */
static void flappy_reset_game(void)
{
    s_flappy_bird_y = 120;
    s_flappy_bird_vy = 0;
    s_flappy_pipe_x = 240;
    s_flappy_gap_y = 70 + (rand() % 90);
    s_flappy_score = 0;
    s_flappy_alive = true;
    s_flappy_pipe_passed = false;
    flappy_refresh_ui();
}

/**
 * @brief Flappy Bird 物理定时器（每 35ms 一次）：重力、移动水管、碰撞检测、计分
 */
static void flappy_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    if (!s_flappy_alive) {
        return;
    }

    // 重力：速度每帧 +1（上限5），位置 += 速度
    s_flappy_bird_vy += 1;
    if (s_flappy_bird_vy > 5) s_flappy_bird_vy = 5;
    s_flappy_bird_y += s_flappy_bird_vy;

    // 水管左移，出界后重置并随机缺口高度
    s_flappy_pipe_x -= 2;
    if (s_flappy_pipe_x < -30) {
        s_flappy_pipe_x = 240;
        s_flappy_gap_y = 70 + (rand() % 90);
        s_flappy_pipe_passed = false;
    }

    // 越过水管（小鸟 x > 水管右沿）→ 得分
    if (!s_flappy_pipe_passed && (52 > (s_flappy_pipe_x + 28))) {
        s_flappy_pipe_passed = true;
        s_flappy_score++;
    }

    // 撞到顶部或底部 → 死亡
    if (s_flappy_bird_y < 0 || (s_flappy_bird_y + 16) > 284) {
        s_flappy_alive = false;
    }

    // 撞到水管（x 重叠 且 y 在缺口之外）→ 死亡
    if ((52 + 16) > s_flappy_pipe_x && 52 < (s_flappy_pipe_x + 28)) {
        lv_coord_t gap_bottom = s_flappy_gap_y + 90;
        if (s_flappy_bird_y < s_flappy_gap_y || (s_flappy_bird_y + 16) > gap_bottom) {
            s_flappy_alive = false;
        }
    }

    flappy_refresh_ui();
}

/**
 * @brief Flappy Bird 输入事件：点击屏幕
 *
 * 存活时：给小鸟一个向上的初速度（-11）
 * 死亡时：重新开始游戏
 */
static void screen_flappy_input_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_flappy_alive) {
        s_flappy_bird_vy = -11;
    } else {
        flappy_reset_game();
    }
}

/**
 * @brief Flappy Bird 返回按钮：停止定时器、删除屏幕、回游戏列表
 */
static void screen_flappy_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    // 停止物理定时器
    if (s_flappy_timer != NULL) {
        lv_timer_del(s_flappy_timer);
        s_flappy_timer = NULL;
    }

    if (guider_ui.screen_game != NULL) {
        lv_scr_load_anim(guider_ui.screen_game, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    }

    // 异步删除屏幕并清空指针
    if (s_flappy_screen != NULL && lv_obj_is_valid(s_flappy_screen)) {
        lv_obj_del_async(s_flappy_screen);
    }
    s_flappy_screen = NULL;
    s_flappy_back_btn = NULL;
    s_flappy_bird = NULL;
    s_flappy_pipe_top = NULL;
    s_flappy_pipe_bottom = NULL;
    s_flappy_score_label = NULL;
}

/**
 * @brief 创建并打开 Flappy Bird 游戏屏
 *
 * 控件：天空背景、上下水管、小鸟、Back按钮、分数标签
 * 并启动 35ms 物理定时器
 */
static void open_game_flappy_screen(void)
{
    if (s_flappy_timer != NULL) {
        lv_timer_del(s_flappy_timer);
        s_flappy_timer = NULL;
    }
    if (s_flappy_screen != NULL && lv_obj_is_valid(s_flappy_screen)) {
        lv_obj_del(s_flappy_screen);
        s_flappy_screen = NULL;
    }

    s_flappy_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_flappy_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_flappy_screen, lv_color_hex(0x6EC6FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_flappy_screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(s_flappy_screen, screen_flappy_input_cb, LV_EVENT_CLICKED, NULL);

    // 上方水管（绿色，无边框圆角）
    s_flappy_pipe_top = lv_obj_create(s_flappy_screen);
    lv_obj_set_style_bg_color(s_flappy_pipe_top, lv_color_hex(0x3EA63E), 0);
    lv_obj_set_style_border_width(s_flappy_pipe_top, 0, 0);
    lv_obj_set_style_radius(s_flappy_pipe_top, 2, 0);

    // 下方水管
    s_flappy_pipe_bottom = lv_obj_create(s_flappy_screen);
    lv_obj_set_style_bg_color(s_flappy_pipe_bottom, lv_color_hex(0x3EA63E), 0);
    lv_obj_set_style_border_width(s_flappy_pipe_bottom, 0, 0);
    lv_obj_set_style_radius(s_flappy_pipe_bottom, 2, 0);

    // 小鸟（黄色圆形）
    s_flappy_bird = lv_obj_create(s_flappy_screen);
    lv_obj_set_size(s_flappy_bird, 16, 16);
    lv_obj_set_style_radius(s_flappy_bird, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_flappy_bird, lv_color_hex(0xFFD24A), 0);
    lv_obj_set_style_border_width(s_flappy_bird, 0, 0);

    // 返回按钮
    s_flappy_back_btn = lv_btn_create(s_flappy_screen);
    lv_obj_set_size(s_flappy_back_btn, 76, 40);
    lv_obj_align(s_flappy_back_btn, LV_ALIGN_TOP_LEFT, 12, 8);
    lv_obj_add_event_cb(s_flappy_back_btn, screen_flappy_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_back = lv_label_create(s_flappy_back_btn);
    lv_label_set_text(label_back, "Back");
    lv_obj_center(label_back);

    // 分数标签（右上角白色文字）
    s_flappy_score_label = lv_label_create(s_flappy_screen);
    lv_obj_align(s_flappy_score_label, LV_ALIGN_TOP_RIGHT, -10, 16);
    lv_obj_set_style_text_color(s_flappy_score_label, lv_color_hex(0xFFFFFF), 0);

    // 重置游戏并启动物理定时器
    flappy_reset_game();
    s_flappy_timer = lv_timer_create(flappy_timer_cb, 35, NULL);
    lv_scr_load_anim(s_flappy_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/**
 * @brief 游戏列表项点击处理：根据列表项序号打开对应游戏
 *
 * item0=2048, item1=记忆游戏, item2=贪吃蛇, item3=Flappy Bird
 */
static void screen_game_item_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *btn = lv_event_get_current_target(e);
    const char *game_name = lv_list_get_btn_text(guider_ui.screen_game_list_1, btn);
    if (game_name == NULL) return;
    if (btn == guider_ui.screen_game_list_1_item[0]) {
        ESP_LOGI(TAG, "open 2048 from game list item 0");
        open_game_2048_screen();
        return;
    }

    if (btn == guider_ui.screen_game_list_1_item[1]) {
        ESP_LOGI(TAG, "open memory game from game list item 1");
        open_game_memory_screen();
        return;
    }

    if (btn == guider_ui.screen_game_list_1_item[2]) {
        ESP_LOGI(TAG, "open snake game from game list item 2");
        open_game_snake_screen();
        return;
    }

    if (btn == guider_ui.screen_game_list_1_item[3]) {
        ESP_LOGI(TAG, "open flappy bird from game list item 3");
        open_game_flappy_screen();
        return;
    }

    ESP_LOGI(TAG, "game item clicked: %s (not configured)", game_name);
}

/**
 * @brief 初始化游戏列表屏事件绑定（屏手势 + 所有列表项点击）
 */
void events_init_screen_game(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_game, screen_game_event_handler, LV_EVENT_ALL, ui);

    for (int i = 0; i < _LIST_NUMBER; i++) {
        if (ui->screen_game_list_1_item[i] == NULL) continue;
        lv_obj_add_event_cb(ui->screen_game_list_1_item[i], screen_game_item_handler, LV_EVENT_CLICKED, ui);
    }
}

/**
 * @brief 小说阅读屏手势事件：右滑返回小说列表
 *
 * 触发右滑时：标记防误触保护（450ms），请求关闭小说，并切回小说列表屏。
 */
static void novel_display_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_RIGHT:
        {
            lvgl_mark_novel_swipe_back_guard(450);
            lvgl_msg_send(LVGL_MSG_NOVEL_CLOSE_REQ, 0, NULL);
            /* non-blocking: skip wait_release to avoid wakeup deadlock */
            ui_load_scr_animation(&guider_ui,
                            &guider_ui.novel_list,            // 目标屏：小说列表
                            guider_ui.novel_list_del,          // 目标屏删除标志
                            &guider_ui.novel_display_del,      // 旧屏（小说阅读）删除标志
                            setup_scr_novel_list,              // 目标屏创建函数
                            LV_SCR_LOAD_ANIM_NONE, 0, 0,
                            false, true);
                               break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 初始化小说阅读屏事件绑定（右滑返回）
 */
void events_init_novel_display (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->novel_display, novel_display_event_handler, LV_EVENT_ALL, ui);
}

static uint32_t s_novel_list_last_right_gesture_tick = 0; // 小说列表最近一次右滑时刻（防误触）

/**
 * @brief 小说列表屏手势事件：右滑返回主菜单
 */
static void novel_list_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_RIGHT:
        {
            lvgl_mark_novel_swipe_back_guard(450);
            s_novel_list_last_right_gesture_tick = lv_tick_get();
            /* non-blocking: skip wait_release to avoid wakeup deadlock */
            ui_load_scr_animation(&guider_ui, &guider_ui.menu_screen, guider_ui.menu_screen_del, &guider_ui.novel_list_del, setup_scr_menu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, false);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 小说列表项点击处理（所有列表项共用的回调）
 *
 * 点击小说时：
 *  - 检查防误触保护（右滑后 450ms 内的点击忽略）
 *  - 发送 LVGL_MSG_NOVEL_OPEN_REQ 消息打开对应小说
 */
static void novel_list_item_common_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    if (lvgl_should_block_novel_click()) {
        ESP_LOGI(TAG, "ignore novel click by shared swipe guard");
        return;
    }
    if (lv_tick_elaps(s_novel_list_last_right_gesture_tick) < 450) {
        ESP_LOGI(TAG, "ignore novel click after right-swipe");
        return;
    }

    lv_obj_t *btn = lv_event_get_current_target(e);
    const char *file_name = ui_gradient_btn_get_text(btn);
    if (file_name == NULL || strlen(file_name) == 0) {
        ESP_LOGE(TAG, "invalid novel file name");
        return;
    }

    ESP_LOGI(TAG, "novel item clicked: %s", file_name);
    lvgl_msg_send(LVGL_MSG_NOVEL_OPEN_REQ, 0, file_name);
}

/**
 * @brief 初始化小说列表屏事件绑定（屏手势 + 所有列表项点击）
 */
void events_init_novel_list (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->novel_list, novel_list_event_handler, LV_EVENT_ALL, ui);

    for (int i = 0; i < _LIST_NUMBER; i++) {
        // 跳过空项（未创建的小说按钮）
        if (ui->novel_list_list_1_item[i] == NULL) continue;

        // 给每个小说按钮绑定点击回调（CLICKED 事件）
        lv_obj_add_event_cb(
            ui->novel_list_list_1_item[i],
            novel_list_item_common_handler,
            LV_EVENT_CLICKED,
            ui
        );
    }
}

/**
 * @brief 设置屏手势事件：右滑返回主菜单
 */
static void setting_screen_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_RIGHT:
        {
            ui_load_scr_animation(&guider_ui, &guider_ui.menu_screen, guider_ui.menu_screen_del, &guider_ui.setting_screen_del, setup_scr_menu_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, false);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 设置屏第 1 项点击：切换到 WiFi 设置屏
 */
static void setting_screen_list_1_item0_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui,
            &guider_ui.screen_wifi_set, guider_ui.screen_wifi_set_del,
            &guider_ui.setting_screen_del, setup_scr_screen_wifi_set,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief 设置屏第 2 项点击：切换到时间设置屏
 */
static void setting_screen_list_1_item0_click_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui,
            &guider_ui.screen_time_set,
            guider_ui.screen_time_set_del,
            &guider_ui.setting_screen_del,
            setup_scr_screen_time_set,
            LV_SCR_LOAD_ANIM_NONE, 0, 0,
            false, true);
    }
}

/**
 * @brief 设置屏第 3 项点击：切换到 OTA 主菜单屏
 */
static void setting_screen_list_1_item2_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui,
            &guider_ui.screen_ota,
            guider_ui.screen_ota_del,
            &guider_ui.setting_screen_del,
            setup_scr_screen_ota,
            LV_SCR_LOAD_ANIM_NONE, 0, 0,
            false, true);
    }
}

/**
 * @brief 初始化设置屏事件绑定（手势 + 3 个列表项点击）
 */
void events_init_setting_screen (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->setting_screen, setting_screen_event_handler, LV_EVENT_ALL, ui);

    lv_obj_add_event_cb(ui->setting_screen_list_1_item0,
        setting_screen_list_1_item0_click_handler, LV_EVENT_ALL, ui);

    lv_obj_add_event_cb(ui->setting_screen_list_1_item1,
        setting_screen_list_1_item0_event_handler, LV_EVENT_ALL, ui);

    lv_obj_add_event_cb(ui->setting_screen_list_1_item2,
        setting_screen_list_1_item2_handler, LV_EVENT_ALL, ui);
}

/**
 * @brief 全局事件初始化入口（GUI Guider 约定保留，当前为空）
 */
void events_init(lv_ui *ui)
{

}

/**
 * @brief 图片列表屏手势事件：右滑返回主菜单
 */
static void screen_img_list_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.menu_screen, guider_ui.menu_screen_del,
                &guider_ui.screen_img_list_del, setup_scr_menu_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

/**
 * @brief 图片列表项点击处理
 *
 * 从全局文件列表中查找文件名对应的完整路径，保存到 img_full_path，
 * 然后切换到图片显示屏（由该屏加载并显示图片）。
 */
static void screen_img_list_item_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *btn = lv_event_get_current_target(e);
    const char *file_name = ui_gradient_btn_get_text(btn);

    if (file_name == NULL || strlen(file_name) == 0) return;

    // 在全局文件列表中查找文件名对应的完整路径
    memset(img_full_path, 0, sizeof(img_full_path));
    for (int i = 0; i < s_file_list.count; i++) {
        if (strcmp(s_file_list.files[i].name, file_name) == 0) {
            snprintf(img_full_path, sizeof(img_full_path), "%s", s_file_list.files[i].full_path);
            break;
        }
    }

    if (strlen(img_full_path) > 0) {
        ESP_LOGI(TAG, "img selected: %s", img_full_path);
    }

    // 切换到图片显示屏
    ui_load_scr_animation(&guider_ui,
        &guider_ui.screen_img_display, guider_ui.screen_img_display_del,
        &guider_ui.screen_img_list_del, setup_scr_screen_img_display,
        LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

/**
 * @brief 初始化图片列表屏事件绑定（屏手势 + 所有列表项点击）
 */
void events_init_screen_img_list(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_img_list, screen_img_list_event_handler, LV_EVENT_ALL, ui);

    // 为所有已创建的文件列表项绑定点击回调
    for (int i = 0; i < s_file_list.count; i++) {
        if (ui->screen_img_list_list_1_item[i] == NULL) continue;
        lv_obj_add_event_cb(
            ui->screen_img_list_list_1_item[i],
            screen_img_list_item_handler,
            LV_EVENT_CLICKED,
            ui
        );
    }
}

/**
 * @brief 图片显示屏手势事件：右滑返回图片列表（并释放已加载的图片内存）
 */
static void screen_img_display_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            // 返回前释放图片解码内存
            img_display_free();

            ui_load_scr_animation(&guider_ui,
                            &guider_ui.screen_img_list,          // 目标屏：图片列表
                            guider_ui.screen_img_list_del,        // 目标屏删除标志
                            &guider_ui.screen_img_display_del,    // 旧屏（图片显示）删除标志
                            setup_scr_screen_img_list,            // 目标屏创建函数
                            LV_SCR_LOAD_ANIM_NONE, 0, 0,
                            false, true);
                         }
    }
}

/**
 * @brief 初始化图片显示屏事件绑定（右滑返回）
 */
void events_init_screen_img_display(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_img_display, screen_img_display_event_handler, LV_EVENT_ALL, ui);
}

/* ===== WiFi 设置屏 ===== */

static lv_keyboard_mode_t wifi_last_kb_mode = LV_KEYBOARD_MODE_TEXT_LOWER; // 记住上次键盘模式（大小写切换恢复用）

/**
 * @brief WiFi 设置屏软键盘事件回调
 *
 * VALUE_CHANGED：键盘模式变化时记录（用于模式切换后恢复）
 * READY：SSID 输入完成后切换到密码输入框；密码输入完成后隐藏键盘
 * CANCEL：隐藏键盘
 */
static void screen_wifi_set_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_keyboard_mode_t cur_mode = lv_keyboard_get_mode(kb);
        if (cur_mode != wifi_last_kb_mode) {
            wifi_last_kb_mode = cur_mode;
            lv_indev_t *indev = lv_indev_get_act();
            if (indev != NULL) {
                /* non-blocking: skip wait_release to avoid wakeup deadlock */
            }
        }
    }
    else if (code == LV_EVENT_READY) {
        lv_obj_t *cur_ta = lv_keyboard_get_textarea(kb);

        if (cur_ta == guider_ui.screen_wifi_set_ta_ssid) {
            // SSID 输入完成 → 自动切换到密码输入框
            lv_keyboard_set_textarea(guider_ui.screen_wifi_set_kb,
                                     guider_ui.screen_wifi_set_ta_pwd);
            lv_obj_clear_state(guider_ui.screen_wifi_set_ta_ssid, LV_STATE_FOCUSED);
            lv_obj_add_state(guider_ui.screen_wifi_set_ta_pwd, LV_STATE_FOCUSED);
            ESP_LOGI(TAG, "SSID input done, switch to password input");
        }
        else if (cur_ta == guider_ui.screen_wifi_set_ta_pwd) {
            // 密码输入完成 → 隐藏键盘
            lv_obj_clear_state(guider_ui.screen_wifi_set_ta_pwd, LV_STATE_FOCUSED);
            lv_obj_add_flag(guider_ui.screen_wifi_set_kb, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "password input done, keyboard hidden");
        }
    }
    else if (code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(guider_ui.screen_wifi_set_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 点击 SSID 输入框：让键盘绑定 SSID 框并显示（文本小写模式）
 */
static void screen_wifi_set_ta_ssid_click_cb(lv_event_t *e)
{
    lv_keyboard_set_textarea(guider_ui.screen_wifi_set_kb,
                             guider_ui.screen_wifi_set_ta_ssid);
    lv_keyboard_set_mode(guider_ui.screen_wifi_set_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    wifi_last_kb_mode = LV_KEYBOARD_MODE_TEXT_LOWER;
    lv_obj_clear_flag(guider_ui.screen_wifi_set_kb, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 点击密码输入框：让键盘绑定密码框并显示（文本小写模式）
 */
static void screen_wifi_set_ta_pwd_click_cb(lv_event_t *e)
{
    lv_keyboard_set_textarea(guider_ui.screen_wifi_set_kb,
                             guider_ui.screen_wifi_set_ta_pwd);
    lv_keyboard_set_mode(guider_ui.screen_wifi_set_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    wifi_last_kb_mode = LV_KEYBOARD_MODE_TEXT_LOWER;
    lv_obj_clear_flag(guider_ui.screen_wifi_set_kb, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief "连接" 按钮点击：校验 SSID 非空后调用 wifi_manager_connect 连接
 *
 * 连接过程异步进行，状态通过 wifi_state_handler 回调更新。
 */
static void screen_wifi_set_btn_connect_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_add_flag(guider_ui.screen_wifi_set_kb, LV_OBJ_FLAG_HIDDEN);

    const char *ssid = lv_textarea_get_text(guider_ui.screen_wifi_set_ta_ssid);
    const char *pwd  = lv_textarea_get_text(guider_ui.screen_wifi_set_ta_pwd);

    // SSID 为空校验
    if (ssid == NULL || strlen(ssid) == 0) {
        lv_label_set_text(guider_ui.screen_wifi_set_label_status,
                          "Please enter SSID!");
        lv_obj_set_style_text_color(guider_ui.screen_wifi_set_label_status,
                                    lv_color_hex(0xff0000), 0);
        return;
    }

    ESP_LOGI(TAG, "WiFi connecting: SSID=%s PWD=%s", ssid, pwd);

    // 显示连接中状态
    lv_label_set_text(guider_ui.screen_wifi_set_label_status, "Connecting...");
    lv_obj_set_style_text_color(guider_ui.screen_wifi_set_label_status,
                                lv_color_hex(0xffff00), 0);

    // 发起连接（异步）
    wifi_manager_connect(ssid, pwd);
}

/**
 * @brief WiFi 设置屏手势事件：右滑返回设置屏
 */
static void screen_wifi_set_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.setting_screen, guider_ui.setting_screen_del,
                &guider_ui.screen_wifi_set_del, setup_scr_setting_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

/**
 * @brief 初始化 WiFi 设置屏事件绑定（屏手势 + SSID/密码输入框 + 键盘 + 连接按钮）
 */
void events_init_screen_wifi_set(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_wifi_set,
        screen_wifi_set_event_handler, LV_EVENT_ALL, ui);

    // SSID 输入框点击
    lv_obj_add_event_cb(ui->screen_wifi_set_ta_ssid,
        screen_wifi_set_ta_ssid_click_cb, LV_EVENT_CLICKED, NULL);

    // 密码输入框点击
    lv_obj_add_event_cb(ui->screen_wifi_set_ta_pwd,
        screen_wifi_set_ta_pwd_click_cb, LV_EVENT_CLICKED, NULL);

    // 软键盘事件
    lv_obj_add_event_cb(ui->screen_wifi_set_kb,
        screen_wifi_set_kb_event_cb, LV_EVENT_ALL, NULL);

    // 连接按钮
    lv_obj_add_event_cb(ui->screen_wifi_set_btn_connect,
        screen_wifi_set_btn_connect_cb, LV_EVENT_ALL, NULL);
}

/* ==================================================================
 * 时间设置屏
 * ================================================================== */

extern struct tm timeinfo;   // 系统时间结构体（来自 ntp_time.c）
extern time_t now;           // 系统时间戳（来自 ntp_time.c）

static lv_obj_t *s_time_active_ta = NULL; // 当前聚焦的时间输入框（年/月/日/时/分/秒）

/**
 * @brief 点击时间输入框：聚焦该框并让键盘绑定它
 */
static void screen_time_set_ta_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *ta = lv_event_get_target(e);
    s_time_active_ta = ta;

    // 清除所有输入框的聚焦状态
    lv_obj_clear_state(guider_ui.screen_time_set_ta_year,  LV_STATE_FOCUSED);
    lv_obj_clear_state(guider_ui.screen_time_set_ta_month, LV_STATE_FOCUSED);
    lv_obj_clear_state(guider_ui.screen_time_set_ta_day,   LV_STATE_FOCUSED);
    lv_obj_clear_state(guider_ui.screen_time_set_ta_hour,  LV_STATE_FOCUSED);
    lv_obj_clear_state(guider_ui.screen_time_set_ta_min,   LV_STATE_FOCUSED);
    lv_obj_clear_state(guider_ui.screen_time_set_ta_sec,   LV_STATE_FOCUSED);

    // 聚焦当前输入框
    lv_obj_add_state(ta, LV_STATE_FOCUSED);

    // 键盘绑定当前输入框并显示
    lv_keyboard_set_textarea(guider_ui.screen_time_set_kb, ta);
    lv_obj_clear_flag(guider_ui.screen_time_set_kb, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 时间设置软键盘事件：READY/CANCEL 时隐藏键盘并取消聚焦
 */
static void screen_time_set_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // 键盘完成或取消：隐藏键盘、清除聚焦
        lv_obj_add_flag(guider_ui.screen_time_set_kb, LV_OBJ_FLAG_HIDDEN);

        if (s_time_active_ta != NULL) {
            lv_obj_clear_state(s_time_active_ta, LV_STATE_FOCUSED);
            s_time_active_ta = NULL;
        }
    }
}

/**
 * @brief 时间设置 "确认" 按钮：校验输入范围后调用 RTC 服务写入时间
 */
static void screen_time_set_btn_confirm_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_add_flag(guider_ui.screen_time_set_kb, LV_OBJ_FLAG_HIDDEN);

    int year  = atoi(lv_textarea_get_text(guider_ui.screen_time_set_ta_year));
    int month = atoi(lv_textarea_get_text(guider_ui.screen_time_set_ta_month));
    int day   = atoi(lv_textarea_get_text(guider_ui.screen_time_set_ta_day));
    int hour  = atoi(lv_textarea_get_text(guider_ui.screen_time_set_ta_hour));
    int min   = atoi(lv_textarea_get_text(guider_ui.screen_time_set_ta_min));
    int sec   = atoi(lv_textarea_get_text(guider_ui.screen_time_set_ta_sec));

    // 输入范围校验
    if (year < 2000 || year > 2099 ||
        month < 1   || month > 12  ||
        day < 1     || day > 31    ||
        hour < 0    || hour > 23   ||
        min < 0     || min > 59    ||
        sec < 0     || sec > 59)
    {
        lv_label_set_text(guider_ui.screen_time_set_label_status,
                          "input out of range");
        lv_obj_set_style_text_color(guider_ui.screen_time_set_label_status,
                                    lv_color_hex(0xFF0000), 0);
        return;
    }

    // 写入 RTC 芯片 + 同步系统时间
    esp_err_t rtc_ret = rtc_time_service_set_manual_and_sync(year, month, day, hour, min, sec);
    if (rtc_ret != ESP_OK) {
        lv_label_set_text(guider_ui.screen_time_set_label_status,
                          "set rtc/system failed");
        lv_obj_set_style_text_color(guider_ui.screen_time_set_label_status,
                                    lv_color_hex(0xFF0000), 0);
        ESP_LOGW(TAG, "manual sync failed: %s", esp_err_to_name(rtc_ret));
        return;
    }

    lv_label_set_text(guider_ui.screen_time_set_label_status,
                      "time set success");
    lv_obj_set_style_text_color(guider_ui.screen_time_set_label_status,
                                lv_color_hex(0x00FF00), 0);

    ESP_LOGI(TAG, "manual set and sync ok: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, min, sec);
}

/**
 * @brief 时间设置 "NTP" 按钮：触发信号量让 sntp_interval_task 立即网络校时
 */
static void screen_time_set_btn_ntp_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_add_flag(guider_ui.screen_time_set_kb, LV_OBJ_FLAG_HIDDEN);

    if (sntp_trigger_sem == NULL) {
        lv_label_set_text(guider_ui.screen_time_set_label_status,
                          "NTP trigger unavailable");
        lv_obj_set_style_text_color(guider_ui.screen_time_set_label_status,
                                    lv_color_hex(0xFF0000), 0);
        return;
    }

    // 释放信号量，通知 sntp_interval_task 执行一次 NTP 同步
    xSemaphoreGive(sntp_trigger_sem);

    lv_label_set_text(guider_ui.screen_time_set_label_status,
                      "Syncing...");
    lv_obj_set_style_text_color(guider_ui.screen_time_set_label_status,
                                lv_color_hex(0xFFFF00), 0);

    ESP_LOGI(TAG, "NTP sync trigger sent");
}

/**
 * @brief 时间设置屏手势事件：右滑返回设置屏（并隐藏键盘）
 */
static void screen_time_set_gesture_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            lv_obj_add_flag(guider_ui.screen_time_set_kb, LV_OBJ_FLAG_HIDDEN);

            ui_load_scr_animation(&guider_ui,
                &guider_ui.setting_screen,
                guider_ui.setting_screen_del,
                &guider_ui.screen_time_set_del,
                setup_scr_setting_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0,
                false, true);
        }
    }
}

/**
 * @brief 初始化时间设置屏事件绑定（屏手势 + 6 个输入框 + 键盘 + 确认/NTP按钮）
 */
void events_init_screen_time_set(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_time_set,
        screen_time_set_gesture_cb, LV_EVENT_ALL, ui);

    // 6 个输入框（年/月/日/时/分/秒）点击聚焦
    lv_obj_add_event_cb(ui->screen_time_set_ta_year,
        screen_time_set_ta_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_time_set_ta_month,
        screen_time_set_ta_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_time_set_ta_day,
        screen_time_set_ta_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_time_set_ta_hour,
        screen_time_set_ta_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_time_set_ta_min,
        screen_time_set_ta_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_time_set_ta_sec,
        screen_time_set_ta_click_cb, LV_EVENT_CLICKED, NULL);

    // 软键盘
    lv_obj_add_event_cb(ui->screen_time_set_kb,
        screen_time_set_kb_event_cb, LV_EVENT_ALL, NULL);

    // 确认按钮（手动设置时间）
    lv_obj_add_event_cb(ui->screen_time_set_btn_confirm,
        screen_time_set_btn_confirm_cb, LV_EVENT_ALL, NULL);

    // NTP 按钮（网络校时）
    lv_obj_add_event_cb(ui->screen_time_set_btn_ntp,
        screen_time_set_btn_ntp_cb, LV_EVENT_ALL, NULL);
}

/* ==================================================================
 * 天气屏
 * ================================================================== */

/**
 * @brief 天气屏手势事件：右滑返回快捷面板
 */
static void screen_weather_gesture_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        open_clock_quick_screen();
    }
}

/**
 * @brief 初始化天气屏事件绑定（右滑返回快捷面板）
 */
void events_init_screen_weather(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_weather, screen_weather_gesture_cb, LV_EVENT_ALL, ui);
}

/* ==================================================================
 * OTA 主菜单屏
 * ================================================================== */

/**
 * @brief OTA 主菜单屏手势：右滑返回设置屏
 */
static void screen_ota_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.setting_screen, guider_ui.setting_screen_del,
                &guider_ui.screen_ota_del, setup_scr_setting_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

/**
 * @brief OTA 菜单第 1 项（OneNET 云升级）点击：切换到 OneNET 升级屏
 */
static void screen_ota_list_1_item0_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui,
            &guider_ui.screen_ota_onenet, guider_ui.screen_ota_onenet_del,
            &guider_ui.screen_ota_del, setup_scr_screen_ota_onenet,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief OTA 菜单第 2 项（本地升级）点击：切换到本地升级屏
 */
static void screen_ota_list_1_item1_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui,
            &guider_ui.screen_ota_local, guider_ui.screen_ota_local_del,
            &guider_ui.screen_ota_del, setup_scr_screen_ota_local,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief OTA 菜单第 3 项（切换分区）点击：切换到分区切换屏
 */
static void screen_ota_list_1_item2_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui,
            &guider_ui.screen_ota_switch, guider_ui.screen_ota_switch_del,
            &guider_ui.screen_ota_del, setup_scr_screen_ota_switch,
            LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

/**
 * @brief 初始化 OTA 主菜单屏事件绑定（手势 + 3 个菜单项点击）
 */
void events_init_screen_ota(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_ota, screen_ota_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_ota_list_1_item0, screen_ota_list_1_item0_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_ota_list_1_item1, screen_ota_list_1_item1_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_ota_list_1_item2, screen_ota_list_1_item2_handler, LV_EVENT_CLICKED, NULL);
}

/* ==================================================================
 * OneNET 云 OTA 屏
 * ================================================================== */

/**
 * @brief OneNET OTA 屏手势：右滑返回 OTA 主菜单
 */
static void screen_ota_onenet_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.screen_ota, guider_ui.screen_ota_del,
                &guider_ui.screen_ota_onenet_del, setup_scr_screen_ota,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

/**
 * @brief OneNET "开始升级" 按钮点击
 *
 * 按钮文字为 "Upgrade done" 时：重置按钮和状态（升级完成后点击进入复位态）
 * 否则：启动 OneNET OTA 升级流程
 */
static void screen_ota_onenet_btn_start_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *btn_lbl = lv_obj_get_child(guider_ui.screen_ota_onenet_btn_start, 0);
    if (btn_lbl == NULL) return;

    const char *btn_text = lv_label_get_text(btn_lbl);

    if (strcmp(btn_text, "Upgrade done") == 0) {
        // 升级完成后状态复位：隐藏跳转按钮，恢复初始按钮文字
        lv_obj_add_flag(guider_ui.screen_ota_onenet_btn_jump, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(btn_lbl, "Check & upgrade");
        lv_label_set_text(guider_ui.screen_ota_onenet_label_status, "Idle");
        lv_obj_set_style_text_color(guider_ui.screen_ota_onenet_label_status, lv_color_hex(0x00ff00), 0);
        ESP_LOGI(TAG, "[OTA] reset from done state");
    } else {
        // 启动 OneNET OTA（后台任务执行，非阻塞）
        lv_label_set_text(btn_lbl, "Upgrade done");
        lv_label_set_text(guider_ui.screen_ota_onenet_label_status, "Checking...");
        lv_obj_set_style_text_color(guider_ui.screen_ota_onenet_label_status, lv_color_hex(0xffff00), 0);
        onenet_ota_start();
        ESP_LOGI(TAG, "OneNET OTA start");
    }
}

/**
 * @brief OneNET "跳转" 按钮：跳转到备用分区并重启
 */
static void screen_ota_onenet_btn_jump_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_label_set_text(guider_ui.screen_ota_onenet_label_status, "Jumping..");
    lv_obj_set_style_text_color(guider_ui.screen_ota_onenet_label_status, lv_color_hex(0xff0000), 0);

    // 跳转并重启（此调用不返回）
    onenet_ota_jump_and_restart();
}

/**
 * @brief 初始化 OneNET OTA 屏事件绑定（手势 + 开始/跳转按钮）
 */
void events_init_screen_ota_onenet(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_ota_onenet, screen_ota_onenet_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_ota_onenet_btn_start, screen_ota_onenet_btn_start_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui->screen_ota_onenet_btn_jump, screen_ota_onenet_btn_jump_handler, LV_EVENT_ALL, NULL);
}

/* ==================================================================
 * 本地 OTA（SD 卡升级）屏
 * ================================================================== */

/**
 * @brief 本地 OTA 屏手势事件（手动位移检测右滑返回 OTA 主菜单）
 *
 * 在 PRESSED 记录起点，PRESSING 判断水平位移超过 35px 且明显大于垂直位移时
 * 判定为右滑返回；GESTURE 事件也做同样处理。
 */
static void screen_ota_local_event_handler(lv_event_t *e)
{
    static int16_t s_last_x = 0;
    static int16_t s_last_y = 0;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev != NULL) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            s_last_x = p.x;
            s_last_y = p.y;
        }
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev != NULL) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            int diff_x = p.x - s_last_x;
            int diff_y = p.y - s_last_y;

            // 手动判定右滑：水平位移 > 35px 且明显大于垂直位移
            if (diff_x > 35 && LV_ABS(diff_x) > (LV_ABS(diff_y) + 10)) {
                ui_load_scr_animation(&guider_ui,
                    &guider_ui.screen_ota, guider_ui.screen_ota_del,
                    &guider_ui.screen_ota_local_del, setup_scr_screen_ota,
                    LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
                return;
            }
        }
    }

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            // 右滑返回 OTA 主菜单（升级进行中时可通过 GUI Guider 配置拦截，此处暂未开启）
            ui_load_scr_animation(&guider_ui,
                &guider_ui.screen_ota, guider_ui.screen_ota_del,
                &guider_ui.screen_ota_local_del, setup_scr_screen_ota,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

static char ota_local_full_path[512];   // 选中的本地固件完整路径

/* 本地 OTA 状态消息框 / 结果面板 */
static lv_obj_t *s_ota_local_msgbox = NULL;        // 升级过程消息框（下载中...）
static lv_obj_t *s_ota_local_result_msgbox = NULL; // 升级完成结果面板

/**
 * @brief 更新本地 OTA 过程消息框文字（由 lvgl_display.c 的 LVGL_MSG_OTA_STATUS 消息调用）
 */
void local_ota_update_msgbox(const char *text)
{
    if (s_ota_local_msgbox == NULL) return;
    lv_obj_t *ta = lv_msgbox_get_text(s_ota_local_msgbox);
    if (ta == NULL) return;
    lv_label_set_text(ta, text);
}

/**
 * @brief 关闭本地 OTA 过程消息框
 */
void local_ota_close_msgbox(void)
{
    if (s_ota_local_msgbox != NULL) {
        lv_msgbox_close(s_ota_local_msgbox);
        s_ota_local_msgbox = NULL;
    }
}

/**
 * @brief 关闭 OTA 完成结果面板（异步删除）
 */
static void ota_local_result_panel_close(void)
{
    if (s_ota_local_result_msgbox != NULL && lv_obj_is_valid(s_ota_local_result_msgbox)) {
        lv_obj_t *obj = s_ota_local_result_msgbox;
        s_ota_local_result_msgbox = NULL;
        lv_obj_del_async(obj);
    } else {
        s_ota_local_result_msgbox = NULL;
    }
}

/**
 * @brief 结果面板 "Exit" 按钮：关闭面板并返回 OTA 主菜单
 */
static void ota_local_result_exit_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ota_local_result_panel_close();

    if (guider_ui.screen_ota == NULL || !lv_obj_is_valid(guider_ui.screen_ota)) {
        setup_scr_screen_ota(&guider_ui);
    }
    lv_scr_load_anim(guider_ui.screen_ota, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/**
 * @brief 结果面板 "跳转" 按钮：切换分区并重启
 */
static void ota_local_result_jump_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ota_local_result_panel_close();
    local_ota_switch_partition();
}

/**
 * @brief 结果面板删除事件：清理指针
 */
static void ota_local_result_panel_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        s_ota_local_result_msgbox = NULL;
    }
}

/**
 * @brief 设置本地 OTA 运行标志（由 lvgl_display.c 调用）
 */
void local_ota_set_running(bool running)
{
    g_is_ota_running = running;
}

/**
 * @brief 处理本地 OTA 完成结果：成功时弹出结果面板（跳转/退出按钮）
 * @param result_code LVGL_OTA_RESULT_* 枚举值
 */
void local_ota_handle_complete_result(int result_code)
{
    ota_local_result_panel_close();

    // 非成功结果不弹面板
    if (result_code != (int)LVGL_OTA_RESULT_SUCCESS) {
        return;
    }

    // 全屏半透明遮罩
    s_ota_local_result_msgbox = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_ota_local_result_msgbox, 240, 284);
    lv_obj_center(s_ota_local_result_msgbox);
    lv_obj_clear_flag(s_ota_local_result_msgbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_ota_local_result_msgbox, 0, 0);
    lv_obj_set_style_border_width(s_ota_local_result_msgbox, 0, 0);
    lv_obj_set_style_bg_color(s_ota_local_result_msgbox, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_ota_local_result_msgbox, LV_OPA_50, 0);
    lv_obj_add_event_cb(s_ota_local_result_msgbox, ota_local_result_panel_delete_cb, LV_EVENT_DELETE, NULL);

    // 居中卡片
    lv_obj_t *card = lv_obj_create(s_ota_local_result_msgbox);
    lv_obj_set_size(card, 200, 116);
    lv_obj_center(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    // 标题
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "Local OTA");
    lv_obj_set_style_text_font(title, &songti_font_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 正文
    lv_obj_t *text = lv_label_create(card);
    lv_label_set_text(text, "Download OK");
    lv_obj_set_style_text_font(text, &songti_font_16, 0);
    lv_obj_set_style_text_color(text, lv_color_hex(0xD1D5DB), 0);
    lv_obj_align(text, LV_ALIGN_TOP_MID, 0, 40);

    // 跳转按钮（左下）
    lv_obj_t *btn_jump = lv_btn_create(card);
    lv_obj_set_size(btn_jump, 72, 30);
    lv_obj_align(btn_jump, LV_ALIGN_BOTTOM_LEFT, 18, -12);
    lv_obj_set_style_radius(btn_jump, 8, 0);
    lv_obj_set_style_border_width(btn_jump, 0, 0);
    lv_obj_set_style_bg_color(btn_jump, lv_color_hex(0x2F80ED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_jump, lv_color_hex(0x2469C8), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_jump, ota_local_result_jump_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_jump = lv_label_create(btn_jump);
    lv_label_set_text(lbl_jump, "Jump");
    lv_obj_set_style_text_font(lbl_jump, &songti_font_16, 0);
    lv_obj_set_style_text_color(lbl_jump, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_jump);

    // 退出按钮（右下）
    lv_obj_t *btn_exit = lv_btn_create(card);
    lv_obj_set_size(btn_exit, 72, 30);
    lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_RIGHT, -18, -12);
    lv_obj_set_style_radius(btn_exit, 8, 0);
    lv_obj_set_style_border_width(btn_exit, 0, 0);
    lv_obj_set_style_bg_color(btn_exit, lv_color_hex(0x6B7280), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_exit, lv_color_hex(0x4B5563), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_exit, ota_local_result_exit_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_exit = lv_label_create(btn_exit);
    lv_label_set_text(lbl_exit, "Exit");
    lv_obj_set_style_text_font(lbl_exit, &songti_font_16, 0);
    lv_obj_set_style_text_color(lbl_exit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_exit);
}

/**
 * @brief 本地 OTA 固件列表项点击：选中 .bin 文件并启动本地升级
 *
 * 从全局文件列表查找文件名对应的完整路径 → 弹出下载中消息框 →
 * 调用 local_ota_start 开始后台升级任务。
 */
static void screen_ota_local_item_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    if (g_is_ota_running) return;   // 升级进行中禁止再次选择
    lv_obj_t *btn = lv_event_get_current_target(e);
    /* 卡片式 item：取唯一文字 label（结构由 ui_gradient_btn_create 决定） */
    const char *file_name = ui_gradient_btn_get_text(btn);
    if (file_name == NULL || file_name[0] == '\0') return;

    // 在全局文件列表中查找完整路径
    ota_local_full_path[0] = '\0';
    for (int i = 0; i < s_file_list.count; i++) {
        if (strcmp(s_file_list.files[i].name, file_name) == 0) {
            snprintf(ota_local_full_path, sizeof(ota_local_full_path),
                     "%s", s_file_list.files[i].full_path);
            break;
        }
    }
    if (ota_local_full_path[0] == '\0') {
        ESP_LOGW(TAG, "local ota file not found: %s", file_name);
        return;
    }

    ESP_LOGI(TAG, "local ota file: %s", ota_local_full_path);

    // 关闭旧消息框，新建下载中消息框
    if (s_ota_local_msgbox != NULL) {
        lv_msgbox_close(s_ota_local_msgbox);
        s_ota_local_msgbox = NULL;
    }
    s_ota_local_msgbox = lv_msgbox_create(NULL, "Local OTA", "Downloading...", NULL, false);
    lv_obj_set_size(s_ota_local_msgbox, 200, 80);
    lv_obj_center(s_ota_local_msgbox);
    lv_obj_set_style_text_font(lv_msgbox_get_title(s_ota_local_msgbox), &songti_font_16, 0);
    lv_obj_set_style_text_font(lv_msgbox_get_text(s_ota_local_msgbox), &songti_font_16, 0);

    // 置运行标志，启动后台升级任务
    g_is_ota_running = true;
    if (local_ota_start(ota_local_full_path) != ESP_OK) {
        g_is_ota_running = false;
        local_ota_update_msgbox("Start failed");
    }
}

/**
 * @brief 初始化本地 OTA 屏事件绑定
 *
 * 屏和列表容器绑定手势回调；每个列表项绑定手势冒泡 + 点击启动升级。
 */
void events_init_screen_ota_local(lv_ui *ui)
{
    // 手势统一由屏处理：容器/子项通过 GESTURE_BUBBLE 冒泡到屏
    lv_obj_add_event_cb(ui->screen_ota_local, screen_ota_local_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_flag(ui->screen_ota_local_list_1, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // item 绑定点击回调 + 手势冒泡（手势冒泡到屏，点击留在 item）
    for (int i = 0; i < 20; i++) {
        if (ui->screen_ota_local_list_1_item[i] == NULL) continue;
        lv_obj_add_flag(ui->screen_ota_local_list_1_item[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(
            ui->screen_ota_local_list_1_item[i],
            screen_ota_local_item_handler,
            LV_EVENT_CLICKED,
            NULL
        );
    }
}

/* ==================================================================
 * 分区切换屏
 * ================================================================== */

/**
 * @brief 分区切换屏手势：右滑返回 OTA 主菜单
 */
static void screen_ota_switch_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.screen_ota, guider_ui.screen_ota_del,
                &guider_ui.screen_ota_switch_del, setup_scr_screen_ota,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

/**
 * @brief 分区切换按钮：调用 local_ota_switch_partition 切换分区并重启
 */
static void screen_ota_switch_btn_switch_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    local_ota_switch_partition();
    ESP_LOGI(TAG, "switch partition and restart");
}

/**
 * @brief 初始化分区切换屏事件绑定（手势 + 切换按钮）
 */
void events_init_screen_ota_switch(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_ota_switch, screen_ota_switch_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_ota_switch_btn_switch, screen_ota_switch_btn_switch_handler, LV_EVENT_ALL, NULL);
}




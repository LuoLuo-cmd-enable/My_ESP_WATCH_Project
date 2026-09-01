/*
 * font_sd.c - SD 卡动态字库适配
 *
 * 通过 LVGL 的 lv_font_load() 从 SD 卡加载 Font_20.bin：
 *   盘符 'S' 由 img_display.c 的 my_fs_init() 注册，映射到 /sdcard
 *   LVGL 会一次性把字形位图读入内存（LV_MEM_CUSTOM -> lvgl_psram_alloc -> PSRAM）
 *
 * 未覆盖字符回退：内置 songti_font_16（UI 固定文字/ASCII）
 */
#include "font_sd.h"
#include "lvgl.h"
#include "esp_log.h"
#include "gui_guider.h"
#include "lvgl_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "FONT_SD"

/* SD 卡字库路径：S: 盘符 + /font/Font_20.bin */
#define SD_FONT_PATH "S:font/Font_20.bin"

#define FONT_SD_TASK_STACK 4096

/* 加载得到的字体对象（lv_font_load 内部分配，不可 free） */
static lv_font_t *s_sd_font_loaded = NULL;
/* 对外暴露的字体：地址固定。先拷贝内置字体，SD 字体加载完成后原地更新 */
static lv_font_t s_sd_font;
static volatile bool s_font_loading = false;

static void font_sd_load_task(void *arg)
{
    (void)arg;

    lv_font_t *loaded = lv_font_load(SD_FONT_PATH);
    if (loaded == NULL) {
        ESP_LOGE(TAG, "load '%s' failed, fallback to built-in songti_font_16",
                 SD_FONT_PATH);
        s_font_loading = false;
        /* 通知 UI：加载失败，允许被挂起的小说打开流程继续（用内置字体兜底） */
        lvgl_msg_send_nonblocking(LVGL_MSG_SD_FONT_READY, 0, NULL);
        vTaskDelete(NULL);
        return;
    }

    /* 原地更新：已引用 font_sd_get() 的控件会自动使用新字体（重绘后生效） */
    s_sd_font = *loaded;
    s_sd_font.fallback = (lv_font_t *)&songti_font_16;
    s_sd_font_loaded = loaded;
    s_font_loading = false;

    ESP_LOGI(TAG, "SD font loaded: %s (line_height=%d)",
             SD_FONT_PATH, s_sd_font.line_height);

    /* 通知 UI 任务刷新已使用该字体的控件 */
    lvgl_msg_send_nonblocking(LVGL_MSG_SD_FONT_READY, 0, NULL);
    vTaskDelete(NULL);
}

esp_err_t font_sd_init(void)
{
    if (s_font_loading) {
        return ESP_OK;   /* 加载中 */
    }

    /* 加载完成前先用内置字体，保证界面立即可用、不阻塞 LVGL 任务 */
    s_sd_font = *(lv_font_t *)&songti_font_16;
    s_sd_font_loaded = NULL;
    s_font_loading = true;

    BaseType_t ok = xTaskCreate(font_sd_load_task, "font_sd",
                                FONT_SD_TASK_STACK, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "create font_sd task failed");
        s_font_loading = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

lv_font_t *font_sd_get(void)
{
    return &s_sd_font;
}

bool font_sd_is_ready(void)
{
    /* 加载失败已回退内置字体也算“就绪”（可正常显示，无需等待） */
    return !s_font_loading;
}

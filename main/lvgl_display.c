#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include "lv_port.h"
#include "lv_demos.h"
#include "st7789_driver.h"
#include "driver/gpio.h"
#include "SD_card.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "ntp_time.h"
#include "gui_guider.h"
#include "events_init.h"
#include "lvgl_display.h"
#include "My_timer.h"
#include "img_display.h"
#include "battery_management.h"
#include "power_sleep.h"
#include "local_ota.h"
#include "novel_progress.h"
#include "video_player.h"
#include "storage_worker.h"
#include "get_weather.h"
#include "custom.h"
#include "onenet_mqtt.h"

#define TAG     "MAIN"

// External file pointers for novel/video playback
extern FILE *fp;
extern long g_file_offset;
extern char g_video_filepath[512];
extern video_format_t g_video_format;



static void update_time_timer_cb(lv_timer_t * timer);


extern void trigger_ntp_sync(void);
extern void local_ota_set_running(bool running);

void wifi_state_handler(WIFI_STATE state)
{
    if (state == WIFI_STATE_CONNECTED) {

        ESP_LOGI(TAG, "Wifi connect success!");
        lvgl_msg_send(LVGL_MSG_WIFI_CONNECTED, 0, NULL);
        trigger_ntp_sync();
        weather_request_sync();
        onenet_start();

    } else {

        ESP_LOGI(TAG, "Wifi disconnect!");
        lvgl_msg_send(LVGL_MSG_WIFI_DISCONNECTED, 0, NULL);

    }
}

/**
 * @brief OTA 跳转后处理：检测 NVS 待上报标志，有则自动开 WiFi
 * @note 新固件上报版本号后，平台 OTA 任务才显示完成；标志用完即删
 */
static void ota_pending_report_handle(void)
{
    nvs_handle_t ota_nvs;
    uint8_t ota_pending_report = 0;
    if (nvs_open("ota", NVS_READONLY, &ota_nvs) == ESP_OK) {
        nvs_get_u8(ota_nvs, "pending_report", &ota_pending_report);
        nvs_close(ota_nvs);
    }
    if (ota_pending_report) {
        ESP_LOGI(TAG, "[Boot] OTA pending report -> auto start WiFi");
        if (nvs_open("ota", NVS_READWRITE, &ota_nvs) == ESP_OK) {
            nvs_erase_key(ota_nvs, "pending_report");
            nvs_commit(ota_nvs);
            nvs_close(ota_nvs);
        }
        wifi_manager_start();
    } else {
        wifi_manager_stop(); /* default OFF; enable via quick panel */
    }
}

#define LVGL_MSG_QUEUE_LEN  20   // Message queue capacity: 20 entries

QueueHandle_t lvgl_msg_queue = NULL;
EventGroupHandle_t lvgl_runtime_event_group = NULL;
bool g_ota_jump_ready = false;   // OTA 下载成功标志（Jump 按钮显示）
TaskHandle_t g_lvgl_task_handle = NULL;
TaskHandle_t g_sntp_time_task_handle = NULL;
TaskHandle_t g_sntp_interval_task_handle = NULL;

lv_ui guider_ui;


// LVGL UI access semaphore (mutual exclusion for screen operations)
SemaphoreHandle_t xGuiSemaphore = NULL;

// true = device is entering/exiting sleep mode (LVGL render loop paused)
volatile bool g_is_sleeping = false;
// Wake tick timestamp, touch input suppressed for 500 ms after wake
volatile uint32_t g_wake_tick = 0;

static uint32_t s_diag_seq = 0;
static uint32_t s_last_health_ms = 0;
static uint32_t s_last_flush_timeout_cnt = 0;
static uint32_t s_lvgl_send_queue_full_total = 0;
static uint32_t s_lvgl_send_queue_full_streak = 0;
static uint32_t s_ui_loop_overrun_total = 0;
static bool s_novel_swipe_guard_armed = false;
static uint32_t s_novel_swipe_guard_tick = 0;
static uint32_t s_novel_swipe_guard_window_ms = 450;

static inline uint32_t diag_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void lvgl_mark_novel_swipe_back_guard(uint32_t hold_ms)
{
    s_novel_swipe_guard_window_ms = (hold_ms > 0U) ? hold_ms : 450U;
    s_novel_swipe_guard_tick = lv_tick_get();
    s_novel_swipe_guard_armed = true;
}

bool lvgl_should_block_novel_click(void)
{
    if (!s_novel_swipe_guard_armed) return false;
    if (lv_tick_elaps(s_novel_swipe_guard_tick) < s_novel_swipe_guard_window_ms) {
        return true;
    }
    s_novel_swipe_guard_armed = false;
    return false;
}

bool lvgl_is_owner_task(void)
{
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    return (g_lvgl_task_handle != NULL && cur == g_lvgl_task_handle);
}

bool lvgl_thread_guard(const char *tag)
{
    if (lvgl_is_owner_task()) return true;
    ESP_LOGE(TAG, "LVGL thread violation: %s", tag ? tag : "unknown");
    return false;
}

bool lvgl_wait_for_pause_ack(uint32_t timeout_ms)
{
    if (lvgl_runtime_event_group == NULL) return false;
    EventBits_t bits = xEventGroupWaitBits(
        lvgl_runtime_event_group,
        LVGL_RT_BIT_PAUSED_ACK,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(timeout_ms));
    return (bits & LVGL_RT_BIT_PAUSED_ACK) != 0;
}

void lvgl_runtime_clear_pause_ack(void)
{
    if (lvgl_runtime_event_group != NULL) {
        xEventGroupClearBits(lvgl_runtime_event_group, LVGL_RT_BIT_PAUSED_ACK);
    }
}

void system_diag_snapshot(const char *reason)
{
    display_flush_state_t flush_state = {0};
    video_player_diag_t video_diag = {0};
    lv_port_get_flush_state(&flush_state);
    video_player_get_diag(&video_diag);

    UBaseType_t q_waiting = (lvgl_msg_queue != NULL) ? uxQueueMessagesWaiting(lvgl_msg_queue) : 0;
    UBaseType_t hwm_lvgl = (g_lvgl_task_handle != NULL) ? uxTaskGetStackHighWaterMark(g_lvgl_task_handle) : 0;
    UBaseType_t hwm_sntp = (g_sntp_time_task_handle != NULL) ? uxTaskGetStackHighWaterMark(g_sntp_time_task_handle) : 0;
    UBaseType_t hwm_sntp_itv = (g_sntp_interval_task_handle != NULL) ? uxTaskGetStackHighWaterMark(g_sntp_interval_task_handle) : 0;
    UBaseType_t q_storage = storage_worker_queue_waiting();
    UBaseType_t hwm_storage = storage_worker_stack_hwm();

    ESP_LOGI(TAG,
             "[DIAG#%lu] reason=%s q=%u q_storage=%u lvgl_hwm=%u storage_hwm=%u sntp_hwm=%u sntp_itv_hwm=%u",
             (unsigned long)++s_diag_seq,
             reason ? reason : "none",
             (unsigned int)q_waiting,
             (unsigned int)q_storage,
             (unsigned int)hwm_lvgl,
             (unsigned int)hwm_storage,
             (unsigned int)hwm_sntp,
             (unsigned int)hwm_sntp_itv);

    ESP_LOGI(TAG,
             "[DIAG] free_int=%uKB min_int=%uKB free_psram=%uKB min_psram=%uKB flush_timeout=%lu queue_full=%lu ui_overrun=%lu video(drop=%lu,stop_to=%lu,last_wait=%lu,max_wait=%lu)",
             (unsigned int)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned int)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned int)(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned long)flush_state.flush_wait_timeouts,
             (unsigned long)s_lvgl_send_queue_full_total,
             (unsigned long)s_ui_loop_overrun_total,
             (unsigned long)video_diag.frame_drop_count,
             (unsigned long)video_diag.stop_timeout_count,
             (unsigned long)video_diag.stop_wait_last_ms,
             (unsigned long)video_diag.stop_wait_max_ms);
}

void lvgl_runtime_health_tick(void)
{
    uint32_t now = diag_now_ms();
    if ((now - s_last_health_ms) < 5000U) return;
    s_last_health_ms = now;

    display_flush_state_t flush_state = {0};
    lv_port_get_flush_state(&flush_state);
    if (flush_state.flush_wait_timeouts != s_last_flush_timeout_cnt) {
        s_last_flush_timeout_cnt = flush_state.flush_wait_timeouts;
        system_diag_snapshot("flush-timeout");
        return;
    }
    system_diag_snapshot("periodic");
}


void app_main(void)
{
    power_sleep_boot_init();

    lvgl_runtime_event_group = xEventGroupCreate();
    if (lvgl_runtime_event_group == NULL) {
        ESP_LOGE(TAG, "lvgl_runtime_event_group create failed");
    }

    lvgl_msg_queue_init();

    print_memory_info("[Boot] Before hardware init");

    SD_card_init();
    Key_Init();
    my_timer_init();
    storage_worker_init();
    battery_init();
    power_sleep_init();

    print_memory_info("[Boot] After SD+Timer+Key init");
    esp_err_t ret = nvs_flash_init();

    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || 
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_LOGW(TAG, "NVS partition full or version mismatch, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());  // Initialize TCP/IP network stack

    if(xTaskCreatePinnedToCore(sntp_time_task, "sntp_time_task", 4096, NULL, 4, &g_sntp_time_task_handle, 0) != pdPASS){
        ESP_LOGE(TAG, "create sntp_time_task failed");
    }
    if(xTaskCreatePinnedToCore(sntp_interval_task, "sntp_interval_task", 4096, NULL, 5, &g_sntp_interval_task_handle, 0) != pdPASS){
        ESP_LOGE(TAG, "create sntp_interval_task failed");
    }
    if(xTaskCreatePinnedToCore(lvgl_diaplay_task, "lvgl_diaplay_task", 8192, NULL, 6, &g_lvgl_task_handle, 1) != pdPASS){
        ESP_LOGE(TAG, "create lvgl_diaplay_task failed");
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    print_memory_info("[Boot] After LVGL task created");
    system_diag_snapshot("lvgl-task-started");
    wifi_manager_init(wifi_state_handler);
    ota_pending_report_handle();   /* OTA 跳转后自动开 WiFi 上报新版本 */
    weather_service_init();
    vTaskDelay(pdMS_TO_TICKS(200));
    print_memory_info("[Boot] After WiFi+Weather init");
    system_diag_snapshot("wifi-init(default-off)");

    uint8_t Key_Num;
    while(1){
        Key_Num = Key_GetNum();
        if (Key_Num == 3) {
            lvgl_msg_send(LVGL_MSG_KEY_CONFIRM, 0, NULL);
        } else if (Key_Num == 2) {
            lvgl_msg_send(LVGL_MSG_KEY_UP, 0, NULL);
        } else if (Key_Num == 1) {
            lvgl_msg_send(LVGL_MSG_KEY_DOWN, 0, NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


   extern time_t now;
   extern struct tm timeinfo;

static lv_obj_t *s_key_nav_selected = NULL;
static lv_obj_t *s_key_nav_list = NULL;

static bool key_nav_item_is_valid(lv_obj_t *obj)
{
    return (obj != NULL && lv_obj_is_valid(obj));
}

/* 选中样式：白色边框高亮
 * 用本地样式(lv_obj_set_style_*)而非 add_style：
 * 渐变按钮的背景是本地样式，优先级高于普通样式，add_style 永远盖不住；
 * 边框不影响背景，对渐变按钮和 lv_list 按钮都清晰可见 */
static void key_nav_apply_selected_style(lv_obj_t *obj, bool selected)
{
    if (!key_nav_item_is_valid(obj)) return;
    lv_obj_set_style_border_width(obj, selected ? 2 : 0, 0);
    if (selected) {
        lv_obj_set_style_border_color(obj, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, 0);
    }
}

static void key_nav_set_selected(lv_obj_t *item, bool ensure_visible)
{
    if(s_key_nav_selected == item){
        if (ensure_visible && key_nav_item_is_valid(item)) {
            lv_obj_scroll_to_view(item, LV_ANIM_OFF);
        }
        return;
    }

    key_nav_apply_selected_style(s_key_nav_selected, false);
    s_key_nav_selected = item;
    key_nav_apply_selected_style(s_key_nav_selected, true);

    if (ensure_visible && key_nav_item_is_valid(item)) {
        lv_obj_scroll_to_view(s_key_nav_selected, LV_ANIM_OFF);
    }
}

static size_t key_nav_collect_current_items(lv_obj_t **out, size_t cap, lv_obj_t **list_out)
{
    if(out == NULL || cap == 0) return 0;
    memset(out, 0, cap * sizeof(out[0]));
    if (list_out != NULL) *list_out = NULL;

    lv_obj_t *scr = lv_scr_act();
    if(scr == NULL || !lv_obj_is_valid(scr)) return 0;

    size_t n = 0;

#define KEY_NAV_PUSH_ITEM(_obj) \
    do{ \
        if ((_obj) != NULL && lv_obj_is_valid((_obj)) && n < cap) { \
            out[n++] = (_obj); \
        } \
    }while(0)

    if(scr == guider_ui.menu_screen && lv_obj_is_valid(guider_ui.menu_screen)){
        if (list_out != NULL) *list_out = guider_ui.menu_screen_list_1;
        KEY_NAV_PUSH_ITEM(guider_ui.menu_screen_list_1_item0);
        KEY_NAV_PUSH_ITEM(guider_ui.menu_screen_list_1_item1);
        KEY_NAV_PUSH_ITEM(guider_ui.menu_screen_list_1_item2);
        KEY_NAV_PUSH_ITEM(guider_ui.menu_screen_list_1_item3);
        KEY_NAV_PUSH_ITEM(guider_ui.menu_screen_list_1_item4);
    }else if (scr == guider_ui.setting_screen && lv_obj_is_valid(guider_ui.setting_screen)){
        if (list_out != NULL) *list_out = guider_ui.setting_screen_list_1;
        KEY_NAV_PUSH_ITEM(guider_ui.setting_screen_list_1_item0);
        KEY_NAV_PUSH_ITEM(guider_ui.setting_screen_list_1_item1);
        KEY_NAV_PUSH_ITEM(guider_ui.setting_screen_list_1_item2);
    }else if (scr == guider_ui.novel_list && lv_obj_is_valid(guider_ui.novel_list)){
        if (list_out != NULL) *list_out = guider_ui.novel_list_list_1;
        for (size_t i = 0; i < _LIST_NUMBER && n < cap; ++i) {
            KEY_NAV_PUSH_ITEM(guider_ui.novel_list_list_1_item[i]);
        }
    }else if (scr == guider_ui.video_list && lv_obj_is_valid(guider_ui.video_list)){
        if (list_out != NULL) *list_out = guider_ui.video_list_list;
        for (size_t i = 0; i < _LIST_NUMBER && n < cap; ++i) {
            KEY_NAV_PUSH_ITEM(guider_ui.video_list_list_item[i]);
        }
    }else if (scr == guider_ui.screen_img_list && lv_obj_is_valid(guider_ui.screen_img_list)){
        if (list_out != NULL) *list_out = guider_ui.screen_img_list_list_1;
        for (size_t i = 0; i < _LIST_NUMBER && n < cap; ++i) {
            KEY_NAV_PUSH_ITEM(guider_ui.screen_img_list_list_1_item[i]);
        }
    }else if (scr == guider_ui.screen_game && lv_obj_is_valid(guider_ui.screen_game)){
        if (list_out != NULL) *list_out = guider_ui.screen_game_list_1;
        for (size_t i = 0; i < _LIST_NUMBER && n < cap; ++i) {
            KEY_NAV_PUSH_ITEM(guider_ui.screen_game_list_1_item[i]);
        }
    }else if(scr == guider_ui.screen_ota && lv_obj_is_valid(guider_ui.screen_ota)){
        if (list_out != NULL) *list_out = guider_ui.screen_ota_list_1;
        KEY_NAV_PUSH_ITEM(guider_ui.screen_ota_list_1_item0);
        KEY_NAV_PUSH_ITEM(guider_ui.screen_ota_list_1_item1);
        KEY_NAV_PUSH_ITEM(guider_ui.screen_ota_list_1_item2);
    }else if (scr == guider_ui.screen_ota_local && lv_obj_is_valid(guider_ui.screen_ota_local)) {
        if (list_out != NULL) *list_out = guider_ui.screen_ota_local_list_1;
        for (size_t i = 0; i < 20 && n < cap; ++i) {
            KEY_NAV_PUSH_ITEM(guider_ui.screen_ota_local_list_1_item[i]);
        }
    }

#undef KEY_NAV_PUSH_ITEM
    return n;
}

static bool key_nav_sync_to_current_screen(bool force_first)
{
    lv_obj_t *items[_LIST_NUMBER] = {0};
    lv_obj_t *list_obj = NULL;
    size_t count = key_nav_collect_current_items(items, _LIST_NUMBER, &list_obj);

    if (count == 0 || list_obj == NULL || !lv_obj_is_valid(list_obj)) {
        s_key_nav_list = NULL;
        key_nav_set_selected(NULL, false);
        return false;
    }

    bool found_selected = false;
    if (!force_first && key_nav_item_is_valid(s_key_nav_selected)) {
        for (size_t i = 0; i < count; ++i) {
            if (items[i] == s_key_nav_selected) {
                found_selected = true;
                break;
            }
        }
    }

    if (force_first || s_key_nav_list != list_obj || !found_selected) {
        s_key_nav_list = list_obj;
        key_nav_set_selected(items[0], true);
    } else {
        key_nav_set_selected(s_key_nav_selected, true);
    }
    return true;
}

static void key_nav_move(int dir)
{
    lv_obj_t *items[_LIST_NUMBER] = {0};
    lv_obj_t *list_obj = NULL;
    size_t count = key_nav_collect_current_items(items, _LIST_NUMBER, &list_obj);

    if (count == 0 || list_obj == NULL || !lv_obj_is_valid(list_obj)) {
        s_key_nav_list = NULL;
        key_nav_set_selected(NULL, false);
        return;
    }

    s_key_nav_list = list_obj;

    size_t cur_idx = 0;
    bool found = false;
    if (key_nav_item_is_valid(s_key_nav_selected)) {
        for (size_t i = 0; i < count; ++i) {
            if (items[i] == s_key_nav_selected) {
                cur_idx = i;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        cur_idx = 0;
    }

    size_t next_idx = cur_idx;
    if (dir > 0) {
        next_idx = (cur_idx + 1U) % count;
    } else if (dir < 0) {
        next_idx = (cur_idx == 0U) ? (count - 1U) : (cur_idx - 1U);
    }
    key_nav_set_selected(items[next_idx], true);
}

static void key_nav_confirm_selected(void)
{
    if (!key_nav_sync_to_current_screen(false)) return;
    if (!key_nav_item_is_valid(s_key_nav_selected)) return;
    lv_event_send(s_key_nav_selected, LV_EVENT_CLICKED, NULL);
}


static void novel_list_item_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (indev != NULL && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT) {
        lvgl_mark_novel_swipe_back_guard(450);
        ESP_LOGI(TAG, "ignore novel click during right-swipe");
        return;
    }
    if (lvgl_should_block_novel_click()) {
        ESP_LOGI(TAG, "ignore novel click in swipe guard window");
        return;
    }

    lv_obj_t *btn = lv_event_get_target(e);
    const char *name = ui_gradient_btn_get_text(btn);
    if (name == NULL || name[0] == '\0') return;
    lvgl_msg_send(LVGL_MSG_NOVEL_OPEN_REQ, 0, name);
}

static void video_list_item_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t *btn = lv_event_get_target(e);
    const char *name = ui_gradient_btn_get_text(btn);
    if (name == NULL || name[0] == '\0') return;
    lvgl_msg_send(LVGL_MSG_VIDEO_OPEN_REQ, 0, name);
}

static void render_novel_list(void)
{
    if (!lvgl_thread_guard("render_novel_list")) return;
    if (guider_ui.novel_list_list_1 == NULL || !lv_obj_is_valid(guider_ui.novel_list_list_1)) return;

    static storage_file_entry_t *entries = NULL;
    size_t entry_cap = _LIST_NUMBER;
    if (entries == NULL) {
        entries = (storage_file_entry_t *)heap_caps_malloc(
            entry_cap * sizeof(storage_file_entry_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (entries == NULL) {
            entries = (storage_file_entry_t *)heap_caps_malloc(
                entry_cap * sizeof(storage_file_entry_t), MALLOC_CAP_8BIT);
        }
        if (entries == NULL) {
            ESP_LOGE(TAG, "render_novel_list alloc failed");
            lv_obj_clean(guider_ui.novel_list_list_1);
            lv_list_add_text(guider_ui.novel_list_list_1, "No memory");
            return;
        }
    }

    size_t count = storage_get_novel_list(entries, entry_cap);

    lv_obj_clean(guider_ui.novel_list_list_1);
    for (int i = 0; i < _LIST_NUMBER; ++i) {
        guider_ui.novel_list_list_1_item[i] = NULL;
    }

    if (count == 0) {
        lv_obj_t *tip = lv_label_create(guider_ui.novel_list_list_1);
        lv_label_set_text(tip, "No novels");
        lv_obj_set_style_text_color(tip, lv_color_hex(0x666666), 0);
        key_nav_sync_to_current_screen(true);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        lv_obj_t *btn = ui_gradient_btn_create(guider_ui.novel_list_list_1, NULL,
                                               entries[i].name, (uint32_t)i);
        guider_ui.novel_list_list_1_item[i] = btn;
        lv_obj_add_event_cb(btn, novel_list_item_event_cb, LV_EVENT_CLICKED, NULL);
    }

    key_nav_sync_to_current_screen(true);
}

static void render_video_list(void)
{
    if (!lvgl_thread_guard("render_video_list")) return;
    if (guider_ui.video_list_list == NULL || !lv_obj_is_valid(guider_ui.video_list_list)) return;

    static storage_file_entry_t *entries = NULL;
    size_t entry_cap = _LIST_NUMBER;
    if (entries == NULL) {
        entries = (storage_file_entry_t *)heap_caps_malloc(
            entry_cap * sizeof(storage_file_entry_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (entries == NULL) {
            entries = (storage_file_entry_t *)heap_caps_malloc(
                entry_cap * sizeof(storage_file_entry_t), MALLOC_CAP_8BIT);
        }
        if (entries == NULL) {
            ESP_LOGE(TAG, "render_video_list alloc failed");
            lv_obj_clean(guider_ui.video_list_list);
            lv_list_add_text(guider_ui.video_list_list, "No memory");
            return;
        }
    }

    size_t count = storage_get_video_list(entries, entry_cap);

    lv_obj_clean(guider_ui.video_list_list);
    for (int i = 0; i < _LIST_NUMBER; ++i) {
        guider_ui.video_list_list_item[i] = NULL;
    }

    if (count == 0) {
        lv_obj_t *tip = lv_label_create(guider_ui.video_list_list);
        lv_label_set_text(tip, "No videos");
        lv_obj_set_style_text_color(tip, lv_color_hex(0x666666), 0);
        key_nav_sync_to_current_screen(true);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        lv_obj_t *btn = ui_gradient_btn_create(guider_ui.video_list_list, NULL,
                                               entries[i].name, (uint32_t)i);
        guider_ui.video_list_list_item[i] = btn;
        lv_obj_add_event_cb(btn, video_list_item_event_cb, LV_EVENT_CLICKED, NULL);
    }

    key_nav_sync_to_current_screen(true);
}

static void update_novel_page_label(const char *text)
{
    if (!lvgl_thread_guard("update_novel_page_label")) return;
    if (guider_ui.novel_display_label_1 == NULL || !lv_obj_is_valid(guider_ui.novel_display_label_1)) return;
    lv_label_set_text(guider_ui.novel_display_label_1, (text != NULL) ? text : "");
}


/* Process all pending messages in the LVGL message queue.
 * Called every iteration of the LVGL display loop. */
static void lvgl_process_msg_queue(void)
{
    lvgl_msg_t msg = {0};
    while (xQueueReceive(lvgl_msg_queue, &msg, 0) == pdTRUE) {
        switch (msg.type) {
            case LVGL_MSG_WIFI_CONNECTED:
                if (!guider_ui.screen_wifi_set_del && guider_ui.screen_wifi_set_label_status != NULL) {
                    lv_label_set_text(guider_ui.screen_wifi_set_label_status, "Connected!");
                    lv_obj_set_style_text_color(guider_ui.screen_wifi_set_label_status, lv_color_hex(0x00ff00), 0);
                }
                wifi_quick_set_state(true);
                break;

            case LVGL_MSG_WIFI_DISCONNECTED:
                if (!guider_ui.screen_wifi_set_del && guider_ui.screen_wifi_set_label_status != NULL) {
                    lv_label_set_text(guider_ui.screen_wifi_set_label_status, "Disconnected");
                    lv_obj_set_style_text_color(guider_ui.screen_wifi_set_label_status, lv_color_hex(0xff0000), 0);
                }
                wifi_quick_set_state(false);
                break;

            case LVGL_MSG_KEY_NEXT_PAGE:
                if (lv_scr_act() == guider_ui.novel_display) {
                    storage_request_novel_page_next();
                }
                break;

            case LVGL_MSG_KEY_PREV_PAGE:
                if (lv_scr_act() == guider_ui.novel_display) {
                    storage_request_novel_page_prev();
                }
                break;

            case LVGL_MSG_KEY_CONFIRM:
                key_nav_confirm_selected();
                break;

            case LVGL_MSG_KEY_UP:
                if (lv_scr_act() == guider_ui.novel_display) {
                    storage_request_novel_page_prev();
                } else {
                    key_nav_move(-1);
                }
                break;

            case LVGL_MSG_KEY_DOWN:
                if (lv_scr_act() == guider_ui.novel_display) {
                    storage_request_novel_page_next();
                } else {
                    key_nav_move(1);
                }
                break;

            case LVGL_MSG_NTP_SYNC_STATUS:
                quick_time_sync_set_status(msg.str_data, (uint32_t)msg.param);
                break;

            case LVGL_MSG_WEATHER_STATUS:
                weather_ui_set_status(msg.str_data, (uint32_t)msg.param);
                break;

            case LVGL_MSG_WEATHER_UPDATED:
                weather_ui_refresh_from_snapshot();
                break;

            case LVGL_MSG_OTA_STATUS:
                ESP_LOGI(TAG, "[OTA] status msg received: %s", msg.str_data);
                /* 下载成功：置位全局标志（无论当前屏幕是否可见，保证 Jump 按钮最终显示）*/
                if (strstr(msg.str_data, "下载成功") != NULL) {
                    g_ota_jump_ready = true;
                }
                /* OneNET OTA 状态：只要屏幕对象存在就更新（屏幕隐藏也生效，切回即见）*/
                if (guider_ui.screen_ota_onenet != NULL &&
                    guider_ui.screen_ota_onenet_label_status != NULL) {
                    lv_label_set_text(guider_ui.screen_ota_onenet_label_status, msg.str_data);
                    /* 下载成功：显示 Jump 跳转按钮 */
                    if (g_ota_jump_ready &&
                        guider_ui.screen_ota_onenet_btn_jump != NULL) {
                        ESP_LOGI(TAG, "[OTA] show jump button");
                        lv_obj_clear_flag(guider_ui.screen_ota_onenet_btn_jump, LV_OBJ_FLAG_HIDDEN);
                    }
                } else if (guider_ui.screen_ota_local != NULL &&
                           !lv_obj_has_flag(guider_ui.screen_ota_local, LV_OBJ_FLAG_HIDDEN)) {
                    local_ota_update_msgbox(msg.str_data);
                }
                break;

            case LVGL_MSG_OTA_PROGRESS:
                if (guider_ui.screen_ota_onenet != NULL &&
                    !lv_obj_has_flag(guider_ui.screen_ota_onenet, LV_OBJ_FLAG_HIDDEN) &&
                    guider_ui.screen_ota_onenet_label_status != NULL) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Download: %d%%", (int)msg.param);
                    lv_label_set_text(guider_ui.screen_ota_onenet_label_status, buf);
                }
                break;

            case LVGL_MSG_OTA_COMPLETE:
                local_ota_set_running(false);
                local_ota_close_msgbox();
                local_ota_handle_complete_result((int)msg.param);
                break;

            case LVGL_MSG_NOVEL_LIST_REFRESH_REQ:
                storage_request_novel_list_refresh();
                break;

            case LVGL_MSG_VIDEO_LIST_REFRESH_REQ:
                storage_request_video_list_refresh();
                break;

            case LVGL_MSG_NOVEL_OPEN_REQ:
                storage_request_novel_open_by_name((const char *)msg.data);
                break;

            case LVGL_MSG_NOVEL_OPEN:
                storage_request_novel_open_by_path((const char *)msg.data);
                break;

            case LVGL_MSG_NOVEL_OPEN_READY:
                ui_load_scr_animation(
                    &guider_ui,
                    &guider_ui.novel_display,
                    guider_ui.novel_display_del,
                    &guider_ui.novel_list_del,
                    setup_scr_novel_display,
                    LV_SCR_LOAD_ANIM_NONE,
                    200, 0, false, true);
                storage_request_novel_page_sync();
                break;

            case LVGL_MSG_NOVEL_PAGE_SYNC_REQ:
                storage_request_novel_page_sync();
                break;

            case LVGL_MSG_NOVEL_PAGE_NEXT_REQ:
                storage_request_novel_page_next();
                break;

            case LVGL_MSG_NOVEL_PAGE_PREV_REQ:
                storage_request_novel_page_prev();
                break;

            case LVGL_MSG_NOVEL_CLOSE_REQ:
                storage_request_novel_close();
                break;

            case LVGL_MSG_NOVEL_LIST_READY:
                render_novel_list();
                break;

            case LVGL_MSG_VIDEO_LIST_READY:
                render_video_list();
                break;

            case LVGL_MSG_NOVEL_PAGE_READY:
            {
                char text[1200] = {0};
                long offset = 0;
                if (storage_get_last_novel_page(text, sizeof(text), &offset)) {
                    g_file_offset = offset;
                    update_novel_page_label(text);
                }
                break;
            }

            case LVGL_MSG_VIDEO_OPEN_REQ:
                storage_request_video_resolve_by_name((const char *)msg.data);
                break;

            case LVGL_MSG_VIDEO_OPEN_READY:
            {
                video_format_t fmt = VIDEO_FORMAT_MJPEG;
                char path[512] = {0};
                if (storage_get_last_video_open(path, sizeof(path), &fmt)) {
                    snprintf(g_video_filepath, sizeof(g_video_filepath), "%s", path);
                    g_video_format = fmt;
                    ui_load_scr_animation(
                        &guider_ui,
                        &guider_ui.video_player, guider_ui.video_player_del,
                        &guider_ui.video_list_del, setup_scr_video_player,
                        LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
                }
                break;
            }

            case LVGL_MSG_VIDEO_STOP_REQ:
                video_player_stop_async();
                break;

            case LVGL_MSG_VIDEO_FRAME:
            {
                uint8_t frame_idx = (uint8_t)msg.param;
                const lv_img_dsc_t *frame = video_player_get_frame_desc(frame_idx);
                if (frame != NULL &&
                    guider_ui.video_player_img != NULL &&
                    lv_scr_act() == guider_ui.video_player) {
                    lv_obj_set_size(guider_ui.video_player_img, frame->header.w, frame->header.h);
                    lv_img_set_src(guider_ui.video_player_img, frame);
                    lv_obj_invalidate(guider_ui.video_player_img);
                }
                video_player_mark_frame_presented(frame_idx);
                break;
            }

            case LVGL_MSG_RETURN_TO_VIDEO_LIST:
                ui_load_scr_animation(&guider_ui,
                    &guider_ui.video_list, guider_ui.video_list_del,
                    &guider_ui.video_player_del, setup_scr_video_list,
                    LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
                break;

            case LVGL_MSG_NOVEL_OPEN_ERROR:
            case LVGL_MSG_VIDEO_OPEN_ERROR:
                ESP_LOGW(TAG, "worker error msg=%d", msg.type);
                break;

            default:
                break;
        }

        if (msg.need_free && msg.data != NULL) {
            free(msg.data);
            msg.data = NULL;
        }
    }
}



void lvgl_diaplay_task(void *param)
{
    LV_UNUSED(param);
    if (lvgl_msg_queue == NULL) {
        lvgl_msg_queue_init();
    }

    g_lvgl_task_handle = xTaskGetCurrentTaskHandle();

    lv_port_init();
    st7789_lcd_backlight(1);
    my_fs_init();
    /* Cache decoded images to reduce SD decode hitch when switching back to clock screen. */
    lv_img_cache_set_size(8);

    setup_ui(&guider_ui);
    battery_status_init();   /* 时钟屏电池图标 + 30s 刷新定时器 */

    lv_timer_create(update_time_timer_cb, 500, NULL);
    if (lvgl_runtime_event_group != NULL) {
        xEventGroupSetBits(lvgl_runtime_event_group, LVGL_RT_BIT_TASK_READY);
    }

    while (1) {
        if (g_is_sleeping) {
            if (lvgl_runtime_event_group != NULL) {
                xEventGroupSetBits(lvgl_runtime_event_group, LVGL_RT_BIT_PAUSED_ACK);
            }
            lvgl_runtime_health_tick();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (lvgl_runtime_event_group != NULL) {
            xEventGroupClearBits(lvgl_runtime_event_group, LVGL_RT_BIT_PAUSED_ACK);
        }

        static uint32_t time_till_next = 10;
        uint32_t t0_ms = diag_now_ms();

        lvgl_process_msg_queue();
        if (lvgl_runtime_event_group != NULL) {
            xEventGroupSetBits(lvgl_runtime_event_group, LVGL_RT_BIT_IN_HANDLER);
        }
        time_till_next = lv_task_handler();
        if (lvgl_runtime_event_group != NULL) {
            xEventGroupClearBits(lvgl_runtime_event_group, LVGL_RT_BIT_IN_HANDLER);
        }

        uint32_t cost_ms = diag_now_ms() - t0_ms;
        if (cost_ms > 200U) {
            s_ui_loop_overrun_total++;
            ESP_LOGW(TAG, "UI loop overrun: %lu ms", (unsigned long)cost_ms);
        }

        if (time_till_next < 10) time_till_next = 10;
        if (time_till_next > 500) time_till_next = 500;

        lvgl_runtime_health_tick();
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }

}





static void update_time_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if (!lvgl_thread_guard("update_time_timer_cb")) return;

    // Guard 1: clock screen must exist and be valid
    if (guider_ui.clock_screen == NULL ||
        !lv_obj_is_valid(guider_ui.clock_screen)) {
        return;
    }

    // Guard 2: only update when clock screen is the active screen
    if (lv_scr_act() != guider_ui.clock_screen) {
        return;
    }

    // Guard 3: time/date labels must exist and be valid
    if (guider_ui.clock_screen_label_time == NULL ||
        guider_ui.clock_screen_label_date == NULL ||
        !lv_obj_is_valid(guider_ui.clock_screen_label_time) ||
        !lv_obj_is_valid(guider_ui.clock_screen_label_date)) {
        return;
    }

    // Guard 4: skip update during screen transition animation
    if (lv_disp_get_scr_prev(NULL) != NULL) {
        return;
    }

    // Update time display (format: 14:20)
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);
    lv_label_set_text(guider_ui.clock_screen_label_time, time_buf);
    // Update date display (format: 2026.8.10)
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%d.%d.%d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday);
    lv_label_set_text(guider_ui.clock_screen_label_date, date_buf);
}


// Print memory usage statistics to serial log
void print_memory_info(const char *label)
{
    ESP_LOGI(TAG, "========== %s ==========", label);
    ESP_LOGI(TAG, "INTERNAL: total=%6d KB  free=%6d KB  min_free=%6d KB",
             heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
             heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);

    ESP_LOGI(TAG, "DMA:     total=%6d KB  free=%6d KB  min_free=%6d KB",
             heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024,
             heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024);

    ESP_LOGI(TAG, "PSRAM:   total=%6d KB  free=%6d KB  min_free=%6d KB",
             heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024,
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
             heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024);

    ESP_LOGI(TAG, "Largest free block: INTERNAL=%d KB  PSRAM=%d KB",
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024,
             heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
    ESP_LOGI(TAG, "====================================");
}




void lvgl_msg_queue_init(void)
{
    lvgl_msg_queue = xQueueCreate(LVGL_MSG_QUEUE_LEN, sizeof(lvgl_msg_t));
    if (lvgl_msg_queue == NULL) {
        ESP_LOGE(TAG, "LVGL消息队列初始化失败");
    } else {
        ESP_LOGI(TAG, "LVGL消息队列数量=%d", LVGL_MSG_QUEUE_LEN);
    }
}

static bool msg_type_needs_strdup(lvgl_msg_type_t type)
{
    return (type == LVGL_MSG_NOVEL_OPEN_REQ ||
            type == LVGL_MSG_VIDEO_OPEN_REQ ||
            type == LVGL_MSG_NOVEL_OPEN);
}

static bool msg_type_uses_str_data(lvgl_msg_type_t type)
{
    return (type == LVGL_MSG_OTA_STATUS ||
            type == LVGL_MSG_NTP_SYNC_STATUS ||
            type == LVGL_MSG_WEATHER_STATUS);
}

static inline bool utf8_is_continuation(uint8_t b)
{
    return (b & 0xC0U) == 0x80U;
}

static size_t utf8_sequence_len(uint8_t lead)
{
    if (lead < 0x80U) return 1;
    if (lead >= 0xC2U && lead <= 0xDFU) return 2;
    if (lead >= 0xE0U && lead <= 0xEFU) return 3;
    if (lead >= 0xF0U && lead <= 0xF4U) return 4;
    return 0;
}

static void copy_utf8_trunc(char *dst, size_t cap, const char *src)
{
    if (dst == NULL || cap == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    size_t out = 0;
    while (*src != '\0' && out < cap - 1) {
        uint8_t lead = (uint8_t)(*src);
        size_t len = utf8_sequence_len(lead);
        if (len == 0) {
            dst[out++] = '?';
            src++;
            continue;
        }
        if (out + len > cap - 1) {
            break;
        }

        bool ok = true;
        for (size_t i = 1; i < len; ++i) {
            if (src[i] == '\0' || !utf8_is_continuation((uint8_t)src[i])) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            dst[out++] = '?';
            src++;
            continue;
        }

        memcpy(dst + out, src, len);
        out += len;
        src += len;
    }
    dst[out] = '\0';
}

static void on_lvgl_queue_send_ok(void)
{
    s_lvgl_send_queue_full_streak = 0;
}

static void on_lvgl_queue_send_fail(lvgl_msg_t *msg)
{
    s_lvgl_send_queue_full_total++;
    s_lvgl_send_queue_full_streak++;
    if ((s_lvgl_send_queue_full_streak % 10U) == 1U) {
        ESP_LOGW(TAG, "LVGL queue full streak=%lu type=%d",
                 (unsigned long)s_lvgl_send_queue_full_streak,
                 (int)msg->type);
    }
    if (msg->need_free && msg->data != NULL) {
        free(msg->data);
        msg->data = NULL;
    }
}

bool lvgl_msg_send(lvgl_msg_type_t type, int32_t param, const void *data)
{
    if (lvgl_msg_queue == NULL) {
        ESP_LOGE(TAG, "[LVGL] Message queue not initialized!");
        return false;
    }

    lvgl_msg_t msg = {
        .type  = type,
        .param = param,
        .need_free = false,
    };

    /* Small status strings stored inline to avoid heap allocations. */
    if (data != NULL && msg_type_uses_str_data(type)) {
        copy_utf8_trunc(msg.str_data, sizeof(msg.str_data), (const char *)data);
        msg.need_free = false;
    } else if (data != NULL && msg_type_needs_strdup(type)) {
        const char *src = (const char *)data;
        size_t len = strlen(src);
        char *copy = (char *)malloc(len + 1);
        if (copy == NULL) {
            ESP_LOGE(TAG, "[LVGL] strdup alloc failed type=%d", type);
            return false;
        }
        memcpy(copy, src, len + 1);
        msg.data = copy;
        msg.need_free = true;
    } else {
        msg.data = (void *)data;
        msg.need_free = false;
    }

    // Blocking send with 100 ms timeout
    BaseType_t result = xQueueSend(lvgl_msg_queue, &msg, pdMS_TO_TICKS(100));
    if (result != pdTRUE) {
        on_lvgl_queue_send_fail(&msg);
        return false;
    }
    on_lvgl_queue_send_ok();
    return true;
}

BaseType_t lvgl_msg_send_nonblocking(lvgl_msg_type_t type, int32_t param, const void *data)
{
    if (lvgl_msg_queue == NULL) {
        return pdFALSE;
    }

    lvgl_msg_t msg = {
        .type  = type,
        .param = param,
        .need_free = false,
    };

    if (data != NULL && msg_type_uses_str_data(type)) {
        copy_utf8_trunc(msg.str_data, sizeof(msg.str_data), (const char *)data);
        msg.need_free = false;
    } else if (data != NULL && msg_type_needs_strdup(type)) {
        const char *src = (const char *)data;
        size_t len = strlen(src);
        char *copy = (char *)malloc(len + 1);
        if (copy == NULL) {
            return pdFALSE;
        }
        memcpy(copy, src, len + 1);
        msg.data = copy;
        msg.need_free = true;
    } else {
        msg.data = (void *)data;
        msg.need_free = false;
    }

    BaseType_t ret = xQueueSend(lvgl_msg_queue, &msg, 0);
    if (ret != pdTRUE) {
        on_lvgl_queue_send_fail(&msg);
        return pdFALSE;
    }
    on_lvgl_queue_send_ok();
    return pdTRUE;
}





void *lvgl_psram_alloc(size_t size)
{
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (p == NULL) {
        // PSRAM allocation failed, fall back to internal RAM
        p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
    }
    return p;
}

void lvgl_psram_free(void *p)
{
    heap_caps_free(p);
}

void *lvgl_psram_realloc(void *p, size_t new_size)
{
    void *new_p = heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM);
    if (new_p == NULL) {
        new_p = heap_caps_realloc(p, new_size, MALLOC_CAP_INTERNAL);
    }
    return new_p;
}






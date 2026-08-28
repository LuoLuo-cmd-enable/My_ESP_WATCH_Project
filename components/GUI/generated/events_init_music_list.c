/*
 * events_init_music_list.c - 音乐列表事件
 * 列表项点击 → 发 MUSIC_OPEN_REQ；右滑返回主菜单
 */
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include "gui_guider.h"
#include "lvgl_display.h"
#include "setup_scr_screen_music_list.h"
#include "custom.h"

static void music_list_item_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_current_target(e);
    const char *file_name = ui_gradient_btn_get_text(btn);
    if (file_name == NULL || file_name[0] == '\0') return;
    ESP_LOGI("MUSIC_LIST", "select music: %s", file_name);
    lvgl_msg_send(LVGL_MSG_MUSIC_OPEN_REQ, 0, file_name);
}

static void music_list_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            ui_load_scr_animation(&guider_ui,
                &guider_ui.menu_screen, guider_ui.menu_screen_del,
                &guider_ui.screen_music_list_del, setup_scr_menu_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

void events_init_music_list(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_music_list, music_list_event_handler, LV_EVENT_ALL, ui);

    for (int i = 0; i < _LIST_NUMBER; i++) {
        if (ui->screen_music_list_list_item[i] == NULL) continue;
        lv_obj_add_event_cb(
            ui->screen_music_list_list_item[i],
            music_list_item_handler,
            LV_EVENT_CLICKED,
            NULL
        );
    }
}

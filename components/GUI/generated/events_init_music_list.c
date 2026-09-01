/*
 * events_init_music_list.c - 音乐列表事件
 * 右滑返回主菜单；列表项点击由 render_music_list 注册处理
 */
#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "lvgl_display.h"
#include "setup_scr_screen_music_list.h"
#include "custom.h"

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

}

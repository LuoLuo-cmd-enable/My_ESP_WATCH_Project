/*
 * setup_scr_screen_music_list.c - 音乐列表界面
 * 架构同 video_list：渐变卡片按钮动态渲染，占位 Loading
 */
#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "setup_scr_screen_music_list.h"
#include "lvgl_display.h"
#include "events_init_music_list.h"
#include "custom.h"

void setup_scr_screen_music_list(lv_ui *ui)
{
    ui->screen_music_list = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_music_list, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_music_list, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_bg_opa(ui->screen_music_list, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_music_list, lv_color_hex(0x010101), LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 纵向渐变卡片列表 */
    ui->screen_music_list_list = lv_obj_create(ui->screen_music_list);
    lv_obj_set_pos(ui->screen_music_list_list, 0, 0);
    lv_obj_set_size(ui->screen_music_list_list, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_music_list_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui->screen_music_list_list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui->screen_music_list_list, LV_SCROLL_SNAP_NONE);

    lv_obj_set_flex_flow(ui->screen_music_list_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->screen_music_list_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(ui->screen_music_list_list, 0, 0);
    lv_obj_set_style_border_width(ui->screen_music_list_list, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_music_list_list, 8, 0);
    lv_obj_set_style_pad_row(ui->screen_music_list_list, 10, 0);

    for (int i = 0; i < _LIST_NUMBER; i++) {
        ui->screen_music_list_list_item[i] = NULL;
    }

    /* 占位提示，实际列表由 render_music_list 动态刷新 */
    lv_obj_t *loading = lv_label_create(ui->screen_music_list_list);
    lv_label_set_text(loading, "Loading...");
    lv_obj_set_style_text_color(loading, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(loading, &songti_font_16, 0);

    lv_obj_update_layout(ui->screen_music_list);
    events_init_music_list(ui);

    lvgl_msg_send_nonblocking(LVGL_MSG_MUSIC_LIST_REFRESH_REQ, 0, NULL);
}

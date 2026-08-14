/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "lvgl_display.h"
#include <strings.h>


void setup_scr_novel_list(lv_ui *ui)
{
    //Write codes novel_list
    ui->novel_list = lv_obj_create(NULL);
    lv_obj_set_size(ui->novel_list, 240, 284);
    lv_obj_set_scrollbar_mode(ui->novel_list, LV_SCROLLBAR_MODE_OFF);

    //Write style for novel_list, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->novel_list, 250, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->novel_list, lv_color_hex(0x010101), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->novel_list, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes novel_list_list_1
    /* 纵向渐变卡片列表容器（替代原 lv_list） */
    ui->novel_list_list_1 = lv_obj_create(ui->novel_list);

    for (int i = 0; i < _LIST_NUMBER; i++) {
            ui->novel_list_list_1_item[i] = NULL;
        }

    // ★ 定向扫描小说目录（占位提示，实际列表由 render_novel_list 动态刷新）
    lv_obj_t *loading = lv_label_create(ui->novel_list_list_1);
    lv_label_set_text(loading, "Loading...");
    lv_obj_set_style_text_color(loading, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(loading, &songti_font_16, 0);

    lv_obj_set_pos(ui->novel_list_list_1, 0, 0);
    lv_obj_set_size(ui->novel_list_list_1, 240, 284);
    lv_obj_set_scrollbar_mode(ui->novel_list_list_1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui->novel_list_list_1, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui->novel_list_list_1, LV_SCROLL_SNAP_NONE);

    lv_obj_set_flex_flow(ui->novel_list_list_1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->novel_list_list_1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(ui->novel_list_list_1, 0, 0);
    lv_obj_set_style_border_width(ui->novel_list_list_1, 0, 0);
    lv_obj_set_style_pad_all(ui->novel_list_list_1, 8, 0);
    lv_obj_set_style_pad_row(ui->novel_list_list_1, 10, 0);

    /* 旧 lv_list 样式全部移除，按钮外观由 ui_gradient_btn_create 提供 */

    //The custom code of novel_list.


    //Update current screen layout.
    lv_obj_update_layout(ui->novel_list);

    //Init events for screen.
    events_init_novel_list(ui);

    lvgl_msg_send_nonblocking(LVGL_MSG_NOVEL_LIST_REFRESH_REQ, 0, NULL);
}

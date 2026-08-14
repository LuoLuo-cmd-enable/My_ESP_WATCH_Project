#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"
#include "setup_scr_video_list.h"
#include "lvgl_display.h"
#include "events_init_video_list.h"
#include "custom.h"

void setup_scr_video_list(lv_ui *ui)
{
    // 鍒涘缓鐣岄潰
    ui->video_list = lv_obj_create(NULL);
    lv_obj_set_size(ui->video_list, 240, 284);
    lv_obj_set_scrollbar_mode(ui->video_list, LV_SCROLLBAR_MODE_OFF);

    // 鑳屾櫙鏍峰紡
    lv_obj_set_style_bg_opa(ui->video_list, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->video_list, lv_color_hex(0x010101), LV_PART_MAIN|LV_STATE_DEFAULT);

    // 创建纵向渐变卡片列表（替代原 lv_list）
    ui->video_list_list = lv_obj_create(ui->video_list);
    lv_obj_set_pos(ui->video_list_list, 0, 0);
    lv_obj_set_size(ui->video_list_list, 240, 284);
    lv_obj_set_scrollbar_mode(ui->video_list_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui->video_list_list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui->video_list_list, LV_SCROLL_SNAP_NONE);

    lv_obj_set_flex_flow(ui->video_list_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->video_list_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(ui->video_list_list, 0, 0);
    lv_obj_set_style_border_width(ui->video_list_list, 0, 0);
    lv_obj_set_style_pad_all(ui->video_list_list, 8, 0);
    lv_obj_set_style_pad_row(ui->video_list_list, 10, 0);

    // 娓呯┖鎵€鏈?item
    for (int i = 0; i < _LIST_NUMBER; i++) {
        ui->video_list_list_item[i] = NULL;
    }

    // 占位提示，实际列表由 render_video_list 动态刷新
    lv_obj_t *loading = lv_label_create(ui->video_list_list);
    lv_label_set_text(loading, "Loading...");
    lv_obj_set_style_text_color(loading, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(loading, &songti_font_16, 0);

    // 旧 lv_list 样式全部移除，按钮外观由 ui_gradient_btn_create 提供


    lv_obj_update_layout(ui->video_list);
    events_init_video_list(ui);

    lvgl_msg_send_nonblocking(LVGL_MSG_VIDEO_LIST_REFRESH_REQ, 0, NULL);
}

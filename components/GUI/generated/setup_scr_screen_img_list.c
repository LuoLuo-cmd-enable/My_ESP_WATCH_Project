#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "SD_card.h"
#include "custom.h"

//进入picture部件后
void setup_scr_screen_img_list(lv_ui *ui)
{
    // 创建界面
    ui->screen_img_list = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_img_list, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_img_list, LV_SCROLLBAR_MODE_OFF);

    // 背景样式
    lv_obj_set_style_bg_opa(ui->screen_img_list, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_img_list, lv_color_hex(0x010101), LV_PART_MAIN|LV_STATE_DEFAULT);

    // 创建纵向渐变卡片列表（替代原 lv_list）
    ui->screen_img_list_list_1 = lv_obj_create(ui->screen_img_list);
    lv_obj_set_pos(ui->screen_img_list_list_1, 0, 0);
    lv_obj_set_size(ui->screen_img_list_list_1, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_img_list_list_1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui->screen_img_list_list_1, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui->screen_img_list_list_1, LV_SCROLL_SNAP_NONE);

    lv_obj_set_flex_flow(ui->screen_img_list_list_1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->screen_img_list_list_1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(ui->screen_img_list_list_1, 0, 0);
    lv_obj_set_style_border_width(ui->screen_img_list_list_1, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_img_list_list_1, 8, 0);
    lv_obj_set_style_pad_row(ui->screen_img_list_list_1, 10, 0);

    // 先清空所有 item
    for (int i = 0; i < _LIST_NUMBER; i++) {
        ui->screen_img_list_list_1_item[i] = NULL;
    }

    // ★ 定向扫描图片目录
    sd_scan_target_dir("/sdcard/images", ".png");

    // ★★★ 添加列表项：渐变卡片按钮，图标用 picture 并缩小到文字大小 ★★★
    int item_idx = 0;
    for (int i = 0; i < s_file_list.count; i++)
    {
        if (item_idx >= _LIST_NUMBER) break;  // ★ 绝对护盾：装满40个强制停手

        ui->screen_img_list_list_1_item[item_idx] = ui_gradient_btn_create(
            ui->screen_img_list_list_1,
            &picture,
            s_file_list.files[i].name,
            item_idx
        );
        item_idx++;
    }

    lv_obj_update_layout(ui->screen_img_list);
    events_init_screen_img_list(ui);
}
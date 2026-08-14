/*
 * setup_scr_screen_ota_local.c
 * 本地升级界面 —— SD卡.bin文件列表
 */

#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"
#include "events_init.h"
#include "SD_card.h"
#include "custom.h"

void setup_scr_screen_ota_local(lv_ui *ui)
{
    // 创建界面
    ui->screen_ota_local = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_ota_local, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_ota_local, LV_SCROLLBAR_MODE_OFF);

    // 背景样式
    lv_obj_set_style_bg_opa(ui->screen_ota_local, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_ota_local, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);

    // 标题
    lv_obj_t *title = lv_label_create(ui->screen_ota_local);
    lv_label_set_text(title, "本地升级");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &songti_font_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 创建纵向滑动按钮列表
    ui->screen_ota_local_list_1 = lv_obj_create(ui->screen_ota_local);
    lv_obj_set_pos(ui->screen_ota_local_list_1, 0, 44);
    lv_obj_set_size(ui->screen_ota_local_list_1, 240, 224);
    lv_obj_set_scrollbar_mode(ui->screen_ota_local_list_1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui->screen_ota_local_list_1, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui->screen_ota_local_list_1, LV_SCROLL_SNAP_NONE);

    lv_obj_set_flex_flow(ui->screen_ota_local_list_1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->screen_ota_local_list_1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(ui->screen_ota_local_list_1, 0, 0);
    lv_obj_set_style_border_width(ui->screen_ota_local_list_1, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_ota_local_list_1, 8, 0);
    lv_obj_set_style_pad_row(ui->screen_ota_local_list_1, 10, 0);

    // 先清空所有item
    for (int i = 0; i < 20; i++) {
        ui->screen_ota_local_list_1_item[i] = NULL;
    }

    // 定向扫描固件目录
    sd_scan_target_dir("/sdcard/firmware", ".bin");

    // 添加.bin文件到列表：复用全局渐变按钮函数
    int item_idx = 0;
    for (int i = 0; i < s_file_list.count; i++)
    {
        if (item_idx >= 20) break;

        ui->screen_ota_local_list_1_item[item_idx] = ui_gradient_btn_create(
            ui->screen_ota_local_list_1,
            NULL,
            s_file_list.files[i].name,
            item_idx
        );
        item_idx++;
    }

    // 提示标签
    lv_obj_t *label_tip = lv_label_create(ui->screen_ota_local);
    lv_label_set_text(label_tip, "请将固件放入SD卡firmware目录");
    lv_obj_set_style_text_color(label_tip, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(label_tip, &songti_font_16, 0);
    lv_obj_align(label_tip, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_update_layout(ui->screen_ota_local);

    // 事件初始化
    events_init_screen_ota_local(ui);
}

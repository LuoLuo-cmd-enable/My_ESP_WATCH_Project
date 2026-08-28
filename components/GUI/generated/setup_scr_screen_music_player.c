/*
 * setup_scr_screen_music_player.c - 音乐播放界面
 * 布局：歌名 / 波形柱(24根) / 进度条+时间 / 上一首·暂停·下一首
 */
#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "setup_scr_screen_music_player.h"
#include "events_init_music_player.h"
#include "custom.h"
#include "music_player.h"

#define WAVE_BAR_NUM    MUSIC_WAVE_BARS   /* 波形柱数量 */
#define WAVE_TOP        60
#define WAVE_HEIGHT     90
#define WAVE_GAP        4
#define WAVE_W          ((240 - WAVE_GAP * (WAVE_BAR_NUM - 1)) / WAVE_BAR_NUM)

void setup_scr_screen_music_player(lv_ui *ui)
{
    ui->screen_music_player = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_music_player, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_music_player, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->screen_music_player, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_music_player, lv_color_hex(0x0A0E1A), LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 歌名 */
    ui->screen_music_player_label_name = lv_label_create(ui->screen_music_player);
    lv_label_set_text(ui->screen_music_player_label_name, "Music");
    lv_obj_set_pos(ui->screen_music_player_label_name, 0, 10);
    lv_obj_set_size(ui->screen_music_player_label_name, 240, 30);
    lv_obj_set_style_text_font(ui->screen_music_player_label_name, &songti_font_16, 0);
    lv_obj_set_style_text_color(ui->screen_music_player_label_name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(ui->screen_music_player_label_name, LV_TEXT_ALIGN_CENTER, 0);

    /* 波形容器 */
    ui->screen_music_player_cont_wave = lv_obj_create(ui->screen_music_player);
    lv_obj_set_pos(ui->screen_music_player_cont_wave, 0, WAVE_TOP);
    lv_obj_set_size(ui->screen_music_player_cont_wave, 240, WAVE_HEIGHT);
    lv_obj_set_style_bg_opa(ui->screen_music_player_cont_wave, 0, 0);
    lv_obj_set_style_border_width(ui->screen_music_player_cont_wave, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_music_player_cont_wave, 0, 0);
    lv_obj_clear_flag(ui->screen_music_player_cont_wave, LV_OBJ_FLAG_SCROLLABLE);

    /* 波形柱：底部对齐，高度初始为 0（底部位于容器底） */
    for (int i = 0; i < WAVE_BAR_NUM; i++) {
        lv_obj_t *bar = lv_obj_create(ui->screen_music_player_cont_wave);
        lv_obj_set_width(bar, WAVE_W);
        lv_obj_set_height(bar, 2);
        lv_obj_set_pos(bar, i * (WAVE_W + WAVE_GAP), WAVE_HEIGHT - 2);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x00E676), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        ui->screen_music_player_wave_bar[i] = bar;
    }

    /* 进度条 */
    ui->screen_music_player_slider = lv_slider_create(ui->screen_music_player);
    lv_obj_set_pos(ui->screen_music_player_slider, 15, WAVE_TOP + WAVE_HEIGHT + 10);
    lv_obj_set_size(ui->screen_music_player_slider, 210, 8);
    lv_obj_set_style_bg_color(ui->screen_music_player_slider, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->screen_music_player_slider, lv_color_hex(0x00E676), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui->screen_music_player_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);

    /* 时间 */
    ui->screen_music_player_label_time = lv_label_create(ui->screen_music_player);
    lv_label_set_text(ui->screen_music_player_label_time, "00:00/00:00");
    lv_obj_set_pos(ui->screen_music_player_label_time, 0, WAVE_TOP + WAVE_HEIGHT + 24);
    lv_obj_set_size(ui->screen_music_player_label_time, 240, 20);
    lv_obj_set_style_text_font(ui->screen_music_player_label_time, &songti_font_16, 0);
    lv_obj_set_style_text_color(ui->screen_music_player_label_time, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(ui->screen_music_player_label_time, LV_TEXT_ALIGN_CENTER, 0);

    /* 控制按钮：上一首 / 播放暂停 / 下一首 */
    ui->screen_music_player_btn_prev = lv_btn_create(ui->screen_music_player);
    lv_obj_set_size(ui->screen_music_player_btn_prev, 50, 50);
    lv_obj_set_pos(ui->screen_music_player_btn_prev, 25, WAVE_TOP + WAVE_HEIGHT + 55);
    lv_obj_set_style_bg_color(ui->screen_music_player_btn_prev, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_radius(ui->screen_music_player_btn_prev, 25, 0);
    lv_obj_t *lbl_prev = lv_label_create(ui->screen_music_player_btn_prev);
    lv_label_set_text(lbl_prev, "|<");
    lv_obj_set_style_text_color(lbl_prev, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_prev, &songti_font_16, 0);
    lv_obj_center(lbl_prev);

    ui->screen_music_player_btn_pause = lv_btn_create(ui->screen_music_player);
    lv_obj_set_size(ui->screen_music_player_btn_pause, 70, 70);
    lv_obj_set_pos(ui->screen_music_player_btn_pause, 85, WAVE_TOP + WAVE_HEIGHT + 45);
    lv_obj_set_style_bg_color(ui->screen_music_player_btn_pause, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_radius(ui->screen_music_player_btn_pause, 35, 0);
    ui->screen_music_player_btn_pause_lbl = lv_label_create(ui->screen_music_player_btn_pause);
    lv_label_set_text(ui->screen_music_player_btn_pause_lbl, "||");
    lv_obj_set_style_text_color(ui->screen_music_player_btn_pause_lbl, lv_color_hex(0x0A0E1A), 0);
    lv_obj_set_style_text_font(ui->screen_music_player_btn_pause_lbl, &songti_font_16, 0);
    lv_obj_center(ui->screen_music_player_btn_pause_lbl);

    ui->screen_music_player_btn_next = lv_btn_create(ui->screen_music_player);
    lv_obj_set_size(ui->screen_music_player_btn_next, 50, 50);
    lv_obj_set_pos(ui->screen_music_player_btn_next, 165, WAVE_TOP + WAVE_HEIGHT + 55);
    lv_obj_set_style_bg_color(ui->screen_music_player_btn_next, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_radius(ui->screen_music_player_btn_next, 25, 0);
    lv_obj_t *lbl_next = lv_label_create(ui->screen_music_player_btn_next);
    lv_label_set_text(lbl_next, ">|");
    lv_obj_set_style_text_color(lbl_next, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_next, &songti_font_16, 0);
    lv_obj_center(lbl_next);

    /* 关闭按钮：停止播放并返回 */
    ui->screen_music_player_btn_close = lv_btn_create(ui->screen_music_player);
    lv_obj_set_size(ui->screen_music_player_btn_close, 35, 35);
    lv_obj_set_pos(ui->screen_music_player_btn_close, 200, 5);
    lv_obj_set_style_bg_color(ui->screen_music_player_btn_close, lv_color_hex(0x505050), 0);
    lv_obj_set_style_radius(ui->screen_music_player_btn_close, 17, 0);
    lv_obj_t *lbl_close = lv_label_create(ui->screen_music_player_btn_close);
    lv_label_set_text(lbl_close, "X");
    lv_obj_set_style_text_color(lbl_close, lv_color_white(), 0);
    lv_obj_center(lbl_close);

    lv_obj_update_layout(ui->screen_music_player);
    events_init_music_player(ui);
}

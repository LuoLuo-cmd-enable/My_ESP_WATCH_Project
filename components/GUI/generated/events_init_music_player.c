/*
 * events_init_music_player.c - 音乐播放界面事件
 * 波形动画（LVGL 定时器按电平跳柱）/ 进度轮询 / 暂停·切换 / seek / 右滑返回
 */
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include "gui_guider.h"
#include "lvgl_display.h"
#include "music_player.h"
#include "setup_scr_screen_music_player.h"

#define WAVE_REFRESH_MS  80    /* 波形刷新周期 */
#define PROGRESS_REFRESH_MS 500 /* 进度刷新周期 */

static lv_timer_t *s_wave_timer = NULL;
static lv_timer_t *s_progress_timer = NULL;

/* 删除定时器（退出播放屏时调用，防重进卡死）*/
static void music_player_timers_delete(void)
{
    if (s_wave_timer != NULL) {
        lv_timer_del(s_wave_timer);
        s_wave_timer = NULL;
    }
    if (s_progress_timer != NULL) {
        lv_timer_del(s_progress_timer);
        s_progress_timer = NULL;
    }
}

/* ---------- 波形：按播放状态 + 电平 + 随机扰动生成柱高 ---------- */
static void wave_update(void)
{
    lv_ui *ui = &guider_ui;
    music_state_t st = music_player_get_state();
    uint8_t lvl = music_player_get_level();

    for (int i = 0; i < MUSIC_WAVE_BARS && ui->screen_music_player_wave_bar[i] != NULL; i++) {
        int h;
        if (st == MUSIC_STATE_PLAYING) {
            /* 柱高 = 电平 ± 随机扰动，模拟音量跳动 */
            int seed = rand() % 30 - 15;
            int base = (int)lvl * 8 / 10 + 5;
            h = base + seed + ((i % 3) * 6);
            if (h < 3) h = 3;
            if (h > 85) h = 85;
        } else if (st == MUSIC_STATE_PAUSED) {
            h = 4;   /* 暂停：低矮静止 */
        } else {
            h = 2;   /* 停止 */
        }
        lv_obj_t *bar = ui->screen_music_player_wave_bar[i];
        lv_obj_set_height(bar, h);
        lv_obj_set_pos(bar, lv_obj_get_x(bar), 90 - h);   /* 底部对齐 */
    }
}

static void wave_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (guider_ui.screen_music_player == NULL ||
        !lv_obj_is_valid(guider_ui.screen_music_player)) {
        return;
    }
    wave_update();
}

/* ---------- 进度条 + 时间 ---------- */
static void progress_update(void)
{
    lv_ui *ui = &guider_ui;
    uint32_t pos = music_player_get_pos_ms();
    uint32_t total = music_player_get_total_ms();

    if (total > 0) {
        lv_slider_set_range(ui->screen_music_player_slider, 0, (int)total);
        lv_slider_set_value(ui->screen_music_player_slider, (int)pos, LV_ANIM_OFF);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lu:%02lu/%02lu:%02lu",
             (unsigned long)(pos / 60000), (unsigned long)((pos / 1000) % 60),
             (unsigned long)(total / 60000), (unsigned long)((total / 1000) % 60));
    lv_label_set_text(ui->screen_music_player_label_time, buf);
}

static void progress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (guider_ui.screen_music_player == NULL ||
        !lv_obj_is_valid(guider_ui.screen_music_player)) {
        return;
    }
    progress_update();
}

/* ---------- 控制按钮回调 ---------- */
static void btn_prev_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lvgl_msg_send(LVGL_MSG_MUSIC_OPEN_REQ, 1, NULL);   /* param=1 上一首 */
}

static void btn_pause_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    music_player_toggle_pause();
    /* 更新暂停按钮图标 */
    music_state_t st = music_player_get_state();
    lv_label_set_text(guider_ui.screen_music_player_btn_pause_lbl,
                      (st == MUSIC_STATE_PAUSED) ? "|>" : "||");
}

static void btn_next_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lvgl_msg_send(LVGL_MSG_MUSIC_OPEN_REQ, 2, NULL);   /* param=2 下一首 */
}

/* ---------- 进度条拖动：seek ---------- */
static void slider_value_changed_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *slider = lv_event_get_target(e);
    /* 仅用户拖动时 seek（避免定时器更新反触发）*/
    if (!lv_obj_has_state(slider, LV_STATE_PRESSED)) return;
    music_player_seek((uint32_t)lv_slider_get_value(slider));
}

/* ---------- 关闭按钮：停止播放并返回菜单 ---------- */
static void btn_close_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    music_player_stop();
    music_player_timers_delete();
    ui_load_scr_animation(&guider_ui,
        &guider_ui.menu_screen, guider_ui.menu_screen_del,
        &guider_ui.screen_music_player_del, setup_scr_menu_screen,
        LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

/* ---------- 右滑返回主菜单 ---------- */
static void music_player_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
            music_player_stop();
            music_player_timers_delete();
            ui_load_scr_animation(&guider_ui,
                &guider_ui.menu_screen, guider_ui.menu_screen_del,
                &guider_ui.screen_music_player_del, setup_scr_menu_screen,
                LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        }
    }
}

void events_init_music_player(lv_ui *ui)
{
    /* 重建前先清旧定时器，防重进卡死 */
    music_player_timers_delete();

    s_wave_timer = lv_timer_create(wave_timer_cb, WAVE_REFRESH_MS, NULL);
    s_progress_timer = lv_timer_create(progress_timer_cb, PROGRESS_REFRESH_MS, NULL);

    /* 初始化显示 */
    lv_label_set_text(ui->screen_music_player_label_name, music_player_get_name());
    lv_label_set_text(ui->screen_music_player_btn_pause_lbl, "||");

    lv_obj_add_event_cb(ui->screen_music_player, music_player_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_music_player_btn_prev, btn_prev_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_music_player_btn_pause, btn_pause_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_music_player_btn_next, btn_next_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_music_player_btn_close, btn_close_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui->screen_music_player_slider, slider_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/* 供外部（如切歌/退出）清理定时器 */
void music_player_ui_cleanup(void)
{
    music_player_timers_delete();
}

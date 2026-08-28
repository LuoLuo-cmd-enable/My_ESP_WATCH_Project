/*
 * setup_scr_screen_ai_chat.c
 * DeepSeek AI 聊天界面（generated-style screen）
 *
 * 布局（flex 纵向）：
 *   [标题栏: 返回按钮 + 标题 + 状态]
 *   [快捷问题行]
 *   [回答滚动区 (flex-grow=1)]
 *   [输入行: textarea + 发送按钮]
 *   [软键盘 (初始隐藏)]
 */

#include "lvgl.h"
#include "gui_guider.h"
#include "ai_chat_events.h"
#include "font_sd.h"

void setup_scr_screen_ai_chat(lv_ui *ui)
{
    /* -------- 屏幕根 -------- */
    ui->screen_ai_chat = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_ai_chat, 240, 284);
    lv_obj_set_scrollbar_mode(ui->screen_ai_chat, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui->screen_ai_chat, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_bg_opa(ui->screen_ai_chat, 255, 0);

    /* flex 纵向布局 */
    lv_obj_set_flex_flow(ui->screen_ai_chat, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_top(ui->screen_ai_chat, 6, 0);
    lv_obj_set_style_pad_bottom(ui->screen_ai_chat, 4, 0);
    lv_obj_set_style_pad_left(ui->screen_ai_chat, 6, 0);
    lv_obj_set_style_pad_right(ui->screen_ai_chat, 6, 0);
    lv_obj_set_style_pad_row(ui->screen_ai_chat, 4, 0);

    /* -------- 标题栏容器（横向） -------- */
    lv_obj_t *cont_header = lv_obj_create(ui->screen_ai_chat);
    lv_obj_set_size(cont_header, 228, 34);
    lv_obj_set_flex_flow(cont_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_header, 6, 0);
    lv_obj_set_style_bg_opa(cont_header, 0, 0);
    lv_obj_set_style_border_width(cont_header, 0, 0);
    lv_obj_set_style_pad_left(cont_header, 0, 0);
    lv_obj_set_style_pad_right(cont_header, 0, 0);
    lv_obj_set_style_pad_top(cont_header, 0, 0);
    lv_obj_set_style_pad_bottom(cont_header, 0, 0);

    /* 返回按钮 */
    ui->screen_ai_chat_btn_back = lv_btn_create(cont_header);
    lv_obj_set_size(ui->screen_ai_chat_btn_back, 40, 30);
    lv_obj_set_style_bg_color(ui->screen_ai_chat_btn_back, lv_color_hex(0x1E2A44), 0);
    lv_obj_set_style_radius(ui->screen_ai_chat_btn_back, 6, 0);
    lv_obj_set_style_shadow_width(ui->screen_ai_chat_btn_back, 0, 0);
    lv_obj_t *back_lbl = lv_label_create(ui->screen_ai_chat_btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_lbl);

    /* 标题 */
    lv_obj_t *title = lv_label_create(cont_header);
    lv_label_set_text(title, "DeepSeek AI");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &songti_font_16, 0);

    /* 状态标签（flex 剩余空间右对齐） */
    ui->screen_ai_chat_label_status = lv_label_create(cont_header);
    lv_obj_set_flex_grow(ui->screen_ai_chat_label_status, 1);
    lv_label_set_text(ui->screen_ai_chat_label_status, "Idle");
    lv_obj_set_style_text_color(ui->screen_ai_chat_label_status, lv_color_hex(0x9A9A9A), 0);
    lv_obj_set_style_text_font(ui->screen_ai_chat_label_status, &songti_font_16, 0);
    lv_obj_set_style_text_align(ui->screen_ai_chat_label_status, LV_TEXT_ALIGN_RIGHT, 0);

    /* -------- 快捷问题行 -------- */
    ui->screen_ai_chat_cont_quick = lv_obj_create(ui->screen_ai_chat);
    lv_obj_set_size(ui->screen_ai_chat_cont_quick, 228, 32);
    lv_obj_set_flex_flow(ui->screen_ai_chat_cont_quick, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui->screen_ai_chat_cont_quick, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ui->screen_ai_chat_cont_quick, 6, 0);
    lv_obj_set_style_bg_opa(ui->screen_ai_chat_cont_quick, 0, 0);
    lv_obj_set_style_border_width(ui->screen_ai_chat_cont_quick, 0, 0);
    lv_obj_set_style_pad_left(ui->screen_ai_chat_cont_quick, 0, 0);
    lv_obj_set_style_pad_right(ui->screen_ai_chat_cont_quick, 0, 0);
    lv_obj_set_style_pad_top(ui->screen_ai_chat_cont_quick, 0, 0);
    lv_obj_set_style_pad_bottom(ui->screen_ai_chat_cont_quick, 0, 0);

    static const char *const quick_txt[] = { "讲笑话", "健康建议", "ESP32简介" };
    for (size_t i = 0; i < sizeof(quick_txt) / sizeof(quick_txt[0]); i++) {
        lv_obj_t *btn = lv_btn_create(ui->screen_ai_chat_cont_quick);
        lv_obj_set_height(btn, 30);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x25405F), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2E46), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_left(btn, 0, 0);
        lv_obj_set_style_pad_right(btn, 0, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, quick_txt[i]);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xDCE8F5), 0);
        /* 快捷问题含动态中文，使用 SD 卡动态字库（未加载时回退内置字体） */
        lv_obj_set_style_text_font(lbl, font_sd_get(), 0);
        lv_obj_center(lbl);
    }

    /* -------- 回答滚动区（flex-grow=1） -------- */
    lv_obj_t *cont_answer = lv_obj_create(ui->screen_ai_chat);
    lv_obj_set_flex_grow(cont_answer, 1);
    lv_obj_set_width(cont_answer, 228);
    lv_obj_set_style_bg_color(cont_answer, lv_color_hex(0x101A30), 0);
    lv_obj_set_style_bg_opa(cont_answer, 255, 0);
    lv_obj_set_style_radius(cont_answer, 8, 0);
    lv_obj_set_style_border_width(cont_answer, 0, 0);
    lv_obj_set_style_pad_all(cont_answer, 8, 0);
    lv_obj_set_scrollbar_mode(cont_answer, LV_SCROLLBAR_MODE_AUTO);

    ui->screen_ai_chat_label_answer = lv_label_create(cont_answer);
    lv_label_set_text(ui->screen_ai_chat_label_answer,
                      "Hi! Ask me anything.\n\n"
                      "Tip: tap input box to show keyboard,\n"
                      "or tap a quick question above.");
    lv_obj_set_width(ui->screen_ai_chat_label_answer, 210);
    lv_label_set_long_mode(ui->screen_ai_chat_label_answer, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(ui->screen_ai_chat_label_answer, lv_color_hex(0xC8CDD6), 0);
    /* AI 回答是任意中文文本，必须使用 SD 卡动态字库（0x4E00-0x9FA5 全覆盖） */
    lv_obj_set_style_text_font(ui->screen_ai_chat_label_answer, font_sd_get(), 0);
    lv_obj_set_style_text_line_space(ui->screen_ai_chat_label_answer, 4, 0);

    /* -------- 输入行 -------- */
    lv_obj_t *cont_input = lv_obj_create(ui->screen_ai_chat);
    lv_obj_set_size(cont_input, 228, 42);
    lv_obj_set_flex_flow(cont_input, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_input, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_input, 6, 0);
    lv_obj_set_style_bg_opa(cont_input, 0, 0);
    lv_obj_set_style_border_width(cont_input, 0, 0);
    lv_obj_set_style_pad_left(cont_input, 0, 0);
    lv_obj_set_style_pad_right(cont_input, 0, 0);
    lv_obj_set_style_pad_top(cont_input, 0, 0);
    lv_obj_set_style_pad_bottom(cont_input, 0, 0);

    ui->screen_ai_chat_ta_input = lv_textarea_create(cont_input);
    lv_obj_set_flex_grow(ui->screen_ai_chat_ta_input, 1);
    lv_obj_set_height(ui->screen_ai_chat_ta_input, 38);
    lv_textarea_set_one_line(ui->screen_ai_chat_ta_input, true);
    lv_textarea_set_placeholder_text(ui->screen_ai_chat_ta_input, "Ask DeepSeek...");
    lv_textarea_set_max_length(ui->screen_ai_chat_ta_input, 200);
    /* 输入内容可能含中文，同样使用 SD 字库 */
    lv_obj_set_style_text_font(ui->screen_ai_chat_ta_input, font_sd_get(), 0);
    lv_obj_set_style_radius(ui->screen_ai_chat_ta_input, 6, 0);
    lv_obj_set_style_bg_color(ui->screen_ai_chat_ta_input, lv_color_hex(0x1E2A44), 0);

    /* 隐藏光标（触摸屏无键盘导航） */
    lv_obj_set_style_width(ui->screen_ai_chat_ta_input, 0, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(ui->screen_ai_chat_ta_input, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_anim_time(ui->screen_ai_chat_ta_input, 0, LV_PART_CURSOR);

    ui->screen_ai_chat_btn_send = lv_btn_create(cont_input);
    lv_obj_set_size(ui->screen_ai_chat_btn_send, 64, 38);
    lv_obj_set_style_bg_color(ui->screen_ai_chat_btn_send, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_bg_color(ui->screen_ai_chat_btn_send, lv_color_hex(0x1565C0),
                              LV_STATE_PRESSED);
    lv_obj_set_style_radius(ui->screen_ai_chat_btn_send, 6, 0);
    lv_obj_set_style_shadow_width(ui->screen_ai_chat_btn_send, 0, 0);
    lv_obj_t *send_lbl = lv_label_create(ui->screen_ai_chat_btn_send);
    lv_label_set_text(send_lbl, "Send");
    lv_obj_set_style_text_color(send_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(send_lbl, &songti_font_16, 0);
    lv_obj_center(send_lbl);

    /* -------- 软键盘（初始隐藏，flex 布局中不占空间） -------- */
    ui->screen_ai_chat_kb = lv_keyboard_create(ui->screen_ai_chat);
    lv_obj_set_size(ui->screen_ai_chat_kb, 240, 150);
    lv_obj_set_style_pad_all(ui->screen_ai_chat_kb, 3, 0);
    lv_obj_add_flag(ui->screen_ai_chat_kb, LV_OBJ_FLAG_HIDDEN);

    /* 三套自定义键盘布局（与 WiFi 设置屏一致） */
    lv_keyboard_set_map(ui->screen_ai_chat_kb, LV_KEYBOARD_MODE_TEXT_LOWER,
                        ai_kb_map_lower, ai_kb_ctrl_lower);
    lv_keyboard_set_map(ui->screen_ai_chat_kb, LV_KEYBOARD_MODE_TEXT_UPPER,
                        ai_kb_map_upper, ai_kb_ctrl_upper);
    lv_keyboard_set_map(ui->screen_ai_chat_kb, LV_KEYBOARD_MODE_SPECIAL,
                        ai_kb_map_special, ai_kb_ctrl_special);
    lv_keyboard_set_textarea(ui->screen_ai_chat_kb, ui->screen_ai_chat_ta_input);

    /* -------- 事件绑定 -------- */
    events_init_screen_ai_chat(ui);

    ui->screen_ai_chat_del = false;
}

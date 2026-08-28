/*
 * ai_chat_events.c
 * DeepSeek AI 聊天屏幕：事件绑定 + UI 更新函数
 *
 * 消息流：
 *   deepseek_ai 任务 --LVGL_MSG_AI_STATUS--> ai_ui_set_status
 *   deepseek_ai 任务 --LVGL_MSG_AI_ANSWER--> ai_ui_show_answer
 */

#include "ai_chat_events.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "deepseek_ai.h"
#include "ui_transition.h"

#define TAG "ai_chat"

/* ==================== 键盘布局（与 wifi_set 屏幕一致） ==================== */

const char *ai_kb_map_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "1#", " ", LV_SYMBOL_CLOSE, LV_SYMBOL_OK, ""
};
const lv_btnmatrix_ctrl_t ai_kb_ctrl_lower[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 2,
    4, 2, 2, 3
};

const char *ai_kb_map_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "abc", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "1#", " ", LV_SYMBOL_CLOSE, LV_SYMBOL_OK, ""
};
const lv_btnmatrix_ctrl_t ai_kb_ctrl_upper[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 2,
    4, 2, 2, 3
};

const char *ai_kb_map_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "+", "-", "*", "/", "=", "%", "@", "#", "!", "\n",
    "_", "<", ">", "[", "]", "(", ")", LV_SYMBOL_BACKSPACE, "\n",
    "abc", " ", LV_SYMBOL_CLOSE, LV_SYMBOL_OK, ""
};
const lv_btnmatrix_ctrl_t ai_kb_ctrl_special[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2,
    4, 2, 2, 3
};

/* ==================== 辅助 ==================== */

static void ai_kb_show(lv_ui *ui)
{
    if (ui->screen_ai_chat_kb == NULL) return;
    if (lv_obj_has_flag(ui->screen_ai_chat_kb, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(ui->screen_ai_chat_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ai_kb_hide(lv_ui *ui)
{
    if (ui->screen_ai_chat_kb == NULL) return;
    if (!lv_obj_has_flag(ui->screen_ai_chat_kb, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(ui->screen_ai_chat_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ==================== 对外 UI 更新 ==================== */

void ai_ui_set_status(const char *text, uint32_t color_hex)
{
    lv_ui *ui = &guider_ui;
    if (ui->screen_ai_chat_label_status == NULL ||
        !lv_obj_is_valid(ui->screen_ai_chat_label_status)) {
        return;
    }
    lv_label_set_text(ui->screen_ai_chat_label_status, (text != NULL) ? text : "");
    lv_obj_set_style_text_color(ui->screen_ai_chat_label_status,
                                lv_color_hex(color_hex), 0);
}

void ai_ui_show_answer(const char *text)
{
    lv_ui *ui = &guider_ui;
    if (ui->screen_ai_chat_label_answer == NULL ||
        !lv_obj_is_valid(ui->screen_ai_chat_label_answer)) {
        return;
    }
    if (text == NULL) return;

    /*
     * 先清空再设置：lv_label_set_text 内部用 lv_mem_realloc 扩展旧文本块。
     * 旧文本（占位符）很小在内部 RAM，新回答 1~2KB 需 PSRAM，跨区 realloc
     * 触发 heap_caps_realloc 的边界 bug → CORRUPT HEAP。
     * 先置空释放旧块，再分配新块（全新 PSRAM 分配，无跨区 realloc）。
     * 不拼接 "AI: " 前缀：避免大栈数组，回答直接显示。
     */
    lv_label_set_text(ui->screen_ai_chat_label_answer, "");
    lv_label_set_text(ui->screen_ai_chat_label_answer, text);
    lv_obj_update_layout(ui->screen_ai_chat_label_answer);
    lv_obj_scroll_to_y(lv_obj_get_parent(ui->screen_ai_chat_label_answer), 0, LV_ANIM_OFF);
}

/* ==================== 发送逻辑 ==================== */

void ai_ui_send_from_input(void)
{
    lv_ui *ui = &guider_ui;
    if (ui->screen_ai_chat_ta_input == NULL ||
        !lv_obj_is_valid(ui->screen_ai_chat_ta_input)) {
        return;
    }

    const char *text = lv_textarea_get_text(ui->screen_ai_chat_ta_input);
    if (text == NULL || text[0] == '\0') return;

    if (deepseek_ai_busy()) {
        ai_ui_set_status("Busy, wait...", 0xF0B429);
        return;
    }

    /* 先回显问题（text 指向 textarea 内部缓冲） */
    char qbuf[256];
    snprintf(qbuf, sizeof(qbuf), "You: %s\n\nWaiting...", text);

    esp_err_t err = deepseek_ai_ask(text);
    if (err != ESP_OK) {
        ai_ui_set_status("Send failed", 0xFF3333);
        return;
    }

    /* 发送成功：清空输入框、隐藏键盘、显示回显 */
    lv_textarea_set_text(ui->screen_ai_chat_ta_input, "");
    ai_kb_hide(ui);
    lv_label_set_text(ui->screen_ai_chat_label_answer, qbuf);
    lv_obj_update_layout(ui->screen_ai_chat_label_answer);
}

/* ==================== 事件回调 ==================== */

static void ai_btn_back_cb(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    ai_kb_hide(ui);
    if (lv_scr_act() == ui->screen_ai_chat) {
        ui_load_scr_with_zoom(ui, &ui->menu_screen, ui->menu_screen_del,
                              &ui->screen_ai_chat_del, setup_scr_menu_screen,
                              LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    }
}

static void ai_btn_send_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_send_from_input();
}

static void ai_quick_btn_cb(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);

    /* 从按钮子标签取问题文本 */
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == NULL || ui->screen_ai_chat_ta_input == NULL) return;

    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl == NULL) return;
    const char *text = lv_label_get_text(lbl);
    if (text == NULL || text[0] == '\0') return;

    if (deepseek_ai_busy()) {
        ai_ui_set_status("Busy, wait...", 0xF0B429);
        return;
    }
    esp_err_t err = deepseek_ai_ask(text);
    if (err != ESP_OK) {
        ai_ui_set_status("Send failed", 0xFF3333);
        return;
    }
    ai_kb_hide(ui);

    char qbuf[256];
    snprintf(qbuf, sizeof(qbuf), "You: %s\n\nWaiting...", text);
    lv_label_set_text(ui->screen_ai_chat_label_answer, qbuf);
    lv_obj_update_layout(ui->screen_ai_chat_label_answer);
}

static void ai_kb_event_cb(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) {       /* OK 键 */
        ai_ui_send_from_input();
    } else if (code == LV_EVENT_CANCEL) { /* 取消/关闭 */
        ai_kb_hide(ui);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        /* 键盘内建的模式切换（ABC/1#）由 LVGL 自动处理 */
    }
}

static void ai_textarea_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        lv_ui *ui = &guider_ui;
        ai_kb_show(ui);
    }
}

/* ==================== 事件绑定入口 ==================== */

void events_init_screen_ai_chat(lv_ui *ui)
{
    if (ui->screen_ai_chat_btn_back != NULL) {
        lv_obj_add_event_cb(ui->screen_ai_chat_btn_back, ai_btn_back_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->screen_ai_chat_btn_send != NULL) {
        lv_obj_add_event_cb(ui->screen_ai_chat_btn_send, ai_btn_send_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->screen_ai_chat_ta_input != NULL) {
        lv_obj_add_event_cb(ui->screen_ai_chat_ta_input, ai_textarea_cb,
                            LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui->screen_ai_chat_ta_input, ai_textarea_cb,
                            LV_EVENT_FOCUSED, NULL);
    }
    if (ui->screen_ai_chat_kb != NULL) {
        lv_obj_add_event_cb(ui->screen_ai_chat_kb, ai_kb_event_cb, LV_EVENT_ALL, ui);
    }

    /* 快捷问题按钮：从容器子对象查找并绑定 */
    if (ui->screen_ai_chat_cont_quick != NULL) {
        uint32_t n = lv_obj_get_child_cnt(ui->screen_ai_chat_cont_quick);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *btn = lv_obj_get_child(ui->screen_ai_chat_cont_quick, i);
            lv_obj_add_event_cb(btn, ai_quick_btn_cb, LV_EVENT_CLICKED, ui);
        }
    }
}

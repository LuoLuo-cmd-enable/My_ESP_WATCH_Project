/*
 * ai_chat_events.h
 * DeepSeek AI 聊天屏幕：事件绑定 + UI 更新函数
 * （custom 扩展模块，避免改动 generated/events_init.c）
 */

#ifndef AI_CHAT_EVENTS_H
#define AI_CHAT_EVENTS_H

#include "lvgl.h"
#include "gui_guider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 绑定 AI 聊天屏幕的所有事件（由 setup_scr_screen_ai_chat 调用） */
void events_init_screen_ai_chat(lv_ui *ui);

/* AI 回答最大显示长度（含 "AI: " 前缀，栈上缓冲） */
#define AI_ANSWER_DISPLAY_MAX   2048

/* AI 键盘三套布局（供 setup 屏幕引用） */
extern const char *ai_kb_map_lower[];
extern const char *ai_kb_map_upper[];
extern const char *ai_kb_map_special[];
extern const lv_btnmatrix_ctrl_t ai_kb_ctrl_lower[];
extern const lv_btnmatrix_ctrl_t ai_kb_ctrl_upper[];
extern const lv_btnmatrix_ctrl_t ai_kb_ctrl_special[];

/** 更新状态标签（由 LVGL_MSG_AI_STATUS 消息驱动） */
void ai_ui_set_status(const char *text, uint32_t color_hex);

/** 显示 AI 回答（由 LVGL_MSG_AI_ANSWER 消息驱动） */
void ai_ui_show_answer(const char *text);

/** 手动触发一次发送（从输入框取文本） */
void ai_ui_send_from_input(void);

#ifdef __cplusplus
}
#endif

#endif /* AI_CHAT_EVENTS_H */

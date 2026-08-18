/*
* Copyright 2023 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __CUSTOM_H_
#define __CUSTOM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

void custom_init(lv_ui *ui);
void create_swipeable_menu(lv_ui *ui);

/* 创建纵向渐变卡片按钮（列表项通用），icon_src 可为 NULL（纯文字）
 * idx 决定渐变色，循环复用 8 组颜色 */
lv_obj_t *ui_gradient_btn_create(lv_obj_t *parent, const void *icon_src,
                                 const char *text, uint32_t idx);
/* 取渐变按钮上的文本（点击事件取文件名用） */
const char *ui_gradient_btn_get_text(lv_obj_t *btn);

/* 电池状态应用层：时钟屏/滑动菜单屏挂载电池图标，30s 定时刷新 */
void battery_status_init(void);
void battery_status_attach_clock(void);
void battery_status_attach_menu(void);


#ifdef __cplusplus
}
#endif
#endif /* EVENT_CB_H_ */

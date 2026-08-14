/*
 * 缩放切屏过渡动画（路线A：截图 + PSRAM + Canvas）
 * 与 ui_load_scr_animation 同签名，带"旧屏缩小→切屏→新屏放大"效果
 */
#ifndef GUI_GENERATED_UI_TRANSITION_H
#define GUI_GENERATED_UI_TRANSITION_H

#include "lvgl.h"
#include "gui_guider.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 设置缩放动画中心点（点击位置），默认屏幕中心 */
void ui_transition_set_center(lv_coord_t x, lv_coord_t y);

/* 查询切屏动画是否进行中（防重入/防误触） */
bool ui_transition_is_busy(void);

/* 与 ui_load_scr_animation 相同签名，带缩放过渡动画 */
void ui_load_scr_with_zoom(lv_ui *ui, lv_obj_t **new_scr, bool new_scr_del, bool *old_scr_del,
                           ui_setup_scr_t setup_scr, lv_scr_load_anim_t anim_type,
                           uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

#ifdef __cplusplus
}
#endif

#endif /* GUI_GENERATED_UI_TRANSITION_H */

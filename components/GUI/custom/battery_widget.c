/*
 * battery_widget.c - 电池电量 UI 组件（LVGL）
 *
 * 组件结构：
 *   [tip] 正极凸起
 *   [=================]  外框（cont，白色描边）
 *   [||||||||||||----]  填充条（fill，按百分比变色：红/黄/绿）
 *   "85%"              百分比文字（label）
 *
 * 数据来源：battery_management.h（数据层），本文件只做 UI 呈现
 */
#include "battery_widget.h"
#include "battery_management.h"
#include "custom.h"

#define BATTERY_FILL_PAD 2  /* 填充条内边距 */
#define BATTERY_TIP_W 4     /* 正极凸起宽 */
#define BATTERY_TIP_H 8     /* 正极凸起高 */

/* 电量颜色阈值（高饱和鲜艳色，黑底上醒目） */
#define BATTERY_COLOR_CRITICAL  20   /* ≤20% 亮红 */
#define BATTERY_COLOR_LOW       50   /* ≤50% 亮黄 */
#define BATTERY_COLOR_GOOD      0x00E676  /* >50% 亮绿（荧光绿） */

battery_widget_t *battery_widget_create(lv_obj_t *parent, int x, int y)
{
    if (parent == NULL) return NULL;

    battery_widget_t *w = (battery_widget_t *)lv_mem_alloc(sizeof(battery_widget_t));
    if (w == NULL) return NULL;
    memset(w, 0, sizeof(battery_widget_t));
    w->parent = parent;

    /* --- 电池外壳（圆角矩形 + 白色粗描边）--- */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, BATTERY_W, BATTERY_H);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_radius(cont, 6, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);   /* 外壳透明，只留边框 */
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);      /* 点按可看详情（预留）*/

    /* --- 正极凸起（右侧小矩形）--- */
    lv_obj_t *tip = lv_obj_create(parent);
    lv_obj_set_size(tip, BATTERY_TIP_W, BATTERY_TIP_H);
    lv_obj_set_pos(tip, x + BATTERY_W, y + (BATTERY_H - BATTERY_TIP_H) / 2);
    lv_obj_set_style_bg_color(tip, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tip, 2, 0);
    lv_obj_set_style_border_width(tip, 0, 0);
    lv_obj_clear_flag(tip, LV_OBJ_FLAG_SCROLLABLE);

    /* --- 电量填充条（放在外壳内，粗描边取消）--- */
    lv_obj_t *fill = lv_obj_create(cont);
    lv_obj_set_pos(fill, BATTERY_FILL_PAD, BATTERY_FILL_PAD);
    lv_obj_set_size(fill, BATTERY_W - 2 * BATTERY_FILL_PAD, BATTERY_H - 2 * BATTERY_FILL_PAD);
    lv_obj_set_style_radius(fill, 3, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);

    /* --- 百分比文字（黑色，深色背景不可见但按需求；时钟屏背景浅色）--- */
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "85%");
    lv_obj_set_pos(label, x - 8, y + BATTERY_H + 2);
    lv_obj_set_width(label, BATTERY_W + 16);
    lv_obj_set_style_text_font(label, &songti_font_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    w->cont = cont;
    w->tip = tip;
    w->fill = fill;
    w->label = label;

    battery_widget_refresh(w);
    return w;
}

void battery_widget_refresh(battery_widget_t *w)
{
    if (w == NULL) return;

    uint8_t pct = battery_get_percent();
    if (pct > 100) pct = 100;

    /* 更新填充条宽度 */
    int fill_w = (int)((BATTERY_W - 2 * BATTERY_FILL_PAD) * pct / 100);
    if (fill_w < 2 && pct > 0) fill_w = 2;   /* 最小可见 */
    lv_obj_set_width(w->fill, fill_w);

    /* 按电量变色 */
    uint32_t color = BATTERY_COLOR_GOOD;
    if (pct <= BATTERY_COLOR_CRITICAL) {
        color = 0xF44336;                    /* 红 */
    } else if (pct <= BATTERY_COLOR_LOW) {
        color = 0xFFC107;                    /* 黄 */
    }
    lv_obj_set_style_bg_color(w->fill, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(w->fill, LV_OPA_COVER, 0);

    /* 更新文字 */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(w->label, buf);
}

void battery_widget_destroy(battery_widget_t *w)
{
    if (w == NULL) return;
    lv_obj_del(w->label);
    lv_obj_del(w->cont);   /* fill 是 cont 的子对象，随 cont 删除 */
    lv_mem_free(w);
}

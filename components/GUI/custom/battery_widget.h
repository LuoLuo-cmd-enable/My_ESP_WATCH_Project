/*
 * battery_widget.h - 电池电量 UI 组件（LVGL）
 *
 * 分层：数据来自 battery_management（数据层），本组件只负责绘制/刷新
 * 可复用到时钟屏、滑动菜单屏等任意父容器
 */
#ifndef _BATTERY_WIDGET_H_
#define _BATTERY_WIDGET_H_

#include "lvgl.h"

/* 组件尺寸（custom.c 挂载定位时引用） */
#define BATTERY_W 44   /* 电池外框宽 */
#define BATTERY_H 20   /* 电池外框高 */

typedef struct {
    lv_obj_t *cont;    /* 电池外壳（含边框）*/
    lv_obj_t *tip;     /* 正极小凸起 */
    lv_obj_t *fill;    /* 内部电量填充条 */
    lv_obj_t *label;   /* 百分比文字 */
    lv_obj_t *parent;  /* 记录父容器 */
} battery_widget_t;

/**
 * @brief 创建电池图标组件（外壳+填充+正极+百分比文字）
 * @param parent 父容器（时钟屏/菜单屏）
 * @param x, y   位置
 * @return 组件句柄（用于刷新/销毁）
 */
battery_widget_t *battery_widget_create(lv_obj_t *parent, int x, int y);

/**
 * @brief 刷新电量显示（从数据层读取最新电量并更新图标+文字）
 * @param widget 组件句柄
 */
void battery_widget_refresh(battery_widget_t *widget);

/**
 * @brief 销毁组件
 */
void battery_widget_destroy(battery_widget_t *widget);

#endif /* _BATTERY_WIDGET_H_ */

#include <stddef.h>
#include <stdio.h>

#include "custom.h"
#include "lvgl.h"
#include "ui_transition.h"

extern lv_ui guider_ui;

/* 点击菜单项时，将缩放动画中心设置为触摸点 */
static void set_click_center(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) indev = lv_indev_get_act();
    if (indev != NULL) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        ui_transition_set_center(p.x, p.y);
    }
}

static void custom_btn_novel_cb(lv_event_t *e)
{
    set_click_center(e);
    ui_load_scr_with_zoom(&guider_ui, &guider_ui.novel_list, guider_ui.novel_list_del,
                          &guider_ui.menu_screen_del, setup_scr_novel_list,
                          LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

static void custom_btn_img_cb(lv_event_t *e)
{
    set_click_center(e);
    ui_load_scr_with_zoom(&guider_ui, &guider_ui.screen_img_list, guider_ui.screen_img_list_del,
                          &guider_ui.menu_screen_del, setup_scr_screen_img_list,
                          LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

static void custom_btn_video_cb(lv_event_t *e)
{
    set_click_center(e);
    ui_load_scr_with_zoom(&guider_ui, &guider_ui.video_list, guider_ui.video_list_del,
                          &guider_ui.menu_screen_del, setup_scr_video_list,
                          LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

static void custom_btn_setting_cb(lv_event_t *e)
{
    set_click_center(e);
    if (lv_scr_act() != guider_ui.menu_screen) {
        return;
    }
    ui_load_scr_with_zoom(&guider_ui, &guider_ui.setting_screen, guider_ui.setting_screen_del,
                          &guider_ui.menu_screen_del, setup_scr_setting_screen,
                          LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

static void custom_btn_game_cb(lv_event_t *e)
{
    set_click_center(e);
    ui_load_scr_with_zoom(&guider_ui, &guider_ui.screen_game, guider_ui.screen_game_del,
                          &guider_ui.menu_screen_del, setup_scr_screen_game,
                          LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
}

extern const lv_img_dsc_t novel;
extern const lv_img_dsc_t picture;
extern const lv_img_dsc_t video;
extern const lv_img_dsc_t config;
extern const lv_img_dsc_t game;

#define MENU_ITEM_COUNT 5
static lv_obj_t *s_menu_imgs[MENU_ITEM_COUNT];

static void menu_scroll_event_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);

    if (lv_area_get_width(&cont_a) == 0) {
        return;
    }

    lv_coord_t cont_x_center = cont_a.x1 + lv_area_get_width(&cont_a) / 2;

    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);
        lv_coord_t child_x_center = child_a.x1 + lv_area_get_width(&child_a) / 2;
        lv_coord_t diff_x = LV_ABS(child_x_center - cont_x_center);

        uint16_t zoom = 300 - (diff_x * 100) / (lv_area_get_width(&cont_a) / 2);
        if (zoom < 200) zoom = 200;
        if (zoom > 300) zoom = 300;
        if (i < MENU_ITEM_COUNT && s_menu_imgs[i] != NULL) {
            lv_img_set_zoom(s_menu_imgs[i], zoom);
        }

        lv_opa_t opa = 255 - (diff_x * 135) / (lv_area_get_width(&cont_a) / 2);
        if (opa < 100) opa = 100;
        /* 只对图片/文字设置不透明度（绘制时直接混合，不触发 layer）：
           若对整个容器设 opa 会触发 layer 渲染，draw buffer 不足时显示为白框 */
        if (i < MENU_ITEM_COUNT && s_menu_imgs[i] != NULL) {
            lv_obj_set_style_img_opa(s_menu_imgs[i], opa, 0);
            lv_obj_t *label = lv_obj_get_child(child, 1);
            if (label != NULL) {
                lv_obj_set_style_text_opa(label, opa, 0);
            }
        }
    }
}

static lv_obj_t *create_menu_item(lv_obj_t *parent, const void *img_src, const char *text, int idx)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 100, 120);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(cont, LV_DIR_NONE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *img = lv_img_create(cont);
    lv_img_set_src(img, img_src);
    if (idx < MENU_ITEM_COUNT) {
        s_menu_imgs[idx] = img;
    }

    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &songti_font_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE8E8E8), 0);

    return cont;
}

void create_swipeable_menu(lv_ui *ui)
{

    lv_obj_t *cont = lv_obj_create(ui->menu_screen);
    lv_obj_set_size(cont, 240, 170);
    lv_obj_center(cont);

    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(cont, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(cont, LV_SCROLL_SNAP_CENTER);

    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_left(cont, 80, 0);
    lv_obj_set_style_pad_right(cont, 80, 0);
    lv_obj_set_style_pad_column(cont, 40, 0);

    struct menu_item_cfg {
        const void *img_src;
        const char *text;
        void (*cb)(lv_event_t *);
    };

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        s_menu_imgs[i] = NULL;
    }

    static struct menu_item_cfg items[] = {
        {&novel, "小说", custom_btn_novel_cb},
        {&picture, "图片", custom_btn_img_cb},
        {&video, "视频", custom_btn_video_cb},
        {&config, "设置", custom_btn_setting_cb},
        {&game, "游戏", custom_btn_game_cb},
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        lv_obj_t *item = create_menu_item(cont, items[i].img_src, items[i].text, (int)i);
        lv_obj_add_event_cb(item, items[i].cb, LV_EVENT_CLICKED, ui);
    }

    ui->menu_screen_list_1 = cont;

    lv_obj_add_event_cb(cont, menu_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_update_layout(cont);
    lv_event_send(cont, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(cont, 0), LV_ANIM_OFF);
}

/* ==================================================================
 * 通用渐变卡片按钮（列表项）
 * 小说 / 图片 / 视频 / 本地OTA 列表共用
 * ================================================================== */
lv_obj_t *ui_gradient_btn_create(lv_obj_t *parent, const void *icon_src,
                                 const char *text, uint32_t idx)
{
    /* 每个按钮的渐变色对（顶色 → 底色），循环复用 */
    static const uint32_t grad_top[] = {
        0x4F9DF0, 0x9B6BF2, 0x37C6C0, 0x52C47C,
        0xF0A43A, 0xEC6E5A, 0xE85FA0, 0x5FA8E8,
    };
    static const uint32_t grad_bottom[] = {
        0x2F6FD0, 0x6E42C0, 0x1E9A96, 0x2E9A56,
        0xD07E1E, 0xC04A3A, 0xC03A7A, 0x3A7AC0,
    };
    static const uint8_t grad_cnt = sizeof(grad_top) / sizeof(grad_top[0]);
    uint8_t g_idx = idx % grad_cnt;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 224, 44);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(card, 14, 0);
    lv_obj_set_style_pad_right(card, 14, 0);
    lv_obj_set_style_pad_column(card, 10, 0);

    /* 渐变底衬 + 圆角（去掉阴影：软渲染开销极大，是滑动卡顿主因） */
    lv_obj_set_style_bg_color(card, lv_color_hex(grad_top[g_idx]), 0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(grad_bottom[g_idx]), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_shadow_opa(card, 0, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(card, LV_DIR_NONE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(grad_bottom[g_idx]), LV_PART_MAIN | LV_STATE_PRESSED);

    /* 可选图标：放大到与文字协调 */
    if (icon_src != NULL) {
        lv_obj_t *img = lv_img_create(card);
        lv_img_set_src(img, icon_src);
        lv_img_set_zoom(img, 65);   /* 80px → 32px */
    }

    /* 文本：过长省略号（循环滚动会持续触发重绘，静止时也耗 CPU，卡顿源之一） */
    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, text != NULL ? text : "");
    lv_obj_set_width(name, 170);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(name, &songti_font_16, 0);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);

    return card;
}

const char *ui_gradient_btn_get_text(lv_obj_t *btn)
{
    if (btn == NULL) return NULL;
    uint32_t n = lv_obj_get_child_cnt(btn);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(btn, i);
        if (ch != NULL && lv_obj_check_type(ch, &lv_label_class)) {
            return lv_label_get_text(ch);
        }
    }
    return NULL;
}

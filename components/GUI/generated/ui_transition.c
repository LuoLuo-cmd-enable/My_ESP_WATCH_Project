/*
 * 缩放切屏过渡动画（路线A：截图 + PSRAM + Canvas）
 *
 * 流程：
 *   phase1: 当前屏截图 → Canvas 从全屏缩小到点击中心
 *   phase1 ready: 真正调用 ui_load_scr_animation 切屏（NONE 立即切）
 *   phase2: 新屏截图 → Canvas 从点击中心放大到全屏
 *
 * 依赖：CONFIG_LV_USE_SNAPSHOT、CONFIG_LV_USE_CANVAS（已启用）
 */
#include <string.h>
#include <math.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ui_transition.h"

#define TAG "UI_TRANS"

#define SCR_W 240
#define SCR_H 284
#define SCR_PX (SCR_W * SCR_H)
#define SCR_BYTES (SCR_PX * sizeof(lv_color_t))

/* 动画结束后的圆角（全屏时切角大小，参考实现为 48） */
#define CORNER_R 48
/* 最小缩放比例 */
#define ZOOM_MIN 0.2f

/* 过渡用的 Canvas 与缓冲（PSRAM 懒分配） */
static lv_color_t *s_canvas_buf = NULL;
static lv_color_t *s_snap_buf = NULL;
static lv_obj_t *s_canvas = NULL;
static bool s_buffers_ready = false;

/* 点击中心（缩放起点/终点） */
static lv_point_t s_zoom_center = {SCR_W / 2, SCR_H / 2};

/* 动画进行中标记，防止重入 */
static volatile bool s_anim_busy = false;

/* phase1 结束时真正切屏所需的参数 */
static lv_ui *s_pending_ui = NULL;
static lv_obj_t **s_pending_new = NULL;
static bool s_pending_new_del = false;
static bool *s_pending_old_del = NULL;
static ui_setup_scr_t s_pending_setup = NULL;
static bool s_pending_auto_del = false;

/* 动画中心插值起止点 */
static lv_point_t s_center_start;
static lv_point_t s_center_end;

static void phase2_ready_cb(lv_anim_t *a);

void ui_transition_set_center(lv_coord_t x, lv_coord_t y)
{
    s_zoom_center.x = x;
    s_zoom_center.y = y;
}

/* 判断当前屏幕是否处于动画画中 */
bool ui_transition_is_busy(void)
{
    return s_anim_busy;
}

/* 判断点是否在圆角矩形内（参考实现同款） */
static bool is_point_in_rounded_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r)
{
    if (x < 0 || x >= w || y < 0 || y >= h) return false;
    if (y >= r && y < h - r) return true;
    if (x >= r && x < w - r) return true;

    int32_t dx, dy;
    if (x < r && y < r) {
        dx = x - r;
        dy = y - r;
    } else if (x >= w - r && y < r) {
        dx = x - (w - r - 1);
        dy = y - r;
    } else if (x < r && y >= h - r) {
        dx = x - r;
        dy = y - (h - r - 1);
    } else if (x >= w - r && y >= h - r) {
        dx = x - (w - r - 1);
        dy = y - (h - r - 1);
    } else {
        return true;
    }
    return (dx * dx + dy * dy) <= r * r;
}

/* 把 s_snap_buf 按 scale/center 缩放到 s_canvas_buf（背景浅黑 + 圆角） */
static void draw_scaled_snapshot(int32_t center_x, int32_t center_y, float scale)
{
    if (!s_buffers_ready) return;

    /* 背景用浅黑色 0x1a1a1a，与滑动菜单页背景一致 */
    lv_color_t bg = lv_color_hex(0x1a1a1a);
    for (int i = 0; i < SCR_PX; i++) {
        s_canvas_buf[i] = bg;
    }

    int32_t tgt_w = (int32_t)(SCR_W * scale);
    int32_t tgt_h = (int32_t)(SCR_H * scale);
    int32_t x_start = center_x - tgt_w / 2;
    int32_t y_start = center_y - tgt_h / 2;

    int32_t draw_x_start = LV_MAX(x_start, 0);
    int32_t draw_y_start = LV_MAX(y_start, 0);
    int32_t draw_x_end = LV_MIN(x_start + tgt_w, SCR_W);
    int32_t draw_y_end = LV_MIN(y_start + tgt_h, SCR_H);

    float inv_scale = 1.0f / scale;
    int32_t corner_r = (int32_t)(CORNER_R * scale);
    int32_t max_r = LV_MIN(tgt_w, tgt_h) / 2;
    if (corner_r > max_r) corner_r = max_r;

    for (int32_t y = draw_y_start; y < draw_y_end; y++) {
        int32_t src_y = (int32_t)((y - y_start) * inv_scale);
        if (src_y < 0) src_y = 0;
        if (src_y >= SCR_H) src_y = SCR_H - 1;
        uint32_t src_row = src_y * SCR_W;
        uint32_t dst_row = y * SCR_W;

        for (int32_t x = draw_x_start; x < draw_x_end; x++) {
            int32_t local_x = x - x_start;
            int32_t local_y = y - y_start;
            if (!is_point_in_rounded_rect(local_x, local_y, tgt_w, tgt_h, corner_r)) {
                continue;
            }
            int32_t src_x = (int32_t)((x - x_start) * inv_scale);
            if (src_x < 0) src_x = 0;
            if (src_x >= SCR_W) src_x = SCR_W - 1;
            s_canvas_buf[dst_row + x] = s_snap_buf[src_row + src_x];
        }
    }

    lv_obj_invalidate(s_canvas);
}

/* 缩放动画执行回调：v 从 255→0 或 0→255，t=0..1 */
static void zoom_anim_cb(void *var, int32_t v)
{
    LV_UNUSED(var);
    float t = v / 255.0f;
    /* 指数缩放：scale = 0.2 * 5^t (0.2 -> 1.0) */
    float scale = ZOOM_MIN * powf(5.0f, t);
    int32_t center_x = s_center_start.x + (int32_t)((s_center_end.x - s_center_start.x) * t);
    int32_t center_y = s_center_start.y + (int32_t)((s_center_end.y - s_center_start.y) * t);
    draw_scaled_snapshot(center_x, center_y, scale);
}

/* 指数缓出 */
static int32_t cubic_ease_out_cb(const lv_anim_t *a)
{
    float t = (float)a->act_time / a->time;
    if (t > 1.0f) t = 1.0f;
    float t_inv = 1.0f - t;
    float progress = 1.0f - (t_inv * t_inv * t_inv);
    return a->start_value + (int32_t)((a->end_value - a->start_value) * progress);
}

/* phase1（缩小旧屏）结束：真正切屏，再启动 phase2（放大新屏） */
static void phase1_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);

    /* 调用原始切屏（NONE 立即切，避免二次动画叠加） */
    ui_load_scr_animation(s_pending_ui, s_pending_new, s_pending_new_del,
                          s_pending_old_del, s_pending_setup,
                          LV_SCR_LOAD_ANIM_NONE, 0, 0, false, s_pending_auto_del);

    /* 截取新屏 */
    lv_img_dsc_t *dsc = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    if (dsc && dsc->data) {
        memcpy(s_snap_buf, dsc->data, SCR_BYTES);
        lv_snapshot_free(dsc);
    }

    /* phase2：从点击中心放大到全屏 */
    s_center_start = s_zoom_center;
    s_center_end.x = SCR_W / 2;
    s_center_end.y = SCR_H / 2;

    lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_canvas);
    lv_obj_update_layout(s_canvas);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_canvas);
    lv_anim_set_exec_cb(&anim, zoom_anim_cb);
    lv_anim_set_time(&anim, 300);
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_path_cb(&anim, cubic_ease_out_cb);
    lv_anim_set_ready_cb(&anim, phase2_ready_cb);
    lv_anim_start(&anim);
}

/* phase2（放大新屏）结束：隐藏 Canvas，完成 */
static void phase2_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);
    s_anim_busy = false;
}

/* 懒分配 PSRAM 缓冲与 Canvas（首次调用时执行） */
static void buffers_init(void)
{
    if (s_buffers_ready) return;

    s_canvas_buf = heap_caps_malloc(SCR_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_snap_buf = heap_caps_malloc(SCR_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_canvas_buf == NULL || s_snap_buf == NULL) {
        ESP_LOGW(TAG, "PSRAM alloc failed, fallback to instant switch");
        if (s_canvas_buf) { heap_caps_free(s_canvas_buf); s_canvas_buf = NULL; }
        if (s_snap_buf) { heap_caps_free(s_snap_buf); s_snap_buf = NULL; }
        return;
    }

    s_canvas = lv_canvas_create(lv_layer_top());
    lv_canvas_set_buffer(s_canvas, s_canvas_buf, SCR_W, SCR_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_canvas, 0, 0);
    lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);

    s_buffers_ready = true;
    ESP_LOGI(TAG, "transition buffers ready (%d bytes x2 PSRAM)", SCR_BYTES);
}

void ui_load_scr_with_zoom(lv_ui *ui, lv_obj_t **new_scr, bool new_scr_del, bool *old_scr_del,
                           ui_setup_scr_t setup_scr, lv_scr_load_anim_t anim_type,
                           uint32_t time, uint32_t delay, bool is_clean, bool auto_del)
{
    LV_UNUSED(anim_type);
    LV_UNUSED(time);
    LV_UNUSED(delay);
    LV_UNUSED(is_clean);

    buffers_init();

    lv_obj_t *old_scr = lv_scr_act();

    /* 动画进行中：直接丢弃本次请求，防止重复切屏破坏状态机 */
    if (s_anim_busy) {
        return;
    }

    if (!s_buffers_ready || old_scr == NULL || !lv_obj_is_valid(old_scr)) {
        /* 降级：直接使用原始切屏 */
        ui_load_scr_animation(ui, new_scr, new_scr_del, old_scr_del, setup_scr,
                              LV_SCR_LOAD_ANIM_NONE, 0, 0, false, auto_del);
        return;
    }

    s_anim_busy = true;

    /* 保存切屏参数，phase1 结束后使用 */
    s_pending_ui = ui;
    s_pending_new = new_scr;
    s_pending_new_del = new_scr_del;
    s_pending_old_del = old_scr_del;
    s_pending_setup = setup_scr;
    s_pending_auto_del = auto_del;

    /* 截取当前屏 */
    lv_img_dsc_t *dsc = lv_snapshot_take(old_scr, LV_IMG_CF_TRUE_COLOR);
    if (dsc == NULL || dsc->data == NULL) {
        if (dsc) lv_snapshot_free(dsc);
        s_anim_busy = false;
        ui_load_scr_animation(ui, new_scr, new_scr_del, old_scr_del, setup_scr,
                              LV_SCR_LOAD_ANIM_NONE, 0, 0, false, auto_del);
        return;
    }
    memcpy(s_snap_buf, dsc->data, SCR_BYTES);
    lv_snapshot_free(dsc);

    /* phase1：全屏 -> 缩小到点击中心（v 255→0） */
    s_center_start.x = SCR_W / 2;
    s_center_start.y = SCR_H / 2;
    s_center_end = s_zoom_center;

    lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_canvas);
    lv_obj_update_layout(s_canvas);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_canvas);
    lv_anim_set_exec_cb(&anim, zoom_anim_cb);
    lv_anim_set_time(&anim, 300);
    lv_anim_set_values(&anim, 255, 0);
    lv_anim_set_path_cb(&anim, cubic_ease_out_cb);
    lv_anim_set_ready_cb(&anim, phase1_ready_cb);
    lv_anim_start(&anim);
}

#include "pager_screen.h"
#include <cstdio>

static lv_obj_t* MakeArc(lv_obj_t* parent, int size, lv_color_t color) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);          // no knob
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    return arc;
}

void PagerScreen::Build() {
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0a0a12), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    arc_wifi_ = MakeArc(screen_, 340, lv_color_hex(0x2a7fc4)); // blue
    arc_7d_   = MakeArc(screen_, 290, lv_color_hex(0xB35000)); // amber
    arc_5h_   = MakeArc(screen_, 240, lv_color_hex(0xE0902a)); // light amber

    center_label_ = lv_label_create(screen_);
    lv_obj_center(center_label_);
    lv_obj_set_style_text_color(center_label_, lv_color_hex(0xf0f0f0), 0);
    lv_label_set_text(center_label_, "");

    // Alert box (hidden until RenderAlert)
    alert_box_ = lv_obj_create(screen_);
    lv_obj_set_size(alert_box_, 300, 300);
    lv_obj_center(alert_box_);
    lv_obj_set_style_bg_color(alert_box_, lv_color_hex(0x7a1010), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(alert_box_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(alert_box_, 180, LV_PART_MAIN);
    lv_obj_set_flex_flow(alert_box_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(alert_box_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);

    alert_agent_ = lv_label_create(alert_box_);
    lv_obj_set_style_text_color(alert_agent_, lv_color_hex(0xffd0d0), 0);
    alert_msg_ = lv_label_create(alert_box_);
    lv_label_set_long_mode(alert_msg_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(alert_msg_, 240);
    lv_obj_set_style_text_color(alert_msg_, lv_color_hex(0xffffff), 0);
}

static void SetArc(lv_obj_t* arc, int pct, bool stale, lv_color_t live) {
    lv_arc_set_value(arc, pct < 0 ? 0 : (pct > 100 ? 100 : pct));
    lv_obj_set_style_arc_color(arc, stale ? lv_color_hex(0x333333) : live,
                               LV_PART_INDICATOR);
}

void PagerScreen::RenderRing(int wifi_pct, const PagerAiProfile* profile,
                             bool ai_stale, int queue_depth) {
    lv_obj_add_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_7d_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_5h_, LV_OBJ_FLAG_HIDDEN);

    SetArc(arc_wifi_, wifi_pct, false, lv_color_hex(0x2a7fc4));
    SetArc(arc_7d_, profile ? profile->seven_d : 0, ai_stale, lv_color_hex(0xB35000));
    SetArc(arc_5h_, profile ? profile->five_h : 0, ai_stale, lv_color_hex(0xE0902a));

    char buf[48];
    if (queue_depth > 0) {
        std::snprintf(buf, sizeof(buf), "%s\n%d pending",
                      profile ? profile->label : "", queue_depth);
    } else {
        std::snprintf(buf, sizeof(buf), "%s", profile ? profile->label : "");
    }
    lv_label_set_text(center_label_, buf);
}

void PagerScreen::RenderAlert(const PagerAlert& a, int remaining) {
    lv_obj_add_flag(arc_wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arc_7d_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arc_5h_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(center_label_, "");
    lv_obj_clear_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);

    char head[40];
    std::snprintf(head, sizeof(head), "%s%s", a.agent,
                  remaining > 1 ? "  (+more)" : "");
    lv_label_set_text(alert_agent_, head);
    lv_label_set_text(alert_msg_, a.message);
}
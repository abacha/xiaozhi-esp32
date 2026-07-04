#include "pager_screen.h"
#include "pager_ai_color.h"
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
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_CLICKABLE); // tap-to-ack lands here

    arc_wifi_ = MakeArc(screen_, 340, lv_color_hex(0x2a7fc4)); // blue (fixed)
    arc_7d_   = MakeArc(screen_, 290, lv_color_hex(0x333333)); // severity-scaled
    arc_5h_   = MakeArc(screen_, 240, lv_color_hex(0x333333)); // severity-scaled

    // Center readout: profile label, the two hero figures, device IP.
    center_box_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(center_box_);
    lv_obj_set_size(center_box_, 200, LV_SIZE_CONTENT);
    lv_obj_center(center_box_);
    lv_obj_set_flex_flow(center_box_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center_box_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center_box_, 2, 0);
    lv_obj_clear_flag(center_box_, LV_OBJ_FLAG_CLICKABLE); // let the tap reach screen_

    profile_label_ = lv_label_create(center_box_);
    lv_obj_set_style_text_color(profile_label_, lv_color_hex(0xf0f0f0), 0);
    lv_label_set_text(profile_label_, "");

    val_5h_ = lv_label_create(center_box_);
    lv_obj_set_style_text_font(val_5h_, &lv_font_montserrat_28, 0);
    lv_label_set_text(val_5h_, "5h --");

    val_7d_ = lv_label_create(center_box_);
    lv_obj_set_style_text_font(val_7d_, &lv_font_montserrat_28, 0);
    lv_label_set_text(val_7d_, "7d --");

    ip_label_ = lv_label_create(center_box_);
    lv_obj_set_style_text_color(ip_label_, lv_color_hex(0x444444), 0);
    lv_label_set_text(ip_label_, "");

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
    lv_obj_clear_flag(alert_box_, LV_OBJ_FLAG_CLICKABLE); // don't swallow the tap in takeover

    alert_agent_ = lv_label_create(alert_box_);
    lv_obj_set_style_text_color(alert_agent_, lv_color_hex(0xffd0d0), 0);
    alert_msg_ = lv_label_create(alert_box_);
    lv_label_set_long_mode(alert_msg_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(alert_msg_, 240);
    lv_obj_set_style_text_color(alert_msg_, lv_color_hex(0xffffff), 0);
}

static void SetArc(lv_obj_t* arc, int pct, unsigned color_hex) {
    lv_arc_set_value(arc, pct < 0 ? 0 : (pct > 100 ? 100 : pct));
    lv_obj_set_style_arc_color(arc, lv_color_hex(color_hex), LV_PART_INDICATOR);
}

static void SetFigure(lv_obj_t* lbl, const char* tag, int pct, bool stale) {
    char buf[16];
    if (stale) std::snprintf(buf, sizeof(buf), "%s --", tag);
    else       std::snprintf(buf, sizeof(buf), "%s %d%%", tag, pct);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, lv_color_hex(AiArcColor(pct, stale)), 0);
}

void PagerScreen::RenderRing(int wifi_pct, const PagerAiProfile* profile,
                             bool ai_stale, const char* ip) {
    lv_obj_add_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_7d_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_5h_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(center_box_, LV_OBJ_FLAG_HIDDEN);

    int five_h  = profile ? profile->five_h : 0;
    int seven_d = profile ? profile->seven_d : 0;

    SetArc(arc_wifi_, wifi_pct, 0x2a7fc4);
    SetArc(arc_7d_, seven_d, AiArcColor(seven_d, ai_stale));
    SetArc(arc_5h_, five_h, AiArcColor(five_h, ai_stale));

    lv_label_set_text(profile_label_, profile ? profile->label : "");
    SetFigure(val_5h_, "5h", five_h, ai_stale);
    SetFigure(val_7d_, "7d", seven_d, ai_stale);
    lv_label_set_text(ip_label_, ip ? ip : "");
}

void PagerScreen::RenderAlert(const PagerAlert& a, int remaining) {
    lv_obj_add_flag(arc_wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arc_7d_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arc_5h_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(center_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);

    char head[40];
    std::snprintf(head, sizeof(head), "%s%s", a.agent,
                  remaining > 1 ? "  (+more)" : "");
    lv_label_set_text(alert_agent_, head);
    lv_label_set_text(alert_msg_, a.message);
}

#include "pager_screen.h"
#include "pager_ai_color.h"
#include <cstdio>
#include <cstring>
#include <cctype>

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

    // Alert takeover: full-screen agent-color fill with absolutely-positioned
    // text, matching the agent-pager design (02-design.md §5.3). Hidden until
    // RenderAlert. Non-clickable so the tap-to-ack reaches screen_.
    alert_box_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(alert_box_);
    lv_obj_set_size(alert_box_, 360, 360);
    lv_obj_center(alert_box_);
    lv_obj_set_style_bg_opa(alert_box_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(alert_box_, 180, LV_PART_MAIN);
    lv_obj_add_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(alert_box_, LV_OBJ_FLAG_CLICKABLE);

    alert_initial_ = lv_label_create(alert_box_);
    lv_obj_align(alert_initial_, LV_ALIGN_CENTER, 0, -64);
    lv_obj_set_style_text_font(alert_initial_, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(alert_initial_, lv_color_hex(0xffffff), 0);

    alert_agent_ = lv_label_create(alert_box_);
    lv_obj_align(alert_agent_, LV_ALIGN_CENTER, 0, -22);
    lv_obj_set_style_text_font(alert_agent_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(alert_agent_, lv_color_hex(0xdddddd), 0);

    alert_host_ = lv_label_create(alert_box_);
    lv_obj_align(alert_host_, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_text_color(alert_host_, lv_color_hex(0xaaaaaa), 0);

    alert_msg_ = lv_label_create(alert_box_);
    lv_obj_align(alert_msg_, LV_ALIGN_CENTER, 0, 56);
    lv_label_set_long_mode(alert_msg_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(alert_msg_, 260);
    lv_obj_set_style_text_align(alert_msg_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(alert_msg_, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(alert_msg_, lv_color_hex(0xffffff), 0);

    alert_hint_ = lv_label_create(alert_box_);
    lv_obj_align(alert_hint_, LV_ALIGN_CENTER, 0, 128);
    lv_obj_set_style_text_color(alert_hint_, lv_color_hex(0x999999), 0);
}

// Agent fill colors, matching the agent-pager takeover palette (§5.3).
static unsigned AgentColor(const char* agent) {
    struct { const char* name; unsigned hex; } kMap[] = {
        {"ken", 0xFF4444}, {"wolf", 0x4A4A4A}, {"arthur", 0x8B5E3C},
        {"enzo", 0x44CC44}, {"arnold", 0x9B44CC}, {"claude", 0xB35000},
    };
    for (auto& m : kMap) if (std::strcmp(m.name, agent) == 0) return m.hex;
    return 0x444444; // unknown
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

void PagerScreen::RenderRing(const PagerAiProfile* profile,
                             bool ai_stale, const char* ip) {
    lv_obj_add_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_7d_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_5h_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(center_box_, LV_OBJ_FLAG_HIDDEN);

    int five_h  = profile ? profile->five_h : 0;
    int seven_d = profile ? profile->seven_d : 0;

    // Outer arc: fraction of the 7-day window left until the usage resets.
    constexpr int kResetWindowS = 7 * 24 * 3600;
    int reset_pct = profile ? (int)((long)profile->reset_in_s * 100 / kResetWindowS) : 0;
    SetArc(arc_wifi_, reset_pct, ai_stale ? 0x333333 : 0x2a7fc4);
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
    lv_obj_set_style_bg_color(alert_box_, lv_color_hex(AgentColor(a.agent)), LV_PART_MAIN);
    lv_obj_clear_flag(alert_box_, LV_OBJ_FLAG_HIDDEN);

    char ini[2] = { (char)std::toupper((unsigned char)a.agent[0]), '\0' };
    lv_label_set_text(alert_initial_, ini);

    char name[24];
    size_t i = 0;
    for (; a.agent[i] && i < sizeof(name) - 1; ++i) name[i] = std::toupper((unsigned char)a.agent[i]);
    name[i] = '\0';
    lv_label_set_text(alert_agent_, name);

    if (a.hostname[0]) {
        lv_label_set_text(alert_host_, a.hostname);
        lv_obj_clear_flag(alert_host_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(alert_host_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(alert_msg_, a.message);

    if (remaining > 1) {
        char h[32];
        std::snprintf(h, sizeof(h), "tap to ack  (+%d)", remaining - 1);
        lv_label_set_text(alert_hint_, h);
    } else {
        lv_label_set_text(alert_hint_, "tap to ack");
    }
}

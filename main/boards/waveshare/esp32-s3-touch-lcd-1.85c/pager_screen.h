#pragma once
#include <lvgl.h>
#include "pager_alert_queue.h"
#include "pager_ai_usage.h"

// Owns a standalone LVGL screen with three view modes. All methods must be
// called with the LVGL lock held (DisplayLockGuard on the Display).
class PagerScreen {
public:
    void Build();                 // create the screen + widgets once
    lv_obj_t* screen() { return screen_; }

    // Idle status ring: two segmented reset clocks (outer = 7-day window in 7
    // day-segments, inner = 5-hour window in 5 hour-segments), each emptying to
    // zero at reset and hued by profile. The 5h/7d usage figures are the
    // severity-colored hero readout, with the device IP at the bottom.
    void RenderRing(const PagerAiProfile* profile, bool ai_stale, const char* ip);
    // Full-screen alert.
    void RenderAlert(const PagerAlert& a, int remaining);

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* arc_week_ = nullptr;      // 7-day reset clock (outer)
    lv_obj_t* arc_sess_ = nullptr;      // 5-hour reset clock (inner)
    lv_obj_t* ticks_ = nullptr;         // segment dividers over both clocks
    lv_obj_t* center_box_ = nullptr;    // container for the ring readout
    lv_obj_t* profile_label_ = nullptr; // profile name (small, top)
    lv_obj_t* val_5h_ = nullptr;        // "5h NN%" hero, severity-colored
    lv_obj_t* val_7d_ = nullptr;        // "7d NN%" hero, severity-colored
    lv_obj_t* ip_label_ = nullptr;      // device IP (small, bottom)
    lv_obj_t* alert_box_ = nullptr;     // full-screen agent-color fill
    lv_obj_t* alert_initial_ = nullptr; // agent initial (big)
    lv_obj_t* alert_agent_ = nullptr;   // agent name (uppercase)
    lv_obj_t* alert_host_ = nullptr;    // hostname (dim)
    lv_obj_t* alert_msg_ = nullptr;     // message (wraps)
    lv_obj_t* alert_hint_ = nullptr;    // "tap to ack"
};
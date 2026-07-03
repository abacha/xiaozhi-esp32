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

    // Idle status ring: wifi %, one AI profile's arcs, pending alert count.
    void RenderRing(int wifi_pct, const PagerAiProfile* profile, bool ai_stale, int queue_depth);
    // Full-screen alert.
    void RenderAlert(const PagerAlert& a, int remaining);

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* arc_wifi_ = nullptr;
    lv_obj_t* arc_7d_ = nullptr;
    lv_obj_t* arc_5h_ = nullptr;
    lv_obj_t* center_label_ = nullptr;  // profile label or pending count
    lv_obj_t* alert_box_ = nullptr;     // container for alert text
    lv_obj_t* alert_agent_ = nullptr;
    lv_obj_t* alert_msg_ = nullptr;
};
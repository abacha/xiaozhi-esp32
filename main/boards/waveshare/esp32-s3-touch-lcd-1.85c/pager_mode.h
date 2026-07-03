#pragma once
#include <lvgl.h>
#include "pager_alert_queue.h"
#include "pager_ai_usage.h"
#include "pager_screen.h"
#include "pager_http.h"
#include "pager_ai_poll.h"

class PagerMode {
public:
    static PagerMode& GetInstance() { static PagerMode inst; return inst; }
    void Init(lv_obj_t* stock_screen); // capture stock screen, build pager screen, start http+poll
    void Tick();                       // call every second from the clock tick
    void RequestRefresh();             // schedule a Tick-equivalent redraw

private:
    void RenderNow();
    static void GestureCb(lv_event_t* e);
    static void TapCb(lv_event_t* e);

    lv_obj_t* stock_screen_ = nullptr;
    lv_obj_t* badge_ = nullptr; // From task 7
    PagerScreen screen_;
    PagerAlertQueue queue_;
    PagerAiUsage usage_;
    PagerHttp http_;
    PagerAiPoll poll_;
    int profile_idx_ = 0;
    bool pager_active_ = false; // true when our screen is loaded
};
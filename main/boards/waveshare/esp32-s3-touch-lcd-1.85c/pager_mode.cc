#include "pager_mode.h"
#include "application.h"
#include "board.h"
#include "display/display.h"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <cstdio>

static int WifiPct() {
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return 0;
    int rssi = ap.rssi;                       // ~ -90..-30
    int pct = 2 * (rssi + 100);               // -100->0, -50->100
    return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
}

static const char* IpStr(char* buf, size_t n) {
    buf[0] = '\0';
    esp_netif_t* nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if (nif && esp_netif_get_ip_info(nif, &ip) == ESP_OK) {
        std::snprintf(buf, n, IPSTR, IP2STR(&ip.ip));
    }
    return buf;
}

void PagerMode::Init(lv_obj_t* stock_screen) {
    stock_screen_ = stock_screen;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    screen_.Build();
    lv_obj_add_event_cb(screen_.screen(), GestureCb, LV_EVENT_GESTURE, this);
    lv_obj_add_event_cb(screen_.screen(), TapCb, LV_EVENT_CLICKED, this);

    badge_ = lv_label_create(stock_screen_);
    lv_obj_align(badge_, LV_ALIGN_TOP_RIGHT, -12, 12);
    lv_obj_set_style_bg_color(badge_, lv_color_hex(0xc42a2a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(badge_, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(badge_, 4, LV_PART_MAIN);
    lv_obj_set_style_text_color(badge_, lv_color_hex(0xffffff), 0);
    lv_obj_add_flag(badge_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(badge_);

    http_.Start(&queue_, []{ PagerMode::GetInstance().RequestRefresh(); });
    poll_.Start(&usage_, []{ PagerMode::GetInstance().RequestRefresh(); });
}

void PagerMode::RequestRefresh() {
    Application::GetInstance().Schedule([]{ PagerMode::GetInstance().RenderNow(); });
}

void PagerMode::Tick() { RenderNow(); }

void PagerMode::RenderNow() {
    auto state = Application::GetInstance().GetDeviceState();
    bool idle = (state == kDeviceStateIdle);
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (!idle) {
        if (pager_active_) { lv_screen_load(stock_screen_); pager_active_ = false; }
        int d = queue_.Depth();
        if (d > 0) {
            char b[16]; snprintf(b, sizeof(b), "%d", d);
            lv_label_set_text(badge_, b);
            lv_obj_clear_flag(badge_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(badge_);
        } else {
            lv_obj_add_flag(badge_, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (!pager_active_) { lv_screen_load(screen_.screen()); pager_active_ = true; }

    if (queue_.Depth() > 0) {
        screen_.RenderAlert(queue_.Front(), queue_.Depth());
    } else {
        const PagerAiProfile* p = (usage_.count > 0)
            ? &usage_.profiles[profile_idx_ % usage_.count] : nullptr;
        char ip[16];
        screen_.RenderRing(WifiPct(), p, usage_.stale, IpStr(ip, sizeof(ip)));
    }
}

void PagerMode::GestureCb(lv_event_t* e) {
    auto* self = static_cast<PagerMode*>(lv_event_get_user_data(e));
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (self->queue_.Depth() > 0 || self->usage_.count == 0) return; // ring view only
    if (dir == LV_DIR_LEFT)  self->profile_idx_ = (self->profile_idx_ + 1) % self->usage_.count;
    if (dir == LV_DIR_RIGHT) self->profile_idx_ = (self->profile_idx_ - 1 + self->usage_.count) % self->usage_.count;
    self->RenderNow();
}

void PagerMode::TapCb(lv_event_t* e) {
    auto* self = static_cast<PagerMode*>(lv_event_get_user_data(e));
    if (self->queue_.Depth() > 0) { self->queue_.PopFront(); self->RenderNow(); }
}
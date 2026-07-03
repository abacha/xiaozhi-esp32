#pragma once
#include <esp_http_server.h>
#include <functional>
#include "pager_alert_queue.h"

class PagerHttp {
public:
    // on_change is invoked (on the http task) after any queue mutation so the
    // owner can request a display refresh via Application::Schedule.
    void Start(PagerAlertQueue* queue, std::function<void()> on_change);
    void Stop();

    PagerAlertQueue* queue() { return queue_; }
    void notify() { if (on_change_) on_change_(); }

private:
    httpd_handle_t server_ = nullptr;
    PagerAlertQueue* queue_ = nullptr;
    std::function<void()> on_change_;
};
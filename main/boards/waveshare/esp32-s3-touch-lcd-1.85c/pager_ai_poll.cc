#include "pager_ai_poll.h"
#include <esp_http_client.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string>

#define TAG "pager_ai_poll"
// Use pylon's IP directly: the box's DHCP DNS doesn't resolve the .nexus.lan
// domain (getaddrinfo fails), though the host is reachable. Proper fix is the
// box's DNS/search-domain; this unblocks the poll meanwhile.
static const char* kUrl = "http://192.168.15.250/ai-usage.json";

struct PollCtx {
    PagerAiUsage* out;
    std::function<void()> cb;
    esp_timer_handle_t timer;
};

static void DoFetch(PollCtx* ctx) {
    std::string body;
    esp_http_client_config_t cfg = {};
    cfg.url = kUrl;
    cfg.timeout_ms = 8000;
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    bool ok = false;
    if (esp_http_client_open(cli, 0) == ESP_OK) {
        int len = esp_http_client_fetch_headers(cli);
        (void)len;
        char tmp[512];
        int r;
        while ((r = esp_http_client_read(cli, tmp, sizeof(tmp))) > 0) body.append(tmp, r);
        int status = esp_http_client_get_status_code(cli);
        if (status == 200 && !body.empty()) {
            cJSON* root = cJSON_Parse(body.c_str());
            if (root) { ok = ParseAiUsage(root, ctx->out); cJSON_Delete(root); }
        }
    }
    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    if (!ok) { ctx->out->stale = true; ESP_LOGW(TAG, "poll failed -> stale"); }
    if (ctx->cb) ctx->cb();
}

// Self-rescheduling one-shot: retry fast (60s) while stale so a pre-WiFi boot
// failure recovers quickly, then back off to 30 min once data is fresh.
static void Tick(void* a) {
    auto* ctx = static_cast<PollCtx*>(a);
    DoFetch(ctx);
    uint64_t next_us = ctx->out->stale ? 60ULL * 1000000 : 30ULL * 60 * 1000000;
    esp_timer_start_once(ctx->timer, next_us);
}

void PagerAiPoll::Start(PagerAiUsage* out, std::function<void()> on_update) {
    auto* ctx = new PollCtx{ out, std::move(on_update), nullptr };
    esp_timer_create_args_t args = {};
    args.callback = Tick;
    args.arg = ctx;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "pager_ai_poll";
    esp_timer_create(&args, &ctx->timer);
    esp_timer_start_once(ctx->timer, 5ULL * 1000000); // first try after WiFi settles
}
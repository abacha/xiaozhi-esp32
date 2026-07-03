#include "pager_ai_poll.h"
#include <esp_http_client.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string>

#define TAG "pager_ai_poll"
static const char* kUrl = "http://pylon.nexus.lan/ai-usage.json";

struct PollCtx {
    PagerAiUsage* out;
    std::function<void()> cb;
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

void PagerAiPoll::Start(PagerAiUsage* out, std::function<void()> on_update) {
    auto* ctx = new PollCtx{ out, std::move(on_update) };
    esp_timer_create_args_t args = {};
    args.callback = [](void* a){ DoFetch(static_cast<PollCtx*>(a)); };
    args.arg = ctx;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "pager_ai_poll";
    esp_timer_handle_t h;
    esp_timer_create(&args, &h);
    DoFetch(ctx);                                   // prime once now
    esp_timer_start_periodic(h, 30ULL * 60 * 1000000); // 30 min
}
#include "pager_http.h"
#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <cstring>

#define TAG "pager_http"

static PagerHttp* g_self = nullptr; // single instance on this board

static char* ReadBody(httpd_req_t* req) {
    int total = req->content_len;
    if (total <= 0 || total > 1024) return nullptr;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return nullptr;
    int off = 0;
    while (off < total) {
        int r = httpd_req_recv(req, buf + off, total - off);
        if (r <= 0) { free(buf); return nullptr; }
        off += r;
    }
    buf[total] = '\0';
    return buf;
}

extern "C" esp_err_t pager_post_alert(httpd_req_t* req);
extern "C" esp_err_t pager_post_clear(httpd_req_t* req);
extern "C" esp_err_t pager_get_status(httpd_req_t* req);
extern "C" esp_err_t pager_get_health(httpd_req_t* req);

void PagerHttp::Start(PagerAlertQueue* queue, std::function<void()> on_change) {
    queue_ = queue;
    on_change_ = std::move(on_change);
    g_self = this;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }
    httpd_uri_t alert = { "/alert",  HTTP_POST, pager_post_alert,  nullptr };
    httpd_uri_t clear = { "/clear",  HTTP_POST, pager_post_clear,  nullptr };
    httpd_uri_t status= { "/status", HTTP_GET,  pager_get_status,  nullptr };
    httpd_uri_t health= { "/health", HTTP_GET,  pager_get_health,  nullptr };
    httpd_register_uri_handler(server_, &alert);
    httpd_register_uri_handler(server_, &clear);
    httpd_register_uri_handler(server_, &status);
    httpd_register_uri_handler(server_, &health);
    ESP_LOGI(TAG, "pager http server on :80");
}

void PagerHttp::Stop() {
    if (server_) { httpd_stop(server_); server_ = nullptr; }
    g_self = nullptr;
}

esp_err_t pager_post_alert(httpd_req_t* req) {
    char* body = ReadBody(req);
    if (!body) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body"); return ESP_FAIL; }
    cJSON* root = cJSON_Parse(body);
    free(body);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json"); return ESP_FAIL; }

    const cJSON* agent = cJSON_GetObjectItemCaseSensitive(root, "agent");
    const cJSON* msg   = cJSON_GetObjectItemCaseSensitive(root, "message");
    const cJSON* id    = cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON* host  = cJSON_GetObjectItemCaseSensitive(root, "hostname");

    char idbuf[16];
    const char* idstr = cJSON_IsString(id) ? id->valuestring : nullptr;
    if (!idstr) { // auto-generate from tick count
        static uint32_t seq = 0;
        snprintf(idbuf, sizeof(idbuf), "auto%lu", (unsigned long)(++seq));
        idstr = idbuf;
    }
    PagerAlert a(cJSON_IsString(agent) ? agent->valuestring : "agent",
                 cJSON_IsString(msg)   ? msg->valuestring   : "",
                 idstr,
                 cJSON_IsString(host)  ? host->valuestring  : "");
    cJSON_Delete(root);

    g_self->queue()->Push(a);
    g_self->notify();
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

esp_err_t pager_post_clear(httpd_req_t* req) {
    char* body = ReadBody(req);
    if (!body) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body"); return ESP_FAIL; }
    cJSON* root = cJSON_Parse(body);
    free(body);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json"); return ESP_FAIL; }

    const cJSON* all = cJSON_GetObjectItemCaseSensitive(root, "all");
    const cJSON* id  = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (cJSON_IsTrue(all)) {
        g_self->queue()->ClearAll();
    } else if (cJSON_IsString(id)) {
        g_self->queue()->ClearById(id->valuestring);
    }
    cJSON_Delete(root);
    g_self->notify();
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

esp_err_t pager_get_status(httpd_req_t* req) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"depth\":%d}", g_self->queue()->Depth());
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t pager_get_health(httpd_req_t* req) {
    int rssi = 0;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"uptime_s\":%lu,\"heap\":%lu,\"rssi\":%d,\"depth\":%d}",
             (unsigned long)(esp_timer_get_time() / 1000000),
             (unsigned long)esp_get_free_heap_size(),
             rssi, g_self->queue()->Depth());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}
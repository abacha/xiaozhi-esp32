#pragma once
#include <cJSON.h>
#include <cstring>

struct PagerAiProfile {
    char id[8];
    char label[24];
    int  five_h;
    int  seven_d;
    int  reset_in_s;       // until the 7-day window resets
    int  five_reset_in_s;  // until the 5-hour window resets
};

struct PagerAiUsage {
    static constexpr int kMax = 6;
    PagerAiProfile profiles[kMax];
    int count = 0;
    bool stale = true; // set false only on a fresh successful parse
};

// Fills `out` from a parsed cJSON tree. Returns false if the shape is wrong.
inline bool ParseAiUsage(const cJSON* root, PagerAiUsage* out) {
    if (!root || !out) return false;
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "profiles");
    if (!cJSON_IsArray(arr)) return false;

    int n = 0;
    const cJSON* p = nullptr;
    cJSON_ArrayForEach(p, arr) {
        if (n >= PagerAiUsage::kMax) break;
        const cJSON* id   = cJSON_GetObjectItemCaseSensitive(p, "id");
        const cJSON* lbl  = cJSON_GetObjectItemCaseSensitive(p, "label");
        const cJSON* f5   = cJSON_GetObjectItemCaseSensitive(p, "five_h");
        const cJSON* d7   = cJSON_GetObjectItemCaseSensitive(p, "seven_d");
        const cJSON* rst  = cJSON_GetObjectItemCaseSensitive(p, "reset_in_s");
        const cJSON* frst = cJSON_GetObjectItemCaseSensitive(p, "five_reset_in_s");
        if (!cJSON_IsNumber(f5) || !cJSON_IsNumber(d7)) continue;

        PagerAiProfile& dst = out->profiles[n];
        std::strncpy(dst.id, cJSON_IsString(id) ? id->valuestring : "", sizeof(dst.id) - 1);
        dst.id[sizeof(dst.id) - 1] = '\0';
        std::strncpy(dst.label, cJSON_IsString(lbl) ? lbl->valuestring : "", sizeof(dst.label) - 1);
        dst.label[sizeof(dst.label) - 1] = '\0';
        dst.five_h    = f5->valueint;
        dst.seven_d   = d7->valueint;
        dst.reset_in_s = cJSON_IsNumber(rst) ? rst->valueint : 0;
        dst.five_reset_in_s = cJSON_IsNumber(frst) ? frst->valueint : 0;
        ++n;
    }
    if (n == 0) return false;
    out->count = n;
    out->stale = false;
    return true;
}

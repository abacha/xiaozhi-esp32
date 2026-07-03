#include "../../main/boards/waveshare/esp32-s3-touch-lcd-1.85c/pager_ai_usage.h"
#include <cJSON.h>
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    const char* json =
      "{\"profiles\":[{\"id\":\"t\",\"label\":\"TRAG\",\"five_h\":27,\"seven_d\":25,\"reset_in_s\":90000},"
      "{\"id\":\"hs\",\"label\":\"Hubstaff\",\"five_h\":63,\"seven_d\":8,\"reset_in_s\":554398}]}";
    cJSON* root = cJSON_Parse(json);
    assert(root);

    PagerAiUsage u;
    bool ok = ParseAiUsage(root, &u);
    cJSON_Delete(root);

    assert(ok);
    assert(u.count == 2);
    assert(std::strcmp(u.profiles[0].label, "TRAG") == 0);
    assert(u.profiles[0].five_h == 27);
    assert(u.profiles[0].seven_d == 25);
    assert(u.profiles[1].five_h == 63);

    // malformed / empty → not ok
    PagerAiUsage u2;
    cJSON* bad = cJSON_Parse("{\"nope\":true}");
    assert(ParseAiUsage(bad, &u2) == false);
    cJSON_Delete(bad);

    std::puts("PASS test_ai_usage");
    return 0;
}

#include "../../main/boards/waveshare/esp32-s3-touch-lcd-1.85c/pager_ai_color.h"
#include <cassert>
#include <cstdio>

int main() {
    // stale wins regardless of value
    assert(AiArcColor(0, true) == 0x333333);
    assert(AiArcColor(100, true) == 0x333333);

    // band interiors
    assert(AiArcColor(0, false) == 0x2FB37A);   // green
    assert(AiArcColor(55, false) == 0x7FB800);  // lime
    assert(AiArcColor(80, false) == 0xE3B23C);  // amber
    assert(AiArcColor(90, false) == 0xF26419);  // orange
    assert(AiArcColor(100, false) == 0xFF3030); // red

    // band boundaries (inclusive upper edge)
    assert(AiArcColor(40, false) == 0x2FB37A);
    assert(AiArcColor(41, false) == 0x7FB800);
    assert(AiArcColor(70, false) == 0x7FB800);
    assert(AiArcColor(71, false) == 0xE3B23C);
    assert(AiArcColor(85, false) == 0xE3B23C);
    assert(AiArcColor(86, false) == 0xF26419);
    assert(AiArcColor(95, false) == 0xF26419);
    assert(AiArcColor(96, false) == 0xFF3030);

    std::puts("PASS test_ai_color");
    return 0;
}

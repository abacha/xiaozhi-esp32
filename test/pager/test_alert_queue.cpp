#include "../../main/boards/waveshare/esp32-s3-touch-lcd-1.85c/pager_alert_queue.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    PagerAlertQueue q;
    assert(q.Depth() == 0);

    // enqueue + FIFO order
    q.Push({"claude", "wait-for-user", "a1", "forge"});
    q.Push({"enzo", "pix ready", "a2", "mob"});
    assert(q.Depth() == 2);
    assert(std::strcmp(q.Front().id, "a1") == 0);

    // targeted clear removes by id, preserves order
    assert(q.ClearById("a1") == true);
    assert(q.Depth() == 1);
    assert(std::strcmp(q.Front().id, "a2") == 0);
    assert(q.ClearById("nope") == false);

    // pop
    q.PopFront();
    assert(q.Depth() == 0);

    // clear-all
    q.Push({"a","b","c","d"});
    q.ClearAll();
    assert(q.Depth() == 0);

    // capacity 8: 9th push drops the oldest
    for (int i = 0; i < 9; ++i) {
        char id[8]; std::snprintf(id, sizeof(id), "n%d", i);
        q.Push({"ag", "m", id, "h"});
    }
    assert(q.Depth() == 8);
    assert(std::strcmp(q.Front().id, "n1") == 0); // n0 dropped

    std::puts("PASS test_alert_queue");
    return 0;
}

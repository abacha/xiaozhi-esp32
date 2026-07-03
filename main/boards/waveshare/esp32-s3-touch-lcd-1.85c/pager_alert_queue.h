#pragma once
#include <cstring>
#include <cstddef>

// Pure C++, no ESP deps — host-testable. Fixed-capacity FIFO of alerts.
struct PagerAlert {
    char agent[24];
    char message[128];
    char id[16];
    char hostname[24];

    PagerAlert() { agent[0] = message[0] = id[0] = hostname[0] = '\0'; }
    PagerAlert(const char* a, const char* m, const char* i, const char* h) {
        Copy(agent, a, sizeof(agent));
        Copy(message, m, sizeof(message));
        Copy(id, i, sizeof(id));
        Copy(hostname, h, sizeof(hostname));
    }
    static void Copy(char* dst, const char* src, size_t n) {
        if (!src) { dst[0] = '\0'; return; }
        std::strncpy(dst, src, n - 1);
        dst[n - 1] = '\0';
    }
};

class PagerAlertQueue {
public:
    static constexpr int kCapacity = 8;

    int Depth() const { return count_; }
    const PagerAlert& Front() const { return buf_[head_]; }

    void Push(const PagerAlert& a) {
        buf_[tail_] = a;
        tail_ = (tail_ + 1) % kCapacity;
        if (count_ == kCapacity) {
            head_ = (head_ + 1) % kCapacity; // overwrite oldest
        } else {
            ++count_;
        }
    }

    void PopFront() {
        if (count_ == 0) return;
        head_ = (head_ + 1) % kCapacity;
        --count_;
    }

    bool ClearById(const char* id) {
        for (int k = 0; k < count_; ++k) {
            int idx = (head_ + k) % kCapacity;
            if (std::strcmp(buf_[idx].id, id) == 0) {
                RemoveAt(k);
                return true;
            }
        }
        return false;
    }

    void ClearAll() { head_ = tail_ = count_ = 0; }

private:
    void RemoveAt(int k) {
        for (int j = k; j < count_ - 1; ++j) {
            int cur = (head_ + j) % kCapacity;
            int nxt = (head_ + j + 1) % kCapacity;
            buf_[cur] = buf_[nxt];
        }
        tail_ = (tail_ - 1 + kCapacity) % kCapacity;
        --count_;
    }

    PagerAlert buf_[kCapacity];
    int head_ = 0, tail_ = 0, count_ = 0;
};

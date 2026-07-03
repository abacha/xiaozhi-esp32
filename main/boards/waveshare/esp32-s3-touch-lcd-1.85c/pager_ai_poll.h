#pragma once
#include <functional>
#include "pager_ai_usage.h"

class PagerAiPoll {
public:
    void Start(PagerAiUsage* out, std::function<void()> on_update);
private:
    static void FetchOnce(PagerAiUsage* out, std::function<void()>* cb);
};
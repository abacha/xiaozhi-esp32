#pragma once

// 5-band severity scale for the AI-usage arcs, keyed on utilization %.
// Mirrors the agent-pager Status Ring design. Returns 0xRRGGBB; stale is grey.
inline unsigned AiArcColor(int pct, bool stale) {
    if (stale) return 0x333333;
    if (pct <= 40) return 0x2FB37A; // green
    if (pct <= 70) return 0x7FB800; // lime
    if (pct <= 85) return 0xE3B23C; // amber
    if (pct <= 95) return 0xF26419; // orange
    return 0xFF3030;                // red
}

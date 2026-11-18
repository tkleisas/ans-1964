#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ansvst {

// ANS track 0 = lowest (21.09 Hz); row 0 of the image = highest track.
inline void paintScoreAt(uint8_t* data, int width,
                         int col, float track, int level,
                         float brushTracks, bool erase, int harmonics)
{
    if (data == nullptr || width <= 0 || col < 0 || col >= width) return;
    const float brush = std::max(1.0f, std::min(128.0f, brushTracks));
    const int n = std::max(1, std::min(16, harmonics));
    const int baseLevel = std::max(0, std::min(255, level));

    for (int h = 1; h <= n; ++h)
    {
        const float tr = track + std::round(72.0f * std::log2((float)h));
        if (tr < 0.0f) continue;
        if (tr >= 720.0f) break;
        const int lev = (int)std::lround((float)baseLevel * std::pow((float)h, -0.5f));
        const float half = brush * 0.5f;
        int lo = (int)std::floor(tr - half);
        int hi = (int)std::ceil(tr + half);
        lo = std::max(0, std::min(719, lo));
        hi = std::max(0, std::min(719, hi));
        for (int t = lo; t <= hi; ++t)
        {
            const float ov = std::min((float)t + 0.5f, tr + half) -
                             std::max((float)t - 0.5f, tr - half);
            if (ov <= 0.0f) continue;
            const int row = 719 - t;
            uint8_t& cell = data[(size_t)row * (size_t)width + (size_t)col];
            if (erase)
            {
                cell = 0;
            }
            else
            {
                const int v = (int)std::lround((float)lev * std::min(1.0f, ov));
                if (v > (int)cell) cell = (uint8_t)std::min(255, v);
            }
        }
    }
}

} // namespace ansvst

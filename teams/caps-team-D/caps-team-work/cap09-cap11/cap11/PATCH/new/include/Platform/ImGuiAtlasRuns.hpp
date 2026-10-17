// ImGuiAtlasRuns.hpp -- turn Dear ImGui's font atlas (alpha coverage) into a
// small list of axis-aligned runs that the engine can draw ONCE, untextured,
// into a render target, which then serves as the atlas texture for every GUI
// frame. This needs no image decoder and no texture-creation API beyond the
// canvas render target route already used by shipping Lua mods. Engine-free,
// host-tested: rasterising the runs back reproduces the quantised atlas
// exactly.
#pragma once
#include <cstdint>
#include <vector>

namespace RC::Platform::NativeGui
{
    struct Run
    {
        uint16_t x{}, y{}, w{}, h{};
        uint8_t alpha{}; // quantised coverage (255 = solid)
    };

    // Quantise coverage to `levels` steps (>= 2), keep 0 as empty.
    inline auto quantise(uint8_t a, int levels) -> uint8_t
    {
        if (a == 0) return 0;
        if (levels >= 256) return a;
        const int step = 255 / (levels - 1);
        int q = ((a + step / 2) / step) * step;
        if (q > 255) q = 255;
        if (q == 0) q = step; // non-zero coverage never vanishes
        return static_cast<uint8_t>(q);
    }

    // Horizontal runs of equal quantised alpha, then vertical merge of runs with
    // identical x/w/alpha on consecutive rows (glyph stems collapse to one quad).
    inline auto atlas_runs(const uint8_t* alpha, int width, int height, int levels = 16) -> std::vector<Run>
    {
        std::vector<Run> done;
        if (!alpha || width <= 0 || height <= 0 || width > 65535 || height > 65535) return done;
        std::vector<Run> open; // runs that may still grow downwards (sorted by x)
        std::vector<Run> row;
        for (int y = 0; y < height; ++y)
        {
            row.clear();
            int x = 0;
            while (x < width)
            {
                const uint8_t q = quantise(alpha[static_cast<size_t>(y) * width + x], levels);
                if (q == 0) { ++x; continue; }
                int e = x + 1;
                while (e < width && quantise(alpha[static_cast<size_t>(y) * width + e], levels) == q) ++e;
                row.push_back(Run{static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(e - x), 1, q});
                x = e;
            }
            std::vector<Run> next_open;
            size_t i = 0, j = 0;
            while (i < open.size() || j < row.size())
            {
                if (j < row.size() && i < open.size() && open[i].x == row[j].x && open[i].w == row[j].w && open[i].alpha == row[j].alpha)
                {
                    Run r = open[i];
                    ++r.h;
                    next_open.push_back(r);
                    ++i;
                    ++j;
                }
                else if (j >= row.size() || (i < open.size() && open[i].x < row[j].x) || (i < open.size() && open[i].x == row[j].x))
                {
                    done.push_back(open[i++]); // closed: no continuation on this row
                }
                else
                {
                    next_open.push_back(row[j++]);
                }
            }
            open.swap(next_open);
        }
        done.insert(done.end(), open.begin(), open.end());
        return done;
    }

    // Test helper: rasterise runs back into an alpha buffer.
    inline auto rasterise(const std::vector<Run>& runs, int width, int height) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> img(static_cast<size_t>(width) * height, 0);
        for (const auto& r : runs)
            for (int y = r.y; y < r.y + r.h; ++y)
                for (int x = r.x; x < r.x + r.w; ++x) img[static_cast<size_t>(y) * width + x] = r.alpha;
        return img;
    }
} // namespace RC::Platform::NativeGui

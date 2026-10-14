// ImGuiCanvasBatcher.hpp -- Dear ImGui draw data -> clipped, textured triangle
// batches in the shape of the engine's FCanvasUVTri (3 x position, UV, linear
// color). Engine-free and host-tested (#11).
//
// The engine's Canvas K2_DrawTriangle has no scissor rectangle, so ImGui's
// per-command ClipRect is applied here on the CPU: every triangle is clipped
// against its command's clip rectangle (Sutherland-Hodgman, 4 planes) with
// perspective-free linear interpolation of UV and color, and the resulting
// convex polygon is fanned back into triangles. Fully outside triangles are
// dropped, fully inside ones are copied unchanged.
#pragma once
#include <imgui.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace RC::Platform::NativeGui
{
    struct UVTri
    {
        float pos[3][2];
        float uv[3][2];
        float color[3][4]; // linear 0..1 (R, G, B, A)
    };

    struct Batch
    {
        ImTextureID texture{};
        std::vector<UVTri> tris;
    };

    struct BatchStats
    {
        size_t commands{}, input_tris{}, output_tris{}, clipped{}, dropped{};
    };

    namespace detail
    {
        struct V
        {
            float x, y, u, v, c[4];
        };
        inline auto lerp(const V& a, const V& b, float t) -> V
        {
            V r;
            r.x = a.x + (b.x - a.x) * t;
            r.y = a.y + (b.y - a.y) * t;
            r.u = a.u + (b.u - a.u) * t;
            r.v = a.v + (b.v - a.v) * t;
            for (int i = 0; i < 4; ++i) r.c[i] = a.c[i] + (b.c[i] - a.c[i]) * t;
            return r;
        }
        // clip polygon against one axis-aligned plane: keep side where s*(coord - k) >= 0
        inline void clip_plane(std::vector<V>& poly, std::vector<V>& tmp, int axis, float k, float s)
        {
            tmp.clear();
            const size_t n = poly.size();
            for (size_t i = 0; i < n; ++i)
            {
                const V& a = poly[i];
                const V& b = poly[(i + 1) % n];
                const float da = s * ((axis == 0 ? a.x : a.y) - k);
                const float db = s * ((axis == 0 ? b.x : b.y) - k);
                if (da >= 0) tmp.push_back(a);
                if ((da >= 0) != (db >= 0))
                {
                    const float t = da / (da - db);
                    tmp.push_back(lerp(a, b, t));
                }
            }
            poly.swap(tmp);
        }
        inline auto srgb_to_linear(float c) -> float
        {
            return c <= 0.04045f ? c / 12.92f : static_cast<float>(std::pow(static_cast<double>((c + 0.055f) / 1.055f), 2.4)); // double pow: already imported (float pow would add a powf import; found by the target import-delta check)
        }
    } // namespace detail

    struct BatchOptions
    {
        bool srgb_to_linear_rgb = false; // ImGui colors are authored in sRGB; enable if the target expects linear
        float scale_x = 1.0f, scale_y = 1.0f;
    };

    inline auto convert(const ImDrawData* dd, BatchStats* stats = nullptr, BatchOptions opt = {}) -> std::vector<Batch>
    {
        std::vector<Batch> out;
        if (!dd || !dd->Valid) return out;
        BatchStats st{};
        std::vector<detail::V> poly, tmp;
        poly.reserve(16);
        tmp.reserve(16);
        const ImVec2 off = dd->DisplayPos;
        for (int l = 0; l < dd->CmdListsCount; ++l)
        {
            const ImDrawList* list = dd->CmdLists[l];
            const ImDrawVert* vtx = list->VtxBuffer.Data;
            const ImDrawIdx* idx = list->IdxBuffer.Data;
            for (const ImDrawCmd& cmd : list->CmdBuffer)
            {
                if (cmd.UserCallback) continue; // backend callbacks are not supported (none are used by mods)
                ++st.commands;
                const float cx0 = cmd.ClipRect.x - off.x, cy0 = cmd.ClipRect.y - off.y;
                const float cx1 = cmd.ClipRect.z - off.x, cy1 = cmd.ClipRect.w - off.y;
                if (cx1 <= cx0 || cy1 <= cy0) continue;
                const ImTextureID tex = cmd.GetTexID();
                if (out.empty() || out.back().texture != tex) out.push_back(Batch{tex, {}});
                auto& tris = out.back().tris;
                for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3)
                {
                    ++st.input_tris;
                    detail::V t[3];
                    float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
                    for (int k = 0; k < 3; ++k)
                    {
                        const ImDrawVert& dv = vtx[cmd.VtxOffset + idx[cmd.IdxOffset + i + k]];
                        t[k].x = dv.pos.x - off.x;
                        t[k].y = dv.pos.y - off.y;
                        t[k].u = dv.uv.x;
                        t[k].v = dv.uv.y;
                        const ImU32 c = dv.col;
                        t[k].c[0] = ((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                        t[k].c[1] = ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                        t[k].c[2] = ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                        t[k].c[3] = ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
                        if (opt.srgb_to_linear_rgb)
                            for (int ch = 0; ch < 3; ++ch) t[k].c[ch] = detail::srgb_to_linear(t[k].c[ch]);
                        minx = std::fmin(minx, t[k].x); maxx = std::fmax(maxx, t[k].x);
                        miny = std::fmin(miny, t[k].y); maxy = std::fmax(maxy, t[k].y);
                    }
                    auto emit = [&](const detail::V& a, const detail::V& b, const detail::V& c) {
                        UVTri u{};
                        const detail::V* p[3] = {&a, &b, &c};
                        for (int k = 0; k < 3; ++k)
                        {
                            u.pos[k][0] = p[k]->x * opt.scale_x;
                            u.pos[k][1] = p[k]->y * opt.scale_y;
                            u.uv[k][0] = p[k]->u;
                            u.uv[k][1] = p[k]->v;
                            for (int ch = 0; ch < 4; ++ch) u.color[k][ch] = p[k]->c[ch];
                        }
                        tris.push_back(u);
                        ++st.output_tris;
                    };
                    if (minx >= cx0 && maxx <= cx1 && miny >= cy0 && maxy <= cy1)
                    {
                        emit(t[0], t[1], t[2]);
                        continue;
                    }
                    if (maxx <= cx0 || minx >= cx1 || maxy <= cy0 || miny >= cy1)
                    {
                        ++st.dropped;
                        continue;
                    }
                    ++st.clipped;
                    poly.assign(t, t + 3);
                    detail::clip_plane(poly, tmp, 0, cx0, +1.0f);
                    if (poly.size() >= 3) detail::clip_plane(poly, tmp, 0, cx1, -1.0f);
                    if (poly.size() >= 3) detail::clip_plane(poly, tmp, 1, cy0, +1.0f);
                    if (poly.size() >= 3) detail::clip_plane(poly, tmp, 1, cy1, -1.0f);
                    for (size_t k = 1; k + 1 < poly.size(); ++k) emit(poly[0], poly[k], poly[k + 1]);
                }
            }
        }
        // drop empty batches
        std::vector<Batch> compact;
        for (auto& b : out)
            if (!b.tris.empty())
            {
                if (!compact.empty() && compact.back().texture == b.texture)
                    compact.back().tris.insert(compact.back().tris.end(), b.tris.begin(), b.tris.end());
                else
                    compact.push_back(std::move(b));
            }
        if (stats) *stats = st;
        return compact;
    }
} // namespace RC::Platform::NativeGui

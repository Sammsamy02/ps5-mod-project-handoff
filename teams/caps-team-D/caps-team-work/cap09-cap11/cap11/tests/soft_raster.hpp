// soft_raster.hpp -- test-only software rasteriser for UVTri batches and for raw
// ImDrawData (reference path with per-pixel scissor). Pixel-centre sampling,
// nearest-texel atlas lookup, straight alpha blending. Never part of a build.
#pragma once
#include <Platform/ImGuiCanvasBatcher.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

struct SoftImage
{
    int w{}, h{};
    std::vector<float> rgba; // straight alpha
    SoftImage(int W, int H) : w(W), h(H), rgba(static_cast<size_t>(W) * H * 4, 0.0f) {}
};

struct SoftTexture
{
    int w{}, h{};
    const std::vector<uint8_t>* alpha{}; // white texels, coverage in alpha
    float sample(float u, float v) const
    {
        if (!alpha || w <= 0 || h <= 0) return 1.0f;
        int x = static_cast<int>(std::floor(u * w));
        int y = static_cast<int>(std::floor(v * h));
        x = x < 0 ? 0 : (x >= w ? w - 1 : x);
        y = y < 0 ? 0 : (y >= h ? h - 1 : y);
        return (*alpha)[static_cast<size_t>(y) * w + x] / 255.0f;
    }
};

inline void soft_tri(SoftImage& img, const RC::Platform::NativeGui::UVTri& t, const SoftTexture& tex, const float* scissor /* x0,y0,x1,y1 or null */)
{
    const float x0 = t.pos[0][0], y0 = t.pos[0][1], x1 = t.pos[1][0], y1 = t.pos[1][1], x2 = t.pos[2][0], y2 = t.pos[2][1];
    const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (std::fabs(area) < 1e-12f) return;
    int minx = static_cast<int>(std::floor(std::fmin(x0, std::fmin(x1, x2))));
    int maxx = static_cast<int>(std::ceil(std::fmax(x0, std::fmax(x1, x2))));
    int miny = static_cast<int>(std::floor(std::fmin(y0, std::fmin(y1, y2))));
    int maxy = static_cast<int>(std::ceil(std::fmax(y0, std::fmax(y1, y2))));
    minx = minx < 0 ? 0 : minx;
    miny = miny < 0 ? 0 : miny;
    maxx = maxx > img.w ? img.w : maxx;
    maxy = maxy > img.h ? img.h : maxy;
    for (int y = miny; y < maxy; ++y)
        for (int x = minx; x < maxx; ++x)
        {
            const float px = x + 0.5f, py = y + 0.5f;
            if (scissor && (px < scissor[0] || px >= scissor[2] || py < scissor[1] || py >= scissor[3])) continue;
            float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
            float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
            float w2 = 1.0f - w0 - w1;
            const float eps = -1e-6f;
            if (w0 < eps || w1 < eps || w2 < eps) continue;
            const float u = w0 * t.uv[0][0] + w1 * t.uv[1][0] + w2 * t.uv[2][0];
            const float v = w0 * t.uv[0][1] + w1 * t.uv[1][1] + w2 * t.uv[2][1];
            float c[4];
            for (int k = 0; k < 4; ++k) c[k] = w0 * t.color[0][k] + w1 * t.color[1][k] + w2 * t.color[2][k];
            const float a = c[3] * tex.sample(u, v);
            float* d = &img.rgba[(static_cast<size_t>(y) * img.w + x) * 4];
            for (int k = 0; k < 3; ++k) d[k] = c[k] * a + d[k] * (1.0f - a);
            d[3] = a + d[3] * (1.0f - a);
        }
}

inline void soft_batches(SoftImage& img, const std::vector<RC::Platform::NativeGui::Batch>& batches, const SoftTexture& tex)
{
    for (const auto& b : batches)
        for (const auto& t : b.tris) soft_tri(img, t, tex, nullptr);
}

// Reference: raw draw data, scissor per command, no CPU clipping.
inline void soft_reference(SoftImage& img, const ImDrawData* dd, const SoftTexture& tex)
{
    using RC::Platform::NativeGui::UVTri;
    for (int l = 0; l < dd->CmdListsCount; ++l)
    {
        const ImDrawList* list = dd->CmdLists[l];
        for (const ImDrawCmd& cmd : list->CmdBuffer)
        {
            if (cmd.UserCallback) continue;
            const float sc[4] = {cmd.ClipRect.x - dd->DisplayPos.x, cmd.ClipRect.y - dd->DisplayPos.y, cmd.ClipRect.z - dd->DisplayPos.x,
                                 cmd.ClipRect.w - dd->DisplayPos.y};
            for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3)
            {
                UVTri t{};
                for (int k = 0; k < 3; ++k)
                {
                    const ImDrawVert& v = list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i + k]];
                    t.pos[k][0] = v.pos.x - dd->DisplayPos.x;
                    t.pos[k][1] = v.pos.y - dd->DisplayPos.y;
                    t.uv[k][0] = v.uv.x;
                    t.uv[k][1] = v.uv.y;
                    t.color[k][0] = ((v.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                    t.color[k][1] = ((v.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                    t.color[k][2] = ((v.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                    t.color[k][3] = ((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
                }
                soft_tri(img, t, tex, sc);
            }
        }
    }
}

// fraction of pixels whose colour differs by more than tol in any channel
inline double soft_diff(const SoftImage& a, const SoftImage& b, float tol, double* max_err = nullptr)
{
    size_t bad = 0;
    double mx = 0;
    for (size_t i = 0; i < a.rgba.size(); i += 4)
    {
        float e = 0;
        for (int k = 0; k < 4; ++k) e = std::fmax(e, std::fabs(a.rgba[i + k] - b.rgba[i + k]));
        mx = std::fmax(mx, e);
        if (e > tol) ++bad;
    }
    if (max_err) *max_err = mx;
    return static_cast<double>(bad) / (a.rgba.size() / 4);
}

inline double soft_coverage(const SoftImage& a)
{
    size_t n = 0;
    for (size_t i = 3; i < a.rgba.size(); i += 4) n += a.rgba[i] > 0.01f;
    return static_cast<double>(n) / (a.rgba.size() / 4);
}

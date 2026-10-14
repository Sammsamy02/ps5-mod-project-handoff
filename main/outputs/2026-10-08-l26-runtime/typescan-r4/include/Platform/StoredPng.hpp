// StoredPng.hpp -- minimal RGBA8 PNG writer (uncompressed "stored" deflate
// blocks). Used once per session to hand the Dear ImGui font atlas to the
// engine's own image import (KismetRenderingLibrary ImportBufferAsTexture2D),
// so the atlas becomes a real UTexture2D with a correct alpha channel, without
// any texture-creation API of our own. Engine-free; host test decodes it with
// an independent decoder (Python zlib) and compares every pixel.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace RC::Platform::NativeGui
{
    namespace png_detail
    {
        inline auto crc_table() -> const std::array<uint32_t, 256>&
        {
            static const std::array<uint32_t, 256> t = [] {
                std::array<uint32_t, 256> r{};
                for (uint32_t n = 0; n < 256; ++n)
                {
                    uint32_t c = n;
                    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    r[n] = c;
                }
                return r;
            }();
            return t;
        }
        inline auto crc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) -> uint32_t
        {
            const auto& t = crc_table();
            for (size_t i = 0; i < n; ++i) c = t[(c ^ p[i]) & 0xFF] ^ (c >> 8);
            return c;
        }
        inline void be32(std::vector<uint8_t>& o, uint32_t v)
        {
            o.push_back(static_cast<uint8_t>(v >> 24));
            o.push_back(static_cast<uint8_t>(v >> 16));
            o.push_back(static_cast<uint8_t>(v >> 8));
            o.push_back(static_cast<uint8_t>(v));
        }
        inline void chunk(std::vector<uint8_t>& o, const char type[4], const std::vector<uint8_t>& data)
        {
            be32(o, static_cast<uint32_t>(data.size()));
            const size_t start = o.size();
            o.insert(o.end(), type, type + 4);
            o.insert(o.end(), data.begin(), data.end());
            be32(o, crc(o.data() + start, o.size() - start) ^ 0xFFFFFFFFu);
        }
    } // namespace png_detail

    // rgba: width*height*4 bytes, row-major, top row first.
    inline auto encode_png_rgba(const uint8_t* rgba, uint32_t width, uint32_t height) -> std::vector<uint8_t>
    {
        using namespace png_detail;
        std::vector<uint8_t> out;
        if (!rgba || width == 0 || height == 0 || width > 16384 || height > 16384) return out;
        static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        out.insert(out.end(), sig, sig + 8);
        std::vector<uint8_t> ihdr;
        be32(ihdr, width);
        be32(ihdr, height);
        ihdr.push_back(8); // bit depth
        ihdr.push_back(6); // colour type RGBA
        ihdr.push_back(0); // deflate
        ihdr.push_back(0); // filter method
        ihdr.push_back(0); // no interlace
        chunk(out, "IHDR", ihdr);
        // raw scanlines with filter byte 0
        std::vector<uint8_t> raw;
        raw.reserve(static_cast<size_t>(height) * (width * 4 + 1));
        for (uint32_t y = 0; y < height; ++y)
        {
            raw.push_back(0);
            const uint8_t* row = rgba + static_cast<size_t>(y) * width * 4;
            raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 4);
        }
        // zlib stream with stored blocks
        std::vector<uint8_t> z;
        z.push_back(0x78);
        z.push_back(0x01);
        size_t pos = 0;
        while (pos < raw.size() || raw.empty())
        {
            const size_t n = std::min<size_t>(65535, raw.size() - pos);
            const bool last = pos + n >= raw.size();
            z.push_back(last ? 1 : 0);
            z.push_back(static_cast<uint8_t>(n & 0xFF));
            z.push_back(static_cast<uint8_t>(n >> 8));
            z.push_back(static_cast<uint8_t>(~n & 0xFF));
            z.push_back(static_cast<uint8_t>((~n >> 8) & 0xFF));
            z.insert(z.end(), raw.begin() + static_cast<long>(pos), raw.begin() + static_cast<long>(pos + n));
            pos += n;
            if (last) break;
        }
        uint32_t a = 1, b = 0;
        for (uint8_t c : raw)
        {
            a = (a + c) % 65521u;
            b = (b + a) % 65521u;
        }
        be32(z, (b << 16) | a);
        chunk(out, "IDAT", z);
        chunk(out, "IEND", {});
        return out;
    }
} // namespace RC::Platform::NativeGui

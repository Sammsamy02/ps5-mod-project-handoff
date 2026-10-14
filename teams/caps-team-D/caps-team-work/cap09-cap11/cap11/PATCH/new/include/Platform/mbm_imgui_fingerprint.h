// mbm_imgui_fingerprint.h -- [FIX F7] layout/config fingerprint of the Dear
// ImGui build a module was compiled with. The GUI host (#11) publishes its own
// value in mbm_gui_v1::config_fingerprint (plus sizeof(ImGuiContext) in
// mbm_gui_v1::sizeof_context); the SDK computes the same value in the mod and
// refuses UE4SS_ENABLE_IMGUI() on any difference (fail closed: a mod built with
// another ImGui config would write into the host's ImGuiContext with a different
// layout and corrupt memory).
//
// Include AFTER imgui.h and imgui_internal.h, in the one unit that fills or
// checks mbm_gui_v1. Internal linkage on purpose: each module evaluates the
// macros exactly as its own unit sees them.
#pragma once
#include <cstddef>
#include <cstdint>

#ifndef IMGUI_VERSION_NUM
#error "include imgui.h (and imgui_internal.h) before mbm_imgui_fingerprint.h"
#endif

namespace mbm_imgui_fp
{
    // One bit per configuration macro that changes a shared structure, a
    // shared function's behaviour or which functions exist.
    static constexpr uint32_t config_bits()
    {
        uint32_t b = 0;
#ifdef IMGUI_DISABLE_DEBUG_TOOLS
        b |= 1u << 0;
#endif
#ifdef IMGUI_ENABLE_TEST_ENGINE
        b |= 1u << 1;
#endif
#ifdef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        b |= 1u << 2;
#endif
#ifdef IMGUI_USE_WCHAR32
        b |= 1u << 3;
#endif
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
        b |= 1u << 4;
#endif
#ifdef IMGUI_DISABLE_DEFAULT_ALLOCATORS
        b |= 1u << 5;
#endif
#ifdef IMGUI_DISABLE_FILE_FUNCTIONS
        b |= 1u << 6;
#endif
#ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
        b |= 1u << 7;
#endif
#ifdef IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
        b |= 1u << 8;
#endif
#ifdef IMGUI_DISABLE_SSE
        b |= 1u << 9;
#endif
#ifdef IMGUI_ENABLE_SSE
        b |= 1u << 10;
#endif
#ifdef IMGUI_DEFINE_MATH_OPERATORS
        b |= 1u << 11;
#endif
#ifdef IMGUI_DISABLE_DEMO_WINDOWS
        b |= 1u << 12;
#endif
#ifdef IMGUI_HAS_DOCK
        b |= 1u << 13;
#endif
#ifdef IMGUI_HAS_VIEWPORT
        b |= 1u << 14;
#endif
#ifdef IMGUI_ENABLE_FREETYPE
        b |= 1u << 15;
#endif
#ifdef IMGUI_ENABLE_STB_TRUETYPE
        b |= 1u << 16;
#endif
#ifdef IMGUI_USE_STB_SPRINTF
        b |= 1u << 17;
#endif
#ifdef IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
        b |= 1u << 18;
#endif
#ifdef IMGUI_DEBUG_PARANOID
        b |= 1u << 19;
#endif
#ifdef IMGUI_DISABLE_METRICS_WINDOW
        b |= 1u << 20;
#endif
#ifdef IMGUI_INCLUDE_IMGUI_USER_H
        b |= 1u << 21;
#endif
#ifdef IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT
        b |= 1u << 22;
#endif
#ifdef IM_VEC2_CLASS_EXTRA
        b |= 1u << 23;
#endif
#ifdef IM_VEC4_CLASS_EXTRA
        b |= 1u << 24;
#endif
#ifdef IMGUI_DISABLE
        b |= 1u << 25;
#endif
#ifdef ImTextureID
        b |= 1u << 26;
#endif
#ifdef ImDrawIdx
        b |= 1u << 27;
#endif
#ifdef IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS
        b |= 1u << 28;
#endif
        return b;
    }

    static constexpr uint32_t fnv1a(uint32_t h, uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
        {
            h ^= static_cast<uint32_t>((v >> (8 * i)) & 0xFFu);
            h *= 16777619u;
        }
        return h;
    }

    // Config bits + the version + the sizes/alignments of every structure the
    // host and a mod both read or write through the shared context.
    static constexpr uint32_t fingerprint()
    {
        uint32_t h = 2166136261u;
        const uint64_t parts[] = {
            config_bits(),
            static_cast<uint64_t>(IMGUI_VERSION_NUM),
            sizeof(ImGuiContext), alignof(ImGuiContext),
            sizeof(ImGuiIO), sizeof(ImGuiStyle), sizeof(ImGuiPlatformIO),
            sizeof(ImGuiViewport), sizeof(ImGuiWindow), sizeof(ImGuiTable),
            sizeof(ImGuiStorage), sizeof(ImGuiTextBuffer), sizeof(ImGuiListClipper),
            sizeof(ImDrawList), sizeof(ImDrawCmd), sizeof(ImDrawData), sizeof(ImDrawVert),
            sizeof(ImDrawIdx), sizeof(ImFontAtlas), sizeof(ImFont), sizeof(ImFontBaked),
            sizeof(ImFontConfig), sizeof(ImTextureData), sizeof(ImTextureRef),
            sizeof(ImTextureID), sizeof(ImWchar), sizeof(ImVec2), sizeof(ImVec4),
            sizeof(ImGuiKeyData), sizeof(ImGuiPayload),
        };
        for (uint64_t v : parts) h = fnv1a(h, v);
        return h;
    }
} // namespace mbm_imgui_fp

#define MBM_IMGUI_CONFIG_FINGERPRINT() (::mbm_imgui_fp::fingerprint())

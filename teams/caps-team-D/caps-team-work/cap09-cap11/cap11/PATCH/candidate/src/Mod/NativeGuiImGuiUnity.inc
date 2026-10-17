// NativeGuiImGuiUnity.inc -- Dear ImGui 1.92.1 (pinned, MIT) compiled as ONE
// unity block inside an EXISTING runtime unit (the build replaces units, it does
// not add new ones). Also used verbatim by the SDK for mods (mbm_sdk_imgui.cpp).
//
// Three C functions ImGui calls directly (strncpy in ImStrncpy, strncmp in
// InputText, sqrtf in the atlas packer) are not in the runtime's import list;
// they are routed to local equivalents here AFTER the C headers are included
// (so libc declarations stay untouched), keeping the runtime's import set
// unchanged (tests/import_delta.py checks this).
// Requires: -DIMGUI_USER_CONFIG="Platform/mbm_imconfig.h" and the pinned
// imgui source directory on the include path.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

namespace mbm_imgui_libc
{
    inline char* strncpy_(char* dst, const char* src, size_t n)
    {
        size_t i = 0;
        for (; i < n && src[i]; ++i) dst[i] = src[i];
        for (; i < n; ++i) dst[i] = '\0';
        return dst;
    }
    inline int strncmp_(const char* a, const char* b, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            const unsigned char ca = static_cast<unsigned char>(a[i]), cb = static_cast<unsigned char>(b[i]);
            if (ca != cb) return ca < cb ? -1 : 1;
            if (!ca) return 0;
        }
        return 0;
    }
} // namespace mbm_imgui_libc

#define strncpy ::mbm_imgui_libc::strncpy_
#define strncmp ::mbm_imgui_libc::strncmp_
#define sqrtf ::mbm_imgui_math::sqrt_f // two direct calls in the atlas packer
#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"
#include "imgui_tables.cpp"
#undef strncpy
#undef strncmp
#undef sqrtf

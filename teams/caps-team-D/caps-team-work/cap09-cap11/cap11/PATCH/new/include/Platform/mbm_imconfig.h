// mbm_imconfig.h -- the ONE Dear ImGui configuration used by the PS5 Mod Bridge
// GUI host (#11) and by every SDK-built native mod. Both sides compile the same
// pinned Dear ImGui 1.92.1 sources with -DIMGUI_USER_CONFIG="<this file>", so
// ImGuiContext / ImGuiIO / ImDrawVert layouts are identical; the SDK checks a
// size fingerprint at UE4SS_ENABLE_IMGUI() and refuses on any difference.
#pragma once

#define IMGUI_DISABLE_FILE_FUNCTIONS          // no fopen/fwrite: no imgui.ini, no file imports
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS // no ShellExecute / open URL
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_OSX_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS      // every module must use the host allocator (SetAllocatorFunctions)
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DEFINE_MATH_OPERATORS           // [fixer] imgui.h is first included by the unit's GUI headers; imgui.cpp needs the ImVec2 operators from that first inclusion
#define IMGUI_DISABLE_SSE                     // [fixer] the PS5 payload sysroot has no <immintrin.h> (first target compile); scalar paths only
#define IMGUI_DISABLE_DEBUG_TOOLS             // no Metrics/Debug Log/ID-stack tool windows
#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS  // math on already-imported C functions only:
#include "mbm_imgui_math.h"

// A failed IM_ASSERT never aborts the game: it is recorded and logged once per
// site (host: [GUI] assert ...; mod: through the host log). Recoverable user
// errors are handled by ImGui's own error recovery (ConfigErrorRecovery).
void mbm_imgui_assert_failed(const char* expr, const char* file, int line);
#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : mbm_imgui_assert_failed(#_EXPR, __FILE__, __LINE__))

// badctx_imconfig.h -- TEST ONLY (review finding F7). The SDK configuration
// (Platform/mbm_imconfig.h) WITHOUT IMGUI_DISABLE_DEBUG_TOOLS and with
// IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS: the reviewer's scenario of a mod built
// with another ImGui config. ImGuiIO / ImGuiStyle / ImVec2 / ImVec4 / ImDrawVert
// / ImDrawIdx keep their sizes, so the OLD six-size check passed it, but the
// ImGuiContext layout differs (debug-tool members). The F7 check must refuse it.
#pragma once
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_OSX_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_SSE
/* IMGUI_DISABLE_DEBUG_TOOLS deliberately NOT defined */
#define IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS
#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
#include "Platform/mbm_imgui_math.h"
void mbm_imgui_assert_failed(const char* expr, const char* file, int line);
#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : mbm_imgui_assert_failed(#_EXPR, __FILE__, __LINE__))

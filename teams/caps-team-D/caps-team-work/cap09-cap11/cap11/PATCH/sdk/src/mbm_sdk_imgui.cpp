// mbm_sdk_imgui.cpp -- the mod's own copy of the pinned Dear ImGui 1.92.1, built
// with the SAME configuration as the host (mbm_imconfig.h) so the context
// layout matches. Link exactly once per GUI mod, together with
// mbm_sdk_entry.cpp compiled with -DMBM_SDK_WITH_IMGUI. Build with
// -fvisibility=hidden so these ImGui symbols never interpose with the host's.
#include "NativeGuiImGuiUnity.inc"

// UE4SSProgram.hpp -- PS5 Mod Bridge native mod SDK.
// Only what C++ mods actually use from upstream UE4SSProgram.hpp:
// UE4SS_ENABLE_IMGUI() (share the host's Dear ImGui context, #11).
#pragma once
#include <mbm/Sdk.hpp>

namespace RC::Sdk
{
    // Implemented in mbm_sdk_imgui.cpp (#11). Returns false (and logs) when the
    // host has no GUI or the ImGui version / data layout differs.
    auto enable_imgui() -> bool;
}

#define UE4SS_ENABLE_IMGUI() ::RC::Sdk::enable_imgui()

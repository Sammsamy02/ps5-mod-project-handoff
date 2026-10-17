// Host-test stub of the port's NativePadInput surface consumed by the patched
// ue4ss_backend.cpp (retained-pad configuration is console-only).
#pragma once

#include <Platform/NativeModSource.hpp>
#include <memory>

namespace RC::Input { class PlatformInputSource; }

namespace modbridge::native
{
    inline void ConfigureRetainedPS5PadInput(const RC::Platform::NativeModSource&,
                                             const std::shared_ptr<RC::Input::PlatformInputSource>&) {}
}

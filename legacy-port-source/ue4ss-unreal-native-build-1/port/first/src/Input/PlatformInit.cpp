#include <Input/PlatformInputSource.hpp>
#ifdef _WIN32
#include <Input/Platform/Win32AsyncInputSource.hpp>
#include <Input/Platform/GLFW3InputSource.hpp>
#endif

namespace RC::Input
{
    auto Handler::init() -> void
    {
#ifndef _WIN32
        init(nullptr);
#else
        register_input_source(std::make_shared<Win32AsyncInputSource>(L"ConsoleWindowClass", L"UnrealWindow"));
        register_input_source(std::make_shared<GLFW3InputSource>());
#endif
    }
} // namespace RC::Input
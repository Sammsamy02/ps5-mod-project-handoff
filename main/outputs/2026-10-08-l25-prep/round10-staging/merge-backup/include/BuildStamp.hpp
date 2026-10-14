#pragma once

// =====================================================================================
// [BUILD] stamp declaration (agent-FF, l26 build-discipline; Claude directive).
// One narrow stamp line, emitted by UnrealInitializer::Initialize as the FIRST
// log line of the initializer startup — before PreInitialize/ScanGame and every
// [Seeds]/[L1x]/[BuildProfile] line:
//
//     [BUILD] id=<..> changes=<..> snapshot=<..>
//
// Defined in src/BuildStamp.cpp. The real id/changes/snapshot come from the
// FG-generated include/GeneratedBuildStamp.hpp (frozen tar snapshot); when
// that generated header is absent, BuildStamp.cpp falls back to an explicit
// UNKNOWN form that self-identifies as not-uploadable, so an unstamped binary
// is evident on console.
//
// Include note: candidate/include itself is not on the -I set (only its
// subdirectories are), so TUs in candidate/src include this header via the
// file-relative form  #include "../include/BuildStamp.hpp".
// =====================================================================================

namespace RC
{
    extern const char* const kBuildStamp;
}

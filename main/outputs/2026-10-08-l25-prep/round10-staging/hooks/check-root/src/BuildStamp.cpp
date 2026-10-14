// =====================================================================================
// [BUILD] stamp definition (agent-FF, l26 build-discipline; Claude directive).
// Consumed by UnrealInitializer::Initialize (first initializer log line).
//
// Real values come from the FG-generated GeneratedBuildStamp.hpp (frozen tar
// snapshot id). Lookup order (first hit wins, all deterministic):
//   1. <GeneratedBuildStamp.hpp>          — the build added candidate/include
//                                           (or the generated dir) to -I;
//   2. "GeneratedBuildStamp.hpp"          — same-directory (candidate/src) or
//                                           any -I dir;
//   3. "../include/GeneratedBuildStamp.hpp" — FG's documented location
//                                           candidate/include/, file-relative,
//                                           zero -I changes required.
// When none is present (today: the generator does not exist yet) the fallback
// names UNKNOWN explicitly: "(generated header missing — do not upload)".
//
// Generated-header contract (for agent FG): it must define the three values as
// NARROW STRING LITERALS, primary names RC_BUILD_ID / RC_CHANGE_IDS /
// RC_SNAPSHOT_ID (bare BUILD_ID / CHANGE_IDS / SNAPSHOT_ID also accepted). A
// present-but-misnamed generated header fails the compile on purpose (never a
// silent UNKNOWN upload).
// =====================================================================================

#include "../include/BuildStamp.hpp"

#if __has_include(<GeneratedBuildStamp.hpp>)
    #include <GeneratedBuildStamp.hpp>
    #define RC_BS_GENERATED 1
#elif __has_include("GeneratedBuildStamp.hpp")
    #include "GeneratedBuildStamp.hpp"
    #define RC_BS_GENERATED 1
#elif __has_include("../include/GeneratedBuildStamp.hpp")
    #include "../include/GeneratedBuildStamp.hpp"
    #define RC_BS_GENERATED 1
#endif

#ifdef RC_BS_GENERATED
    #if defined(RC_BUILD_ID)
        #define RC_BS_ID RC_BUILD_ID
    #elif defined(BUILD_ID)
        #define RC_BS_ID BUILD_ID
    #else
        #error "GeneratedBuildStamp.hpp is present but defines neither RC_BUILD_ID nor BUILD_ID (string literal required)"
    #endif

    #if defined(RC_CHANGE_IDS)
        #define RC_BS_CHANGES RC_CHANGE_IDS
    #elif defined(CHANGE_IDS)
        #define RC_BS_CHANGES CHANGE_IDS
    #else
        #error "GeneratedBuildStamp.hpp is present but defines neither RC_CHANGE_IDS nor CHANGE_IDS (string literal required)"
    #endif

    #if defined(RC_SNAPSHOT_ID)
        #define RC_BS_SNAPSHOT RC_SNAPSHOT_ID
    #elif defined(SNAPSHOT_ID)
        #define RC_BS_SNAPSHOT SNAPSHOT_ID
    #else
        #error "GeneratedBuildStamp.hpp is present but defines neither RC_SNAPSHOT_ID nor SNAPSHOT_ID (string literal required)"
    #endif

    namespace RC
    {
        const char* const kBuildStamp = "[BUILD] id=" RC_BS_ID " changes=" RC_BS_CHANGES " snapshot=" RC_BS_SNAPSHOT;
    }
#else
    namespace RC
    {
        const char* const kBuildStamp =
            "[BUILD] id=UNKNOWN changes=UNKNOWN snapshot=UNKNOWN (generated header missing \xE2\x80\x94 do not upload)";
    }
#endif

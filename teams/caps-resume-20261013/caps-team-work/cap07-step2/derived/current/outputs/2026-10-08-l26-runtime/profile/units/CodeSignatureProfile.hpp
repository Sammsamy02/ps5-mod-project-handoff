#pragma once
#include "CodeImageCache.hpp"
#include "PakMountCodeProfile.hpp"
#include "ThreadNameProfile.hpp"
#include "FNameAbiProfile.hpp"
#include "SeedTable.h"
#include "EngineListenerOpsProfile.hpp"
#include "EngineToStringProfile.hpp"
// Generated retained Townfall profile inputs; path is configured, not observed.
namespace rc::codeprofile {
namespace seeds=RC::Unreal::UnrealInitializer::SeedTable;
inline constexpr uint8_t kScoPrefix[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x58,0x02,0x00,0x00,0x48,0x8b,0x05,0x4d,0x14};
// [Seeds][L26/L27 pin admission, agent-EQ 2026-10-09] rc::codecache::
// named_entry admits a code address ONLY when a registered pin matches the
// exact (runtime address, size) pair, and admission (L13AdmissionOps::
// code_identity -> bridge_code_cache_context -> Cache::entry) queries that
// pin registry — never a raw segment-bounds test. The 2026-10-09 11:36
// console run proved the omission this closes: slot 2 (0x63D590, pin 1) and
// slot 4 SCO (0x890AA0, pin 2) passed code_identity, but the L27 ToString
// bind (0x63FA00) was refused 'constructor startup image identity/base
// unavailable or mismatched' and Phase 6's engine delete-listener ops
// (0x8841A0/0x884240) hit the same unregistered-pin failure inside
// seed_refused (silent throw; only the L15 binding-log destructor line
// appeared before teardown). Registering the three profile-pinned labels
// here gives them the SAME startup-authenticated, whole-image-sha-bound
// prefix pins slots 2/4 already use. Bytes are single-sourced from the
// L26/L27 profile headers (same sha-pinned image 3b6cfad4...; both headers
// byte-verified against the pinned eboot at file offsets label+0x4000).
inline constexpr codecache::Pin kPins[]={
 rc::pakmountprofile::kPins[0],
 rc::pakmountprofile::kPins[1],
 rc::pakmountprofile::kPins[2],
 rc::pakmountprofile::kPins[3],
 rc::pakmountprofile::kPins[4],
 rc::pakmountprofile::kPins[5],

 {1,"fname_ctor_wchar",seeds::kFNameCtorWchar.label,fnameprofile::kPrefix,sizeof(fnameprofile::kPrefix)},
 {2,"static_construct_object_internal",seeds::kStaticConstructObject.label,kScoPrefix,sizeof(kScoPrefix)},
 NativeNameThread::kNamePin,
 {4,"engine_fname_tostring_internal",rc::engtostring::kToStringInternalLabel,rc::engtostring::kToStringInternalPrefix,sizeof(rc::engtostring::kToStringInternalPrefix)},
 {5,"engine_add_uobject_delete_listener",rc::englistener::kAddDeleteListenerLabel,rc::englistener::kAddPrefix,sizeof(rc::englistener::kAddPrefix)},
 {6,"engine_remove_uobject_delete_listener",rc::englistener::kRemoveDeleteListenerLabel,rc::englistener::kRemovePrefix,sizeof(rc::englistener::kRemovePrefix)}
};
inline constexpr codecache::Profile kProfile{
 "/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin",
 seeds::kImageSha256.data(),194241424ull,0xbc602c8ull,seeds::kBaseObserved,kPins,12
};
}

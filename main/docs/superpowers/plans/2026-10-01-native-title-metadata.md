# Native title metadata decoder Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use checkbox syntax.

**Goal:** Decode captured PS5 title metadata using a reusable native format interface without assuming a commercial title prefix, installed state or build acceptance.

**Architecture:** A pure bounded `param.json` decoder exposes owning fixed C POD fields. Source capture, filesystem permissions, title enumeration and immutable paging belong to separate owners; the decoder performs no I/O. Descriptive version/name and metadata-file digest never become a build fingerprint.

**Tech Stack:** Existing pinned nlohmann JSON/picosha2 sources and ordinary pinned PS5 SDK C++ runtime, focused C caller + C++ implementation tests, compile/link-only native evidence.

**Spec:** `docs/superpowers/specs/ps5-title-metadata-primary-source-investigation.md`, pinned root-field param.json profile. The AppInfoJson database profile and PS4 SFO are outside this slice.

## Constraints / review focus

- Root object with required `titleId`: bounded ASCII `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`. No PPSA/CUSA-only branches; authored SDK `FAKE02932` must decode.
- Optional root string `contentVersion`, maximum128 UTF-8 bytes. Missing version is unavailable; `masterVersion`/filename/Nexus name cannot substitute.
- Optional object `localizedParameters`, containing optional `defaultLanguage` and the corresponding locale object `titleName`. Use only the explicitly declared default locale. Missing locale/name remains unavailable; malformed present selected fields fail. No silent English/first-match guess.
- Entire input: at most65536 bytes, valid UTF-8, duplicate JSON keys rejected, depth<=16, total values+keys<=4096, individual parsed strings<=4096 bytes. Use bounded SAX validation/extraction with strict native exception/allocator failure handling. The pinned nlohmann DOM destructor allocates inside a noexcept path; an injected allocation failure exposed SIGABRT, so owning production extraction must avoid that path without changing vendor bytes.
- ABI-v1 exact headers/reserved fields; owning NUL-terminated fixed fields title ID65, name257, version129, locale65, metadata SHA25665. Full captured metadata hash may be reported as metadata identity only. Source-size/scoped availability flags are scalars.
- All installed/running/build-fingerprint/engine-acceptance/runtime/install/authenticity flags zero. Return no platform evidence from descriptive labels or a JSON file digest.
- Invalid backing/ABI/known overlap is no-write preflight. Operational parse/schema/limit/failure clears output positive flags and owned fields, with only safe constant reason/status. No retained pointers, source mutation, filesystem, dynamic loading or account/device calls.
- C++ parsing may allocate within strict input bounds; don't claim allocation-free. SDK compilation is not native runtime proof.

## Task — Sol native

Own only `outputs/ps5-mod-bridge/native-host/title/{metadata.h,metadata.cpp,README.md,verify.py,tests/*}` and `.superpowers/native-observations/native-title-metadata-implementation.md`. Root owns Makefile/host/provider/Git integration. Other agents own shared UI/HTTP; leave those files alone.

- [x] Define minimal exact v1 API/types and send names to root. Use clear status/reason enums; keep output fields fixed and owning.
- [x] Write focused compiled behavioral RED using a missing implementation stub. Include actual pinned SDK-authored JSON, unrelated title IDs/locales, and synthetic versions; no copied private console names.
- [x] Implement bounded strict decoder and metadata-only identity with optional fields and whole-output failure clearing.
- [x] Run focused GREEN for default locale selection, absent metadata, misleading masterVersion/prefix, bad UTF-8/duplicate/deep/oversize JSON, malformed present fields, exact string/input limits, aliases/ABI, metadata digest comparison, allocation failure and clearing an earlier positive result.
- [x] Compile/link fresh C caller and current implementation for pinned SDK using verified local C++ includes/libraries only. Never execute/deploy; record any ordinary build limitation without tools/dependency changes.
- [x] Record exact host and compile-only evidence and freeze ownership. No broad matrices, sanitizer/review loop or device/network/account/game/system/Git operations.

## Follow-through

- [x] Root verifies and commits the decoder separately from actual source capture/paged title inventory. Default app provider stays unavailable until that physical seam is proved.

Verified checkpoint: five host groups/197 assertions passed, allocation injections at first/middle/final attempts cleared the whole result and recovered, and the actual C ABI fixture passed. Root passed `make test-title`. Fresh two-TU PS5 compile/link verified actual used SDK/runtime/resource bytes and stayed unexecuted. Source capture, enumeration, paging and real provider remain separate work.

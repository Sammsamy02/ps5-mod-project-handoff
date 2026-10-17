# Automatic downloaded INI preparation

The actual application can now select a strict PAK or INI preparation handler
from its downloaded archive inventory. A new `download_prepare` job keeps the
existing five Nexus source/idempotency fields. Filename counts choose a handler
only; the selected core independently rescans and validates the original
archive. Unknown/multiple classes remain pending without a prepared artifact.

The new resident INI route reuses archive intake, unique Engine.ini selection,
journaled materialization and automatic config-v2 conversion. It derives the
project's PS5 config path, mount and hash seed from the captured base rather
than receiving mod-specific target fields. The original download/member are
unchanged. A single SystemSettings scalar source is admitted; other regular
archive files and source headers outside that section require another handler.
No optional README or component-only exception is inferred automatically.

The config core's additive operation-gated v1/v2 entrypoints preserve old
layouts and symbols. One latched caller predicate follows callbacks, bounded
phase/row work and the PAK writer. Allocator cleanup cannot be cancelled; final
completion follows cleanup and the last gate. Blocking callbacks and bounded
parser/selector/merge/hash calls remain cooperative.

## Verified local results

- The actual owned HTTPS app test passed **26 checks across seven archive
  requests**, including manual and automatic preparation of the unchanged
  published **582-byte Engine RAR**. Both produced the same **297,483,249-byte
  PAK**, SHA256 `8bcf5eb17599e40a02575943eeb6515e7c70e669a74fc962f2d6cbd32aea8d58`.
- Independent pinned repak decoding compared **4,699 payload paths/hashes**
  with the retained config artifact: every row matched. The shared merge made
  30 scalar winners, 2 replacements and 28 additions, yielding 3,626 config
  bytes. The previously verified oracle preserves the other 4,698 base payloads.
- A mixed real ZIP reports two unhandled members without output. An unselected
  INI section, duplicate candidate paths, wrong-game profile and unknown DLL
  archive produce no prepared artifact. Idempotency, kind conflicts, original
  source identities, credential-free archive requests, private-path/secret
  exclusions and shutdown cleanup passed.
- Focused config gate controls passed **8 cases / 68 assertions**: pre-I/O
  stop, post-read stop, a real 130-byte partial output, final-release deadline
  expiry and unchanged v1/v2 output. Every admitted allocation was released.
- The existing PAK job test ran unchanged against the new app and passed
  **25 checks**; its independent union oracle reported zero mismatches.
- The matching native application compiled and linked: **2,524,976 bytes**,
  SHA256 `258e694231d30f482282134a9d2b2ec022d0e9ffdaedbe04898d2a4b65114422`.
  Its 105 sources, 41 objects and recorded application/dependency headers match
  current files, with no host headers. PT_DYNAMIC matches SHT_DYNAMIC at 336
  bytes / 21 entries, ending in DT_NULL. Imports remain unchanged at 136; SHA1,
  SHA256 and zeroize retain their single intended providers. Native execution
  was not attempted.
- All **254 paused baseline files** remained byte-identical. No console,
  live account, game or system files were contacted or changed.

Evidence lives under `.superpowers/native-dependency-spike/`:

- `native-config-prepare-jobs-1/green-host-2-final/receipt.json`
- `native-config-prepare-jobs-1/pak-owner-regression-1/receipt.json`
- `config-file-job-operation-gates-1/green-1/receipt.json`
- `native-config-prepare-jobs-1/configured-native-1/loopback-download/manifest.json`
- `native-config-prepare-jobs-1/native-artifact-audit-1/receipt.json`

## Practical limits and next work

This proves unchanged-download-to-private-byte-preparation for the admitted
classic PAK/config subset. It does not establish game loading, PS5 asset cooking
or recognition/effects of every CVar, including Windows-specific keys copied
by the scalar merger. The captured-base profile is development data, with a
Mac file path; it is not on-console game discovery or a launch-ready profile.

UE4SS still needs a real engine provider and game-thread connection. Owned Lua
bindings do not provide those. DLL semantics, automatic prerequisites, free
download handoff, durable publication/recovery and the complete console Nexus
store/login UI remain unfinished. The current UI still submits analysis jobs;
these preparation intents are available through the session-protected job API.

The broader goal remains active. Next eligible local work is to connect an
existing container handler to the same original-download workflow, or close a
concrete engine-port gap that does not retry the stopped native placement,
mapbase memory-read or curl-upgrade actions. No claimed universal compatibility
follows from these successful fixtures.

Queue records currently prepare against the original base independently, so
they do not yet compose multiple mods or resolve conflicts. Installing several
separate full-base rebuilds could mask earlier changes. A shared batch composer
must combine changes before any future publication.

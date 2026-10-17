# Native jobs: downloaded archive to private format preparation

The optional preparation profile joins the actual Nexus download job to the
existing archive/PAK converter. The original downloaded ZIP/RAR/7z bytes enter
the callable preparer through the exact reopened read-only download FD. There
is no individual-mod rewrite, subprocess, AI call or expected output input.

The PAK route currently admits one standalone classic unsigned, unencrypted v3 None
mod PAK against an immutable captured v11 None base, both at most 512 MiB. The
base's mount, paths, seed and file contents are derived from that base. This
does not establish target discovery, platform asset conversion or game loading.
The INI route admits a single Engine.ini whose scalar source settings belong
to SystemSettings. The separate IoStore profile admits constrained None/v8
containers through the existing source-preserving composer. UE4SS, DLL and
prerequisite runtime paths remain unfinished.

Supply `--pak-profile` alongside the existing download/OAuth/archive jobs build
arguments. Its JSON contains exactly `base_path`, `base_bytes`, `base_sha256`,
`nexus_game`, `timeout_ms`. The base path is an explicit canonical absolute path,
its expected SHA256 is independently checked at app startup, and its read-only
descriptor and immutable backing remain borrowed until successful jobs cleanup.
The Nexus domain is an explicit source admission binding, never proof that a
console installation or game version matches. Missing or different-game profiles
remain unresolved. A larger successfully downloaded archive retains analysis
but reports the separate preparation limit.

The session/CSRF/Origin protected `/api/jobs` accepts the same five flat source
fields, with six kinds:

* `download_analyze`: existing download/analysis only.
* `download_prepare_pak`: default whole-bundle policy. Any extra regular members
  refuse preparation before materialization/output acquisition.
* `download_prepare_pak_component`: explicit component preparation. All regular
  members remain in the private journal; extras are reported as unhandled and
  `whole_bundle_handled` remains false.
* `download_prepare_ini`: strict single-config archive preparation. Reuses
  archive scan, Engine.ini basename selection and journaled materialization.
  Any other regular file is reported as unhandled before materialization or
  output acquisition. All source headers must be SystemSettings; an unselected
  section refuses preparation rather than being silently ignored. The shared
  scalar parser still rejects unsupported arrays or invalid syntax. No component
  policy or inferred optional README applies to this route.
* `download_prepare_iostore`: one strict IoStore group, using a separate
  captured TOC/header/observed UCAS length profile. Empty PAK companions and
  bounded plaintext readme documents are explicitly content-verified. Other
  regular members, material PAK companions and multiple groups refuse.
* `download_prepare`: automatic selection of the strict IoStore, PAK or INI route.
  The retained filename inventory provides a routing hint; the chosen core
  rescans and validates the original archive before acquiring output. A unique
  IoStore hint is tried first, including its companion PAK; otherwise a unique
  PAK or unique INI is tried. Multiple/unknown source classes need another handler and acquire no
  output. Automatic dispatch never enables the component-only exception or
  treats a filename as compatibility evidence.

For INI preparation, the existing automatic config-v2 job finds one project
root and one PS5 platform Engine.ini from the complete validated captured PAK
inventory. It derives the writer mount and seed from that same base. Neither
the downloaded mod nor the app supplies a project target path, mount or seed.
The source archive/member remain unchanged; scalar assignments update only
the selected config payload while the writer verifies every preserved payload.
Public reports identify the handler and selected section policy and include
merge counts and digest only for a completed artifact. Header coverage and
`whole_bundle_handled` describe this file/section handler scope, not recognition
or behavior of every CVar; Windows-specific keys may be unknown to the game.
Game loading and actual setting effects require separate runtime evidence.
Each queue record prepares against the configured original base independently.
This is not a combined multi-mod generation: separately rebuilt full-base PAKs
can mask one another if mounted together. A future batch composer must combine
the admitted changes before publication rather than install these independent
outputs as if the queue established conflict resolution.

Generated job-ID leaves own the private0700 quarantine and exclusive0600 output;
archive names and game paths are never destinations. The shared operation gate
is latched across intake, bounded CPU work, I/O and internal cleanup. The outer
owner uses one monotonic deadline, syncs the completed output, closes writable
access, reopens the same inode read-only and observes its SHA256 in physical byte
order. Callback write order cannot hash the file: PAK headers are backfilled.
`physical_readback_observed` is artifact identity, not an independent proof of
format semantics or runtime compatibility. Public reports expose no private
filesystem path, signed download URI or account credential. Runtime and
installation flags remain false.

Cleanup ignores cancellation and only unlinks identity-bound owned leaves.
Uncertain close or substituted inode/directory retains the context/journal for
retry, refuses further work and fails shutdown. Borrowed base/auth/runtime/
scratch owners stay alive until successful cleanup. There is no persistent
queue or process-loss recovery integration yet. Syscalls, allocation, libarchive
and bounded parsers remain cooperative; this is not a hard interruption promise.

PAK SHA1 consumers use the isolated pinned Mbed TLS 3.6.7 SHA1 provider and the
existing SHA1-only PAK config. Transport SHA256/TLS/PSA consumers retain their
selected config and sole lifecycle owner; no legacy crypto archive is mixed in.
The config job's one-shot SHA1 and SHA256 calls compile under the selected
transport context: the same-pin SHA1 provider supplies the former and transport
crypto supplies the latter. Neither context enables an alternate SHA1 layout.
Native links use `archive/link_layout.py`'s caller-local pinned SDK script and
reject a PT_DYNAMIC segment whose geometry differs from SHT_DYNAMIC. Native
compile/link and header/import checks remain separate from console execution.

`tests/nexus_pak_jobs_verify.py` serves the unchanged retained real ZIP through
owned actual HTTPS account/download services. Its independent repak oracle
compares the complete decoded base-plus-mod virtual-path/content union after
preparation. The test fixture and repak are development checks, never app
dependencies, target selection authorities or prerequisites during installation.

`tests/nexus_config_jobs_verify.py` exercises the same actual app routes with
the unchanged real Engine RAR. Independent repak decoding compares every
virtual path and payload with the retained config oracle, separate from the
application's output identity hash. Mixed regular members and unselected
source sections exercise refusal paths. Those fixtures do not establish
installation or CVar support on the console.

The IoStore resident owner and its exact profile/API contract are described in
`../iostore/PRIVATE_PREPARATION.md`. Each IoStore job also prepares independently;
the existing batch core is not an implicit multi-mod queue coordinator.

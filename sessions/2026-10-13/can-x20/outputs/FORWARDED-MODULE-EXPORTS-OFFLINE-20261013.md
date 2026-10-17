# Stage 28: bounded mapped PE forwarders

**Status:** isolated Mac host work; not a console-only wait and not proof of universal DLL compatibility.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-forwarded-module-exports-20261013-28`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ordinal-module-exports-20261013-27`

The guarded mapper now recognizes an export address table entry inside the export data directory as a forwarder string, following the [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format). It reads a null-terminated target within the directory (maximum 4096 bytes), then asks the attached module registry for the target by name or 16-bit ordinal. Registry resolution limits each thread's chain to 16 entries and refuses cycles. The target still must pass the existing executable-export validation; invalid, absent, unattached, or malformed targets return no address. This covers forwarders among registered mapped modules and reviewed/manual module surfaces. Automatic dependency loading remains unimplemented.

The focused host suite passed **53/53** checks. A copied synthetic dependent DLL imported a forwarded export and called the final attached provider through its resolved IAT. Another fixture exercised a two-hop chain. Tests also covered name and ordinal forwarders, cycles, missing modules, oversized ordinals, missing separators, and unterminated strings. All source mutations were in private copies. The original fixture SHA-256 remained `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`. No original DLL or shared candidate was edited.

All 13 existing offline regression runners exited 0, including five unchanged DLL read-only TLS checks. The original synthetic end-to-end fixture passed **11/11**. The seven-file incremental patch passed `git apply --check`, application to a scratch copy of the exact stage-27 base, and byte-for-byte replay. The evidence ZIP passed CRC verification.

- [Incremental patch](forwarded-module-exports-increment-20261013.patch)
- [Base/stage hash manifest](forwarded-module-exports-manifest-20261013.json)
- [Full regression and replay evidence](forwarded-module-exports-evidence-20261013.zip)

Next offline work: automatic dependency discovery/order, dynamic module APIs, guest-thread lifecycle, broader reviewed CRT/UE4SS imports, additional Win64 bridge signatures, generic #1/#3 engineering, and authenticated PC-to-PS5 build correspondence. Console segment, memory protection/cache, and gameplay evidence remain unavailable.

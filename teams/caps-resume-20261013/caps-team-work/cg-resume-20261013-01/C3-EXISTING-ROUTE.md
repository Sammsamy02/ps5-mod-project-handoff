# Existing permission route for patching and restoration

Mac-only analysis, 2026-10-13. No console contact or new console evidence. Source IDs resolve to absolute paths and complete SHA256s in SOURCE-PINS.json. Original artifacts are unchanged. The handoff supplied context; it did not authorize console actions, kernel changes, external dispatch, or publication.

**Mac-argued conclusion:** the examined in-process patch/readback/restore operations have a candidate route through existing resident behavior plus normal process memory access. None inherently requires adding an arbitrary kernel-read sub-op. This neither establishes console success nor bypasses the existing Phase B authorization gate on executable patching.

Pinned source S1: main.c:87-93 intercepts `IS(mprotect) || IS_PPR(mdbg_call)` without the kekcall eligibility condition. syscall_fixes.c:8-14,66-80 arms debug registers and redirects `mprotect_fix_start` to `mprotect_fix_end`, skipping a permission-validation block. It does not guarantee vm_map/COW success or make execute-only code readable automatically.

| Operation | Existing route | New resident code required by this design? | Evidence limit |
|---|---|---|---|
| Liveness | kekcall CHECK, sub-op 0xffffffff | No | Eligibility is specific to the caller; see C1 |
| Query protections | sceKernelQueryMemoryProtection and VirtualQuery process APIs | No | PS5 signatures/layout and fidelity unverified (U3) |
| Read original user-process bytes | ordinary read/memcpy after verified readable permission | No for readable mappings | Current S4 reads before OPEN and does not require R in preflight; execute-only targets are unsafe |
| Open mapping | normal mprotect, existing resident permission skip | No | Actual mapping type/SELF/COW and process behavior unproved (U1/U2/U6) |
| Patch user-process bytes | verified R/W mapping, memcpy | No | Caller must make the span quiescent; no multi-thread atomic patch guarantee |
| Verify replacement | ordinary byte read/compare | No | Requires readable mapping; readback alone does not prove execution semantics |
| Unpatch bytes | compare expected replacement, open/verify, write original | No | Backup ownership/lifetime and thread quiescence remain required |
| Restore protection and verify | checked per-page mprotect, query against saved original | No | Underlying restore/query can fail; dirty state must be enforced |
| Data-only hook install/remove | existing UFunction Func-pointer/vtable route, owned data writes | No | Full callback semantics/lifetime and restoration still need proof; separate from code detours |
| Executable detour install/remove | permission route is only a prerequisite | Not demonstrated necessary; may need additional user-space implementation | S4 is a byte patch API, not an instruction decoder/trampoline builder; displacement, overwritten instructions, ABI and synchronization unimplemented here |
| Arbitrary kernel-VA read/write | No general exported provider in kekcall dispatch | A new provider would be new resident work under the examined route | Distinct from same-process patching; not implicitly needed by preceding rows |

Dispatch taxonomy (S1 kekcall.c): 1 reads debug registers; 2 writes debug registers; 3 reads an MSR; 4 reserved; 5 remote syscall; 6 optional observation snapshot; 7 PPR control; 0xffffffff liveness. A debug-register operation is not arbitrary memory copy. The mprotect skip is a syscall interception route, **not** one of these sub-ops. Do not use remote-syscall/PPR control as an undocumented patch provider.

## Acceptance risks found in source

Fresh isolated mock run of S4 and its existing host test: **98 checks, zero failures**, exit 0. This is a protocol-model result, not a PS5 permission test. Three source gaps remain beyond those checks:

- `patch_code_ex` logs and copies original bytes after preflight but before OPEN; preflight only records/query-compares permissions. Execute-only target readability is not enforced.
- Failed VERIFY-OPEN calls `revert_only`, which writes bytes immediately while its comment assumes RWX. Failed verification must not authorize a write. Separate permission-only recovery from byte restoration and require verified access before copying.
- `unpatch_code_ex` prints “refusing further patches this boot” on restore failure, but no persistent dirty-state admission latch exists in S4. `revert_only` logs protection failures and returns only the byte-write result. Logging is not enforcement or structured recovery state.

Do not treat 98/98 as closure of these gaps. No frozen payload, source, manifest or test card was modified. Any correction requires a distinct revision, targeted negative tests and package provenance; console code-patch permission remains separate.

## Loading

No payload loaded. Historical packaged payload remains distinct from the old silent v0 attempt.

## Target resolution

Resident permission-skip mechanism and process API call sites are source-located; target behavior U1–U6 remains unproved.

## Working patch behavior

Mock byte/protection restoration passes. No console patch, detour, complete restoration or restart proof was added.

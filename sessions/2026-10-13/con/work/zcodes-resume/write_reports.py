from pathlib import Path
import hashlib,json,re,subprocess,shutil
base=Path('/WORKSPACE/Documents/Codex/2026-10-13/con')
wt=Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013')
what=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
stage=wt/'caps-team-work/cg-resume-20261013-01'
out=base/'outputs/zcodes-continuation-20261013'
assert not (stage/'CHECKSUMS.json').exists(), 'Do not overwrite finalized package'
stage.mkdir(parents=True,exist_ok=True)
(stage/'evidence').mkdir(exist_ok=True)
sources={
'S1':what/'outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip',
'S2':wt/'caps-team-work/cap01-03/probe/KernelReadProbe-v1.c',
'S3':wt/'caps-team-work/cap01-03/probe/classifier_v1_regression.c',
'S4':wt/'caps-team-work/cap03/src/CodePatch.cpp',
'S5':wt/'caps-team-work/cap03/DESIGN.md',
'S6':wt/'caps-team-work/cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md',
'S7':wt/'caps-team-work/cap13/REPORT/evidence/simplest-mod-census/cme_upstream_shape.cpp',
'S8':wt/'caps-team-work/cap13/BUILD/src/sdk/mbm/NativeModAbi.h',
'S9':wt/'caps-team-work/cap13/native-loader/src/win64_bridge.h',
'S10':wt/'caps-team-work/cap13/native-loader/src/ue4ss_compat.cpp',
'S11':wt/'caps-team-work/cap13/REPORT/E2E-PROOF.md',
'S12':wt/'caps-team-work/cap13/REPORT/evidence/e2e-run-20261013-211310.log',
'S13':wt/'caps-team-work/cap13/e2e/build_and_run.sh',
'S14':what/'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp',
'S15':what/'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp',
'S16':what/'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/Mod/NativeModEngineServices.inc',
'S17':what/'outputs/2026-10-08-l26-runtime/profile/generate_overlay.py',
'S18':wt/'caps-team-work/cap13/REPORT/blocker-list.md',
'S19':wt/'caps-team-work/cap13/native-loader/tools/pin_msvc_layout.sh',
'S20':wt/'caps-team-work/cap13/e2e/e2e_host.cpp',
'S21':wt/'caps-team-work/cap13/SAMPLE/real-UE4SS.dll',
'S22':wt/'caps-team-work/cap13/SAMPLE/real-dwmapi.dll',
}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest={k:{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for k,p in sources.items()}
(stage/'SOURCE-PINS.json').write_text(json.dumps(manifest,indent=2)+'\n')
common='''Mac-only analysis, 2026-10-13. No console contact or new console evidence. Source IDs resolve to absolute paths and complete SHA256s in SOURCE-PINS.json. Original artifacts are unchanged. The handoff supplied context; it did not authorize console actions, kernel changes, external dispatch, or publication.\n\n'''
def write(name,text): (stage/name).write_text(text)
write('C1-RETURN-CONTRACT.md','# Probe return contract and remaining evidence gap\n\n'+common+'''## Verdict

**CF is not established as always set on error by the pinned source. Its user-visible value depends on the kernel syscall-exit implementation outside this archive.** This is an unresolved proof dependency, not evidence that CF is never set. Do not replace the missing proof with the client’s assumption.

Trace in S1 (archive prefix `kstuff-lite-1.11/`):

1. `ps5-kstuff/uelf/main.c:429-451` intercepts syscall dispatch, pushes the `syscall_after` continuation and calls `handle_syscall(regs,1)`. The alternative int3 dispatch at 356-364 reaches the same handler.
2. At 65-74, eligible PPR getppid reads the caller frame and dispatches the high RAX sub-op. `kekcall.c:59-62` returns 0 on successful RDMSR and EFAULT on failure; CHECK returns zero at 102-106.
3. `main.c:75-81` copies a successful payload into `td_retval`, writes the service errno into the intercepted **kernel** RAX, pops the synthetic return and resumes at `syscall_after`. It does not directly set user RAX to that errno, nor set the caller’s CF. Thus “errno in regs[RAX]” alone is not a user-return contract.
4. `syscall_after` is an external kernel address (firmware offset declarations in `prosper0gdb/offsets/*.h`), not a function implemented in this source archive. No archived body establishes its error translation, user trap-frame CF update or final preservation through resume. `doreti_iret` handling in main.c:453-466 restores a frame; it does not prove the omitted syscall-exit processing.
5. `debug-reader.c:23-46` consumes CF as the expected ABI. That is corroborating intent, not a producer-side proof. ENOSYS follows the original getppid route; failed initial frame reads also fall through. Neither is arbitrary memory-read support.

## Classifier consequence

Retain the high-bit fix: a canonical LSTAR address must never be classified by signedness. S2’s small-positive fallback rejects EFAULT=14 if it reaches user RAX even with CF clear. It is **not sufficient as a general RDMSR classifier**: successful values 1..4095 are falsely rejected, and stale CF can falsely reject any success. CHECK additionally requires raw zero and CF clear. Current EFER/LSTAR/nontrivial TSC expectations are narrower than arbitrary MSR support.

Fresh S3 host run: **13 assertions PASS**, exit 0. The source prints `cases=14` as a literal; count the assertion lines, not that summary. This checks classification policy only and cannot establish how the target produces CF. The test also passes classification and `v` as separate arguments to `expect`; sequence the call and value check in a future revision for compiler-independent evidence.

Closing C1 requires already-authorized offline evidence for the exact firmware syscall-exit path, or a future separately authorized diagnostic recording raw RAX+CF for success and error. No such console run was made here; C1 stays **partially resolved**. Do not rerun v0 or relabel old logs as v1.

## Loading

No payload was loaded. Existing v1 build status is historical.

## Target resolution

The intercept and sub-op dispatch are source-located; the target kernel syscall-exit body is missing from this proof.

## Working patch behavior

Not exercised; arbitrary kernel-VA reads remain unestablished.
''')
write('C2-CONSTRUCT-ENABLED-BLOCKED.md','# Object construction and the unchanged DLL flow\n\n'+common+'''Appending `construct_object` would expose an existing engine service; it would **not** establish unchanged UE4SS C++ DLL compatibility. Matrix follows the S7 reconstruction, which is a COFF census object with stand-in STL types, not an upstream DLL.

| Flow step | Existing support and effect of the new row | Remaining obligation |
|---|---|---|
| DllMain, start_mod, CppUserModBase construction | Some fixture PE entry/export execution recorded in S12; construct row has no effect | Real CRT bootstrap, allocator pairing, MSVC base/vtable/wstring layout; S10 remains a sketch with no export bodies |
| Register ClientRestart hook | S8 has register_ufunction_hook; S16:394 onward resolves the path and registers pre/post callbacks through the existing Func-pointer route | Exact mangled export, UTF16 string demarshal, both by-value MSVC std::function objects, callback ownership/copy/destruction/invocation and return descriptor; no unchanged DLL hook proof |
| Receive callback and read controller | Host context exists in S16’s mbm_hook_call construction | Convert into the upstream context layout; preserve return/parameter semantics and guest lifetime; native structures cannot be blindly reinterpreted |
| Read CheatManager and CheatClass property slots | property_address/get services exist in S8; S7 dereferences the returned pointer | Resolve and type-check reflected object properties; verify game-thread access and object liveness. No live proof for this reconstruction |
| Fallback class lookup | find_first_of exists (S16:276 onward) | A UObject instance named by class is not automatically a UClass suitable for construction. Validate class identity; prefer the existing CheatClass when valid. Match intended fallback semantics |
| Construct engine object | NEW row would remove the C-table exposure gap; S14:146-197 already dispatches by engine version | Implement/admit the row, bind the engine target, validate UClass/outer, error/null behavior and game thread; pin guest FStaticConstructObjectParameters layout and translate into native parameters |
| Write created object to property | S7 writes directly through the returned property pointer; property_set exists for a C-ABI port | Correct slot type, object validity, engine assignment semantics and later observable behavior. Success requires object created with the intended class/outer and visible in the intended game flow |
| Remove hooks and unload | Host unregister/remove support exists | Guest callback destruction, matching allocators and no surviving invocation after unload; restore all changed state required by the operation |

**ABI correction:** S18 B5 asserts SysV returns an eight-byte pair of ints in RAX:RDX and prescribes EDX→high-RAX repacking. Fresh Apple clang assembly for a two-int aggregate on both x86_64 SysV and Win64 packs it in **RAX**; real host libc++ std::pair<int,int> also returns in RAX. This contradicts the general SysV assertion. It does not pin the actual MSVC STL type: nontrivial type rules and toolchain version still need guest evidence. S9 explicitly discusses 9–16-byte aggregates; it does not prove the reported aggregate-8 refusal. Remove the unverified repack requirement from the implementation assumptions. The by-value std::function blocker remains independently real.

The proposed two-object construction row covers only defaults. If a DLL directly writes Name/flags/template/etc. into the guest parameter object, Class/Outer-only translation silently loses requested behavior. Either extend generic typed parameter support with versioned layout pins, or refuse unsupported values. S19 suppresses compiler failures with `|| true`; its final “Pinned” message alone cannot certify layouts.

Append at table end, check struct_size, preserve old readers. S8 defines integer ABI version 1; “1.1” is prose, not an existing numeric encoding. Review version-check compatibility before a bump. Object creation is a state change. MBM_ROUTE_DATA_ONLY_PARTIAL describes a data-only route with timing differences, not a general state-change flag; do not repurpose it to admit construction. Review a separate service permission/capability contract.

## Loading

Fixtures provide a partial Mac mechanism proof; the new row changes no loader or CRT blocker.

## Target resolution

Host lookup/binding implementations exist. Cross-build semantic correspondence remains unproved.

## Working patch behavior

No reconstruction or unchanged upstream gameplay DLL has demonstrated this full flow. The earlier claim “everything before construct works” is source-level possibility, not execution evidence.
''')
write('C3-EXISTING-ROUTE.md','# Existing permission route for patching and restoration\n\n'+common+'''**Mac-argued conclusion:** the examined in-process patch/readback/restore operations have a candidate route through existing resident behavior plus normal process memory access. None inherently requires adding an arbitrary kernel-read sub-op. This neither establishes console success nor bypasses the existing Phase B authorization gate on executable patching.

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
''')
write('C4-E2E-ACCEPTANCE.md','# DLL execution evidence and the next acceptance target\n\n'+common+'''KF’s report is present (S11), with a completed final log S12. This continuation read the files; it did not rerun the wedge-prone suite. Snapshot hashes in SOURCE-PINS identify exactly what was reviewed, without claiming the agent’s live ownership status.

## Loading

S12 records **98 PASS check lines, zero FAIL check lines, 19 RESULT PASS blocks**, and a separate bridge6 case wedged on all three attempts. Four WEDGE lines describe that single case, not four independent test cases. The runner also invokes a nonexistent `cap13-mod-feature-probe.dll` and logs `cannot open` without failing the suite. Thus “all scheduled cases passed” is false. The 98 checks remain actual logged passes; the suite’s completion status is not a reliable aggregate verdict.

S13 explains why: run_case polls liveness without `wait`/exit-status collection, returns zero even after permanent failure, and the compile pipeline lacks pipefail. Final grep counts are informational, not an exit gate. Correct a future runner revision to require successful build and child exit codes, expected named case inventory and nonzero check counts, with TIMEOUT/MISSING/FAIL separate from PASS. Preserve old raw logs and do not blindly replay wedged attempts.

Inputs are not all #9-ABI fixtures. KF executed a hand-crafted PE fixture plus locally compiled UE4SS C-lifecycle samples (`cap13-ue4ss-flavor-*`), using a test completion resolver and host-text bridge entry stubs. Their provenance is still **local fixture/sample**, not downloaded upstream gameplay DLLs. The local mbm_mod_entry_v1 samples were chiefly TLS-refusal cases. No real C++ UE4SS object-model flow or genuine CRT initialization was established by the direct-DllMain/noentry samples.

Unchanged upstream framework S21 is rejected for real static TLS; unchanged proxy S22 is rejected for unresolved imports. They are legitimate negative tests, not successful upstream execution. Staged shim coverage differs from the custom test resolver; S11 reports 0/4 for the console-dump sample under the staged registry alone.

## Target resolution

Mapping, relocation and selected test import crossings have recorded Mac evidence. The custom UE4SS service bodies record strings and return canned values; they do not establish that the intended engine action occurred. Actual game functions/cross-build correspondence were not exercised.

## Working patch behavior

No unchanged upstream gameplay DLL behavior is proven. A recorded execute_lua_in_mod call with the expected string proves the crossing, not actor dumping. CreateThread is explicitly refused, so poller lifecycle return is not working polling. The bridge6 wedge is unresolved; its attribution to Rosetta is a hypothesis, not proof of PS5 correctness.

## Acceptance target decision

Choose **(b) a genuine upstream C++ gameplay mod release binary** as the next positive target; positive execution is currently blocked on artifact acquisition and the ABI/runtime gaps in C2. Option (a) framework stress is already a useful negative test, but real static TLS prevents a positive run under the current loader policy. Do not remove the guard simply to make that binary pass.

Artifact needed: the original release DLL or ZIP, upstream release URL/tag, intended game and build, UE4SS/SDK version and an observable expected behavior. Verify license/provenance and SHA256, analyze without modifying the binary, then select a supported envelope or report the precise refusal. No user upload is the only theoretical acquisition route, but no verified suitable upstream release binary was located in the supplied project evidence. Do not invent one or silently substitute a rebuilt example.

Positive acceptance requires immutable input hashes; actual entry→CRT→constructors where present; resolved and executed imports reported separately; real callback behavior; observable game-side outcome and cleanup. Mac execution and subsequent PS5 behavior receive distinct verdicts. No console action is authorized by this decision.
''')
write('C5-RETARGETING-PROPOSAL.md','# Automatic retargeting verification proposal\n\n'+common+'''Design proposal only; no implementation or runtime policy changes. Intended outcome: unchanged gameplay DLLs for their intended game/build produce their intended behavior. Sound stays deferred. Engine differences belong in adapters; image-specific facts belong in generated profiles.

Three approaches were considered: direct name/pattern substitution (small but does not establish ABI or patch semantics); a general binary translator (broad but unsupported by the available runtime/evidence); and **typed engine-adapter mediation with three independent admission gates** (recommended, reuses existing services and refuses unsupported direct patches). The last approach gives verifiable incremental behavior without claiming arbitrary machine-code compatibility.

| Gate | Input and output | Existing grounding | Admission proof still required |
|---|---|---|---|
| 1 Named target | DLL import/reflection request → canonical semantic target and typed signature | S7’s import census; S8 reflection/service table; S16 hook path lookup | Exact symbol/ordinal with library version; encoding/overload/type validation; name absence/ambiguity fails. Matching a framework export does not locate an engine function |
| 2 Cross-build correspondence | Canonical target + PC/PS5 build identities → unique PS5 binding with evidence | S15:1352-1369 authenticates cached code identity/prefix and binds the generated SCO seed; S14:28-33 stores that binding | Prove same semantic operation using independent structure/call-site/signature evidence for the specific images. A PC byte pattern/name/RVA is not presumed portable. Multiple/no candidates refuse |
| 3 Instruction and call compatibility | Typed binding + guest/compiler/layout facts + requested operation → admitted adapter/callback/patch plan | S14 version-dispatched construction wrappers and limited integer bridge S9 | Guest ABI/STL/context/struct layout, native engine signature, ownership, threading and exceptions; for direct patches, instruction decoding, relocatability, relative addressing, preserved state, branch reach, quiescence and restoration. Address resolution alone never passes this gate |

The examined generator S17 is a pinned layout overlay generator with image/hash/two-source checks, not a demonstrated general PC→PS5 function matcher. Reuse the generator’s provenance and fail-closed policy; do not label universal correspondence as already implemented. Likewise the SCO call-site actually examined is generated-seed + authenticated cached-prefix admission, not proof of portable blind scanning.

Each request carries a generic descriptor: operation kind (read/call/hook/replace), engine/version, guest signature and type-layout fingerprint, source/target image digests, profile/schema digest, correspondence evidence IDs, native signature, route requirements, lifetime/rollback contract. Engine adapter translates semantics; generated profile supplies image-specific facts. No hand-entered game addresses or mod-name branches belong in shared runtime.

After all three gates, admit the least invasive route that preserves the requested semantics. Existing data-only UFunction hooks can cover mediated engine events; they do not preserve every native-entry hook’s timing or arbitrary replacement instructions. A mod using hardcoded PC addresses, foreign prologues, unsupported GS/TLS/EH or incompatible assembly gets a named refusal. In-memory relocation/IAT binding is ordinary loading; do not rewrite original DLL files or disguise changed mod logic as retargeting.

Runtime receipts must separately retain LOADING (input hash, mapping/relocations, per-import result, entry/CRT); TARGET RESOLUTION (three gate results and provenance, without execution implications); and WORKING PATCH BEHAVIOR (expected observable event, before/after state, callback execution, restoration and restart evidence where applicable). A denied gate records why and performs no dependent mutation.

Validation obligations: absent/ambiguous names; wrong image/profile; changed prefix; multiple correspondence candidates; low-level ABI mismatch; parameter-layout changes/direct writes; callback ownership and unload; unsupported instruction relocation; concurrent mutation; dirty restore refusal; genuine upstream behavior. Fixtures validate mechanism; they cannot certify an upstream mod or a PS5 run.

Current status across the three reporting axes: **Loading** partial Mac fixture/sample evidence; **Target resolution** source-backed native service binding but no demonstrated automatic cross-build pipeline; **Working patch behavior** no unchanged upstream gameplay DLL proof. C2’s construction row is one dependency, not full acceptance.
''')
# Capture fresh classifier stdout and count actual assertion lines.
p=subprocess.run([str(base/'work/zcodes-resume/classifier-regression')],capture_output=True,text=True,timeout=10)
assert p.returncode==0
(stage/'evidence/classifier-fresh.log').write_text(p.stdout+p.stderr)
classifier_count=sum(bool(re.search(r'\bPASS \(got=',x)) for x in p.stdout.splitlines())
log=sources['S12'].read_text()
audit={'label':'Mac-only saved evidence audit; no E2E rerun','classifier_assertions':classifier_count,
'classifier_printed_summary':'cases=14 (literal; incorrect)',
'e2e_pass_lines':len(re.findall(r'^CHECK: .* PASS',log,re.M)),
'e2e_fail_lines':len(re.findall(r'^CHECK: .* FAIL',log,re.M)),
'e2e_pass_result_blocks':len(re.findall(r'^RESULT: PASS',log,re.M)),
'e2e_missing_inputs':re.findall(r'^cannot open (.+)$',log,re.M),
'e2e_wedge_lines':len(re.findall(r'^.*WEDGE.*$',log,re.M)),
'e2e_wedge_attempts':len(re.findall(r'WEDGE detected',log)),
'cap03_host_summary':re.findall(r'^\[HOST\] SUMMARY:.*$',(base/'work/zcodes-resume/cap03-host/run.log').read_text(),re.M)}
assert classifier_count==13 and audit['e2e_pass_lines']==98 and audit['e2e_pass_result_blocks']==19 and len(audit['e2e_missing_inputs'])==1
(stage/'evidence/AUDIT.json').write_text(json.dumps(audit,indent=2)+'\n')
shutil.copy2(base/'work/zcodes-resume/cap03-host/run.log',stage/'evidence/cap03-fresh-host.log')
for f in ('aggregate_probe.cpp','aggregate-sysv.s','aggregate-win64.s','real_pair_probe.cpp','real-pair-sysv.s'):
 shutil.copy2(base/'work/zcodes-resume'/f,stage/'evidence'/f)
write('README.md','# Zcodes continuation findings\n\n'+common+'''The Mac continuation produced five analysis deliverables. C1 remains partially open: the archive does not contain the target kernel syscall-exit body needed to prove CF. C2/C3/C4 have actionable source-backed findings; C5 is a design proposal, not an approved implementation.

| Task | Result |
|---|---|
| C1 Return contract | Producer-side CF guarantee unproved; fallback is not a general MSR classifier. Fresh suite has 13 assertions, all pass, despite printing 14 |
| C2 Construction flow | One row removes a service-exposure gap, not C++ DLL ABI/callback/layout gaps. The prescribed aggregate-8 repack is contradicted by fresh compiler evidence |
| C3 Existing route | Existing resident mprotect skip is the candidate; new arbitrary kernel reads are not inherently required. Fresh 98/98 mock checks pass, but pre-open reads, unsafe rollback and unenforced dirty state remain acceptance risks |
| C4 KF evidence | Local fixtures/samples executed; genuine framework/proxy refused. Log has 98 passing checks, one wedged case and one missing input hidden by runner success. Positive upstream gameplay target remains blocked |
| C5 Retargeting | Proposed independent name, correspondence and compatibility gates; generated profiles and engine adapters retain their separate roles |

## Loading

Partial Mac fixture/sample execution was recorded by KF. This continuation audited saved logs and ran only local protocol/classifier tests and assembly-only ABI probes. It did not rerun KF’s wedge-prone suite. No console payload was built or loaded.

## Target resolution

Source-backed process permission interception and native engine binding exist. Automatic PC→PS5 semantic correspondence and complete foreign C++ ABI mediation are not established.

## Working patch behavior

Host protocol model passes 98 checks. Unchanged upstream gameplay DLL behavior, console patch/restoration and restart proof remain unverified. Treat source risks in C3 as blockers before console use of the current payload.

## Next work

First correct the Mac evidence runner in a new revision with nonzero build/child exits, missing-input failures and bounded timeout verdicts. Correct the protocol’s access/recovery/dirty-state gaps with targeted negative tests in a separate staged revision; preserve the frozen package. Add the construction service only with compatible table growth and typed parameter/admission decisions. Acquire and classify a genuine upstream gameplay DLL before claiming upstream compatibility. No console action follows automatically from any of these steps.

Files C1 through C5 contain the matrices and evidence limits. SOURCE-PINS.json holds full input paths and hashes; evidence/AUDIT.json reconciles counts. CHECKSUMS.json pins this package. Original shared status reports were deliberately left intact; use this as an additive handoff.
''')
# Verify source stability across analysis/package generation and pin deliverables.
assert all(sha(sources[k])==v['sha256'] for k,v in manifest.items()), 'Reviewed source changed during packaging'
checks={str(p.relative_to(stage)):sha(p) for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'CHECKSUMS.json').write_text(json.dumps(checks,indent=2)+'\n')
assert not out.exists()
shutil.copytree(stage,out)
print(json.dumps({'stage':str(stage),'deliverable':str(out),'files':len(checks)+1,'audit':audit},indent=2))

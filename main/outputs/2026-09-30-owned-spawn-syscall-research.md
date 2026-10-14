# Owned spawned-process syscall diagnostic research — 2026-09-30

## Conclusion and scope

A separate, explicit `OWNED-SPAWN` diagnostic using only fixed syscall numbers 1, 4, 20, 24 and 25 is a small source-grounded design candidate. It removes the overloaded resolver/getpid slot, unknown `getpid+0xa` wrapper and syscall 591 from the diagnostic. It should terminate its own spawned process rather than return into the launcher-supplied saved continuation. Page size remains unavailable.

This is **not yet an executable target contract**. Pinned FreeBSD source establishes the ordinary amd64 raw ABI; the pinned community SDK agrees on the five numbers. Neither establishes that instructions in the diagnostic's mapped executable region are permitted on the reported PS5 firmware 13.60 without the excluded SDK CRT permission patch. The existing direct-instruction ban must stay intact until a separately reviewed narrow profile is designed. No product change, verifier change, console contact, transmission or execution was performed here.

The final application remains console-only. A development/build computer and local source fixtures do not become a PC/phone/AI runtime requirement. This diagnostic establishes no game capability or mod translation support.

## Source pins and evidence limits

| Component | Exact pin and use |
| --- | --- |
| Community SDK source review | `ps5-payload-dev/sdk@f7fd02e6e195902a449b5e664917be8c01888b34`: syscall number header, CRT termination policy and existence of syscall-origin permission adjustment. |
| WebKit Autoloader fork | `itsPLK/ps5-elfldr@bb1e117988217a0239679029601e93c7286394d7`: socket dispatch, spawned child, entry continuation and inherited stdio. Its `elfldr.c` was compared byte-for-byte with upstream `54966675d3884ba9c773f0f52646f8f46b92cbbc` and matches. |
| FreeBSD amd64 comparison | `freebsd/freebsd-src@d106002967eff712894ab8b5337bbbff106ce5e0`, resolved from `releng/11.4`: actual upstream raw return/error ABI and ordinary process-call implementations. This comparison is not a claim that Sony's current kernel equals this FreeBSD tree. |

The reviewed SDK pin is a post-release revision. The v0.43 release commit is `d9c9519116944a7f1c22d262012c53b80b3520b7`; historical source pins, URLs and hashes remain intact. See [SDK provenance correction](2026-09-30-native-sdk-host-route.md#release-identity-correction-and-impact).

The release/source chain is documented in [WebKit Autoloader identification](2026-09-30-webkit-autoloader-identification.md). Installed loader bytes, firmware identity and the target's effective syscall permissions remain unmeasured. Source scratch is `/tmp/owned-spawn-syscall-source`; its `source-manifest.json` records exact URLs, sizes and SHA-256 for 21 files. Earlier SDK scratch is `/tmp/minimal-loader-source`. Source line references below use the actual downloaded raw file, rather than the web extractor's compressed line numbering.

## Fixed ordinary operations

Both the [SDK header](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/include/freebsd/sys/syscall.h#L9) and [FreeBSD header](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/sys/syscall.h#L9) define these same constants:

| Number | Operation | Arguments and ordinary meaning |
| --- | --- | --- |
| 1 | `exit` | One integer status; terminates the calling process. |
| 4 | `write` | `fd`, owned buffer pointer, byte count; use inherited fd 1 only. |
| 20 | `getpid` | No arguments; current process ID. |
| 24 | `getuid` | No arguments; current thread credentials' real UID. |
| 25 | `geteuid` | No arguments; current thread credentials' effective UID. |

The getters are ordinary self-process observations: pinned [kern_prot.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_prot.c#L102) returns the current process PID and, at lines 237–258, the current credentials' real/effective UIDs. They do not enumerate other processes or change credentials. UID zero is a valid observation and must not be rejected as an error. A UID is not proof of additional console capabilities.

There is no `getpagesize` operation in this five-call contract. Do not substitute a linker alignment, host page size, SDK macro or assumed 16 KiB value for a native measurement. The report should say unavailable, reason `not_queried_in_profile`.

## Actual upstream amd64 raw ABI

The kernel entry [exception.S:539–558](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/amd64/amd64/exception.S#L539) records the syscall number from RAX and arguments from RDI, RSI, RDX, R10, R8 and R9. [trap.c:943–989](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/amd64/amd64/trap.c#L943) selects that numbered call. None of the proposed calls needs arguments 4–6.

[vm_machdep.c:372–408](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/amd64/amd64/vm_machdep.c#L372) implements the raw result convention: successful calls clear carry and return `td_retval[0]` in RAX, with a secondary result in RDX. Ordinary errors set carry and put the ABI-mapped error number in RAX. Special internal restart/just-return cases have different control-flow handling. The [FreeBSD ELF vector](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/amd64/amd64/elf_machdep.c#L50) supplies no alternate errno table.

Raw errors therefore are not Linux-style negative RAX and are not libc's `-1`. [libc SYS.h](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/amd64/SYS.h#L38) branches on carry; [cerror.S](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/amd64/sys/cerror.S#L51) stores errno and manufactures `-1` in RAX/RDX. A freestanding diagnostic can preserve raw RAX plus an immediately captured carry bit, without libc errno or TLS. Capture carry before any instruction that changes flags. A positive error number can otherwise be mistaken for a PID/UID.

Use five private assembly thunks with compile-time fixed numbers, rather than a public number-taking dispatcher. Their C-facing result contract should be an explicit `{raw_value, carry}` pair with the assembly satisfying the selected C ABI. Do not use a Linux inline-assembly clobber recipe: [exception.S:583–600](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/amd64/amd64/exception.S#L583) restores RDX as a secondary result and clears R8/R9/R10 on its fast return path; SYSCALL also uses RCX/R11. Compiler/register preservation and immediate carry capture require review even for calls with no arguments.

## Ownership, termination and continuation

The fork's [socket dispatch](https://github.com/itsPLK/ps5-elfldr/blob/bb1e117988217a0239679029601e93c7286394d7/socksrv.c#L124) routes ELF input to `elfldr_spawn`. The child setup reaches an existing program's main-call site; [entry preparation](https://github.com/itsPLK/ps5-elfldr/blob/bb1e117988217a0239679029601e93c7286394d7/elfldr.c#L345) pushes the prior target RIP on its stack and transfers execution to the ELF. A diagnostic `ret` resumes that target program continuation. It does not return report status to the TCP server. The source does not establish what that continuation does on the user's installed firmware; resuming it is an avoidable uncertainty.

For a profile used exclusively with this owned spawned child, call process `exit` after report completion/failure, and never fall through to `ret`. Upstream [kern_exit.c:177–181](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_exit.c#L177) calls `exit1` with no normal return. Its [_exit manual](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/sys/_exit.2#L45) documents process termination and closing descriptors. This bypasses libc `exit` cleanup, which this freestanding diagnostic does not need. An unexpected return from syscall 1 should reach a private non-returning failure terminator, not the saved target continuation. This is not a guarantee that failure is recoverable on the console.

Ownership must be selected by a reviewed launch profile, not inferred from getpid, pointer contents or a kernel probe. The same process-exit artifact must never be used as a callable plugin inside a shared host/game process. A callable-return profile remains a separate artifact/contract. The SDK's [payload_terminate](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/crt.c#L157) tries kernel-backed symbol discovery to distinguish injected versus spawned contexts. That detector is excluded; this adapter must not borrow it. Loader-supplied kernel fields and the overloaded first argument need not be read at all.

## Output and failure semantics

The fork duplicates the accepted socket onto the child stdin/stdout/stderr before transfer, then returns the child PID without waiting for diagnostic completion. A receiver preserving its read half can collect fd 1 output; source dispatch closes only its own accepted descriptor. The listener is loopback-only, so this source supports an on-console receiver path. It does not support a direct remote PC connection to that listener. The existing manager/unified sender closes without collecting stdout; receiver integration remains a separate checkpoint.

A bounded owned buffer and a bounded write-attempt loop preserve the current 4096-byte/eight-attempt limits if desired. Each successful short write advances only by the returned byte count. Zero progress, carry-set error, a count beyond the requested range or exhausted attempts fails output. Upstream [sys_generic.c:570–610](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/sys_generic.c#L570) reports bytes transferred and can clear an interrupt/would-block error after partial transfer; the result must therefore be checked, not assumed complete.

The five-call contract cannot impose a kernel-write deadline or suppress SIGPIPE. Upstream write/socket behavior can signal a broken pipe; successful exit may also wait while descriptors close/drain. Bounded attempts are not bounded wall-clock time. Do not add signal, socket-option, poll or clock operations implicitly. A disconnected receiver can yield no final report; receiver EOF alone proves neither clean exit nor successful report. A complete validated report establishes the included observations and completion marker only; child termination needs separate evidence if the caller claims it.

## Minimal reviewed design recommendation

1. A new artifact/profile explicitly named `OWNED-SPAWN`, never auto-selected from payload argument pointers. Fixed private operations only: three self getters, fd-1 output and self-process exit. No resolver, `getpid+0xa`, syscall 591, generic dispatcher, dynamic imports, kernel reads/writes, permission patch or game access.
2. A schema revision distinguishes direct raw calls from resolver observations. Per getter: call kind `direct_self_syscall`, fixed number, raw result, carry, status and interpreted value. Carry-set results have unavailable value and preserve the raw error number. Resolver codes are absent/null with `not_attempted`, never reused for kernel errno. Page size is unavailable. Include launch profile and termination policy without claiming target ownership was measured.
3. Format a bounded report from local data. Stop output on failure, then request syscall-1 termination with a small fixed status. Zero means emission finished, not that every getter or console capability exists. Getter failures remain visible independently. The report cannot retrospectively certify its own final write/exit; receiver completion and process exit are separate facts.
4. Preserve the callable candidate and its direct-instruction ban. Any future verifier exception must be confined to the separately named artifact, exact reviewed thunk ranges/bytes and the five constants; retain rejection of direct syscall instructions anywhere else. Check control flow, fixed fd/buffer bounds, no variable-number path and no normal return from the owned entry. Merely renaming syscall source does not satisfy the ban.
5. Verify carry-success/error, UID zero, valid PID, partial/zero/oversized write, truncated receiver input and unexpected exit return through owned fixtures. Static ELF/disassembly and a syscall-result fixture are useful development evidence; they do not prove console permission. Never execute FreeBSD-numbered raw instructions as an ordinary macOS/Linux host test: syscall numbers and ABI have different meanings there. An explicitly controlled FreeBSD environment or emulator can test upstream behavior without console contact.

## Alternative: existing libkernel getpid stub tail

The SDK's [syscall.c](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/syscall.c#L38) takes an accepted getpid stub, adds `0xa`, loads a chosen syscall number and calls that address. The identified loader supplies a getpid stub in its first argument field. This offers a second candidate with the same explicit ownership and five fixed operations: use a reviewed existing libkernel tail rather than putting direct SYSCALL instructions in the diagnostic's own mapped code. It could avoid the separate direct-instruction verifier exception and the arbitrary payload-code origin hypothesis.

It is not automatically safer or defined merely because the SDK does it. The actual target stub bytes must establish that offset `+0xa` is an instruction boundary with the required register entry contract and an appropriate return path for all five calls. The generated SDK export stubs do not provide those installed executable bytes. The supplied pointer is an external callable address; a null/range check cannot validate it, and reading bytes to self-validate can itself fault. A reviewed launcher contract and exact target-byte evidence are needed; no kernel memory lookup or privilege-changing fallback is acceptable.

Keep error-tail semantics separate from the direct raw ABI. If the library tail branches to errno/TLS handling and manufactures `-1`, carry after the returned library function may have been overwritten; it cannot be reported as the raw kernel carry bit without byte-level proof. `-1` could establish getter/write failure but does not expose a raw kernel errno unless a separately permitted interface supplies it. If an exact reviewed tail preserves raw RAX/carry through return, that specific observation contract can instead be used. Neither behavior is proved by the current source exports.

For this alternative, reports should name `call_kind: libkernel_stub_tail`, the fixed requested number and the proven result convention; leave raw carry/kernel errno unavailable when they cannot be captured before the wrapper tail. Resolver status still stays absent/not-attempted. Retain the non-returning owned-spawn termination policy and unavailable page size. Do not add a generic dispatcher, silently switch between alternatives after failure, scan modules or trust an arbitrary first-slot callback as a resolver. The choice between a direct-origin profile and a verified stub-tail profile is a next-design decision; both remain experimental under the existing evidence.

## Remaining hypotheses and decision gates

- **Target instruction-origin permission:** pinned [SDK patch.c](https://github.com/ps5-payload-dev/sdk/blob/f7fd02e6e195902a449b5e664917be8c01888b34/crt/patch.c#L22) contains an explicit current-process syscall-origin permission adjustment. Its existence is evidence of a restriction the freestanding adapter cannot assume away. The loader restores credentials before detaching; its use of libkernel stubs does not prove arbitrary payload-code syscall origins are accepted. No patch, origin spoofing or bypass fallback is recommended. If the target disallows this contract, this profile is unsupported under the present constraints.
- **Sony raw ABI equivalence:** matching constants and upstream carry convention are strong comparison evidence, not an observed firmware-13.60 ABI. A separately reviewed diagnostic would need to establish the raw convention before values acquire native evidence status.
- **Ownership and receiver availability:** the exact source socket path owns a child, but installed/cache identity and an on-console receiver preserving output must be established independently. Do not silently apply this profile to another launcher path.
- **Exit behavior:** syscall 1 ordinarily exits the calling process upstream. Permission, installed target behavior, absence of unintended continuation and output-drain timing remain unmeasured. No source-only claim of safe console termination or absence of hangs is justified.

The next review can concretely decide whether to create this narrow profile and its schema/verifier fixtures. This research does not approve hardware execution or remove any existing verification gate.

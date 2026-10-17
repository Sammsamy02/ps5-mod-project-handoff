# worker-liveprobe — LIVEFACTS_PROBE standalone native diagnostic payload

Date: 2026-10-04. Output area (only place written):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-liveprobe/`

## What was built

ONE standalone native diagnostic payload answering loader-fact questions in
its own process: a genuine SDK ELF **ET_DYN** with the pinned crt1 `_start`
entry (crt calls `main`), NOT a PRX. It appends one bounded JSONL record per
experiment to `/data/modbridge-livefacts-probe.jsonl` and exits cleanly
(exit 0 all recorded / 90 log-open failure, nothing written / 92
write-close failure). Single main thread; no other `/data` writes; no
network; no console contact (the controller dispatches it later).

| File | Role |
|---|---|
| `src/probe_abi.hpp` | Pinned 352-byte `ModuleInfo`/segment ABI (re-spelled from worker-1/prosperodev provenance with LP64 static_asserts) + explicitly UNPINNED declarations for `sceKernelGetExecutableModuleHandle` (int64(void), marked) and `sceKernelGetModuleList` (both spellings, see E4). |
| `src/probe_record.hpp/.cpp` | Bounded JSONL record builder (hard 2048-byte cap incl. `\n`, whole-token-or-nothing appends, explicit `__trunc` marking, auto-comma JSON validity) + file sink (open O_WRONLY\|O_CREAT\|O_APPEND\|O_CLOEXEC 0600, EINTR-bounded retries, single bounded write per record) in the spirit of `windows-candidate/result_sink.c`. Own decimal/hex formatters — no libc printf dependency. |
| `src/livefacts_probe.cpp` | `main` + E1..E7 + summary; guarded so any failure is an explicit record, never silent abort. |
| `tests/test_probe_record.cpp` | Host test (pinned LLVM clang): escaping, INT64_MIN, hex, auto-comma, truncation bound, sink write. |
| `build_probe.py` | Fresh `build/<uuid>/` per run (history never overwritten); pinned toolchain only; all steps receipted. |

Record envelope (≤ 2 KiB, strings JSON-escaped with `\u00xx` for
control/non-ASCII bytes):
`{"marker":"modbridge.livefacts","schema":1,"build_id":"<id>","seq":N,"exp":"<id>","rc":..,"errno":..,"fields":{...}}`
and a final `{"exp":"summary","status":"COMPLETED",...}`.

## Exact build commands (what build_probe.py runs)

```
# host record-writer test (pinned LLVM, no target code):
<llvm>/clang -std=c++17 -Wall -Wextra -Werror tests/test_probe_record.cpp src/probe_record.cpp -o test-probe-record
./test-probe-record host-record-demo.jsonl        # PASSED; lines json.loads-valid

# supplier TU compile evidence (READ-ONLY from worker-1's directory, not linked):
<sdk>/bin/prospero-clang++ -std=c++17 -Wall -Wextra -Werror -fPIC -I <supplier>/src \
    -c <supplier>/src/live_module_facts.cpp -o supplier-live_module_facts.o
<sdk>/bin/prospero-clang++ ... <supplier>/src/live_module_os_ops_sce.cpp ...
# both exit 0; object sha256 BYTE-IDENTICAL to worker-1's receipt:
#   bc7c6a43c747098587fa02a865f127670af34fcb177b4520f88bff59d569c1d5
#   76733b651e5d91c1f1bca9729fa3a1931506e24e1c8343df89ba78492f2d14a5

# probe TUs (env LLVM_CONFIG=<llvm>/llvm-config):
<sdk>/bin/prospero-clang++ -std=c++17 -Wall -Wextra -Werror -fPIC -fno-exceptions \
    -fno-rtti -O1 -DLIVEFACTS_BUILD_ID=... -DLIVEFACTS_LOG_PATH=... -I src -c ...

# link (prior-art sdk-elf-link pattern from windows-candidate/build.py line 222,
# caller-local linker script written by the sha-pinned link_layout.py):
<sdk>/bin/prospero-clang -nostartfiles -Wl,-T,<out>/elf_x86_64.dynamic-only.x \
    -Wl,-e,_start -fno-lto -fPIC -pthread -Wl,--no-undefined -Wl,-Map,<out>/link.map \
    probe_record.o livefacts_probe.o <sdk>/target/lib/crt1.o -o livefacts-probe.elf
```

Built artifact (run `3e5b946f4f6e454595ed1bc2c53fb823`, build_id
`lfp-84fdce4cba6a46b2ae85037cd0757548`):
`build/3e5b946f4f6e454595ed1bc2c53fb823/livefacts-probe.elf`
sha256 `bfc113b719e6e42a2efd71af92ff152d179fa746a9675ffe7eda8b13cf4a5030`,
113104 bytes. Earlier run dirs (`c41e47e3…`, `2182ea88…`, `6f61e93d…`)
are retained build history with their own receipts; only the final run is
the deliverable.

## Symbols evidence

- ELF64 LE, `e_type=ET_DYN(3)`, `e_machine=AMD64(62)`, entry `_start`
  (crt1.o; disassembly: `_start` saves rsp, aligns, calls `__crt_start`).
  `main` is defined at 0xde0 (called by crt). Exactly one PT_DYNAMIC,
  terminal DT_NULL, 21 dynamic entries (link_layout.validate_dynamic_layout).
- Defined: `_start`, `__crt_start`, `main`, `kernel_get_fw_version`
  (all T, statically from crt1.o/this probe), `_DYNAMIC` (r, linker-provided).
- Dynamic imports (12) and providers (build fails if ANY import lacks an SDK
  provider; full table in receipt `import_providers`):

| Import | Providers |
|---|---|
| `sceKernelGetModuleList`, `sceKernelGetExecutableModuleHandle`, `sceKernelGetModuleInfo`, `sysctl`, `getpid`, `open`, `write`, `close`, `__error` | libkernel.so, libkernel_web.so, libkernel_sys.so, libkernel_stub_weak.so |
| `memset`, `memcpy`, `strlen` | libSceLibcInternal.so, libc_stub_weak.so |

- **Discovered (link-time):** `dlopen`/`dlclose` do NOT become dynamic
  imports — `libc.a(dlfcn.o):(.text.dlopen)` resolves them statically into
  crt1.o's bundled rtld (`__dlopen`/`__dlclose`, link.map lines 291-298).
  E6 retention therefore goes through the SDK payload's own loader.
- **Discovered (build-time):** `kernel_get_fw_version` is exported by NO
  `.so` in the SDK but is DEFINED (T) in `crt1.o`; since the payload links
  crt1.o, E7 needs no dynamic import and uses the pinned
  `target/include/ps5/kernel.h` declaration `uint32_t kernel_get_fw_version(void)`.

## Experiments: what each will prove/deny (target run is controller-owned)

| id | question | proves on success | denies on failure |
|---|---|---|---|
| e1_dtdebug_map | does the PS5 rtld fill this payload's `DT_DEBUG`, and is the game image visible in `r_debug.r_map`? | DT_DEBUG present+nonzero, link-map walk yields ≤64 entries with `l_addr` + first 64 bytes of `l_name` escaped; rc 0 | rc -1 absent entry, -2 unfilled (identity then needs E3 route), -3 null r_map |
| e2_proc_pathname | `sysctl(KERN_PROC_PATHNAME)` for a payload process | escaped own executable path | rc/errno recorded; E6 then records skipped (-9) honestly |
| e3_exec_handle | unpinned `sceKernelGetExecutableModuleHandle` signature/semantics | raw int64 + errno; plausible handle validates: `sceKernelGetModuleInfo` 352-byte size echo + segmentCount + first 4 segment {perm,size} hex | negative/oversized raw or no echo → handle route unusable |
| e4_module_list_unit | `sceKernelGetModuleList` spelling (3-arg prosperodev vs 4-arg brief) AND size unit (bytes=4N vs elements=N) | the variant whose rc=0 with a handle that validates under the 352-byte echo oracle | each variant's rc/errno recorded; all four in ONE bounded record |
| e5_proc_vmmap | is KERN_PROC_VMMAP permitted for a payload process | rc 0, final buffer, entries per `sizeof(kinfo_vmentry)`, growth ≤ 4 MiB | rc/errno recorded; mapping correspondence stays absent (never guessed) |
| e6_self_retention | can RTLD_NOLOAD retain SELF, and is the loader handle domain-equal to E3's module handle | handle non-null, dlclose rc, `self_domain_equal` 0/1 (worker-1 fact 6 applied to self) | null handle → handle-validated lease needs E3/E4 |
| e7_fw_version | firmware version | raw uint32 hex+dec via pinned header + crt1 symbol | n/a (statically linked; records errno verbatim anyway) |

E4 safety design (documented per brief): no SDK header declares the
function; the pinned prosperodev spelling is 3-arg while the brief supplies
4-arg. BOTH are called with BOTH size readings through a 6-argument function
pointer in which every argument register (rdi,rsi,rdx,rcx,r8,r9) holds either
0 or a pointer into owned zeroed 8 KiB scratch, so whichever way the real
entry point reads its registers it either writes only inside our buffers or
fails in-kernel gracefully (EFAULT-style) and the rc/errno are recorded.
Disassembly confirms the indirect calls with register-loaded arguments.

## Honest boundary

Host evidence = compile/link/nm/readelf/objdump/disassembly plus the host
record-writer test ONLY. Nothing was executed on a PS5; no console contact;
no target JSONL exists yet. Every E1..E7 outcome above is a hypothesis until
the controller dispatches `livefacts-probe.elf` in the live game process and
returns `/data/modbridge-livefacts-probe.jsonl`. Failures will be explicit
records with rc/errno, never fabricated successes. E1 intentionally
dereferences DT_DEBUG-derived pointers (that is the experiment), so a
corrupt link map could fault the payload mid-run — the record stream up to
that point is still valid bounded JSONL.

## Deviations / notes

- Worker-1's supplier TUs compiled as-is (exit 0, byte-identical objects),
  so the probe was still written directly against raw pinned SDK APIs (the
  experiments need no supplier class), per the fallback clause being
  unnecessary but the evidence retained.
- Two supplier-header spellings (prosperodev `sceKernelGetModuleInfo`) are
  re-declared in `probe_abi.hpp` with identical static_asserts rather than
  `#include`ing worker-1's header, keeping the payload self-contained; the
  supplier's own objects above prove the shared ABI compiles identically.
- Exit codes: 0 (all records + summary), 90 (log open failed, nothing
  written), 92 (write/close failed; partial records possible).
- No file outside this worker's output directory was created or modified;
  the Codex worktree was never reset/stashed/cleaned/checked out.

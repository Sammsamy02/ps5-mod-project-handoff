# PROBE-PRX-BUILD.md — console PRX build preparation for the standalone pad probe

worker-real-input-pad, 2026-10-04. PREPARED ONLY: nothing here has been
compiled with the Prospero toolchain or run on console (this worker's bounds
forbid console contact; console execution belongs to the scoped verifier
route). Host logic is validated (`build_and_test.sh`, 17 assertions, ASan+UBSan,
receipt in `build_and_test_receipt.txt`); this document records the exact
console build route, mirroring the proven `native-coherent-pad-runtime-1/build-1`
commands read-only.

## 1. What the probe PRX is

`pad_probe/PadProbe.cpp` + `pad_probe/pad_probe_main.cpp` compiled together
with the RETAINED provider and Handler sources — the same translation units the
composed runtime already links (`PS5PadSource.cpp`, `PS5PadNative.cpp` — real
`scePad*` backend —, `Handler.cpp`, `keydef_modifier_ops.cpp`). The exported
entry `modbridge_pad_probe_run(const char* jsonl_path, unsigned long long
poll_budget, int requested_user)` runs the bounded probe on the calling thread
and returns the final record string; all evidence lands in the JSONL file.

There is deliberately NO new module_start of its own to wire into the loader:
the next console payload (or a dedicated probe payload) calls the exported
symbol from its existing probe path, exactly like earlier native probes in
`outputs/ps5-mod-bridge/native-probe*`.

## 2. Compile (each TU, once)

Toolchain + flags mirror `.superpowers/native-dependency-spike/
native-coherent-pad-runtime-1/build-1/ue4ss_backend.command.json` (exit 0,
unmodified). Common flags from that command file that the probe TUs need:
`-std=c++23 -O0 -fno-lto -fPIC -fms-extensions -D_LIBCPP_ENABLE_EXPERIMENTAL`
plus `-DHAS_INPUT -DRC_INPUT_BUILD_STATIC` and the include set:

```
PROSPERO_CLANG=outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++
INC="-I pad_probe_dir \
 -I .superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port \
 -I .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include \
 -I .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include \
 -I .superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/patched/include \
 -I .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers"
```

Objects to produce (into a NEW build dir under the owning team's outputs —
never into the retained spike dirs):

1. `pad_probe.o`      ← `pad_probe/PadProbe.cpp`
2. `pad_probe_main.o` ← `pad_probe/pad_probe_main.cpp` (exports
   `modbridge_pad_probe_run`; everything is inside `#ifdef __PROSPERO__`)
3. `pad-PS5PadSource.o` ← retained `PS5PadSource.cpp`
4. `pad-PS5PadNative.o` ← retained `PS5PadNative.cpp` (real backend; `#error`
   guard requires `__PROSPERO__`, so this TU is console-only)
5. `input-handler.o` ← retained `src/Input/Handler.cpp` (or reuse the already
   linked `coherent-full-1/input-handler.o` + `input-keydef.o` from the
   composed runtime link set, read-only)
6. `keydef-ops.o` ← `pad_probe/keydef_modifier_ops.cpp`

## 3. Link

Two routes, in order of preference:

- **Route A (payload object reuse, zero new link):** add objects 1–6 to the
  NEXT payload link (the established `ld.lld -m elf_x86_64 -r ...` route of
  `native-coherent-pad-runtime-1/build-1/link.command.json`) and call
  `modbridge_pad_probe_run` from the payload's existing probe entry. The
  composed runtime's libc/libc++/libunwind/libpthread tail (ps5-payload-sdk +
  retained pure-libc objects) already satisfies Handler.cpp/provider needs —
  the composed runtime links these exact sources.
- **Route B (standalone probe PRX):** `ld.lld -m elf_x86_64 -r` over objects
  1–6 plus the same SDK tail objects listed in that link command
  (`libunwind.a`, `libc++abi.a`, `libc++.a`, retained `libc.a`,
  `libpthread.a`) into `pad_probe.o`, then package through the existing
  probe-payload route. Route B exists so the probe can run WITHOUT staging the
  full activated runtime — the point of the first hardware test.

Either way the probe must run on the same serialized ownership terms as the
composed runtime: sole processing thread, no overlapping `process_event`, and
teardown after the loop joins (Handler.hpp lifecycle comment). The probe is
single-threaded by construction and bounded by `poll_budget` iterations.

## 4. What must be true before running (preconditions)

- `scePad*`/`sceUserService*` resolve in the game process (they are the same
  SDK symbols the composed runtime's `PS5PadNative.o` references).
- `/data/pad_probe/events.jsonl` path is writable by the game process
  (earlier runtime-ack work already writes JSONL status under /data).
- One controller connected and activated for the logged-in user; `User = -1`
  semantics = first entry of `sceUserServiceGetLoginUserIdList`.
- The composed runtime must NOT be started concurrently in the same process:
  `ConfigureRetainedPS5PadInput` and the probe would race for the pad service
  lease. The probe payload route (B) exists precisely to keep this isolated.

## 5. Known gaps (honest)

- The `-r` link has NOT been executed with the Prospero toolchain in this
  worktree by this worker; symbol-level surprises (e.g. Handler.cpp's
  `Output::send` reference) are only covered indirectly: the composed runtime
  links `output.o` in its tail, so Route A inherits those symbols; Route B
  must add `coherent-full-1/output.o` (+ `output-device.o`, `writer-device.o`,
  `fmt.o`) if the linker demands them.
- `PS5_PadData` layout statics (`sizeof==120`, field offsets) are asserted in
  PS5PadNative.cpp:7-9 at compile time — they will re-verify on the real
  target at build time, which is desirable for the first hardware run.
- No timing/sleep is inserted between polls (mirrors the runtime's per-tick
  cadence loosely; `scePadReadState` caching + the provider's timestamp guard
  make duplicate reads cheap). If the console route requires a rate cap, the
  caller wraps `modbridge_pad_probe_run` in its existing pacing, not the probe.

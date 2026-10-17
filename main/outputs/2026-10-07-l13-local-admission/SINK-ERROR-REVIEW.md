# L13 local sink/error correction

Scope: candidate sources and host checks only. No console contact, upload, launch, or pad change. The original l12 bridge still hashes to `19b43b4f204f938386282f9ea678cce12a9de043910ef1a14c5474c3befe87b8`.

## What the retained evidence establishes

`/tmp/l12-harvest/out-1791385434/supplier-current-session-101.jsonl` has a complete 64-byte Control D transfer at `0x081d2a40`, not a short pool transfer. The surviving filename is `modbridge-copyin-p00000000081d2a40-00000001`. The later pool attempt at `0x22c0000000` fails before any write (`sink=0`, raw diagnostic `0x7ffdfffe`). These are different addresses and operations.

The l12 source already used the same formatter and `/data/plugins/` parent for both Control D and pool probes, with `O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW`, mode `0600`. The target SDK `target/include/sys/fcntl.h` defines `O_WRONLY=0x1`, `O_NOFOLLOW=0x100`, `O_CREAT=0x200`, `O_TRUNC=0x400`; the exact create flags are therefore `0x701`. A missing `O_CREAT` is not this source's defect. `O_CREAT` does not create missing parent directories or preserve the meaning of an absolute path across a root/jail change.

The error conversion is a confirmed source defect: `int32_t(0x80020002)` is negative; the caller negated it into `0x7ffdfffe` before trying to match `0x80020000`. The same bug changes signed SCE EFAULT `0x8002000e` into `0x7ffdfff2`. The old unsigned-positive error shape could also be counted as transferred bytes. The observed `0x7ffdfffe` is consistent with SCE ENOENT, not proof of a consumed-channel sentinel or memory fault.

## Namespace lifetime and causal limits

Retained ploader source `outputs/2026-10-04-zcode-team/worker-staging-next/ploader-build/main.c` saves the game's root and jail directories at lines 316–317, changes both to its root vnode at 375–376, calls `inject_prx` at 866, then restores credentials and both directories (344–347, 871) before detaching. The candidate bridge's module-start flow starts an asynchronous runtime owner. Thus a pathname usable during injection cannot be assumed usable in later pool admission. This source supports the namespace-lifetime explanation; it does not by itself prove which lookup component failed in the already captured PID 101 run, nor prove the current console ploader binary has exactly this source.

The candidate retains `/data/plugins` as a read-only directory descriptor during module startup (`O_DIRECTORY|O_NOFOLLOW`; target flags `0x20100`). This is lifecycle resource capture: it creates no sink file, performs no copyin oracle, and reads no pool bytes. The eager `bridge_io_controls()` invocation immediately after `bridge_spanpub_publish()` is removed. That location now captures only the directory descriptor. VirtualQuery-first admission determines when the first lazy sink operation occurs.

Every later probe opens its safe generated relative basename through the retained parent using libkernel `_openat`. The basename includes the source address, a captured startup monotonic nanosecond nonce, PID, and probe counter. `O_EXCL` prevents collisions from reopening or truncating prior evidence even if those identifiers ever repeat. Candidate creation flags are `0xf01` (`0x701` from l12 plus `O_EXCL=0x800`); `O_TRUNC` is redundant on a fresh exclusive create. It closes the writer before reopening the same relative name, then reads back with `sceKernelRead`; an absolute `fopen` readback would repeat the namespace bug. No raw syscall, `chdir`, process-root modification, or guessed fallback directory is used. This does not guarantee that restored credentials retain permission to create in the held directory; any policy or open failure remains unavailable and is logged. Console behavior is unproved.

`_openat` and `sceKernelRead` are new direct libkernel imports. Retained export declarations are in `.superpowers/native-dependency-spike/ue4ss-public-import-declarations-1/sources/ps5-payload-dev/sdk/sce_stubs/libkernel.c` lines 278–280 and 3162–3164. `_openat` uses the `openat` variadic signature from SDK `target/include/fcntl.h:325`; the fourth mode argument is promoted to `int`. The parent task owns candidate packaging/import verification. These imports must resolve to libkernel; a raw local syscall substitute is unacceptable.

## Behavior and interfaces

- `units/CopyinSink.hpp`: production SCE/POSIX/direct-negative error decoder and bounded persistent fault ledger. It recognizes signed and zero-extended SCE bit patterns before arithmetic.
- Existing `bridge_kernel_write_copyin_probe(addr,len)` remains; new `bridge_kernel_write_copyin_probe_until(addr,len,absolute_steady_deadline_ms)` adds deadline checks around open, writes, and readback. The deadline shares the `std::chrono::steady_clock` milliseconds epoch with admission.
- New `bridge_copyin_range_faulted(addr,len)` retains write-stage EFAULT evidence. It conservatively excludes the entire original requested range, including a partially copied prefix; captured bytes cannot override that fault. A 64-entry ledger saturates to refuse, never discards old fault evidence.
- `bridge_write_copyin_capture(addr,...)` now accepts an interior address and returns the remaining captured prefix. Overlapping old captures are invalidated before a new attempt.
- Short writes advance by the actual count. Zero progress or a return larger than the requested chunk stops unavailable. A following signed/unsigned SCE EFAULT is unreadable; only genuinely transferred bytes contribute to `sink`/`stop`. Write EFAULT records relative offset and exact address. Sink-open/readback errors never become memory-fault evidence.
- Invalid/overflow ranges are rejected before I/O. A 64-probe process cap bounds retained evidence files. No unsupported relative-unlink primitive is invented; these evidence files remain on disk. This is a process cap, not a cross-launch cleanup claim.
- `units/CopyinStartup.hpp` wraps the actual startup capture/conversion/context/start chain in scope cleanup. An empty owner on any return or exception releases the retained parent, because failed startup never sets `runtime_active` and entry teardown then skips the bridge stop call. A live owner retains the descriptor, including on an exception, so it is not closed underneath initialization. Normal directory release follows a successful runtime stop/join, or a stop with no owner.

Deadline is a scheduling bound: no new sink operation starts once checked time reaches the deadline, late results do not publish captures, and mandatory closes still happen. A kernel call or diagnostic channel write already in progress is not preemptible, so this is not a hard wall-clock cancellation guarantee.

## Diagnostics

The exact pathname is encoded without another logger ABI: `pool_copyin_path_len`, followed by ordered `pool_copyin_path_word` values (four little-endian ASCII bytes each). Decode exactly the reported length. Probe address low/high, requested length, create flags, mode, and parent descriptor are emitted. `pool_copyin_open_raw` preserves raw return bits and `pool_copyin_open_errno` is normalized; read-open and directory-open have separate raw/normalized fields. `pool_copyin_probe_rc` now retains the original raw bits rather than the old negated magnitude. Consumers must not expect the mangled `0x7ffdfffe` sentinel.

## Host verification

Run `python3 outputs/2026-10-07-l13-local-admission/sink-tests/run_sink_tests.py` from the worktree. The script extracts the candidate bridge's marked production region verbatim, includes the actual production helper, compiles with `-std=c++17 -Wall -Wextra -Werror`, and runs local filesystem/kernel adapters. No mirrored decoder or probe implementation is under test.

- `sink-receipts/red-baseline.log`: 15 actual behavioral failures against the copied l12 production region before correction.
- `sink-receipts/red-retained-directory.log`: 16 failures including the missing retained-directory/deadline/fault-history feature contract before its implementation.
- `sink-receipts/red-startup-lifetime-and-names.log`: 5 behavioral failures before the review corrections: exclusive-create flags, cross-generation naming, collision refusal/evidence preservation, and missing startup cleanup.
- `sink-receipts/green-sink-tests.log`: 60 assertions pass, 0 failures. Signed/unsigned ENOENT/EFAULT, partial counts, partial then EFAULT, no-progress/oversize, interior captures, fault precedence, exact path/flags telemetry, overflow rejection, close-before-readback, expired and overrun deadlines, bounded files, and missing retained directory are covered. Review regressions exercise the production startup-scope helper with real directory descriptors on failed return, exception, successful owner transfer, and live-owner exception; they also verify distinct names after a simulated process generation resets the counter and unchanged evidence bytes after an exact-name collision.
- The sandbox-lifetime host fixture rejects absolute `/data/plugins/...` opens after a simulated root change, while the actual production relative creation and readback succeed through a previously held real host directory descriptor. This verifies the local design, not PS5 sandbox policy.

No full target build or console validation is claimed by this report. The parent task runs composition checks and integrates admission ordering.

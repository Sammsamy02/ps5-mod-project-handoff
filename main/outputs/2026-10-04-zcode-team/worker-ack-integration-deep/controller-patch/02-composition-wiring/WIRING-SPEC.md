# WIRING-SPEC — RuntimeAckConfig supplier at module_start (composition-3)

Audience: controller. Everything here is proposal-only until the controller
applies bundle 02 and composes. Facts marked MEASURED were verified in this
workspace; nothing has run on a console.

## S1. Who supplies the config

The module_start caller that will call
`PrepareActivatedUE4SSRuntime(context, activation, title, runtime_ack)` inside
the next composed runtime. It is the ONLY place a `RuntimeAckConfig` may be
born; the bootstrap never reads a global. The caller already owns the
`mfa_config` used for `mfa_activate`, so:

```
RuntimeAckConfig runtime_ack;
runtime_ack.status_root = mod_root;              // the EXACT mfa_config::root string
runtime_ack.pid        = static_cast<long>(getpid());
runtime_ack.now_ms     = now_ms_monotonic();
auto owner = PrepareActivatedUE4SSRuntime(std::move(context), std::move(activation),
                                           expected_title, runtime_ack);
owner->start();
...
owner->stop();   // emits stopped + closes the descriptor (point 5)
```

## S2. pid and now_ms (measured facts, no new imports)

- `pid` = `getpid()` (`<unistd.h>`). Precedent inside this very closure:
  composition-2's `lmf-sce-ops` (`live_module_os_ops_sce.cpp:108,210`) already
  calls `getpid()`. MEASURED: `getpid` is in the frozen import contract
  (`native-full-runtime-aligned-entry-1/import-contract.json`).
- `now_ms` = `clock_gettime(CLOCK_MONOTONIC)` converted to milliseconds —
  byte-for-byte the semantics of the vendored component's own default
  `posix_now_ms` (`worker-installer-ps5io/vendored/modforge_activation.c`,
  CLOCK_MONOTONIC, sec*1000 + nsec/1000). MEASURED: `clock_gettime` is in the
  frozen import contract. Keeping the value a parameter (as the diff does)
  preserves host testability; the supplier is the only caller-side clock.

Reference implementation of the helper (same shape as posix_now_ms):

```cpp
#include <ctime>
static std::uint64_t now_ms_monotonic() noexcept
{
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0; // degraded: header still records a value
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000u
         + static_cast<std::uint64_t>(ts.tv_nsec) / 1000000u;
}
```

## S3. status_root decision (aligns RUNTIME-ACK.md and the e2e sink)

`status_root` MUST be the exact mod root string the caller passed as
`mfa_config::root` when it called `mfa_activate` (e.g. `/data/modbridge/PPSA33286`).
The vendored `mfa_status_open` joins `runtime-status.txt` under it
(`O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC`), producing
`<mod root>/runtime-status.txt` — precisely the path worker-installer-ps5io's
RUNTIME-ACK.md §1 defines and worker-e2e-final's installer records read
(`running` = last complete state line == `mods-started`). Do NOT pass the
`activation_root` (native-entries installation root); it is a different
directory and only applies when native entries are listed.

Open timing: the diff opens the file inside `PrepareActivatedUE4SSRuntime`
after `MakeActivatedUE4SSModSource` validated the activation — during
module_start, while /data is still reachable (contract §3), and only when the
activation was actually consumed. An empty `status_root` (or a failed open)
leaves the session unacknowledged; startup is never blocked (host-proven T1,
T6, T8, A6, A8).

## S4. Session exclusivity rules (adversarial evidence)

- ONE sink per process. A5 (suite/adversarial_ack_test.cpp) demonstrates that
  two concurrently open sinks on the same root corrupt the status file
  mid-file (the stale writer punches a NUL hole past the new header; the
  reader then sees no state line at all). In-process re-entry is already
  refused upstream by `module_entry.c`'s `entry_guard` (`-20`), and a genuine
  new session happens in a NEW game process whose `mfa_status_open` replaces
  the file (A4 proves clean replacement with fresh session facts and seq
  restart).
- A failed session stays terminal: `stop()` after a failure adds no `stopped`
  line (T5, A2).
- Double/triple `stop()` and the destructor emit at most one `stopped` line
  (T2, A3, A9).

## S5. Order of integration (controller)

1. Apply bundle 01 to the canonical outputs bootstrap tree (or `patch -p1`
   with the enclosed `runtime-ack.diff`). Re-run `suite/build.sh` — it
   re-checks the drift gate first.
2. Apply bundle 02 files to the spike tree per REPLACE-MANIFEST.json
   (activated_mod_source stays untouched).
3. Extend `build_runtime.py` with the bounded bootstrap-replacement mechanism
   (see `build_runtime-bootstrap-replace.PROPOSAL.md`) and recompose
   (composition-3). Gate expectations: `undefined_delta.added == []` —
   MEASURED: every new symbol the ack-enabled units reference is either
   defined by the replaced pair itself, defined in the composition-2 closure
   (`mfa_status_*` T-symbols; `libc++.a` string.cpp.o `to_string`,
   string append/assign/insert — all present in `runtime.map`), or already in
   the frozen import contract. If the gate reports anything else: STOP and
   reconcile; never bypass.
4. Re-run `package_prx.py --verify-only` then `--link` (packaging re-run is
   the controller step; its inputs are treated as frozen here — see
   `../PACKAGE-PRX-INPUT-DECISION.md`: no input-list change required).
5. The module_start supplier (S1/S2/S3) is authored with the next game-bound
   module's caller unit and hash-attested like any extra unit.

# DIFF.md — r-s1 supplier diagnostic sink (worker-diagnostic-sink)

Worker: worker-diagnostic-sink, 2026-10-04/05. LOCAL ONLY: no web, no console,
no nested subagents. All artifacts under `outputs/2026-10-04-zcode-team/worker-diagnostic-sink/`.

## What is staged

- `staging/diagnostic.c` — the composition-6 packaging unit `diagnostic.c`
  (lineage baseline sha256 `931d79691b6363f046731182720ef7bec13a51c4a99e778943b199a97fb7d8b3`)
  plus ONE additive durable sink. sha256 of the staged file:
  `f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818`.
- `staging/diagnostic.supplier-sink.diff` — the unified diff reproduced below.
- No header changes, no signature changes, no call-site changes anywhere
  (bridge r13's 34 `::diagnostic_event` call sites and the entry's calls are
  untouched). Every pre-existing function in diagnostic.c is byte-unchanged;
  `diagnostic_event` gains exactly one trailing call.

## Unified diff

Baseline: `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/diagnostic.c`
(byte-identical to the comp6 overlay copy
`outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2/diagnostic.c`,
which is a symlink to the lineage file).

```diff
--- .superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/diagnostic.c
+++ outputs/2026-10-04-zcode-team/worker-diagnostic-sink/staging/diagnostic.c
@@ -1,7 +1,30 @@
 /* Diagnostic sink uses only the three kernel imports proven by the v2 probe. */
+/* r-s1 (2026-10-04, worker-diagnostic-sink): the lineage diagnostic.c
+   (baseline sha256 931d79691b6363f046731182720ef7bec13a51c4a99e778943b199a97fb7d8b3)
+   plus ONE additive durable sink for the composition-6 supplier runtime:
+   every diagnostic_event(tag, value) also appends
+     {"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,
+      "tag":"...","value":D,"value_hex":"0xHEX","t_ms":MS}
+   to /data/modbridge-supplier-diag.jsonl so launch-10 verification can read
+   the observed registration counts (bridge_lease_reg, bridge_lease_reg_spans,
+   bridge_vmentry_entries, ...) from a durable, attributable, machine-
+   parseable channel — launch-9's shared text log lost them to the pinned
+   marker collision + dead-channel silent-drop (see ANALYSIS.md).
+   IO pattern is the console-proven xwp writer (worker-writability-probe-build
+   src/record_sink.c + src/module_entry.c): lazy
+   sceKernelOpen(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600), bounded
+   partial-write retry, EVERY failure tolerated and NEVER fatal. No new
+   imports: the sink uses the same three kernel imports this TU already
+   binds; clock_gettime/getpid/__error are already in the frozen import
+   contract (verified present in import-contract.json and already undefined
+   in the comp6 combined-supplier.o). The shared entry channel below is
+   byte-unchanged: this sink is strictly additive. Signature and call sites
+   of diagnostic_event are unchanged. */
 #include <stddef.h>
 #include <stdint.h>
 #include <sys/fcntl.h>
+#include <time.h>
+#include <unistd.h>
 #include <probe_config.h>
 #include "diagnostic.h"
 #include "generated/import_names.h"
@@ -142,6 +165,187 @@
     return fd < 0 ? 0 : sceKernelClose(fd);
 }
 
+/* ---- supplier diagnostic jsonl sink (r-s1, additive, never fatal) ---- */
+#ifndef SUPPLIER_DIAG_PATH
+/* Overridable ONLY for host fixtures; console builds never define it. */
+#define SUPPLIER_DIAG_PATH "/data/modbridge-supplier-diag.jsonl"
+#endif
+#define SUPPLIER_DIAG_MARKER "modbridge.supplier-diag/v1"
+/* Fixture-driven choice (host-test attempt 3): the cap must exceed the whole
+   startup burst PLUS recovery headroom, else a transient early failure
+   (constructor-time events before /data is writable) would permanently lock
+   the sink — the exact silent-loss mode this sink exists to prevent. 64
+   matches the established bounded-retry constant family
+   (DIAGNOSTIC_WRITE_ATTEMPTS / IG_WRITE_ATTEMPTS). */
+#define SUPPLIER_DIAG_OPEN_ATTEMPTS 64u
+#define SUPPLIER_DIAG_WRITE_ATTEMPTS 64u
+#define SUPPLIER_TAG_MAX 96u
+
+static int supplier_fd = -1;
+static int supplier_open_errno;          /* last open failure, 0 = none */
+static unsigned supplier_open_attempts;  /* hard cap on open retries */
+static unsigned supplier_write_failures; /* counted, never fatal */
+static unsigned long long supplier_seq;  /* emitted-attempt counter */
+
+/* __error: identity row copied verbatim from the frozen contract (the xwp
+   ingame_abi.h spelling); errno word for the tolerated open failures. */
+extern int *__error(void);
+
+/* CLOCK_MONOTONIC milliseconds — the SAME time domain the vendored
+   mfa_status writer uses for runtime-status.txt "t=" values, so launch-10
+   can correlate the two channels. Failure tolerated (t_ms=0). */
+static unsigned long long supplier_t_ms(void)
+{
+    struct timespec ts;
+    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
+        return 0;
+    return (unsigned long long)ts.tv_sec * 1000ull
+         + (unsigned long long)ts.tv_nsec / 1000000ull;
+}
+
+static void supplier_append(char *buffer, size_t *used, size_t capacity,
+                            const char *text)
+{
+    while (*text && *used < capacity)
+        buffer[(*used)++] = *text++;
+}
+
+static void supplier_append_ch(char *buffer, size_t *used, size_t capacity,
+                               char c)
+{
+    if (*used < capacity)
+        buffer[(*used)++] = c;
+}
+
+static void supplier_append_u64(char *buffer, size_t *used, size_t capacity,
+                                unsigned long long value)
+{
+    char digits[24];
+    unsigned n = 0;
+    if (value == 0) {
+        supplier_append_ch(buffer, used, capacity, '0');
+        return;
+    }
+    while (value > 0 && n < sizeof(digits)) {
+        digits[n++] = (char)('0' + (value % 10ull));
+        value /= 10ull;
+    }
+    while (n > 0)
+        supplier_append_ch(buffer, used, capacity, digits[--n]);
+}
+
+static void supplier_append_hex32(char *buffer, size_t *used, size_t capacity,
+                                  uint32_t value)
+{
+    static const char digits[] = "0123456789abcdef";
+    int shift;
+    supplier_append(buffer, used, capacity, "0x");
+    for (shift = 28; shift >= 0; shift -= 4)
+        supplier_append_ch(buffer, used, capacity, digits[(value >> shift) & 15]);
+}
+
+static void supplier_append_tag(char *buffer, size_t *used, size_t capacity,
+                                const char *tag)
+{
+    static const char hexd[] = "0123456789abcdef";
+    unsigned i;
+    if (tag == NULL)
+        return;
+    for (i = 0; tag[i] != '\0' && i < SUPPLIER_TAG_MAX; ++i) {
+        unsigned char c = (unsigned char)tag[i];
+        if (c == (unsigned char)'"' || c == (unsigned char)'\\') {
+            supplier_append_ch(buffer, used, capacity, '\\');
+            supplier_append_ch(buffer, used, capacity, (char)c);
+        } else if (c < 0x20 || c >= 0x7f) {
+            supplier_append(buffer, used, capacity, "\\u00");
+            supplier_append_ch(buffer, used, capacity, hexd[(c >> 4) & 15]);
+            supplier_append_ch(buffer, used, capacity, hexd[c & 15]);
+        } else {
+            supplier_append_ch(buffer, used, capacity, (char)c);
+        }
+    }
+}
+
+/* Lazy open per the xwp pattern; bounded retries; every failure recorded in
+   supplier_open_errno and tolerated. Startup is single-threaded (the entry
+   guard serializes module_start/module_stop), so no lock is taken. */
+static int supplier_open(void)
+{
+    int fd;
+    if (supplier_fd >= 0)
+        return supplier_fd;
+    if (supplier_open_attempts >= SUPPLIER_DIAG_OPEN_ATTEMPTS)
+        return -1;
+    ++supplier_open_attempts;
+    fd = sceKernelOpen(SUPPLIER_DIAG_PATH,
+                       O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
+    if (fd < 0) {
+        int *e = __error();
+        supplier_open_errno = e ? *e : 0;
+        return -1;
+    }
+    supplier_fd = fd;
+    return fd;
+}
+
+static void supplier_write_event(const char *tag, uint32_t value)
+{
+    char buffer[512];
+    const size_t capacity = sizeof(buffer) - 2;
+    size_t used = 0, offset = 0;
+    unsigned attempts = 0;
+    int fd;
+    /* Same binding guard the entry's marker writer uses: with unproven
+       kernel bindings NOTHING is written and nothing can fail. */
+    if (!diagnostic_ready())
+        return;
+    fd = supplier_open();
+    if (fd < 0)
+        return; /* tolerated: line dropped, entry channel untouched */
+    /* Build the WHOLE line before writing (single-append semantics like
+       ig_sink_write_record): a partial write can only ever leave a prefix
+       in the file when sceKernelWrite itself partially succeeds, never a
+       locally truncated JSON object. supplier_seq counts emitted attempts;
+       a failed write consumes its seq (visible gap = failed write). */
+    ++supplier_seq;
+    supplier_append(buffer, &used, capacity,
+                    "{\"marker\":\"" SUPPLIER_DIAG_MARKER "\",\"schema\":1,\"seq\":");
+    supplier_append_u64(buffer, &used, capacity, supplier_seq);
+    supplier_append(buffer, &used, capacity, ",\"pid\":");
+    supplier_append_u64(buffer, &used, capacity, (unsigned long long)getpid());
+    supplier_append(buffer, &used, capacity, ",\"tag\":\"");
+    supplier_append_tag(buffer, &used, capacity, tag);
+    supplier_append(buffer, &used, capacity, "\",\"value\":");
+    supplier_append_u64(buffer, &used, capacity, (unsigned long long)value);
+    supplier_append(buffer, &used, capacity, ",\"value_hex\":\"");
+    supplier_append_hex32(buffer, &used, capacity, value);
+    supplier_append(buffer, &used, capacity, "\",\"t_ms\":");
+    supplier_append_u64(buffer, &used, capacity, supplier_t_ms());
+    if (supplier_open_errno != 0) {
+        /* Self-reporting: the first line written after a (recovered) open
+           failure carries the recorded errno, then stops repeating. */
+        supplier_append(buffer, &used, capacity, ",\"open_errno\":");
+        supplier_append_u64(buffer, &used, capacity,
+                            (unsigned long long)supplier_open_errno);
+        supplier_open_errno = 0;
+    }
+    supplier_append(buffer, &used, capacity, "}\n");
+    /* Integrity gate: emit valid JSON or NOTHING. */
+    if (used < 3 || buffer[used - 1] != '\n' || buffer[used - 2] != '}')
+        return;
+    while (offset < used && attempts < SUPPLIER_DIAG_WRITE_ATTEMPTS) {
+        int64_t written = sceKernelWrite(fd, buffer + offset, used - offset);
+        ++attempts;
+        if (written <= 0 || (uint64_t)written > used - offset) {
+            ++supplier_write_failures;
+            return; /* tolerated: this line dropped, later events retry */
+        }
+        offset += (size_t)written;
+    }
+    if (offset != used)
+        ++supplier_write_failures;
+}
+
 void diagnostic_event(const char *tag, uint32_t value)
 {
     char buffer[512];
     size_t used = 0;
     append(buffer, &used, sizeof(buffer) - 2, "[" PROBE_TITLE "] " PROBE_MARKER " ");
     append(buffer, &used, sizeof(buffer) - 2, tag);
     append(buffer, &used, sizeof(buffer) - 2, " value=");
     hex(buffer, &used, value);
     write_line(buffer, used);           /* shared entry channel: UNCHANGED */
+    supplier_write_event(tag, value);   /* r-s1 additive durable sink */
 }
```

(The `staging/diagnostic.supplier-sink.diff` file in this dir is the same diff
generated by `diff -u`; it is authoritative if this embedding ever drifts.)

## Which build unit this belongs to

The **packaging bootstrap unit `diagnostic.c`** — one of the ten
`BOOTSTRAP_UNITS` of the pinned packaging script
`outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py` (line 138),
compiled by its `link_pipeline` (line ~657) as:

```
compiler = PIN["sdk"]/bin/prospero-clang        # .c → prospero-clang
b.run([compiler, *CLANG_FLAGS, *includes, "-c", src, "-o", build/"diagnostic.o"])
CLANG_FLAGS = -Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
              -fvisibility=hidden -ffreestanding -fno-builtin
includes    = -I overlay/generated -I overlay/probe -I overlay/lifecycle
              -I overlay -I lua_include
src         = PIN["lineage"] / "diagnostic.c"    # overlay symlink today
```

It is NOT a composition runtime unit: `--replace-runtime-unit` is the §3.2
retained-unit mechanism, allow-listed to `{"program"}` — it does NOT apply.
It is NOT an `--extra-unit` (those are composed-object units). The overlay
`--stage-unit`/`--stage-header` knobs REFUSE to shadow `diagnostic.c`
(`build_overlay` raises on any existing/symlinked overlay path).

## How the controller wires it in (exact)

One bounded revision of the OUTER driver
`outputs/2026-10-04-zcode-team/worker-comp6-driver/build_runtime_composition6.py`
(NOT sha-pinned; the pinned files — `build_runtime_composition6_fixed.py` and
`package_prx.py` — stay byte-identical), using the SAME in-memory injection
class the driver already uses for the entry swap and the `-std=c++20` flag
injection:

1. Constants (near `BRIDGE_DEFAULT`):
   ```python
   DIAG_SINK_DEFAULT = TEAM / 'worker-diagnostic-sink/staging/diagnostic.c'
   DIAG_SINK_MEASURED_SHA = ('f5e96c41c4a96d6032269aee671d8fe12c695db'
                             '749cdd75792fc2ca3e2d62818')
   ```
2. Arguments:
   ```python
   ap.add_argument('--diagnostic-source', type=Path, default=DIAG_SINK_DEFAULT,
                   help='supplier diagnostic-sink edition of the packaging '
                        'bootstrap unit diagnostic.c (worker-diagnostic-sink)')
   ap.add_argument('--diagnostic-sha256', default=DIAG_SINK_MEASURED_SHA)
   ```
3. In `package_phase`, immediately after the existing
   `units[units.index(OLD_ENTRY_UNIT)] = NEW_ENTRY_UNIT` block:
   ```python
   if 'diagnostic.c' not in units:
       raise SystemExit('packaging script BOOTSTRAP_UNITS drifted; expected diagnostic.c')
   check_sha(Path(args.diagnostic_source), args.diagnostic_sha256,
             'supplier diagnostic sink')
   units[units.index('diagnostic.c')] = str(Path(args.diagnostic_source).resolve())
   ```
   (`check_sha` already exists in the driver.) Pathlib semantics make the
   substitution transparent to BOTH consumers of the units list:
   - the hash-evidence loop (`src = PIN["lineage"] / name` — an absolute
     right-hand side resolves to the absolute path, `is_file()` holds), and
   - the compile loop (`src, stem = lin / name, Path(name).stem` → the object
     stays `build/diagnostic.o`, unit stem unchanged; includes still resolve
     `probe_config.h`, `generated/import_names.h`, `diagnostic.h` from the
     overlay).
4. Run the driver unchanged otherwise (`--entry-source/--bridge-source` and
   the launch-10 candidates as the controller already plans). To fall back to
   the shipped baseline sink: `--diagnostic-source <lineage>/diagnostic.c
   --diagnostic-sha256 931d7969...` — or a conscious driver tweak; the gate
   refuses silently shipping the wrong copy because the sha is checked at
   run time and the packaging receipt records the source sha.

NEVER edit the lineage file in place: it is shared with the frozen
aligned-entry build evidence and the overlay symlinks resolve to it.

## Compile gate (already rehearsed with the exact console toolchain)

The exact packaging compile argv (prospero-clang, the frozen CLANG_FLAGS,
the lineage include set) was executed host-side against the staged file —
PASS, `-Werror` clean:

- receipt object: `fixtures/receipts/diagnostic-console-gate.o`
  (sha256 `d14d0ac238f7713c41b3e1c0668cf904b19b2618e7c370f9f9fb11bbbe9eb32a`),
  compiled with SDK `ps5-payload-sdk@v0.43` `prospero-clang` +
  `LLVM-21.1.8-macOS-ARM64`, `-target x86_64-sie-ps5` wrapper flags,
  `-I <lineage>/generated -I <lineage>/probe -I <lineage>/lifecycle
  -I <lineage> -I .../LuaRaw/include`.
- SDK wrapper caveat (pre-existing, not introduced by this file): the SDK's
  `bin/clang` is a broken COPY of the LLVM_BINDIR dispatcher (needs
  `LLVM_BINDIR` + `llvm-clang`/`llvm-config` names); the rehearsal shimmed a
  temp bin dir. If the real launch-10 packaging pass hits the same wrapper
  error, that is the environment issue the ledger already flags separately —
  reconcile it there, never by editing the shared SDK.
- Symbol delta vs the shipped `prx-candidate-composition6-2/build/diagnostic.o`
  global/undefined set: exactly `{__error, clock_gettime, getpid}` — all
  three are in the frozen 191-import contract (`import-contract.json`)
  AND already undefined in the candidate's `combined-supplier.o`, so the
  §3.3 combined-unresolved pre-check expectation set is UNCHANGED (zero new
  imports, zero contract edits, no EXTENDED_IMPORTS rows).

## Interaction with the shared channels (additive-only)

- `/data/modbridge-supplier-diag.jsonl` is a NEW path; no writer for it
  exists anywhere in the repo today (grepped outputs/ and .superpowers/ —
  only planning docs mention the intended name).
- The shared entry log (`PROBE_LOG_PATH` = `/data/townfall_runtime_aligned_entry.log`)
  behavior is byte-unchanged (host-proven: 205/205 legacy lines during the
  fixture run, identical format and ordering). The sink does NOT write
  UE4SS.log and does NOT touch runtime-status.txt.
- The sink opens ONE extra fd for process lifetime (same policy as the
  existing `channel_fd`); no close path added, none needed.

# REPORT.md — installer composition recipe (worker-installer-composition)

Origin: worker-installer-composition, 2026-10-04 (ZCode team, PS5 mod-manager).
Everything here was built and verified host-side only; nothing ran on a
console and nothing was uploaded anywhere.

## 1. What was delivered

| Artifact | Path (relative to this dir) | Status |
|---|---|---|
| Controller build script | `compose_installer.py` | works: `--flavor host`, `--flavor console`, `--flavor all` |
| Console payload ELF candidate | `composition/build/console/modbridge-installer.payload.elf` | 647928 bytes, linked with `-Wl,--no-undefined,-z,now`, dynamic layout validated (21 dynamic entries, terminal DT_NULL) |
| Import provider check | `composition/receipts/console-import-providers.json` | 116 dynamic imports, **0 missing** — every one defined by a pinned SDK target lib |
| Host composition binary | `composition/build/host/modinst_composed` | same adapted core over `PS5IO_HOST=1` |
| Host smoke test | `test/smoke_host.sh` | **PASS=26 FAIL=0** (log: `composition/receipts/smoke-host.log`) |
| Owned adapters | `owned/ps5_io_dir.{h,c}`, `owned/e2e_status.{h,c}`, `owned/modinst_archive_prepare.{h,c}` | the gap binding, the JSONL interface, the stage-2a command |
| Provenance receipts | `composition/receipts/` | vendor manifest (sha256 per input, cross-worker ABI/copy equality checks), patch chain, build commands, provider tables |

Rebuild from scratch: `python3 compose_installer.py --flavor all && sh test/smoke_host.sh`.
The script re-vendors from the original worker/archive dirs every run,
applies the recorded patch chain (exact anchors, occurrence-count verified),
and fails loudly on any drift.

## 2. Composition map

One link configuration, two flavors:

```
worker-7 modinst.c ──vendored copy──> modinst_core.c
   │  (patch chain: bio_* -> ps5io_*; rm_rf/recover enumeration ->
   │   ps5io_dir_*; open/stat/rename/unlink/fsync/clock -> ps5io_*;
   │   e2e JSONL hooks; `prepare-archive` dispatch + flag consumption)
   ├─ worker-installer-ps5io ps5_io.{c,h}        (unmodified vendored)
   ├─ worker-installer-ps5io ps5_prepare_bridge  (vendored + 1 recorded
   │   corrective patch, see §4 finding F1)
   ├─ owned ps5_io_dir.{c,h}                     (gap binding, §3)
   ├─ owned modinst_archive_prepare.{c,h}        (stage 2a command)
   ├─ owned e2e_status.{c,h}                     (bounded JSONL)
   ├─ worker-7 modforge_activation.c + headers   (unmodified vendored)
   └─ archive closure intake.c + private/posix_compat.c
        (compile contract identical to archive/verify.py)

console link (exactly verify.py's native assembly):
  prospero-clang -resource-dir <pinned LLVM 21 resource>
    <9 objects> libarchive.a -> liblzma.a -> libz.a -> libmbedcrypto.a
    -Wl,--no-undefined,-z,now -Wl,-T,<caller-local dynamic layout script
    from archive/link_layout.py (never edits the SDK)
  codec selection validated by archive/dependency_build.py
  prepare_archive("native", profile) (3.8.7 / 5.8.4 / 1.3.2 pinned hashes);
  native mbedcrypto from the pinned task1-fix1 native artifact.

host link: /usr/bin/clang, same 9 objects + host closure
  (prepare_archive("host", ...) + pinned host libmbedcrypto.a), PS5IO_HOST=1.
```

Deviation from verify.py, recorded in `receipts/console-build.json`: the
adapted installer TUs compile with `-Wall -Wextra` but not
`-Wpedantic -Werror` (those stay on the closure TUs `intake.c` /
`posix_compat.c`). Nothing else differs.

Import verification (c): `llvm-nm -D --undefined-only` over the ELF; every
name must appear `--defined-only` in some pinned `target/lib/*.so`
(the worker-installer-ps5io `build_console.sh` pattern). Result: 37 symbols
from the libkernel family (the full sceKernel file set + getpid + the rmdir
BSD shim), 79 from the libc family (stdio/string/malloc/opendir/readdir/
closedir), 0 unresolved. Any future unresolvable symbol aborts the build
with the exact names.

## 3. The directory-enumeration gap — bound, with receipts

Needed by the core: `rm_rf` (staging/prev cleanup), journal-directory scan
(`recover_all`), and rmdir. Verified against the pinned SDK (llvm-nm -D over
every `target/lib/*.so`):

- **Missing exports (the blocked pure-sceKernel route):** `sceKernelDopen`,
  `sceKernelDread`, `sceKernelDclose` are defined by **no** pinned target
  lib. `sceKernelGetdirentries` IS exported (libkernel family) and declared
  by the pinned SharpProspero `Interop/Kernel/KernelFile.cs:187`, but the
  pinned set declares **no dirent struct layout** (no SceDirent in any SDK
  header), so decoding its buffer would be an unverifiable layout guess.
- **Chosen binding (nm-verified):** the SDK's own `target/include/dirent.h`
  (BSD layout) + libc `opendir`/`readdir`/`closedir`
  (libSceLibcInternal/libc_stub_weak/libScePosixForWebKit) + `rmdir`
  (libkernel family) — the same provider class already relied on for
  memcpy/snprintf, and the route the SDK's own `samples/list_files` payload
  uses. `ps5io_dir_rm_rf` is stat-free (never follows symlinks: it only
  unlinks readdir results and rmdirs opendir-proven directories) and treats
  ENOENT as gone, matching worker-7's host semantics; `ps5io_dir_each_json`
  is recover_all's enumeration rebound.

Honesty: these are link-time verifications. Whether the console runtime's
libc actually serves opendir/readdir usefully is a target question the
console E2E run answers (the E2E plan's stage-3 rollback probes exercise
exactly this path).

## 4. Findings surfaced by composing (cross-worker, action needed)

- **F1 (fixed by recorded patch, upstream should adopt):** the vendored
  `ps5_prepare_bridge.c` gated success on `inventory.installation_supported`,
  but intake ABI v1 keeps that field **reserved-zero**
  (`archive/groups.c:263` rejects nonzero; `archive/tests/corpus_test.c:50`
  asserts `==0`). Against the real 3.8.7 closure every prepare would fail
  with `PS5_PREPARE_E_SCAN` (the bridge's host stubs faked `1`). My patch
  removes the inverted gate on the vendored copy;
  `receipts/patches.json` -> bridge. worker-installer-ps5io should fix
  upstream and re-run its host tests against a reserved-zero stub.
- **F2 (NOT fixed; upstream worker-7):** `plan_report`'s requirement loop
  (`for (j...) if (mfx_cmp(...)==0) { found = j; break; } if (!found)
  blocker;`) treats index 0 as "not found": a requirement satisfied by the
  first selected mod reports a false blocker. Avoided in the smoke fixtures
  (no `--requires`); worker-7 should use a separate boolean.
- **F3 (behavior note):** the real intake demands the quarantine directory
  have mode & 077 == 0 and be empty and owned by euid (`intake.c:411-413`);
  the owned prepare command creates it 0700. Any controller reuse must too.

## 5. Host smoke test evidence (what "works" means here)

`test/smoke_host.sh` (26/26): baseline records; two fixture bundles; stage 1
register (downloaded); **stage 2a prepare-archive over the real pinned
3.8.7 closure** on saved corpus `minimap-v3.1.zip` (quarantine members +
`manifest-v1.bin` present, journal retained); stage 2b prepare (ready 1);
stage 3 install (staged files, loadlist, journal gone); stage 4/5 simulated
runtime ack (`activated` -> running=0, `mods-started` -> running=1, derived
only from the status file); rollback probes (graceful `--cancel-at rename1`
restores the old set byte-for-byte; hard `--crash-at staged` exits 86 and
`recover` rolls the staging back, `.next` gone); stage 6 uninstall (file
removed, mod back to prepared); bounded JSONL `e2e-status.jsonl` with
begin/end lines per stage. Override the zip with `CORPUS=<path>` (e.g.
`infinite-stamina-v1.zip`; `leon-2k-v1.1.zip` also works but scans 100 MB).

## 6. Exact remaining gaps before the console E2E run

1. **Download owner hookup (E2E stage 1):** the owner interface must place
   archives at `/data/modbridge/intake/<id>.zip` (content-addressed, sha256
   recorded) and register the mod (`register-mod`, state `downloaded`).
   Nothing in this composition downloads.
2. **Runtime ack wiring (RUNTIME-ACK.md §4):** the injected runtime still
   emits no `mfa_status_*` lines — the controller must add the five
   module_start emissions (activated/engine-ready/mods-started/failed/
   stopped). Without it stage 5 can never report `running=1` (honest fail).
   `simulate-runtime` covers the file-format path only, host-side.
3. **UE4SS module presence:** mods with `lua_mod` roles install files, but
   in-game startup requires the deferred UE4SS runtime component actually
   present and enumerable from the activation views; capability table rows
   (`state/caps.tsv`) gate `planned`/`integrating` frameworks today.
4. **prepare-archive -> records wiring:** the owned command reports the
   prepared quarantine (stdout + JSONL) but does not yet write the
   downloaded->prepared transition into worker-7's records line store;
   the controller either wires that or uses the bundle flow for stage-2
   state transitions during the first E2E.
5. **Console verification of the libc dirent route** (§3) and of
   fsync/durability semantics (`ps5_io` HONESTY note) — answered only by
   the target run.
6. **Deploy path:** payload delivery via the pinned SDK loader path onto a
   jailbroken PS5 with `/data/modbridge` writable (E2E precondition C4);
   nothing here packages or uploads the ELF.

## 7. Controller run sequence

```sh
# 1. compose (host + console) and verify imports
cd outputs/2026-10-04-zcode-team/worker-installer-composition
python3 compose_installer.py --flavor all
#    -> receipts/console-import-providers.json must end with "missing": []
# 2. host regression of the composed configuration
sh test/smoke_host.sh            # expect: SMOKE RESULT: PASS=26 FAIL=0
# 3. deploy composition/build/console/modbridge-installer.payload.elf via
#    the pinned loader path (controller-owned; this worker never uploads)
# 4. execute CONSOLE-E2E-PLAN.md stages 0-6 in one uninterrupted operator
#    sequence, with gaps §6.1/§6.2 closed first; watch <state>/e2e-status.jsonl
#    alongside records for stage-level outcomes (a missing "end" line means
#    the command died — by design, never fabricated).
```

Command surface of the composed binary (both flavors):
`records | register-mod | prepare | prepare-archive --archive F --quarantine D
| install [--mods|--remove] | simulate-runtime | recover | make-bundle`,
plus `--crash-at/--cancel-at/--cancel-after` probes from worker-7.

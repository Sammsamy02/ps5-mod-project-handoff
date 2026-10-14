# REPORT.md — worker-odr-emitter-check

Delivers the mechanical post-build ODR emitter check for the composition-6 one-pass
build, implementing ODR-SAFETY.md (worker-odr-header-safety) section 3 invariant +
section 6 remediation item 6. All work under
`outputs/2026-10-04-zcode-team/worker-odr-emitter-check/`; read-only elsewhere.

## Deliverables

| File | What |
|---|---|
| `CLAIM.json` | pre-registered intent (written first) |
| `check_odr_emitters.py` | the tool: stdlib-only, ELF64/mmap, parses the run dir's `runtime.map` + composed object + all map input objects; executes nothing |
| `receipt-run3-pass.json/.stdout/.stderr` | host test on composition-4 run-3 — **PASS, exit 0** |
| `receipt-run3-neg-progfresh.*` | `--require-program-fresh` on run-3 — **FAIL exit 1** (A5: program.o fresh=False) |
| `receipt-run3-neg-strict.*` | `--strict-inline-bodies` on run-3 — **FAIL exit 1** (B4b: request_shutdown divergence) |

## Usage

```
python3 check_odr_emitters.py RUNDIR [--json PATH] [--require-program-fresh] [--strict-inline-bodies]
```
Exit 0 = PASS, 1 = FAIL, 2 = env error. Composed object auto-detected as the single
>8 MB `*.o` in the run dir (run-3: `coherent-tick-file-witness-runtime.o`).

## One-line integration for `outputs/2026-10-04-zcode-controller/comp6_one_pass.py`

Insert after the build step produces the fresh run dir (its `run()` helper already
dies on nonzero exit, which is the gate behavior):

```python
run('odr-emitter-check', [sys.executable, TEAM / 'worker-odr-emitter-check/check_odr_emitters.py', RUNDIR, '--require-program-fresh', '--json', TEAM / 'worker-odr-emitter-check/receipt-one-pass.json'])
```

(Controller owns that file; not edited from this worker's scope. Keep
`--require-program-fresh` for the one-pass run — that is the FINDING-4 gate.)

## What the tool checks (16 hard checks)

* **(a) Validate stale-inline** — `A1` map COMDAT survivor of
  `.text._ZN2RC8Platform28ValidateNativeProgramContextE…` == exactly `[program.o]`;
  `A2` every per-unit emitter's `.text.<mangled>` body byte-identical; `A3` retained
  emitters ⊆ {program.o}; `A4` composed object defines exactly 1 such symbol and the
  section its symtab entry indexes hashes equal to the emitter body; `A5`
  (`--require-program-fresh`) program.o is a fresh run-dir unit.
* **(b) 13 audited retained units** (cppmod, cppmod-base, luamod, settings,
  program-signatures, program-object-to-string, 3×generator, lua-library,
  csv_data_commands, native-jmap, native-ownedmappedresolvercollector) — `B1` zero
  defined symbols naming `20NativeProgramContext`/`17NativeLoaderFacts`/D2 ctors;
  `B2` their `native_mod_source` emitters ⊆ {luamod.o, settings.o,
  program-signatures.o}; `B3` no map section of theirs names the audited types
  except NMS text/relocs; `B4a` (HARD) retained emitters of each UE4SSProgram
  offset-sensitive inline byte-identical among themselves; `B4b` (advisory,
  `--strict-inline-bodies` promotes to HARD) fresh copies byte-match the retained
  hash — this is the mechanical form of the insertion-point argument: if the new
  header shifted a member those bodies read, the fresh program.o copy diverges from
  the retained copies and the gate fails.
* **(c) NativeLoaderFacts/native_mod_source** — `C1` `24MakeNativeProgramContext`
  defined by zero retained units; `C2` `17NativeLoaderFacts` token in zero retained
  symbols; `C3` all `native_mod_source` bodies identical; `C4` map survivor ∈
  allow-list; `C5` composed object defines exactly 1 with emitter-equal bytes;
  `C6` each MakeNativeProgramContext overload in the composed object carries the
  fresh native-loadercontext.o bytes.
* **(d) D2 ctors** — `D1` no standalone `PsMappedImageCallerC1/C2` anywhere;
  `OwnedMappedImageCallerC1/C2` confined to fresh native-ownedmappedscansource.o.
* **(e) `E1`** general retained sweep: retained `20NativeProgramContext` definers
  == {program.o} exactly — catches any future retained contact beyond the 13.

Composed-object byte proofs resolve symbol → section index
(SHN_XINDEX/SHT_SYMTAB_SHNDX, 201806 sections in run-3) because the linker's
composed section-name strings are not reliable.

## Host test on composition-4 run-3 — PASS (exit 0)

380 composition units (15 fresh + 365 retained/pinned), 197548 map input refs,
8548 linker-internal inputs. Key facts:

* Validate survivors: `[program.o]`; TRUE per-unit emitters
  {program.o, native-loadercontext.o, bootstrap-ue4ss_backend.o} — all bodies
  `e9b1ac1d…` identical; composed section sha == emitter body sha.
* `native_mod_source` TRUE emitters {luamod.o, settings.o, program-signatures.o,
  program.o} — the map shows only the COMDAT survivor (luamod.o). **The map alone
  under-reports emitters; the symbol-table scan is the real emitter set.** All four
  bodies `8823328e…` identical — the offset-safety of the retained reads is proven
  byte-for-byte.
* `MakeNativeProgramContext` (both overloads): defined only by fresh
  native-loadercontext.o; composed bytes == its bytes. Zero retained contact.
* B1/E1: exactly zero retained type-token definers outside program.o — the audited
  13 are clean; program.o is the only stale-layout contact and comp6 replaces it.
* Offset-sensitive inlines: can_process_events / is_event_loop_thread /
  are_mods_being_touched / get_current_imgui_context — retained hash == fresh
  bootstrap + native-programfactory hash, byte-identical.

### Advisories on run-3 (expected composition-4 shape, not failures)

1. program.o is RETAINED — comp6 must replace it (FINDING-4);
   `--require-program-fresh` is the post-one-pass gate (negative receipt proves it
   fails run-3).
2. retained program.o also emits native_mod_source — becomes fresh at replacement;
   C3 identity covers offset-safety either way.
3. B4b: `request_shutdown` fresh bootstrap body (`57b7afbb…`) ≠ retained program.o
   body (`d56491a2…`). Hex-diffed: identical member-offset constants
   (0x240/0x250/0x471); the only difference is call plumbing (bootstrap direct
   `e8 rel32` vs program GOT-indirect `ff 15`, +3 bytes). Same vintage (VAL bodies
   match), so this is TU-flag codegen, not a layout shift — hence B4b is advisory
   by default and `--strict-inline-bodies` promotes it for runs whose compile flags
   are pinned.

### What the checks prove vs cannot

Prove: no retained unit emits any definition whose symbol names the audited types
or D2 ctors; every layout-sensitive body actually linked (Validate, NMS,
offset-sensitive inlines) is byte-identical across ALL emitters — retained vintage
== fresh vintage; the composed object carries exactly those bytes (symbol→section
hash). Cannot: detect stale field-offset immediates inside sections whose symbols
name neither the audited types nor an offset-sensitive inline (non-inline helpers
compiled elsewhere) — that residue remains covered by the audit's source-grep +
insertion-point argument. Allow-lists key on basename (documented conflation limit).

The real proof target is the post-one-pass fresh run: same command with
`--require-program-fresh` must PASS there — run-3 passing only proves the shape.

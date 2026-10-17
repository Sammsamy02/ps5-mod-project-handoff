# CAP13 addendum — genericity audit findings + first Mac E2E proof results

Agent KF (ZCode), 2026-10-13. Addendum to `DESIGN/native-dll-loader.md` (JH) —
**read-only w.r.t. that design and the staged sources**; everything here is
proposal + measured evidence. Mac-only; no console contact.

Companion documents:
- `REPORT/GENERICITY-AUDIT.md` — the audit (findings F1–F19).
- `REPORT/E2E-PROOF.md` — the first execution of the loader on real PE bytes.
- `REPORT/UNSEEN-MOD-TEST.md` — generalization over previously-unseen DLLs.
- `REPORT/evidence/e2e-run-<timestamp>.log` — raw run logs.
- `e2e/` — the Mac host (`e2e_host.cpp`, `build_and_run.sh`).

---

## A.1 (F1) Fix the `_CxxThrowException` shim row key

`win_api_shim.cpp` registers the fail-closed row under `"CxxThrowException"`;
every real MSVC DLL imports `_CxxThrowException` (with the leading underscore —
verified in the import tables of `cap13-feature-probe.dll` and the real
UE4SS-distribution binaries). As staged, any such mod false-refuses at load.
Fix: rename the row key. One line; no contract change.

## A.2 (F2) Empty base-relocation directory + nonzero slide

`pe_load` skips relocations when the directory is empty and proceeds even with
a nonzero slide. Windows semantics make such images base-locked; the E2E
showed the sampled lld-link DLLs happen to be position-independent (RIP-relative
codegen, no absolute pointers in data), so they execute correctly at any base —
but that is a per-file property the loader does not verify. Recommendation:
when `slide != 0 && DataDirectory[BASERELOC].Size == 0`, either refuse or emit
a named load-time diagnostic with an explicit `allow_no_relocs` option
(default refuse); also honor `IMAGE_FILE_RELOCS_STRIPPED` (COFF characteristic
0x0001). Deciding which is an integrator call — this addendum only states the
gap precisely.

## A.3 (F20, new) Refine the static-TLS refusal: EMPTY vs REAL TLS directories

**Measured during the unseen-mod runs.** The refusal rule (design §3e) refuses
any image whose TLS data directory is present. Ground truth from the sampled
corpus:

| Image | TLS dir | StartAddressOfRawData == EndAddressOfRawData | AddressOfCallBacks | Classification |
|---|---|---|---|---|
| every `cap13-mod-*.dll` (lld-link, 2026-10-13 build) | present (40 B) | yes (0..0) | 0 | **EMPTY** |
| `cap13-feature-probe.dll` (lld-link) | present (40 B) | yes | 0 | **EMPTY** |
| real `UE4SS.dll` v3.0.1 (MSVC) | present (40 B) | no (0x2d8 bytes of TLS data) | non-null callback array | **REAL** |

An EMPTY directory contains no thread-local data and no callbacks: the image
cannot reach `gs:[0x58]`/`gs:[0x30]` for its own TLS because it has none — the
directory is a linker artifact (lld-link emits it when `_tls_index`-class
symbols exist). The refusal is therefore **correct but over-broad**: as staged
it refuses this entire class of small DLLs for a wall they never touch.

Recommended refinement (one predicate in `pe_loader.cpp`, before the refusal):
```c
// design 3e, refined: refuse TLS that can actually reach the TEB.
bool tls_reaches_teb =
    tls.StartAddressOfRawData != tls.EndAddressOfRawData ||  // has TLS data
    tls.AddressOfCallBacks != 0;                             // has callbacks
```
- EMPTY dir → admissible, with a load-time diagnostic ("image carries an empty
  TLS directory (linker artifact); no TEB access possible from it").
- REAL dir → the existing named refusal stands.

The distinction is mechanical, uses only already-parsed bytes, and was cross-
checked against the real framework image (whose refusal must NOT be relaxed).
`e2e_host`'s `tls` case prints exactly this classification for each input.

## A.4 Mac-host execution facts (all measured 2026-10-13, macOS 15.7.8, Rosetta 2)

These are properties of the Mac validation host, not of the PS5 design. They
matter because they bound what the Mac E2E can prove:

1. **Rosetta virtualizes 4 KiB pages.** `mmap` in a translated x86_64 process
   returns 4 KiB-aligned but NOT 16 KiB-aligned addresses. The loader's
   default `page_size = 0x4000` rounds `mprotect` ranges outward and hits
   neighboring mappings → `ENOMEM`. The E2E therefore runs with
   `LoadOptions::page_size = 0x1000`. **PS5 keeps 0x4000** (kernel-aligned
   mappings), where the 16 KiB RWX-partition rule of design §3b applies
   unchanged. Side effect: at 4 KiB granularity the small samples do not
   require an RWX page at all (sections separate naturally).
2. **Freshly written executable bytes need `sys_icache_invalidate`** before
   first execution under Rosetta, else stale page content (zeros) executes.
   Documented in `e2e/e2e_host.cpp`.
3. **`movabs r11` from JIT'd anonymous-page code wedges this Rosetta build**
   (reproduced minimally: 22-byte trampoline with `movabs r11, imm64` → process
   enters state `UE` inside `com.apple.rosetta.exceptionserver`; the identical
   sequence with `movabs rax` runs fine; `mov r11` from ordinary host
   `__text` runs fine). Consequence for the Mac host: the staged
   `bridge_stamp_tramp` anon-page trampoline path **cannot be exercised under
   Rosetta**; the E2E uses per-row naked thunks in host `__text` that set r11
   and jump into the *staged* `cap13_g2h_thunk4s0..s4` shared thunks — every
   staged bridge instruction that is ordinary `__text` still runs and passed.
   The stamped-trampoline path itself is PS5-targeted (no Rosetta there) and
   is explicitly listed as unvalidated-on-Mac.
4. **The staged stack-arg staging thunks (s1..s4 classes, >4 args) wedge in the
   same Rosetta exception path** even from host text via a full crossing
   (measured: 6-arg `call6` → `thunk4s2` crossing wedges 3/3 under a watchdog).
   ≤4-argument g2h crossings (s0 class) pass. On PS5 these classes must be
   validated at the console rung; the Mac result must not be read as a bridge
   bug — the disassembly matches the design, and the s0 class (which shares
   every instruction except the `.if n_stack` staging block) passes.
5. Rosetta also exhibits an unrelated flaky demand-translation wedge on this
   machine (state `UE`, unreapable by `kill -9`). The runner (`build_and_run.sh`)
   watchdogs every case and retries; permanent wedges are recorded as WEDGE
   rows in the evidence log rather than hidden.

## A.5 What the Mac E2E proves (summary — full detail in E2E-PROOF.md)

- The staged `pe_loader` maps, relocates, resolves imports for, and hands to
  execution **real Windows x64 PE DLLs** on a machine where their bytes can
  run — with zero modifications to the staged loader/bridge sources.
- DllMain via the staged `dllmain_invoker` (`call3`), lifecycle exports via
  `call0/call1`, and real guest→host import crossings through the staged g2h
  shared thunks all executed, with per-event evidence and byte-identity of the
  original files.
- Refusals fired with precise named reasons on: x86 machine, non-PE bytes,
  TLS directories (both the real-framework case and the lld-link-artifact
  case), and unresolvable import sets (real `dwmapi.dll`: 133 of 135 imports
  unresolved → fail-closed with the complete list).
- The staged `win_api_shim` registry alone resolves 0/4 to 2/135 of the real
  sample import censuses — quantifying the M1 census-generation work remaining
  (and confirming that "imports resolvable" and "code executes" must be
  reported separately, per the user's requirement 6).

## A.6 Still owed before any console claim

- J1/J2/J3 exactly as in design §9 (this Mac work touches none of them).
- The anon-page stamped-trampoline path (A.4.3) and the s1..s4 stack-staging
  classes (A.4.4) need console-side validation — the Mac cannot exercise them.
- M1: census-generated shim rows (the staged 16 rows vs 634-import framework
  reality), the per-signature float/mixed thunk generator (F5), and the pinned
  CppUserModBase layout bake (ue4ss_compat `kLayoutPinned`).

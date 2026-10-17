# cap05 crash capture - notes (Team C)

Labels: **console-proven** = read from a console log that already exists (path + line cited);
**Mac-only** = computed on the Mac. No console contact happened in this work.

## 1. What already exists on the console (console-proven)

- Every crashed process gets a PS5 kernel fatal-signal dump in the klog: reason/signal, thread, pid,
  fault address, all 16 registers + rip + eflags, 10 last-branch records, a frame-pointer backtrace,
  and the full module list with `xotext start:end` and `fingerprint`.
  Example: `l25-prep/evidence/launch-l26-B6b-launch3-20261012/klog-20261012-162139.log` lines 6154-6428.
- The klog is a live TCP stream with no history; the Mac tap file is the only copy (agent-T). The harvest
  already picks up the newest `klog-*.log`. Launch 13 had no klog file - a crash then would be lost.
- The system also writes its own coredump + screenshot/video
  (`devlog/system/sce_coredumps.0/PPSA33286_<ts>/prosperocore-...prosperodmp`, same klog lines 6461-6489).
  Not used here (Sony format, path not known to be reachable by FTP).
- UE4SS.log is written with one `write()` per message, so its last lines survive a crash (agent-CL F3).
  (Not re-verified by the reviewer. If the UE4SS.log PASS lines are missing after the crash, crashcap's two
  `EXPECT PASS` lines still decide the verdict.)

So the capture is already there; what was missing was a trustworthy way to READ it.

## 2. Findings (Mac-only unless marked)

1. **Fingerprint = GNU build-id.** The kernel's module `fingerprint` is the `.note.gnu.build-id` of the
   ELF (20 bytes). B6b bare PRX note = `32f8da09...00000000` = launch-3 klog line 6428 (console-proven);
   game eboot image note `04bb4330...00000000` = the eboot fingerprint in the same block. This binds a
   crash to an exact local build without guessing (agent-Z11 had concluded the fingerprint "does not
   reproduce"; it is simply the note).
2. **Address rule, checked on two modules.** `xotext start` = load address of the executable PT_LOAD
   (vaddr 0) and `end - start` = its memsz rounded up to 0x4000: our PRX 0x1256b57 -> 0x1258000 =
   0x81ce4000-0x80a8c000; eboot 0x7479b1c -> 0x747c000 = 0x787c000-0x400000. So
   `vaddr = addr - xotext_start + exec_vaddr`. crashcap checks this per module and refuses names if
   it does not hold.
3. **Exact final symbols without changing the build.** The pipeline keeps the object the final writer
   consumed (`pass-1/build/combined-supplier.o`). Re-applying the writer's placement rule
   (DynamicWriter.cs Gather() + code-group loop, retained harness copy) and comparing bytes:
   R11 77,993/77,993 code sections, 18,977,850 bytes compared, 0 mismatches; B6b 76,323/76,323.
   Negative controls: layout shifted 0x10 -> 76,418 sections fail; B6b object vs R11 PRX -> refused;
   one flipped code byte -> refused. ChatGPT's staged DynamicWriter/Program.cs producer is not needed.
4. **The current sidecar names are wrong.** `map_reconstructed` rows (order-assumed, no alignment):
   0 of 18 real launch-3 frames agree; 0 of 2,994 random symbols at the right address; median error
   ~300 KB (`evidence/sidecar-map-reconstructed-check.json`). Do not use that tier.
5. **The old symbolizer misses most crashes.** `phasea-tools/symbolize_crash.py` only opens a block on a
   `signal:` line: 0 of 1 blocks in launch 3, 1 of 8 in the l23 klog; it reads one register per line
   and no backtrace. crashcap parses all 55 dump banners in the 20 retained klog files that have any (some files are copies).
6. **Launch-3 crash re-read** (console-proven klog + Mac-only analysis,
   `evidence/CRASH-REPORT-launch3-B6b-reanalysis.md`): game code calls our script-hook entry
   `RC::Unreal::ThunkDetail::Thunk<61>` -> `ensure_str` -> `to_wstring` -> `wstring_convert` ->
   `basic_string<wchar_t>` -> `operator new` fails -> `__cxa_throw` -> `failed_throw` (no handler) ->
   `std::terminate` -> abort. 14 direct calls confirmed by bytes (call target = start of the next function down), 0 contradictions.
   This agrees with the launch-3 report's "heap exhausted" but the entry point was the Thunk<61>
   script-hook path, not `VmtEngineTickDrain` as written there.

## 3. ChatGPT cap05 prep - re-checked

| Claim | Now |
|---|---|
| capture/build binding UNVERIFIED | closed: fingerprint == build-id (finding 1) |
| runtime base convention unproven | closed for exec-vaddr-0 modules (finding 2) + call-edge checks; other layouts are refused |
| needs a DynamicWriter producer change | not needed (finding 3) |
| crash_report.py, 8/8 tests | wraps the old parser: misses abort blocks (finding 5) |
| historical 9ef26a8f/7fead02d mismatch | stale; current sidecar pairs to bfd95db6, but its map rows are wrong (finding 4) |

The two SUSPECT pak-overlay files were not used. `CLAUDE-RESUME-HANDOFF.md` was not opened.

## 4. Design choices

- **No in-process signal handler.** The kernel dump already has everything, and a handler would need a
  new import (`sigaction` NID = a shipped value needing two sources) and could clash with the game's own
  crash path. The opt-in test function is the only runtime change.
- The test function is gated by an exact token. The token stops accidents only; it is NOT a security
  boundary (it is plain text in the PRX, and a hostile mod could crash the game anyway). It returns
  `false, message` on refusal (no Lua error, no longjmp over C++ objects).
- "Our runtime" is recognised by build-id binding to a final-symbols file, never by module name.

## 5. Open questions / limits

- Game frames get module+offset and "is a call site" checks only (stripped eboot); names would need the
  profile generator's anchors, bound by build-id (crashcap accepts any ELF via `--image`).
- System libraries (libkernel, libSceLibcInternal) stay module+offset (no local images).
- Kernel backtraces follow frame pointers; a function without a frame is skipped (shown as
  `CALL-TARGET-DIFFERS`, never as a wrong name).
- If the klog interleaves another writer inside a dump line, crashcap recovers what it can and lists
  missing registers (seen in `runtime/l25-variant-A/klog-20261011-204957.log` line ~2680).
- `candidate/src/LuaMod.cpp` already differs from the R11 row (96e8ff4d vs 95591da5): someone is
  mid-change; the stage script re-anchors safely.

## 6. Relayed user observation (outside cap05)

The user reports Leon's textures show as a grey/white checkerboard, and the outdoor scene after the door
is fully lit (no Townfall darkness, monster clearly visible). Mac-only reading, unverified: the
checkerboard is UE's default "missing texture" look - the pak's textures did not load/decode on PS5
(PC-cooked texture formats); the over-bright scene suggests a material/post-process/lighting asset from
a pak (or a shader fallback) replaced the game's dark grading or fog. This is not a crash, so cap05
cannot capture it; it belongs to the pak/shader capabilities (#7, #8, #12). Useful next evidence for
them: which pak was mounted in that launch and the UE4SS.log lines around the door/level load.

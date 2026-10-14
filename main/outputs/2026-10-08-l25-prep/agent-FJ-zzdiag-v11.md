# agent-FJ — zzdiag v11 (pickup read probe) — 2026-10-08-l25-prep

## Deliverables (ONLY these; console untouched — REST MODE)

- outputs/2026-10-08-l25-prep/zzdiag-v11/main.lua — 4084 bytes
  (under the 4096 admission cap; v10 precedent was 4092 size-exact —
  the loadlist row must record the ACTUAL 4084).
- outputs/2026-10-08-l25-prep/zzdiag-v11/EXPECTED-MARKERS.txt — full
  marker -> interpretation -> fix-site map.
- This report. No tree source edits. No mod edits. No console contact.

## What v11 is

v10 chassis with the M1-M6 dry-run payload REPLACED by the pickup
probe (byte budget): kept the 6s T auto-probe, the +18s one-shot on
the game thread via ExecuteInGameThread, the +45s WD no-drain split,
F7 rerun, "[zz]" logging, and the pcall-everything law. The [DRY]
no-viewport attestation concept became the "[PRB] done" end-of-body
attestation (script contains no viewport call).

Probe ladder (all steps individually pcall'd, errors logged as
indented stripped 60-char tails):

- P0 acquisition: FindAllOf("PickupInteractable") first 3, else
  FindFirstOf fallback (" ffo" suffix names the arm); none ->
  "[PRB-0] none" + SKIP lines (pickup may only exist near player —
  acquisition itself is still proven).
- P1 pick first IsValid wrapper ("[PRB-1] pick t<i>"), read
  p.PickupInventoryItem -> ok/type; userdata gets a :type() probe
  ("[PRB-1] udt ...").
- P2 inv.ItemId -> ok/type. P3 tostring(id) -> val<40. P4
  id:ToString() via the pcall(id.ToString, id) call form — the member
  INDEX is fenced inside the same closure (id.ToString(id)) so index
  and call are both fenced (SAFETY LAW reconciled with Claude's
  form). P5 struct fields if table form (first 12 keys) else
  "struct-probe skipped-not-table". P6 known-good control: tostring
  of the actor wrapper.
- Per-target compact repeats "[PRB-1..4] t<i>" — same P number names
  the same hop as the main run (t1 duplicates main = corroboration).
- "[PRB] done" end attestation.

Safety law respected: no FindFunction, no bpfn, no class-children
enumeration, no AddToViewport, wrappers never trusted (pcall
IsValid), conservative Lua only.

## Verification performed (all local, zero console contact)

- luac (Lua 5.5.1, the only Lua toolchain on this Mac; no luac5.4
  exists) -p: PASS. Syntax is 5.4/5.5-conservative.
- Offline stub harness (/tmp/zzdiag-v11-harness.lua, NOT in the
  outputs tree): stubbed print/FindAllOf/FindFirstOf/LoopAsync/
  ExecuteInGameThread/RegisterKeyBind/Key and metatable-driven
  fake wrappers. Exercised: happy path (FName-like id with __tostring
  + ToString), table-struct inv (P5 field listing), struct-read
  throw (type=e + tail), id nil (SKIP no-id), id throw, ToString
  member missing (ok=0 + "attempt to call a nil value (field
  'ToString')" + raw val head + stripped tail), no-target none+SKIP
  path, FindFirstOf fallback ("n=1 ffo"), F7 rerun (K1/body/K2), WD
  silence after a run, T-cycle tokens. All marker shapes match
  EXPECTED-MARKERS.txt. (Harness limitation: stock Lua type() cannot
  return "userdata", so the "[PRB-1] udt" branch and
  skipped-not-table branch were verified by code inspection only;
  both are pcall-fenced and luac-checked.)
- Harness bug found and fixed during testing (dump() iterated the
  print-capture table while print appended to it — infinite loop);
  it was a harness defect, not a main.lua defect. main.lua was never
  changed because of it.

## Key design notes / deviations worth knowing

- Target repeats use base 1 (not 2): during harness testing base 2
  spilled the target TS line onto "[PRB-5] t<i>", colliding with the
  P5 struct-probe marker space. Base 1 keeps P-n meaning identical
  across main and target lines; documented in EXPECTED-MARKERS.txt.
- On ok=0 lines the val= field shows the RAW error head (may include
  a chunk prefix) and the indented line shows the file:line-stripped
  60 chars; documented so the harvester reads both correctly.
- type=e on a marker line means "that read threw" (the error value's
  type() would otherwise misleadingly print "string").

## Harvest pointer

Every marker, its interpretation (which P outcome names which
failing hop: struct read vs field read vs userdata ToString vs
conversion) and the fix-site mapping (bridge reverse converters in
LuaFNameBridge; property-read returns in LuaUObject/CustomProperty/
NativePropertyReadBackend) are in zzdiag-v11/EXPECTED-MARKERS.txt,
including the engine-side corroborating markers ([FFO] query/add
fallback, [UF] nofn) and absence patterns.

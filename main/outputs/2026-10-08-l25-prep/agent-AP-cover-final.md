# Agent AP — cover-document final update: review-2 item results folded into REVIEW-FOR-CLAUDE-l25.md

Date: 2026-10-08 (latest). Scope: EDIT ONLY
`outputs/2026-10-08-l25-prep/packet/REVIEW-FOR-CLAUDE-l25.md` (this report
is the only other file written). Sources read before editing:
`agent-AA-offset-verify.md`, `agent-AE-variant-c.md`,
`agent-AC-container-audit.md`, `agent-AO-gate-extensions.md`,
`agent-AD-item4-logging.md` (header/scope), `claude-review2-20261008-l25.md`.
All prior structure preserved; every addition cites its source report.

## 1. Sections updated (in document order)

1. **Top banner** — new "UPDATE (review-2 item results, 2026-10-08 latest)"
   paragraph after the existing review-2 update, pointing to §5c / §3
   items 9-10 / decisions (f)-(h) and naming what is still in flight
   (AB, AH, AK, AL, AM).
2. **§1 HEADER** — one new bullet: profile offsets PROVEN WRONG (AA):
   delete listeners real 0x68 not 0x78 (0x78 is the crit), create 0x58,
   crit 0x18 bytes not 0x28; registration read the LOCK as Data →
   non-heap Realloc → allocator correctly rejects → CE-108255-1;
   mechanism now fully explained in-family; variant C feasible (AE);
   G1-G3 landed (AC/AO).
3. **§3 EVIDENCE STORY** — two appended items:
   - Item 9, THE FINAL LINK: AA's offset root cause closes the chain —
     assumed S=0x28 vs real 0x18 lazy mutex shifts every post-ObjObjects
     member; registration at 0x78 read crit fast-state as Data and mutex
     halves as Num/Max; explains (a) G's stall at the Add statement
     (Contains wild/NULL read), (b) Z11's symbolized chain + review 2's
     reading (allocator CORRECTLY rejected the non-heap Realloc; +0x10 =
     fatal logging path) = CE-108255-1, (c) the write-back
     crit-corruption = save-load detonator matching Z1, plausibly the
     kernel copyin/nonsleeping-lock signature. Out-of-family attribution
     no longer needed.
   - Item 10, the fix shape (AC/AE/AO): B-as-built would die at
     LuaMod.cpp:7995 one call later; G1-G3 landed and compile-verified;
     AE proved engine Add @0x4841A0 / Remove @0x484240 live, locked
     (recursive crit), engine-allocator end-to-end — variant C feasible,
     A+C = recommended production shape; fix stack = profile correction
     (primary) + C (proper) + B-with-G1 (control).
4. **§5 BUILD STATE** — one NOTE appended to the status block: R's dry-run
   plans predate the AD/AO edits; final build must re-verify against the
   current tree (LuaMod `cef8d02f…`, UnrealInitializer `cac95e48…`,
   UObjectArray `4f4afddf…`); the `68ecf9d8…`/`efdef253…` composition
   references describe the pre-AD/AO state.
5. **§5c REVIEW-2 ITEM RESULTS (NEW SECTION)** — per-item results:
   - Item 1 AA: LANDED, DECISIVE — full corrected layout (create 0x58,
     delete 0x68 + Num/Max, crit 0x78 size 0x18 with lazy-ptr 0x80 /
     counter 0x88, PrimarySerialNumber 0x90, bShouldRecycle 0x94, total
     0x98, ObjObjectsCritical 0x30, ObjAvailableList 0x48), engine
     functions located + string-confirmed (Add 0x4841A0 / Remove
     0x484240 / dispatch 0x483FF0 / Shutdown 0x4843A0), root cause =
     generator ASSUMED S=0x28 (profile = the wrong-S column, never
     measured), mechanism consequence, create-value latent (Z8 §1.1).
   - Item 2 AB: PENDING (no report file); incidental AE/AA progress
     noted (engine GMalloc @0x9340950, Realloc vtable+0x40) without
     claiming AB's mandate.
   - Item 3 AC: LANDED — LuaMod.cpp:7995 as B's first post-skip site
     (same 3-touch GMalloc append, UpdateThread), rank-2 GetFullName →
     engine ProcessEvent(Conv_NameToString)+GMalloc class at the 7 R3
     prints, 8 live + 2 config-off sites, G1 mandatory / G2 / G3 /
     G4-optional, DO-NOT-GATE list, main.lua flagged mod-boundary.
   - Item 4 AD: LANDED (earlier) — pre-Append logging fields + guarded
     reads + refusal code 6; note that its Data/Num/Max print
     profile-pinned values (diagnostic under old pins, real after (f)).
   - Variant C AE: LANDED — FEASIBLE — engine Add present/live (14 boot
     callers, 69+ Remove sites; survived linking unlike Z6's
     dead-stripped family), seed-route location, SysV signature, lock
     proof (recursive), allocator chain (0xBEF20 → 0x9340950 →
     vtable+0x40), 8-hunk additive patch +
     MODBRIDGE_L25_ENGINE_ADD_LISTENER, B+C skipped / A+C production,
     independent AA corroboration, boundaries (not compiled; pinned to
     sha 3b6cfad4).
   - Gates AO: LANDED in candidate tree — G1 (nm-proven compiled out in
     B), G2 (7 sites `[n: X] @0xptr`, unconditional both variants),
     G3 (preprocessor-proven force-off), compile matrix all green
     host+Prospero A+B, CHANGES.json repinned, boundaries (no suites,
     no twin build, no target link, no console).
   - In-flight list: AB, AH, AK, AL, AM, AP.
6. **§6 DECISIONS** — (d) and (e) each carry a short UPDATE pointer to
   (g)/(f); three new items appended (see §2 below).
7. **§7 INDEX** — manifest-repoint note extended with AA/AC/AD/AE/AO; new
   index bullet "review-2 item results" listing the five landed reports
   with one-liners, the in-flight set (AB/AH/AK/AL/AM/AP), and AE's
   out-of-packet artifact location (variant-c-proposal/).
8. **§8 HONESTY BOX** — superseded confounder bullet REPLACED with
   "conconfounder RESOLVED" (mechanism fully explained in-family; one
   nuance plausible-not-proven: the exact kernel copyin-signature
   linkage via crit corruption); four new NOT-proven bullets
   (corrected-offset registration not console-tested; variant C not
   compiled; B-with-G1 not yet built / R must rebuild against updated
   tree; AB/AH + AK/AL/AM in flight); race-narrowing bullet annotated
   post-AA (primary fault = mis-aimed write; Z6 mechanism-B caveat stands
   at correct offsets; C eliminates it); B-unsafe bullet annotated with
   G1's added Lua-side loss.

## 2. The decisions list now before Claude

(a) Approve upload of variant A (full SAFETY BOX discipline); B
    contingent, staged only per LAUNCH-PLAN §4. [unchanged]
(b) l26 copyin-gating proposal — adopt or defer. [unchanged]
(c) Launch order — review-2 B-FIRST order supersedes; all other
    discipline unchanged. [unchanged]
(d) Variant C adoption timing — UPDATE: AE landed, feasibility proven;
    superseded by (g).
(e) Profile-offset correction conditional on AA — UPDATE: condition
    landed, offsets proven wrong; superseded by (f).
(f) **Approve the profile correction values** — 0x58 create / 0x68
    delete / 0x78 crit (size 0x18) / 0x90 PrimarySerialNumber / 0x98
    total, plus consumer-gated bonus fields (ObjObjectsCritical 0x30,
    ObjAvailableList 0x48, crit lazy-ptr 0x80 / counter 0x88,
    bShouldRecycleObjectIndices 0x94) — applied via the AM template once
    AL's consumer map lands; PRIMARY fix (alone removes the
    crit-corruption/allocator-reject class at both :528 and
    LuaMod.cpp:7995).
(g) **Variant C adoption — A+C production shape** — fold
    MODBRIDGE_L25_ENGINE_ADD_LISTENER into the twins now (before R's
    --execute) or immediately post-B; packet + Claude + user OK;
    not yet compiled; labels pinned to image sha 3b6cfad4.
(h) **Approve the AO gate extensions (G1-G3) into both variants** —
    G1 mandatory for B to mean anything; G2 (both variants,
    log-text-only); G3 settings-flip guard; R's build must compile
    against the updated tree (LuaMod `cef8d02f…`, UnrealInitializer
    `cac95e48…`) and regenerate the stale dry-run plans.

## 3. Boundaries

No source, build, profile, manifest, or evidence file touched. No claims
added beyond what the five source reports state; in-flight agents (AB,
AH, AK, AL, AM) are marked PENDING with no content invented for them.
AO is reported as LANDED per its on-disk report (the dispatch note said
"being implemented"; the report shows G1-G3 done, compile-verified,
CHANGES.json repinned) — flagged here so the coordinator can reconcile
if AO's packet row was expected to stay in flight.

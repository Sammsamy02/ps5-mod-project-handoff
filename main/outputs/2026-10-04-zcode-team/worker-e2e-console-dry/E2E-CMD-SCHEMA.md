# E2E-CMD-SCHEMA.md — config-file-driven entry (G1 fallback), exact design

Origin: worker-e2e-console-dry, 2026-10-04. This is the design the installer
owner implements if the G1 argv gate resolves NO-GO (pldmgr forwards no
arguments). It follows `worker-argv-probe/REPORT.md` verbatim: "argc==0 /
path-only argv[0] → no forwarding → G1 NO-GO for argv-driven installers; E2E
adopts config-file-driven entry (FTP-STOR a sha-verified
`/data/modbridge/intake/e2e-cmd.json`, bare dispatch, installer reads
state/roots/verb from it) — chosen over a baked-args wrapper (one payload for
all stages, no per-command build/upload/alias churn, input verified before
dispatch, STOR already the gated intake route)."

Status: DESIGN ONLY. Nothing here exists in the current payload
(`e3489297…`, 681528 B); implementing it is a NEW composition revision with
new tuple IDs, a new build receipt, re-proven imports, and a re-run of all
three suites (bridge 75, F-R2 87, fault 186) plus new config-entry cases —
per the studio iteration rules (distinct IDs, no silent edits). A G1-fallback
run stages THAT revision's sha via a fresh re-pin; never this payload.

## 1. Entry rule (exact)

In `main(int argc, char **argv)`, BEFORE the existing argv parse loop
(`modinst_core.c` line ~1545):

```c
if (argc < 2) {                       /* bare dispatch: argc==0 or argv[0]-only */
    /* reads /data/modbridge/intake/e2e-cmd.json; on success replaces
     * argc/argv with the synthesized vector and falls through into the
     * EXISTING parse loop; on any failure: usage(); return EX_BAD;  */
    if (e2e_cmd_entry(&argc, &argv) != 0) { usage(); return EX_BAD; }
}
```

- `argc >= 2` (argv dispatch works, G1 GO): config entry is NEVER consulted.
  A leftover `e2e-cmd.json` is ignored and left unconsumed; the controller's
  pre-STOR absence check (EXECUTION doc §2) catches staleness.
- `argc < 2` and file absent: loud `usage()` + `EX_BAD` (EX_USAGE path), exit
  code identical to today's bare invocation. NO retry, NO fallback verb.
- `argc < 2` and file present but invalid: refuse (see §6), file left in
  place for controller inspection, `usage()` + `EX_CONFIG` 6 (see the §6
  AMENDMENT below; absent-file stays `EX_BAD`).
- The fixed path is a compile-time constant:
  `#define E2E_CMD_PATH "/data/modbridge/intake/e2e-cmd.json"`. No env vars,
  no relative resolution (a bare payload has no meaningful cwd contract).

## 2. File format — top-level schema (version 1)

One JSON object, flat. Exactly these keys, in any order:

```json
{
  "schema":     1,
  "verb":       "<command>",
  "state_root": { "state": "<abs dir>", "roots": "<abs base>" },
  "mods":       ["<id>", ...],
  "options":    { "<flag>": <value>, ... }
}
```

| Key | Type | Required | Maps to | Notes |
|---|---|---|---|---|
| `schema` | int | yes | — | must be exactly `1`; anything else refuses |
| `verb` | string | yes | the command token | one of: `records`, `register-mod`, `prepare`, `prepare-archive`, `install`, `simulate-runtime`, `recover` |
| `state_root.state` | string | yes | `--state <v>` | absolute path (`/data/modbridge/state`) |
| `state_root.roots` | string | yes | `--roots <v>` | absolute base (`/data/modbridge/roots`) |
| `mods` | array of strings | no | `--mods <a,b,…>` (comma-joined, order preserved) | legal ONLY for `prepare` and `install`; presence with any other verb refuses |
| `options` | object | no | see §3 | verb-specific flags; keys are snake_case forms of the argv flags |

Deny-list: `make-bundle` is a host fixture verb and is NEVER expressible
through config entry (explicit verb allow-list above). The fault-injection
flags `--crash-at`, `--cancel-at`, `--cancel-after` and `--variant` /
`--defaults` / `--state-name` / `--code` / `--detail` are host-test-only:
they have NO config `options` keys and refuse if attempted. Console E2E runs
never inject faults through the dispatch file; crash/recover coverage stays
in the host suites (26/26 + 186/186 evidence).

## 3. `options` key → argv flag mapping (mechanical, one-to-one)

Each legal key maps to EXACTLY one argv token pair appended to the
synthesized vector; the existing main() parser remains the single point of
validation (no semantic is re-implemented):

| options key | type | argv token | legal verbs |
|---|---|---|---|
| `title` | string | `--title <v>` | register-mod, prepare, prepare-archive, install, simulate-runtime |
| `id` | string | `--id <v>` | register-mod |
| `bundle` | string | `--bundle <v>` | register-mod |
| `archive` | string | `--archive <v>` | prepare-archive |
| `quarantine` | string | `--quarantine <v>` | prepare-archive |
| `mod` | string | `--mod <v>` (singular) | prepare-archive |
| `remove` | string | `--remove <v>` | install |
| `rt_state` | string | `--rt-state <v>` | simulate-runtime |
| `choices` | string | `--choices <v>` | prepare, install |
| `no_auto_bundle` | bool `true` only | `--no-auto-bundle` | prepare-archive |

Rules: unknown key anywhere (top level, `state_root`, `options`) → refuse.
Wrong JSON type for a key → refuse. `mods` together with `options.remove` →
refuse (install is either add or remove per the CLI grammar). An
`options` key illegal for the verb is still passed through and rejected by
the existing per-verb `usage()` checks — but the config layer ALSO refuse
(eager, see §6) so a malformed file never reaches dispatch. String values
≤ `LINE_MAX_`; `mods` ≤ 16 entries; `options` ≤ 16 keys.

## 4. Per-stage exact command JSON (the only files this run ever STORs)

Conventions: `state` = `/data/modbridge/state`, `roots` =
`/data/modbridge/roots` in every file (shown once here; repeated verbatim in
every staged file). `PPSA33286` is the title for every command that takes
`--title`. The controller stores each file locally as
`cmd/stage<N>-<seq>-<verb>.json` under the run's evidence dir and STORs it to
the SAME fixed console path before each dispatch.

### Stage 0 — baseline (read-only)

```json
{"schema":1,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}
```

### Stage 1-owner — register the owner-placed archive

```json
{"schema":1,"verb":"register-mod","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"id":"movspeed","title":"PPSA33286","bundle":"/data/modbridge/intake/movspeed.zip"}}
```

### Stage 2a — prepare-archive with auto-bridge

```json
{"schema":1,"verb":"prepare-archive","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","archive":"/data/modbridge/intake/movspeed.zip","quarantine":"/data/modbridge/intake/movspeed.q","mod":"movspeed"}}
```

### Stage 2b — bundle flow (three dispatches)

2b-1 register:
```json
{"schema":1,"verb":"register-mod","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"id":"mm-skin","title":"PPSA33286","bundle":"/data/modbridge/intake/mm-skin.mfb"}}
```
2b-2 register:
```json
{"schema":1,"verb":"register-mod","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"id":"mm-lua","title":"PPSA33286","bundle":"/data/modbridge/intake/mm-lua.mfb"}}
```
2b-3 prepare:
```json
{"schema":1,"verb":"prepare","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"mods":["mm-skin","mm-lua"],"options":{"title":"PPSA33286"}}
```

### Stage 3 — transactional install (all three mods)

```json
{"schema":1,"verb":"install","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"mods":["movspeed","mm-skin","mm-lua"],"options":{"title":"PPSA33286"}}
```

### Stages 4-5 — runtime ack, SIMULATED (two dispatches)

4:
```json
{"schema":1,"verb":"simulate-runtime","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","rt_state":"activated"}}
```
5:
```json
{"schema":1,"verb":"simulate-runtime","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","rt_state":"mods-started"}}
```

### Stage 6 — uninstall (three dispatches, order fixed: mm-lua, mm-skin, movspeed)

6-1:
```json
{"schema":1,"verb":"install","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","remove":"mm-lua"}}
```
6-2: same with `"remove":"mm-skin"`.
6-3: same with `"remove":"movspeed"`.

### Interrupted-transaction rollback (only, if stage 3/6 aborts mid-transaction)

```json
{"schema":1,"verb":"recover","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}
```

Formatting note for the controller: the files above are the exact bytes to
STOR (compact, one line, UTF-8, trailing newline optional but then included
in the sha). The parser is whitespace-tolerant; pretty-printing is allowed
but the staged bytes are whatever was sha-verified at STOR time.

## 5. JSON subset grammar for the bounded parser (implementation contract)

No JSON library is vendored in the payload; the entry uses a ~150-line
recursive-descent parser for this deliberately restricted grammar:

- One top-level object. Depth ≤ 2 (`state_root` and `options` are the only
  nested objects; `mods` is the only array; arrays of objects refuse).
- Types: string (`"..."` with escapes limited to `\"`, `\\`, `\/`; `\uXXXX`
  refuses — every value in §4 is plain ASCII paths/ids), int (schema only),
  bool (`true` only, for `no_auto_bundle`).
- File size ≤ 4096 bytes (every §4 file is far under; oversize refuses
  before parsing). No BOM, no comments, no trailing commas.
- Parser never writes; it fills a bounded argv vector (`char *argv[64]`,
  each token ≤ `LINE_MAX_`) pointing into one heap buffer freed at exit.

## 6. Consumption + refusal matrix (prevents stale-file re-execution)

> AMENDMENT 2026-10-04 (worker-e2e-promotion-apply, per
> `worker-e2e-suite-completion/EXCONFIG6-MEMO.md`, ACKing the implemented
> build-r1 config-entry delta in `worker-e2e-config-entry/owned/modinst_core.c`
> sha `43ea66c1…`, promoted to `worker-d1-chain-relink/composition/receipts/patches.json`
> this date): a config file that is PRESENT but fails validation exits
> **6 (`EX_CONFIG`, distinct from `EX_BAD`)**; absent-file and consume-failure
> remain `EX_BAD` (2); all other refusal semantics (loud `say` line +
> `usage()`, file left in place, verb never executed, no begin line, never
> auto-retried) unchanged. Mirrored in `worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` §4.

The fatal hazard of a fixed-path dispatch file is a stale file re-executing
a mutating verb on an accidental re-dispatch. Contract:

| Situation | Behavior |
|---|---|
| File parses + validates | installer `rename()`s `e2e-cmd.json` → `e2e-cmd.json.consumed` (same dir, atomic on /data) BEFORE `e2e_status_begin` and before executing the verb |
| Second bare dispatch, no fresh STOR | file absent → `usage()` + `EX_BAD`; NOTHING executes (no repeat install) |
| Parse error / unknown key / bad type / deny-listed verb or flag | refuse: `say("error bad_request e2e-cmd: <reason>")`, `usage()`, `EX_CONFIG` 6 (see AMENDMENT below); file LEFT in place (controller inspects, then DELEs; never auto-retried) |
| `rename()` fails | refuse with `error e2e-cmd consume failed` + `EX_BAD`, verb NOT executed |
| `.consumed` already exists from a previous dispatch | `rename()` overwrites it (the previous command is already durably recorded in records.txt/e2e-status.jsonl); this is not an error |
| argv dispatch (argc≥2) with a leftover cmd file | argv path wins; file untouched (pre-STOR absence check catches it next config dispatch) |

The consumed rename is also the controller's per-dispatch proof that THIS
file drove THIS dispatch: after each dispatch the controller RETRs
`e2e-cmd.json` (must be gone) and `e2e-cmd.json.consumed` (must exist and
hash equal to the staged bytes).

## 7. e2e-status.jsonl identity — unchanged

Config entry synthesizes argv and enters the EXISTING parse/dispatch path,
so `e2e_status_begin(cmd, title)` / status / end lines are byte-identical to
the argv-driven expectations (key order fixed by
`worker-f-r2-fix/owned/e2e_status.c`: begin `{"event":"begin","cmd":…,"title":…,"stage":…}`,
end `{"event":"end","exit":…}`; stage mapping records→`stage-0-baseline`,
register-mod→`stage-1-download`, prepare/prepare-archive→`stage-2-prepare`,
install→`stage-3-install`, simulate-runtime→`stage-4-5-runtime-ack`). The
dispatch mechanism is proven by the controller receipt (cmd-file sha per
dispatch + consumed rename), not by new status fields — no schema drift.

## 8. Required evidence for the implementing revision (installer owner)

1. New tuple(s) with distinct IDs (e.g. `g1-cmd-entry-parse`, `g1-cmd-entry-consume`,
   `g1-cmd-entry-allowlist`) appended AFTER `r2-install-status-ok`; chain
   grows 51 → N; `composition/receipts/patches.json` extended per-tuple.
2. Import set re-proven: still 116 dynamic imports, missing=[] (the entry is
   stdio + string ops only; no new provider symbols).
3. Suites re-run green on the new chain (bridge 75, F-R2 87, fault 186) PLUS
   new host cases: valid §4 files for each verb drive the identical
   transcript as their argv twins (byte-compare e2e-status.jsonl); refusal
   matrix cases (unknown key, bad verb, deny-listed flag, oversize, absent
   file, stale re-dispatch) each exit EX_BAD (absent-file; validation
   refusals exit `EX_CONFIG` 6 per the §6 AMENDMENT) with no state change
   and (for refusals) no begin line emitted.
4. New build receipt + STAGING re-pin (new sha, new size law prefix), the
   only manifest row the controller then updates.

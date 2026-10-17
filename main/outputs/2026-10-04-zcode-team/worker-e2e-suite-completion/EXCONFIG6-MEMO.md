# EXCONFIG6-MEMO.md — EX_CONFIG (6) for present-but-refused e2e-cmd files: decision memo

Worker: worker-e2e-suite-completion, 2026-10-04. Scope: the controller-flagged
delta in `worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md` sec 7(1) — the
implemented entry refuses an INVALID dispatch file with exit 6 while schema
`E2E-CMD-SCHEMA.md` sec 6/§8(3) specify EX_BAD. DECISION REQUESTED: ack the
delta (with a one-line schema amendment) or require schema-verbatim EX_BAD.

## 1. The schema's own wording (verbatim)

`worker-e2e-console-dry/E2E-CMD-SCHEMA.md`:

- sec 1: "`argc < 2` and file present but invalid: refuse (see §6), file left
  in place for controller inspection, `usage()` + `EX_BAD`."
- sec 6 table, row 3: "Parse error / unknown key / bad type / deny-listed
  verb or flag | refuse: `say("error bad_request e2e-cmd: <reason>")`,
  `usage()`, `EX_BAD`; file LEFT in place (controller inspects, then DELEs;
  never auto-retried)"
- sec 8(3) (evidence requirements): "refusal matrix cases (unknown key, bad
  verb, deny-listed flag, oversize, absent file, stale re-dispatch) each exit
  EX_BAD with no state change and (for refusals) no begin line emitted."

Note the schema puts "absent file" and validation refusals in the SAME
EX_BAD bucket; the implementation splits them.

## 2. What the implementation does (built + verified this session)

`worker-e2e-config-entry/owned/modinst_core.c` (sha `43ea66c1…`, the build-r1
payload source): exit taxonomy `EX_OK 0 / EX_IO 1 / EX_BAD 2 / EX_NOTREADY 3 /
EX_CHOICES 4 / EX_CANCELLED 5` (lines 78-83) extended with
`#define EX_CONFIG 6 /* e2e-cmd present but refused (distinct code) */`
(line 1554). Per design sec 2:

| situation | exit |
|---|---|
| dispatch ok | 0 |
| file absent at the slot (bare or `--e2e-cmd` target) | 2 (EX_BAD) — schema sec 1/6 verbatim |
| consume-rename failure | 2 (EX_BAD) — schema sec 6 row 4 verbatim |
| file present but refused (BOM / oversize / non-UTF-8 / parse / unknown key / bad type / deny-listed verb or option / allow-list violations) | **6 (EX_CONFIG)** — verb never executes, file left in place |

Host evidence (re-run this session on the byte-exact build-r1 host binary,
`4659c24c…`): 13/13 refusal cases exit 6 with exactly one
`{"event":"status","stage":"e2e-cmd","outcome":"refused",…}` line where a
valid state dir exists and none where it does not; absent-file and valueless
flag exit 2; no begin line ever precedes a refusal (host receipt in this
worker's `host-test/HOST-TEST-RECEIPT.md`, check lines identical to the
original PASS receipt).

## 3. Pros of ACKing EX_CONFIG 6 (keep the implementation)

1. **Preserves the schema's own refusal semantics.** Everything §6 actually
   requires of a refusal is intact: loud error line, `usage()`, verb NOT
   executed, file LEFT in place, never auto-retried, no begin line, no state
   change. The delta is ONE bit of information: the exit CODE.
2. **Exit code is the only always-available machine signal for "input was
   rejected".** The one-line `e2e-status.jsonl` report exists only when
   `state_root.state` itself validated; parse-level failures (BOM, oversize,
   non-UTF-8, JSON parse, bad schema) log stdout ONLY (design sec 3). On the
   console the controller's evidence loop reads files and exit codes — a
   distinct exit code classifies the failure even when no report line exists.
3. **Disambiguates the two non-happy paths the controller must handle
   differently.** Absent (2) = expected idle/stale state; proceed with a
   fresh STOR. Refused (6) = a bad file is sitting in the intake; per §6 the
   controller must inspect and DELE before any retry — treating both as
   EX_BAD invites a stale-file DELE/retry loop being skipped after a parse
   refusal (or an unnecessary DELE after mere absence).
4. **`6` is the next free slot in THIS payload's own exit taxonomy** (0-5
   already defined; `EX_CANCELLED 5` immediately precedes it). It extends the
   established local convention instead of importing BSD sysexits(78); the
   constant's meaning is pinned at the definition site with a comment.
5. **Controller-requested.** Design sec 2 marks it
   "controller-requested delta"; the tasking that produced the revision
   explicitly demanded a distinct refusal code. ACKing is the documented,
   non-silent path; reverting now would spend another composition revision
   to LOSE information.
6. **Zero dispatch-path drift.** The refusal path RETURNs from main before
   any begin line (never through the F-R2 exit shim), proven host-side; no
   argv-dispatch behavior changes (legacy precedence tests E/G pass).

## 4. Cons / costs of ACKing (the honest price)

1. **Schema text must change.** `E2E-CMD-SCHEMA.md` §1, §6 row 3, §8(3) — and
   the MODE B expected-exit notes in
   `worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` §4 — currently say
   EX_BAD. Without a one-line amendment (or a recorded controller waiver),
   docs and binary contradict each other during RUN-CONFIG B, and a future
   reader could "fix" the binary back to 2 as a schema-compliance bug.
2. **Controller gates must know 6.** Any existing check that treats
   nonzero-and-not-2 as unexpected/abort-worthy must learn: bare dispatch
   ending 6 (not 2) means refuse-with-file-present. The staged doc's
   "`{"event":"end","exit":2}` = refusal path — STOP" guidance is unaffected
   (verb-level refusals still exit 2 through the normal path) but the
   config-entry refusal class is new.
3. **`EX_CONFIG` is not a portable name.** BSD sysexits uses 78 for
   EX_CONFIG; readers coming from that convention may expect 78. Mitigated
   by the local `#define` + comment and the amended schema.
4. **A refusal exit of 6 with the file left in place** means an accidental
   re-dispatch refuses again (same 6) — identical to EX_BAD behavior; no new
   hazard, but the amended doc should keep §6's "never auto-retried" wording
   attached to code 6 as well.

## 5. Alternatives

- **Revert to EX_BAD 2 (schema-verbatim):** no doc change, but conflates
  absent vs rejected in the one signal that is always emitted, against the
  tasking that ordered a distinct code; costs a new revision + rebuild +
  re-run of everything for a strict information loss.
- **Keep 6 but write no amendment:** rejected — silent drift; studio rules
  require receipted deltas, and design sec 7(1) itself demands the amendment
  or waiver BEFORE RUN-CONFIG B.

## 6. Recommendation

**ACK EX_CONFIG 6** and apply the one-line schema amendment:

> In `E2E-CMD-SCHEMA.md` §1, §6 (row 3) and §8(3): a config file that is
> PRESENT but fails validation exits **6 (EX_CONFIG, distinct from EX_BAD)**;
> absent-file and consume-failure remain EX_BAD (2); all other refusal
> semantics (loud `say` line + `usage()`, file left in place, verb never
> executed, no begin line, never auto-retried) unchanged. Mirrored in
> `STAGED-E2E-COMMANDS.md` §4 expected-exit notes for MODE B.

Rationale in one line: the implementation keeps every behavior the schema
actually specified and adds exactly one distinguishable exit class where the
schema had collapsed two operationally different situations — amending one
sentence of a DESIGN-ONLY document is the cheap, honest resolution; anything
else either loses information or invites a silent drift.

This memo takes no action beyond this directory; the amendment and the
RUN-CONFIG B dispatch remain controller work.

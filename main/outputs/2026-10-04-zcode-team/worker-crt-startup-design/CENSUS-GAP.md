# CENSUS-GAP — worker-crt-startup-design (2026-10-04)

## Missing input

`worker-dll-mod-retention/baseline-dll-census.json` — the exact observed set of
CRT startup symbols retained by the two DLLs — is NOT present in this workspace.

Verified by: filename search (`*census*`, `*dll*`), content search for
`_initterm` / `seh_filter` / `DllMain` across `*.py/*.md/*.c/*.h/*.json`, a
workspace-wide `find` for `worker-dll-mod-retention`, and a tree listing of
`outputs/` (only `2026-10-04-zcode-team/worker-e2e-console-staging/BLOCKED.md`
exists there — a sibling worker reporting the same missing controller tree,
including `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`).

## Consequence

The "exact observed set" is NOT fabricated anywhere in these deliverables.
DESIGN.md section 2.2 carries a PENDING admission table containing only the two
symbols named in the task prompt itself (`_initterm`, `_seh_filter_dll`), each
explicitly attributed to the prompt, not to a census.

## Fill-in recipe (when the census lands)

1. For each observed startup symbol row in the census, append a row to the
   DESIGN.md 2.2 table: symbol, census source path, admission per the 2.2
   decision rule (attach-time eager vs lazy first-call), and the `crt_startup`
   entry point that serves it.
2. Any observed symbol outside the roles documented in DESIGN.md section 1.1
   (walker entry points, filter, atexit/_onexit, attach adapter) is out of scope
   of this design and needs its own note rather than a silent row.
3. Re-run composition section 3 against the actual entry-adapter owner once it
   exists; the interface contract there is from the task brief, not from found
   source.

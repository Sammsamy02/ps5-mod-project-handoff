# INTEGRATION.md — mod-manager hook sketch for overlay_builder.py

Agent GK (Claude round-6 item 3a); classic-pak + substitutions contract by
agent HI (round-7 items A+B). This is a SKETCH for GG's
`outputs/2026-10-08-l25-prep/mod-manager/modmanager.py` to call the
generalized IoStore overlay builder. It does not edit GG's files; nothing
here is wired into the mod manager yet.

## Why

`modmanager.py` today stages UE4SS-style file mods (Lua/INI/PRX + loadlist
rows). Asset mods on modern Unreal are IoStore containers: a base-game
`.utoc/.ucas` pair plus mod `.utoc/.ucas` trios. The overlay builder turns
N mod trios into ONE derived index + ONE sidecar next to the base container,
with journals, sha verification and uninstall — the exact staged-transaction
shape the mod manager already enforces for its own uploads.

## CLI contract (stable, JSON-out, exit-code semantics)

```
overlay_builder.py --paks-dir DIR --index NAME.utoc COMMAND [options]
```

Commands and their JSON results (stdout is ALWAYS a single JSON document):

| Command | Purpose | Key result fields |
|---|---|---|
| `install --mod [ID=]PATH.utoc [--mod ...] [--classic-pak [ID=]PATH.pak ...] [--retoc-tool PATH] [--retoc-version V] [--substitutions FILE] [--remove-mod ID] [--priority N] [--conflict reject|later-wins] [--dry-run] [--expect-existing] [--base-ucas-size N] [--base-container-header FILE]` | add/remove IoStore and classic-PAK mods, apply substitution directives, (re)derive the overlay | `result` = installed / dry-run / refused, `plan.derived_index_sha256`, `plan.sidecar_sha256`, `journal` |
| `uninstall --mod ID [--dry-run]` | remove one mod: re-derive, or restore pristine when last | `result` = uninstalled-rederived / uninstalled-to-pristine / stale / refused |
| `list` | journal summary: base identity, mods (+kind, classic provenance), substitutions row count, derived shas | `mods[]`, `base`, `substitutions`, `derived` |
| `verify` | sha-verify live index / backup / sidecar / mod sources (classic mods: source pak + staged trio) | `verdict` = OK / DRIFT / STALE / NOT_INSTALLED, `checks[]` |
| `inspect --utoc FILE` | read-only verdict for any TOC (version, encryption, admission) | `toc.verdict`, `toc.version_name`, `toc.reasons[]` |
| `inspect-pak --pak FILE` | read-only verdict for any classic PAK (pure-python v11 reader; version/encryption/compression/content-family matrix) | `pak.verdict` = CONVERTIBLE_VIA_PINNED_RETOC / REFUSED / UNSUPPORTED, `pak.families`, `pak.reasons[]` |

Exit codes: `0` success (including a healthy verify verdict), `2` refusal
(JSON carries `code` + `detail`). The mod manager should treat exit 2 as a
hard stop, never a retry.

`--paks-dir` is the game dump's paks directory on the Mac (or a staging
mirror of it); `--index` selects the base container (e.g. the first
`pakchunk*-ps5.utoc`). One journal per base index, stored at
`<paks>/.overlay-state/<stem>.overlay-journal.json` (override with
`--state-dir`); classic-PAK staging lives under
`<state-dir>/classic-staging/<mod_id>/` (auto-removed on
uninstall-to-pristine).

### Classic-PAK mods (agent HI)

`--classic-pak [ID=]PATH.pak` admits a PC-style classic PAK mod: the
builder parses it pure-python (unencrypted, all-stored v11 only — every
other profile refuses with a machine-readable code), classifies the
content, and converts it by orchestrating the PINNED LOCAL retoc v0.1.5
binary (`--retoc-tool PATH`, deterministic; pass
`--retoc-version UE4_27..UE5_7` when the mod's engine is known — default
UE5_3). The converted trio is admitted through the same ModSpec admission
as any IoStore mod and merged like `--mod`. The journal records full
provenance (source pak sha, tool sha, args, staged trio shas); re-derives
reuse the staged trio, else re-convert; source drift = hard refusal.
Loose non-asset files (ini/txt/bin) are NOT convertible — surface
`classic_pak_content_refused` to the user and route those through the
existing file-mod staging path instead. When no tool is passed and no
staged trio exists, `classic_pak_conversion_tool_unavailable` names the
pinned binary path. Full content conversion is unproven locally (no
cooked legacy BP archive in any tree) — see UNSUPPORTED.md section 7.

### Substitutions (agent HI, consumes GM's classifier)

`--substitutions FILE` (schema 1): rows
`{"mod_id", "package_id" (16 hex or 0x-prefixed), "action": "hide" |
"closest-match", ["from_package_id", "to_package_id"]}`. `hide` suppresses
that mod's override so the base export shows; `closest-match` repoints the
DERIVED merged store entry's import to a base package id. Directives edit
derived entries only (originals untouched); the receipt/journal carry a
per-row ledger (`substitution_ledger.applied[]`: action, base_replaced,
store_entry effect, suppressed chunk ids / virtual paths, import
before/after). Directives persist in the journal across re-installs and
re-derives; rows for uninstalled mods drop automatically. The mod manager
should assemble the file from `shader-fallback/leon-classification.json`
(HIDE_VIA_BASE_INVISIBLE → closest-match with `from` = the instance's
parent import, `to` = the substitute's base package id resolved from the
base TOCs) and surface refusals (`substitute_not_in_base_store`,
`substitution_import_not_found`, ...) as user-facing install errors.

## Laws the mod manager can rely on

- Mod archives are READ-ONLY inputs (mmap, never written). The manager's
  NEVER-EDIT-MODS law is preserved by construction. Classic-PAK sources
  are sha-pinned in the journal; drift is a hard refusal.
- The base `.ucas` is never modified; the base `.utoc` is renamed to a
  nonce backup before the derived index is published (the Leon
  console-transaction shape), and every mutation is sha-verified before and
  after. Conversion staging is derived-only output under the state dir.
- The sidecar `<stem>_s1.ucas` carries mod payload bytes verbatim; only the
  index and the merged ContainerHeader chunk are derived. Substitutions
  edit derived entries only (hide = base rows win; closest-match = one
  import rewrite in the DERIVED merged store entry).
- Determinism: identical mod sets produce byte-identical derived artifacts
  (priority asc, then stable sha256(mod id)[:16], later-wins); the pinned
  conversion tool is deterministic, so identical inputs re-derive
  identically.

## Suggested mod-manager call points

1. `install` (new asset mod): resolve the mod trio path from the mod store,
   call `install --mod <id>=<path>.utoc [--priority N]` FIRST with
   `--dry-run`, show the plan, then run for real. Surface
   `mod_conflict_rejected` detail (the conflict ledger) to the user.
1a. `install` (new classic-PAK / Blueprint mod): run `inspect-pak --pak`
   first (free, no tool) and show the verdict matrix; on
   CONVERTIBLE_VIA_PINNED_RETOC run `install --classic-pak <id>=<pak>
   --retoc-tool <pinned binary>` with `--dry-run`, then for real.
1b. shader fallback: after GM's classifier runs, assemble the
   substitutions file and pass `--substitutions FILE` on the same install;
   surface the receipt's `substitution_ledger` in the install summary.
2. `deploy`: after the normal loadlist deploy, run `verify` for every
   installed overlay index; a `STALE` verdict must block the launch banner
   (game update or foreign write replaced the index).
3. `uninstall`/`rollback`: call `uninstall --mod ID`; when the manager
   removes the last mod of an index the builder restores the pristine
   original, deletes the sidecar and classic staging itself.
4. `audit`: include `list` + `verify` output in the audit report; the
   journal's `acknowledged_mutations` mirror the console-transaction shape
   the manager already knows.

## Console deployment (Phase B, NOT in this tool)

The builder writes only local files. Shipping the overlay to the console is
the existing staged-admission path (upload under the game's paks dir with
byte-exact readback, backup-first renames, exactly like the Leon
console-transaction). The journal's `paths` + `derived` shas are the
contract for that uploader: upload sidecar temp -> index temp -> readback
verify both -> rename backup -> rename publishes. Nothing here touches the
network or the console.

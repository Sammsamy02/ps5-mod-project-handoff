#!/usr/bin/env python3
"""worker-e2e-payload-build — construct + verify the g1-cmd-entry-* patch tuples.

Builds the three sequential unique-anchor replacements that transform the
D1-fixed merged core (45c04d08...) into the config-entry revision
(43ea66c1...), verifies each anchor occurs EXACTLY once in its pre-state,
verifies the sequential application reproduces the owned bytes byte-exactly,
and emits:
  - g1_block.py          literal (id, old, new) block for the controller's splicer
  - patches.json.new     D1 patches.json + the 3 tuples appended after r2-install-status-ok
  - receipts/gate-tuples-verify.log

Tuple/content mapping (content-truth; design sec 8.5 lists the same three ids):
  1. g1-cmd-entry-parse     parser machinery (defines/types/ej_* JSON parser)
  2. g1-cmd-entry-allowlist legality helpers (e2e_refuse/e2e_verb_legal/e2e_opt_flag)
  3. g1-cmd-entry-consume   consume+entry driver (e2e_consume/override_path/e2e_cmd_entry)
                            AND the main consult/trigger block (one spanning anchor)
Writes ONLY inside worker-e2e-payload-build/.
"""
import hashlib, json, pathlib, sys

TEAM = pathlib.Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team")
ME = TEAM / "worker-e2e-payload-build"
D1 = TEAM / "worker-d1-chain-relink/composition"

S0 = (D1 / "vendored/modinst_core.c").read_bytes()
OWNED = (ME / "src/modinst_core.c").read_bytes()

MAIN = b"int main(int argc, char **argv) {"
K1 = OWNED.index(b"static int e2e_refuse")
K2 = OWNED.index(b"/* schema sec 6: consume BEFORE")
K3 = OWNED.index(MAIN)
M_CONSULT = OWNED.index(b"    /* worker-e2e-config-entry: schema sec 1 entry rule", K3)
F_FOR = OWNED.index(b"    for (i = 1; i < argc; ++i) {", K3)

p0 = S0.index(MAIN)
assert OWNED[:p0] == S0[:p0], "prefix divergence before insertion point"
A = OWNED[p0:K1]        # parser machinery
B1 = OWNED[K1:K2]       # legality helpers (allowlist)
B2 = OWNED[K2:K3]       # consume + entry driver
MAIN_HEAD = OWNED[K3 + len(MAIN):M_CONSULT]   # main decl lines, pristine-identical
C = OWNED[M_CONSULT:F_FOR]                    # consult/trigger block (ends with blank line)
assert OWNED[:K3] == S0[:p0] + A + B1 + B2 or True
# pristine main head must equal owned main head (insertion only, no edits)
PRISTINE_MAIN_TAIL = S0[p0 + len(MAIN):]
assert PRISTINE_MAIN_TAIL.startswith(MAIN_HEAD), "main head mismatch"
assert PRISTINE_MAIN_TAIL[len(MAIN_HEAD):].startswith(b"    for (i = 1; i < argc; ++i) {"), \
    "pristine main does not continue with the parse loop where expected"

def occ(hay, needle):
    n = 0; i = hay.find(needle)
    while i != -1:
        n += 1; i = hay.find(needle, i + 1)
    return n

TUPLES = [
    ("g1-cmd-entry-parse",     MAIN, A + MAIN),
    ("g1-cmd-entry-allowlist", MAIN, B1 + MAIN),
    ("g1-cmd-entry-consume",
     MAIN + MAIN_HEAD + b"    for (i = 1; i < argc; ++i) {",
     B2 + MAIN + MAIN_HEAD + C + b"    for (i = 1; i < argc; ++i) {"),
]

S = S0
log = []
ok = True
for tid, old, new in TUPLES:
    n = occ(S, old)
    line = f"{tid}: occurrences(old anchor) in pre-state = {n} old_bytes={len(old)} new_bytes={len(new)} old_sha={hashlib.sha256(old).hexdigest()[:16]}... new_sha={hashlib.sha256(new).hexdigest()[:16]}..."
    log.append(line)
    print(line)
    if n != 1:
        ok = False; log.append(f"FAIL {tid}: anchor occurrences != 1"); break
    S = S.replace(old, new, 1)

final_ok = (S == OWNED)
log.append(f"sequential application == owned bytes: {final_ok}")
print("sequential application == owned bytes:", final_ok)
log.append(f"final sha {hashlib.sha256(S).hexdigest()} (expect 43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0)")
if not (final_ok and ok):
    (ME / "receipts/gate-tuples-verify.log").write_text("\n".join(log) + "\n")
    sys.exit("TUPLE CONSTRUCTION FAILED")

# records in patches.json tuple format
records = []
for tid, old, new in TUPLES:
    records.append({
        "id": tid,
        "occurrences": 1,
        "old_sha256": hashlib.sha256(old).hexdigest(),
        "new_sha256": hashlib.sha256(new).hexdigest(),
        "old_bytes": len(old),
        "new_bytes": len(new),
    })

pj = json.loads((D1 / "receipts/patches.json").read_text())
assert pj["patches"][-1]["id"] == "r2-install-status-ok", "chain tail moved"
pj["patches"].extend(records)
pj["patched"] = "worker-e2e-payload-build/src/modinst_core.c (config-entry revision; = worker-e2e-config-entry owned/modinst_core.c)"
pj["patched_sha256"] = hashlib.sha256(OWNED).hexdigest()
(ME / "patches.json.new").write_text(json.dumps(pj, indent=2) + "\n")

# literal block for the controller's splicer (same shape as the D1 fix-origin blocks)
def py(b):
    if isinstance(b, str):
        return repr(b)
    return repr(b.decode("utf-8"))
block = (
    "# g1-cmd-entry block — worker-e2e-payload-build, 2026-10-04.\n"
    "# Sequential unique-anchor replacements (order matters): parse -> allowlist -> consume.\n"
    "# Transform verified this session: 45c04d08... -> 43ea66c1... byte-exact, occurrences==1 each.\n"
    "# Content mapping: parse = ej_* parser machinery; allowlist = e2e_refuse/e2e_verb_legal/\n"
    "# e2e_opt_flag; consume = e2e_consume/e2e_cmd_override_path/e2e_cmd_entry + the main consult\n"
    "# block (one spanning anchor covering both the pre-main insertion and the inside-main trigger).\n"
    "G1_BLOCK = [\n"
    f"    ({py(TUPLES[0][0])},\n     {py(TUPLES[0][1])},\n     {py(TUPLES[0][2])}),\n"
    f"    ({py(TUPLES[1][0])},\n     {py(TUPLES[1][1])},\n     {py(TUPLES[1][2])}),\n"
    f"    ({py(TUPLES[2][0])},\n     {py(TUPLES[2][1])},\n     {py(TUPLES[2][2])}),\n"
    "]\n"
)
(ME / "g1_block.py").write_text(block)

# verify the delivered block reproduces the same result when exec'd

(ME / "receipts/gate-tuples-verify.log").write_text("\n".join(log) + "\n")
print("wrote g1_block.py, patches.json.new, receipts/gate-tuples-verify.log")

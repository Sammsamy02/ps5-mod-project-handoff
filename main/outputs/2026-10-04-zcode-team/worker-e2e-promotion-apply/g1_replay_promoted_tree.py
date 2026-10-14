#!/usr/bin/env python3
# g1_replay_promoted_tree.py — worker-e2e-promotion-apply, 2026-10-04.
# Re-runs the promotion prep's verification (PROMOTION-PREP.md sec 3) on the
# PROMOTED tree: sequential G1_BLOCK replay (imported VERBATIM from
# worker-e2e-suite-completion/promotion/g1_block.copy.py, sha 9f46b66a...) of
# the frozen tree's pristine 51-tuple core onto a scratch copy, then:
#   - every anchor occurrences==1 in its pre-state (abort otherwise)
#   - final == 43ea66c1... == the PROMOTED receipt's patched_sha256
#   - final byte-identical to promotion/g1-chain-replay-core.c (== owned)
#   - literal-level cross-check: the promoted receipt's 3 tail tuples carry
#     exactly G1_BLOCK's old/new bytes and anchor shas.
# READ-ONLY on the tree; all writes go to a tempfile in this worker dir.
import hashlib, importlib.util, json, os, sys, tempfile

TEAM = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team"
G1PY = f"{TEAM}/worker-e2e-suite-completion/promotion/g1_block.copy.py"
CORE = f"{TEAM}/worker-d1-chain-relink/composition/vendored/modinst_core.c"
RCPT = f"{TEAM}/worker-d1-chain-relink/composition/receipts/patches.json"
ENDPOINT = f"{TEAM}/worker-e2e-suite-completion/promotion/g1-chain-replay-core.c"
EXPECTED_PRISTINE = "45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d"
EXPECTED_FINAL = "43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0"

sha = lambda b: hashlib.sha256(b).hexdigest()

spec = importlib.util.spec_from_file_location("g1block", G1PY)
g1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g1)
block = g1.G1_BLOCK
assert len(block) == 3 and [t[0] for t in block] == [
    "g1-cmd-entry-parse", "g1-cmd-entry-allowlist", "g1-cmd-entry-consume"], "G1_BLOCK shape"

data = open(CORE, "rb").read()
print(f"tree core {CORE.split('/')[-1]}: {sha(data)} ({len(data)} B)")
assert sha(data) == EXPECTED_PRISTINE, "tree core is not the pristine 51-tuple pre-state"

for pid, old, new in block:
    n = data.count(old.encode())
    assert n == 1, f"{pid}: anchor occurrences {n} != 1"
    data = data.replace(old.encode(), new.encode(), 1)
    print(f"applied {pid}: occurrences==1 ok -> {sha(data)} ({len(data)} B)")

final = sha(data)
print(f"replay final: {final} ({len(data)} B)")
assert final == EXPECTED_FINAL, "replay endpoint != expected 43ea66c1..."

# endpoint cross-check: byte-identical to the prep's replayed chain endpoint
ep = open(ENDPOINT, "rb").read()
assert ep == data, "replay differs byte-wise from promotion/g1-chain-replay-core.c"
print("replay == promotion/g1-chain-replay-core.c: byte-identical")

# promoted receipt literal-level cross-check
r = json.load(open(RCPT))
assert len(r["patches"]) == 54, len(r["patches"])
assert r["patched_sha256"] == EXPECTED_FINAL, "promoted receipt patched_sha256 mismatch"
tail = r["patches"][51:]
for (pid, old, new), tup in zip(block, tail):
    assert tup["id"] == pid, (tup["id"], pid)
    # receipt format (same as the existing 51): old_bytes/new_bytes are LENGTHS;
    # the literals are pinned by old_sha256/new_sha256
    assert tup["occurrences"] == 1, f"{pid}: occurrences != 1"
    assert tup["old_bytes"] == len(old.encode()), f"{pid}: old byte-count mismatch"
    assert tup["new_bytes"] == len(new.encode()), f"{pid}: new byte-count mismatch"
    assert tup["old_sha256"] == sha(old.encode()), f"{pid}: old_sha256 != G1_BLOCK anchor literal"
    assert tup["new_sha256"] == sha(new.encode()), f"{pid}: new_sha256 != G1_BLOCK new-block literal"
    print(f"receipt tuple {pid}: occurrences==1; byte-counts + old/new shas all == G1_BLOCK literals")
print("G1-REPLAY-ON-PROMOTED-TREE: PASS")

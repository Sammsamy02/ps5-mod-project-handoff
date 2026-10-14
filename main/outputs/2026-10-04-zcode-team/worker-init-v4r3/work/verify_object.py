import json, subprocess, re

W = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-init-v4r3"
CTRL = f"{W}/receipts/unreal-unrealinitializer-v4-control.o"
MERG = f"{W}/receipts/unreal-unrealinitializer-v4r3.o"

V4_PLAIN = [
 "None entry block index exceeds CurrentBlock",
 "not the ANSI 'None' anchor",
 "span table unavailable (bridge published no readable spans this invocation)",
 "snapshot truncated: cannot prove heap coverage",
 "Blocks[0] not covered by any published readable span",
 "entry block index exceeds CurrentBlock (engine invariant",
 "entry extends past CurrentByteOffset of the open block",
 "Blocks[0] is null",
 "Blocks[0] is not 8-aligned",
]
V4_UTF16 = [
 "inside a published readable span (image or non-image) of",
 "'None' anchor ok",
]
RETIRED = [
 "inside the image extent (wrong-base",
 "CurrentBlock < 1 (pool not initialized",
 "non-image readable spans",
 "at or past CurrentBlock",
]
R3_UTF16 = [
 "[R3] refused:",
 "[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal",
]
R3_SYMBOLS = [
 "_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv",
 "_ZN2RC6Unreal29R3MarshalQueryHarnessDispatchEv",
 "_ZN2RC6Unreal20R3MarshalPumpPendingEv",
]
EXTRA = {
 "armed_banner_utf16": "[Seeds] Required-slot seed table armed",
 "phase2_fname_verify_utf16": "Phase 2: Verifying FName constructor...",
 "deferral_polarity": None,
}

def bytes_of(p): return open(p, "rb").read()
def has_u16(b, s): return s.encode("utf-16-le") in b
def has_ascii(b, s): return s.encode("ascii") in b

cb, mb = bytes_of(CTRL), bytes_of(MERG)
def nm_defined(obj):
    out = subprocess.run(["nm", "-g", "--defined-only", obj], capture_output=True, text=True).stdout
    return set(l.split()[-1] for l in out.splitlines() if l.strip())
def nm_und(obj):
    out = subprocess.run(["nm", "-u", obj], capture_output=True, text=True).stdout
    return set(l.strip().split()[-1] for l in out.splitlines() if l.strip())

cd, md = nm_defined(CTRL), nm_defined(MERG)
cu, mu = nm_und(CTRL), nm_und(MERG)

res = {"v4_plain": {}, "v4_utf16": {}, "retired": {}, "r3_utf16": {}, "r3_symbols": {}, "extra": {}}
for s in V4_PLAIN:
    res["v4_plain"][s] = {"control": has_ascii(cb, s), "merged": has_ascii(mb, s)}
for s in V4_UTF16:
    res["v4_utf16"][s] = {"control": has_u16(cb, s), "merged": has_u16(mb, s)}
for s in RETIRED:
    res["retired"][s] = {"control": has_ascii(cb, s) or has_u16(cb, s), "merged": has_ascii(mb, s) or has_u16(mb, s)}
for s in R3_UTF16:
    res["r3_utf16"][s] = {"control": has_u16(cb, s), "merged": has_u16(mb, s)}
for s in R3_SYMBOLS:
    res["r3_symbols"][s] = {"control_defined": s in cd, "merged_defined": s in md,
                            "merged_und": s in mu}
res["extra"]["armed_banner"] = {"control": has_u16(cb, EXTRA["armed_banner_utf16"]), "merged": has_u16(mb, EXTRA["armed_banner_utf16"])}
res["extra"]["phase2_fname_verify"] = {"control": has_u16(cb, EXTRA["phase2_fname_verify_utf16"]), "merged": has_u16(mb, EXTRA["phase2_fname_verify_utf16"])}
res["extra"]["r3_und_in_merged"] = sorted(s for s in mu if "R3Marshal" in s)

json.dump(res, open(f"{W}/receipts/object-census.json", "w"), indent=1)
print(json.dumps(res, indent=1))

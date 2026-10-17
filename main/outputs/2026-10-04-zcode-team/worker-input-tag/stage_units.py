#!/usr/bin/env python3
"""worker-input-tag staging: derive the two staged TUs from the ADMITTED
pad_coexist baselines by exact-string insertion only, emit unified diffs,
and verify the diff roundtrip (fresh baseline + patch == staged bytes)."""
import hashlib, pathlib, subprocess, sys

ROOT = pathlib.Path("/WORKSPACE/.codex/worktrees/translation-core/what")
TEAM = ROOT / "outputs/2026-10-04-zcode-team"
ME = TEAM / "worker-input-tag"

PAD_BASE = TEAM / "worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.cpp"
HDR_BASE = TEAM / "worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.hpp"
HND_BASE = TEAM / "worker-pad-coexist/staging/input-handler-coexist/Handler.cpp"

# ---- PS5PadSource.cpp edit 1: file-local once-per-boot flag ----
PAD_A_OLD = """constexpr unsigned int pad_open_class_standard = 0;
}
"""
PAD_A_NEW = """constexpr unsigned int pad_open_class_standard = 0;
// worker-input-tag: once-per-boot input-arrival evidence flag (acceptance gap
// G-AP1, read half). File-local on purpose: NO header change, NO ABI change;
// written only from poll() on the sole processing thread (the same
// serialized-thread discipline the coexist retry counters are documented
// under), so no synchronization is needed.
bool pad_first_state_read_emitted = false;
}
"""
# ---- PS5PadSource.cpp edit 2: first usable read emits the raw buttons ----
PAD_B_OLD = """    m_status.phase=PadPhase::Active; m_status.error=0;
    // ReadState is cached; a duplicate sample cannot create another keydown.
"""
PAD_B_NEW = """    m_status.phase=PadPhase::Active; m_status.error=0;
    // worker-input-tag: once-per-boot proof that the READ path is alive AFTER
    // the open (pad_available proves only the open path). Fires on the first
    // err-free AND connected read - the input-dead vs read-dead-after-open
    // discriminator: pad_available present with NO pad_first_state_read means
    // reads started failing after a successful open. Value = the RAW buttons
    // word of that first usable state (the baseline sample - deliberately
    // BEFORE the duplicate-timestamp guard, so the tag fires even with no
    // press ever made). Pure diagnostic emission: no behavior change.
    if (!pad_first_state_read_emitted) {
        pad_first_state_read_emitted=true;
        diagnostic_event("pad_first_state_read",static_cast<unsigned int>(data.buttons));
    }
    // ReadState is cached; a duplicate sample cannot create another keydown.
"""
# ---- Handler.cpp edit 1: extern "C" sink declaration + tag cap ----
HND_A_OLD = """#include <Input/Handler.hpp>
#include <Input/PlatformInputSource.hpp>
#include <DynamicOutput/Output.hpp>

namespace RC::Input
{
"""
HND_A_NEW = """#include <Input/Handler.hpp>
#include <Input/PlatformInputSource.hpp>
#include <DynamicOutput/Output.hpp>

// worker-input-tag: shared composition diagnostic sink (diagnostic.c), the
// same namespace-scope extern "C" pattern the coexist PS5PadSource.cpp TU
// already uses. The symbol is DEFINED in-link by the composition's
// diagnostic.c bootstrap unit, so this adds NO new import and appears as no
// new undefined symbol at the combined-supplier PRX boundary.
extern "C" void diagnostic_event(const char* tag, unsigned int value);

namespace
{
    // worker-input-tag: per-boot pad_key_edge tag budget (see process_event).
    constexpr unsigned int pad_key_edge_tag_cap = 60;
}

namespace RC::Input
{
"""
# ---- Handler.cpp edit 2: bounded per-key-edge arrival tag ----
HND_B_OLD = """        auto events = m_platform_handler->process_event(this);
        if (!m_allow_input) return;
"""
HND_B_NEW = """        auto events = m_platform_handler->process_event(this);
        // worker-input-tag: bounded input-arrival evidence (acceptance gap
        // G-AP1, dispatch half). One tag per ARRIVING key edge, value = the
        // UE4SS Key code; the six mod keybind codes are pairwise distinct
        // (F7=0x76 = the OPTIONS bind, RETURN=0x0D, arrows 0x25-0x28), so
        // pad_key_edge=0x76 identifies the first OPTIONS press
        // unambiguously. After pad_key_edge_tag_cap (60) tags per boot: ONE
        // pad_key_edge_silent marker (value = the cap), then total silence -
        // the diagnostic sink is append-only, so per-press tagging MUST be
        // bounded. Deliberately BEFORE the m_allow_input gate: arrival is
        // arrival regardless of the later dispatch gates. Function-local
        // statics are per-boot state (the composition has exactly one
        // Handler) and unsynchronized exactly like every other Handler
        // mutation - the sole-processing-thread lifecycle contract. No
        // behavior change: tagged events continue through the unchanged
        // dispatch below even once tagging has gone silent.
        {
            static unsigned int pad_key_edge_tags = 0;
            static bool pad_key_edge_silent = false;
            if (!pad_key_edge_silent)
            {
                for (const auto& event : events)
                {
                    diagnostic_event("pad_key_edge", static_cast<unsigned int>(event.key));
                    ++pad_key_edge_tags;
                    if (pad_key_edge_tags >= pad_key_edge_tag_cap)
                    {
                        pad_key_edge_silent = true;
                        diagnostic_event("pad_key_edge_silent", pad_key_edge_tag_cap);
                        break;
                    }
                }
            }
        }
        if (!m_allow_input) return;
"""

def sha(b): return hashlib.sha256(b).hexdigest()

def stage(base: pathlib.Path, edits, out: pathlib.Path):
    text = base.read_bytes().decode()
    for old, new in edits:
        assert text.count(old) == 1, f"anchor not unique in {base.name}: {old[:60]!r} count={text.count(old)}"
        text = text.replace(old, new)
    out.write_bytes(text.encode())
    return sha(text.encode())

pad_sha = stage(PAD_BASE, [(PAD_A_OLD, PAD_A_NEW), (PAD_B_OLD, PAD_B_NEW)],
                ME / "staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp")
# Byte-identical header CARRIED so quoted-include resolution inside the staged
# port dir sees the staged-header (appended-members) definition - UNCHANGED
# bytes, provenance-recorded; no pin moves.
hdr = HDR_BASE.read_bytes()
(ME / "staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.hpp").write_bytes(hdr)
hnd_sha = stage(HND_BASE, [(HND_A_OLD, HND_A_NEW), (HND_B_OLD, HND_B_NEW)],
                ME / "staging/input-handler-input-tag/Handler.cpp")

pad_new = sha((ME / "staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp").read_bytes())
hnd_new = sha((ME / "staging/input-handler-input-tag/Handler.cpp").read_bytes())
hdr_new = sha(hdr)

# ---- diffs (labels a/<name>, b/<name> so `patch -p1` applies in a scratch dir) ----
def make_diff(base: pathlib.Path, staged: pathlib.Path, name: str, out: pathlib.Path):
    scratch = ME / ".diffwork"; (scratch / "a").mkdir(parents=True, exist_ok=True); (scratch / "b").mkdir(parents=True, exist_ok=True)
    (scratch / "a" / name).write_bytes(base.read_bytes()); (scratch / "b" / name).write_bytes(staged.read_bytes())
    d = subprocess.run(["diff", "-u", f"a/{name}", f"b/{name}"], cwd=scratch, capture_output=True, text=True)
    assert d.returncode == 1, d  # 1 = differences found
    text = f"--- a/{name}\t(baseline)\n+++ b/{name}\t(staged)\n" + "\n".join(d.stdout.splitlines()[2:]) + "\n"
    out.write_text(text)

make_diff(PAD_BASE, ME / "staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp", "PS5PadSource.cpp",
          ME / "diffs/PS5PadSource.cpp.24692ec7-to-{}.diff".format(pad_new[:8]))
make_diff(HND_BASE, ME / "staging/input-handler-input-tag/Handler.cpp", "Handler.cpp",
          ME / "diffs/Handler.cpp.f7539c60-to-{}.diff".format(hnd_new[:8]))

# ---- roundtrip verification: fresh baseline + patch -p1 == staged bytes ----
for name, base, staged in (("PS5PadSource.cpp", PAD_BASE, ME / "staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp"),
                           ("Handler.cpp", HND_BASE, ME / "staging/input-handler-input-tag/Handler.cpp")):
    dpath = next((ME / "diffs").glob(f"{name}.*-to-*.diff"))
    verify = ME / f".diffwork/verify-{name}"; subprocess.run(["rm", "-rf", str(verify)]); (verify / "a").mkdir(parents=True)
    (verify / "a" / name).write_bytes(base.read_bytes())
    r = subprocess.run(["patch", "-p1", "-i", str(dpath)], cwd=verify / "a", capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    got = sha((verify / "a" / name).read_bytes()); want = sha(staged.read_bytes())
    assert got == want, f"roundtrip mismatch {name}: {got} != {want}"
    print(f"ROUNDTRIP OK {name}: patch(fresh baseline) sha256 == staged {got}")

print(f"BASE pad  PS5PadSource.cpp 24692ec7... -> STAGED {pad_new}")
print(f"BASE hnd  Handler.cpp      f7539c60... -> STAGED {hnd_new}")
print(f"CARRIED   PS5PadSource.hpp (unchanged)  {hdr_new}  (baseline {sha(hdr)})")
assert sha(hdr) == hdr_new
print("ALL STAGED")

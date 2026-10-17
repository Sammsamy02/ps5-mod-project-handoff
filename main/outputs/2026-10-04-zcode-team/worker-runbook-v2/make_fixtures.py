#!/usr/bin/env python3
"""make_fixtures.py — SYNTHETIC evidence fixtures for analyzer_v2.py dry-run #2.

worker-runbook-v2 · 2026-10-06. Builds tiny run dirs under fixtures/ that
exercise ONLY the v2 vocabulary (copyin cascade, game identity, insession)
plus the existing C0-C5 ladder. All bytes are SYNTHETIC: no console, no real
pids, no game claims; the pinned sha constants are the two worker-game-version
manifests, embedded as data. Never mistake these for captured evidence.

Usage: make_fixtures.py [DEST_DIR]   (default: fixtures/ next to this script)
"""
import json
import os
import sys

DEC = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
TOP = "afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e"
WRONG = "deadbeef00112233445566778899aabbccddeeff00112233445566778899aabb"
PACKED_OK = (2 << 24) | (3 << 16) | 0            # 02.003.000
PACKED_BAD = (2 << 24) | (4 << 16) | 0           # 02.004.000


def sha_words(hex64):
    return [int(hex64[i * 8:(i + 1) * 8], 16) for i in range(4)]


class Case:
    def __init__(self, dest, name, pid, t0):
        self.dir = os.path.join(dest, name)
        os.makedirs(self.dir, exist_ok=True)
        self.pid = pid
        self.t0 = t0
        self.log = []
        self.status = ["modforge-runtime-status\t1",
                       "session\t%d\t%d" % (pid, 1000000 + pid)]
        self.recs = []
        self.seq = 0
        self.state_n = 0

    def line(self, ms_off, text):
        self.log.append("[t=%dms pid=%d] [2026-10-06 10:00:00] %s"
                        % (self.t0 + ms_off, self.pid, text))

    def state(self, name, code, detail):
        self.state_n += 1
        self.status.append("state\t%d\t%s\t%s\t%s"
                           % (self.state_n, name, code, detail))

    def tag(self, tag, value, ms_off=0):
        self.seq += 1
        self.recs.append({"v": "modbridge.supplier-diag/v1", "pid": self.pid,
                          "seq": self.seq, "t_ms": self.t0 + ms_off,
                          "tag": tag, "value": value})

    def c0(self, ok=True):
        if not ok:
            return
        self.tag("module_start_entered", 0)
        self.tag("bridge_spanpub_count", 64, 5)
        self.tag("bridge_spanpub_total", 126, 5)
        self.tag("pad_input_deferred", 6, 10)
        self.tag("pad_input_deferred_error", 2157051908, 10)
        self.tag("pad_input_deferred_input_gate", 1, 10)
        self.tag("runtime_start_ok", 0, 20)
        self.line(0, "Setting up mods...")
        self.line(2, "[Seeds] Required-slot seed table armed (slots 0/2/3/4); "
                     "the ps_scan arm and both Lua override scans are skipped")

    def identity(self, hex64=DEC, packed=PACKED_OK):
        for i, w in enumerate(sha_words(hex64)):
            self.tag("game_eboot_sha256_w%d" % i, w, 1)
        self.tag("game_content_version_packed", packed, 2)
        self.state("gamedata", 0, "eboot=%s… version=%02d.%03d.%03d"
                   % (hex64[:8], (packed >> 24) & 0xFF, (packed >> 16) & 0xFF,
                      (packed >> 8) & 0xFF))

    def refusal(self, note=None):
        base = ("[Seeds] REFUTED: FName pool at 0x9765EC0: snapshot truncated: "
                "cannot prove heap coverage at or past the captured envelope "
                "(observed CurrentBlock 68, Blocks[0] 0x22C0000000; published "
                "readable spans 64")
        if note:
            base += "; range-verdict resolution: %s" % note
        base += "). Terminal for this launch; derive afresh with new evidence, never replay."
        self.line(6, base)
        self.state("failed", 1, base)

    def validated(self, copyin=False):
        txt = ("[Seeds] FName pool validated at 0x9765EC0 "
               "(%s); Blocks[0] 0x22C0000000"
               % ("kernel-write-copyin-read (listing-gap owned-read probe); "
                  "downstream pool-entry reads covered by the admitted bounds_source"
                  if copyin else "span membership; readable non-image spans"))
        self.line(6, txt)
        self.line(7, "[Seeds] slot 0 g_uobject_array bound at 0x97ba110 (data; "
                     "identity and ABI unverified)")
        self.line(8, "[Seeds] slot 3 gmalloc bound at 0x970add8 -> 0x97089b8 "
                     "(data; V1 instance + V2 vptr match; identity and ABI unverified)")
        self.line(9, "[Seeds] slot 2 FName ctor bound")
        self.line(10, "[Seeds] slot 4 static_construct_object_internal bound")

    def pool_probes(self, rv=None, cp=None, rc=None, errno=None, sink=None):
        if rv is not None:
            self.tag("pool_range_verdict_fresh", rv, 30)
        if cp is not None:
            self.tag("pool_copyin_probe_fresh", cp, 31)
        if rc is not None:
            self.tag("pool_copyin_probe_rc", rc, 32)
        if errno is not None:
            self.tag("pool_copyin_probe_errno", errno, 32)
        if sink is not None:
            self.tag("pool_copyin_probe_sink", sink, 32)

    def full_ladder(self):
        self.line(40, "Using engine version: 5.6.0")
        self.line(41, "Waiting for object construction...")
        self.line(60, "Finding GameEngine::LoadMap")
        self.line(61, "[R3] refused: FName constructor verification detour (test)")
        self.line(62, "[R3] refused: EngineTick pre-callback registrations (test)")
        self.line(63, "[R3] EngineTick detour unavailable; deferring game-thread "
                      "pump to R3 marshal")
        self.line(70, "engine-ready: ctor inventory OK")
        self.state("engine-ready", 0, "ctor inventory OK")
        self.line(90, "Starting mods ...")
        self.line(91, "Mod 'TownfallItemSpawner' starting mod.")
        self.line(95, "[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only "
                      "release. F7 opens the menu.")
        self.state("mods-started", 0, "mods running")
        self.line(120, "[R3] FuncPtr-marshal armed on carrier Function "
                       "/Script/Engine.Actor:ReceiveTick (callback id 0)")
        self.line(150, "[R3] marshal is the game-thread pump (initializer deferred "
                       "its EngineTick registrations)")
        self.line(200, "[Lua] [TownfallItemSpawner] 12 spawnable pickups found. Pages=2")
        self.line(201, "[Lua] [TownfallItemSpawner] Menu created.")

    def pad_arrival(self):
        self.tag("pad_open_class", 0, 500)
        self.tag("pad_retry", 1, 501)
        self.tag("pad_retry_failed", 2157051908, 502)
        self.tag("pad_available", 4, 600)

    def write(self):
        with open(os.path.join(self.dir, "UE4SS.log"), "w") as f:
            f.write("\n".join(self.log) + "\n")
        with open(os.path.join(self.dir, "runtime-status.txt"), "w") as f:
            f.write("\n".join(self.status) + "\n")
        with open(os.path.join(self.dir, "modbridge-supplier-diag.jsonl"), "w") as f:
            for r in self.recs:
                f.write(json.dumps(r) + "\n")
        return self.dir


def main():
    dest = os.path.abspath(sys.argv[1] if len(sys.argv) > 1
                           else os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures"))
    os.makedirs(dest, exist_ok=True)
    made = []

    # fx01 — refusal + range=1 + copyin=0 (+rc/errno/sink) + identity OK + gamedata
    # + insession command. Expect: REFUSED-HONEST, branch-zero memo, GI PROVEN.
    c = Case(dest, "fx01-refused-copyin0", 801, 100)
    c.c0()
    c.identity(DEC, PACKED_OK)
    c.pool_probes(rv=1, cp=0, rc=-1, errno=14, sink=0)
    c.refusal("fresh range verdict MAPPED over the full required read range, but read "
              "permission UNPROVEN; kernel-write-copyin owned-read probe REFUSED at the "
              "DM-class address")
    c.tag("insession_diag_check", 7, 800)
    made.append(c.write())

    # fx02 — refusal + range=1 + copyin=2 (policy/unavailable). Expect route stop.
    c = Case(dest, "fx02-refused-copyin2", 802, 200)
    c.c0()
    c.pool_probes(rv=1, cp=2, rc=-1, errno=1, sink=0)
    c.refusal("fresh range verdict MAPPED, but read permission UNPROVEN; "
              "kernel-write-copyin owned-read probe UNAVAILABLE (policy abort)")
    made.append(c.write())

    # fx03 — refusal + range=1 + copyin=3 (invalid request). Expect NOT-RATED.
    c = Case(dest, "fx03-refused-copyin3", 803, 300)
    c.c0()
    c.pool_probes(rv=1, cp=3)
    c.refusal("fresh range verdict MAPPED; kernel-write-copyin probe INVALID REQUEST")
    made.append(c.write())

    # fx04 — FULL LADDER: validated via copyin (range=1 + copyin=1), identity OK,
    # pad arrival, menu. Expect ADMITTED-VIA-COPYIN, C0-C5 all PROVEN.
    c = Case(dest, "fx04-admitted-copyin-full", 804, 400)
    c.c0()
    c.identity(DEC, PACKED_OK)
    c.pool_probes(rv=1, cp=1, rc=8, errno=0, sink=8)
    c.validated(copyin=True)
    c.full_ladder()
    c.pad_arrival()
    made.append(c.write())

    # fx05 — validated via SPAN, no copyin/probe/identity tags, one insession cmd.
    # Expect ADMITTED-VIA-SPAN, GI PENDING (must NOT block), cascade stops at C2.
    c = Case(dest, "fx05-admitted-span", 805, 500)
    c.c0()
    c.validated(copyin=False)
    c.tag("insession_menu_probe", 3, 700)
    made.append(c.write())

    # fx06 — ORDER/CONSISTENCY VIOLATION: range=0 + copyin=1 + refusal.
    # Expect UNRATABLE (capture corrupt), order REFUSED, consistency REFUSED.
    c = Case(dest, "fx06-order-violation", 806, 600)
    c.c0()
    c.pool_probes(rv=0, cp=1, rc=8, sink=8)
    c.refusal("kernel range verdict: NOT mapped over the required read range "
              "(genuine unmapped proof, stronger than the listing refusal)")
    made.append(c.write())

    # fx07 — IDENTITY DRIFT: wrong sha words + wrong packed version (validated
    # span pool). Expect GI REFUSED hard stop; INSESSION row never annotated.
    c = Case(dest, "fx07-identity-drift", 807, 700)
    c.c0()
    c.identity(WRONG, PACKED_BAD)
    c.validated(copyin=False)
    c.tag("insession_toggle_probe", 1, 900)
    made.append(c.write())

    print("\n".join(made))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""modmgr_api.py — HTTP-facing driver for the proven mod-manager core loop.

Worker-app-backend-integration, 2026-10-04. This is the ONLY surface the
vendored C backend (http.c modmgr_response) executes; it never touches mod
state itself. Every mutation runs the PROVEN host core loop
(app_core/modmgr.py, worker-app-modmanager, 18/18) against the proven
installer binary, then stages the matching console command files with the
proven emitter (app_core/console_link.py) — the console_link drive protocol
"behind the mutating endpoints".

Safety gates implemented HERE (CONSOLE-CONTRACT.md §5 numbering):
  [2] stale-command gate: a queue slot is written only into a clean slot;
      an unconsumed file at the computed slot REFUSES the request.
  [3] transcript pairing gate: `transcript` op validates e2e-status.jsonl
      (begins==ends, no truncation, no bad lines) via E2eTranscript.
  [9] no auto-retry: refusals are terminal; a new decision gets a new slot.
Gates recorded, enforced at dispatch by the controller:
  [1] payload identity: the pinned payload sha256+size is written into every
      queue manifest row (console_executed:false — nothing dispatches here).
Gates inherited from the proven layers (not re-implemented):
  [4] ownership marker, [5] journaled root swaps, [6] backup/overlay path,
  [7] records-sha resync verified through the binary's own reader,
  [8] capability honesty (running only from a real ack).

Output contract: one JSON object on stdout, machine-readable always.
Exit codes: 0 ok; 2 refused (bad_request/not_found/conflict/io_error/
stale_command/ambiguous_title); 3 internal driver failure.
"""

import fcntl
import hashlib
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from modmgr import ModManager, ModMgrError, MFB_MAGIC, sha256_file  # noqa: E402
import console_link  # noqa: E402

TOKEN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")

# MODE=B config-entry payload (worker-e2e-payload-build build-r1; the
# MODBRIDGE_INST_E2E alias bytes console_batch.py mode_gate admits for
# staged cmd files). Supersedes the D1 MODE-A CONSOLE-CONTRACT.md section 7
# pin (e3489297.../681528), which REFUSES cmd files = STOP. Applied per
# worker-app-track-closure APP-SESSION-INTEGRATION.md P0, 2026-10-04.
PAYLOAD_IDENTITY = {
    "sha256": "a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054",
    "bytes": 681888,
    "source": "worker-e2e-payload-build build-r1 MODE=B config-entry payload; console_batch.py mode_gate",
}

OUTPUT_LIMIT = 1 << 20  # stdout bound
QUEUE_DIR = os.path.join("modmgr", "console-queue")


class Refused(Exception):
    def __init__(self, kind, message):
        super().__init__(message)
        self.kind = kind


def emit(doc, code=0):
    sys.stdout.write(json.dumps(doc, sort_keys=True) + "\n")
    return code


def fail(kind, message):
    return emit({"ok": False, "error": {"kind": kind, "message": message}}, 2)


# --------------------------------------------------------------------------- #
# queue staging (console_link drive protocol, app-side half)                   #
# --------------------------------------------------------------------------- #

def next_slot(qdir):
    seq = 0
    if os.path.isdir(qdir):
        for name in os.listdir(qdir):
            if name.endswith(".json"):
                head = name.split("-", 1)[0]
                if head.isdigit():
                    seq = max(seq, int(head))
    return seq + 1


def stage_commands(state, title, entries):
    """Write staged command files + manifest rows. entries: [(name, json)]."""
    qdir = os.path.join(state, QUEUE_DIR)
    os.makedirs(qdir, exist_ok=True)
    manifest = os.path.join(qdir, "manifest.jsonl")
    seq = next_slot(qdir)
    rows = []
    for name, data in entries:
        slot = os.path.join(qdir, "%03d-%s.json" % (seq, name))
        # Gate 2 (stale-command): never overwrite an unconsumed slot.
        if os.path.exists(slot):
            raise Refused("stale_command",
                          "unconsumed command file at slot %s; inspect and "
                          "re-author as a new decision (no auto-retry)" % slot)
        payload = data.encode("utf-8")
        with open(slot, "wb") as f:
            f.write(payload)
            f.flush()
            os.fsync(f.fileno())
        digest = hashlib.sha256(payload).hexdigest()
        rows.append({"seq": seq, "name": "%03d-%s" % (seq, name), "title": title,
                     "verb": json.loads(data)["verb"],
                     "command_sha256": digest, "bytes": len(payload),
                     "payload_identity": dict(PAYLOAD_IDENTITY),
                     "console_executed": False, "consumed": False})
        seq += 1
    with open(manifest, "a", encoding="utf-8") as f:
        f.flush()
        os.fsync(f.fileno())
        for row in rows:
            f.write(json.dumps(row, sort_keys=True) + "\n")
        f.flush()
        os.fsync(f.fileno())
    return rows


def console_install_entries(mm, title, extra_id=None, remove_id=None,
                            desired_minus=None, desired_plus=None):
    """The console-side command sequence reproducing the host mutation."""
    # Same desired-set rule as modmgr.ModManager._desired_set: the
    # records-installed set plus the app-disabled overlay.
    desired = set(m["id"] for m in mm.inst.records().mods_for(title)
                  if m["state"] == "installed") | set(mm.disabled_ids(title))
    if extra_id:
        desired.add(extra_id)
    if desired_plus:
        desired.add(desired_plus)
    if desired_minus:
        # Console-side disable: recompose WITHOUT the mod so its rows leave
        # the console loadlist exactly as the host-side strip did.
        desired.discard(desired_minus)
    state, roots = mm.state, mm.roots
    c = lambda verb, mods=None, **opts: console_link.build_command(
        verb, state, roots, mods=mods, options=opts)
    if remove_id:
        return [("install-remove", c("install", title=title, remove=remove_id))]
    if extra_id:
        rec = mm.inst.records().mod(extra_id)
        bundle = rec["bundle"] if rec else None
        pre = []
        if bundle and bundle.endswith(".zip"):
            pre.append(("register-%s" % extra_id,
                        c("register-mod", id=extra_id, title=title, bundle=bundle)))
            pre.append(("prepare-archive-%s" % extra_id,
                        c("prepare-archive", title=title, archive=bundle,
                          quarantine=os.path.join(state, "intake", extra_id + ".q"),
                          mod=extra_id)))
        elif bundle and bundle.endswith(".mfb"):
            pre.append(("register-%s" % extra_id,
                        c("register-mod", id=extra_id, title=title, bundle=bundle)))
            pre.append(("prepare-%s" % extra_id, c("prepare", title=title,
                                                   mods=[extra_id])))
        return pre + [("install-desired",
                       c("install", title=title, mods=sorted(desired)))]
    # enable/disable/uninstall-consistency: the console recomposes the exact
    # desired set (minus a disabled mod, plus an enabled one).
    return [("install-desired", c("install", title=title, mods=sorted(desired)))]


# --------------------------------------------------------------------------- #
# operations                                                                   #
# --------------------------------------------------------------------------- #

def resolve_title(mm, title):
    games = mm.inst.records().games
    if title:
        if not TOKEN.match(title):
            raise Refused("bad_request", "title must be a token: %r" % title)
        return title
    if len(games) == 1:
        return games[0]["title"]
    if not games:
        raise Refused("not_found", "no game registered in records")
    raise Refused("ambiguous_title",
                  "multiple game rows %s; pass ?title=" % sorted(
                      g["title"] for g in games))


def items_for(mm, title):
    """UI-contract projection: app.js filters items on transaction_confirmed
    and a 64-hex sha256; everything else passes through as extra fields."""
    projection = mm.list_installed(title)
    rec = mm.inst.records()
    out = []
    for m in projection["mods"]:
        item = dict(m)
        r = rec.mod(m["id"])
        item["name"] = r["title"] if r else m["id"]
        item["transaction_confirmed"] = (m["state"] == "installed"
                                         and not projection["loadlist_diverged"])
        item["sha256"] = None
        if m["bundle"] and os.path.exists(m["bundle"]):
            try:
                item["sha256"] = sha256_file(m["bundle"])
            except OSError:
                item["sha256"] = None
        out.append(item)
    return projection, out


def op_list(mm, title):
    if not title and not mm.inst.records().games:
        # Honest empty state: nothing installed yet is 200/items:[], not an
        # error — the app's Installed tab renders empty either way.
        return emit({"ok": True, "op": "list", "title": None, "generation": 0,
                     "installed": False, "loadlist_diverged": False,
                     "records_sha": None, "live_sha": None,
                     "disabled_overlay": [], "items": []})
    title = resolve_title(mm, title)
    projection, items = items_for(mm, title)
    return emit({"ok": True, "op": "list", "title": title,
                 "generation": projection["generation"],
                 "installed": projection["installed"],
                 "loadlist_diverged": projection["loadlist_diverged"],
                 "records_sha": projection["records_sha"],
                 "live_sha": projection["live_sha"],
                 "disabled_overlay": projection["disabled_overlay"],
                 "items": items})


def op_mutation(mm, title, kind, mod_id=None, source=None):
    title = resolve_title(mm, title)
    if mod_id is not None and not TOKEN.match(mod_id):
        raise Refused("bad_request", "mod id must be a token: %r" % mod_id)
    if kind == "install":
        if not source or not os.path.isfile(source):
            raise Refused("not_found", "source archive missing: %s" % (source,))
        with open(source, "rb") as f:
            magic = f.read(8)
        if magic != MFB_MAGIC and not source.lower().endswith(".zip"):
            raise Refused("bad_request",
                          "unsupported source (need .zip or .mfb): %s" % source)
        result = {"installed_stdout": mm.install(title, source, mod_id)}
        entries = console_install_entries(mm, title, extra_id=mod_id)
    elif kind == "uninstall":
        result = {"uninstalled": mm.uninstall(title, mod_id)}
        entries = console_install_entries(mm, title, remove_id=mod_id)
    elif kind == "disable":
        result = mm.disable(title, mod_id)
        entries = console_install_entries(mm, title, desired_minus=mod_id)
    elif kind == "enable":
        result = mm.enable(title, mod_id)
        entries = console_install_entries(mm, title, desired_plus=mod_id)
    else:
        raise Refused("bad_request", "unknown op %r" % kind)
    queue = stage_commands(mm.state, title, entries)
    projection, items = items_for(mm, title)
    return emit({"ok": True, "op": kind, "title": title, "result": result,
                 "queue": queue,
                 "verification": {
                     "generation": projection["generation"],
                     "loadlist_diverged": projection["loadlist_diverged"],
                     "records_sha": projection["records_sha"],
                     "live_sha": projection["live_sha"],
                     "mods": [{"id": i["id"], "state": i["state"],
                               "enabled": i["enabled"]} for i in items]}})


def op_transcript(mm):
    path = os.path.join(mm.state, "e2e-status.jsonl")
    if not os.path.exists(path):
        return emit({"ok": True, "op": "transcript", "present": False,
                     "valid": True, "problems": []})
    with open(path, "r", encoding="utf-8") as f:
        t = console_link.E2eTranscript(f.read())
    ok, problems = t.validate()
    begins, ends, unmatched = t.pairing()
    return emit({"ok": True, "op": "transcript", "present": True, "valid": ok,
                 "problems": problems, "begins": begins, "ends": ends,
                 "unmatched": unmatched, "last_exit": t.last_exit()})


def op_queue(mm):
    manifest = os.path.join(mm.state, QUEUE_DIR, "manifest.jsonl")
    rows = []
    if os.path.exists(manifest):
        with open(manifest, "r", encoding="utf-8") as f:
            rows = [json.loads(x) for x in f if x.strip()]
    return emit({"ok": True, "op": "queue", "rows": rows})


# --------------------------------------------------------------------------- #

def main(argv):
    import argparse
    ap = argparse.ArgumentParser(prog="modmgr_api")
    ap.add_argument("op", choices=("list", "install", "uninstall",
                                   "enable", "disable", "transcript", "queue"))
    ap.add_argument("--binary", required=True)
    ap.add_argument("--state", required=True)
    ap.add_argument("--roots", required=True)
    ap.add_argument("--title")
    ap.add_argument("--id", dest="mod_id")
    ap.add_argument("--file", dest="source")
    a = ap.parse_args(argv)
    try:
        mm = ModManager(a.binary, a.state, a.roots)
        lock = os.path.join(mm.overlay_root, "api.lock")
        holder = open(lock, "a+")
        try:
            fcntl.flock(holder.fileno(), fcntl.LOCK_EX)
            if a.op == "list":
                return op_list(mm, a.title)
            if a.op == "transcript":
                return op_transcript(mm)
            if a.op == "queue":
                return op_queue(mm)
            if a.op == "install":
                return op_mutation(mm, a.title, "install", a.mod_id, a.source)
            return op_mutation(mm, a.title, a.op, a.mod_id)
        finally:
            fcntl.flock(holder.fileno(), fcntl.LOCK_UN)
            holder.close()
    except Refused as e:
        return fail(e.kind, str(e))
    except ModMgrError as e:
        return fail(e.kind, str(e))
    except (OSError, ValueError) as e:
        return fail("io_error", str(e))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

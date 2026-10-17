#!/usr/bin/env python3
"""modmgr.py — PS5 mod-manager app-side core loop (host implementation).

Worker-app-modmanager, 2026-10-04. Everything here is HOST-side: the console
stays controller-only (see CONSOLE-CONTRACT.md).

Design rule: the proven installer binary does every mutation it already owns
(register-mod / prepare / prepare-archive / install [--remove] / recover);
this module never re-derives compose, journal, swap or recovery logic. The
app layer adds only what the CLI lacks:

  * list      — records.txt states + live loadlist.txt parse + disable overlay
                -> honest per-mod {state, enabled, files, bytes} projection.
  * install   — zip (via the F-R1 auto-bridge prepare-archive) or mfb
                (register -> prepare), then `install --mods <desired set>`
                where the desired set preserves everything already installed
                plus the app-disabled overlay, so adding one mod never drops
                the others (the CLI recomposes ONLY the listed set).
  * disable   — guarded loadlist row edit: remove the mod's `mod` row and its
                `file` rows, attributed EXACTLY via the mod's own bundle index
                (records mod row -> bundle path -> mfb index paths). Files stay
                on disk. Atomic tmp+rename+fsync; sidecar keeps the removed rows
                for enable; records game-row loadlist sha is resynced and then
                VERIFIED by re-reading through the binary's own `records` verb.
  * enable    — exact inverse from the sidecar.
  * uninstall — the CLI's `install --remove` path (F-R2 removal semantics:
                full root recompose without the mod, records back to prepared,
                generation++), then the overlay is re-applied/cleaned.

Root grammar (FORMATS.md "Load list"): UTF-8, TAB-separated, <=5 fields,
`#` comments, unknown record kinds skipped but preserved byte-exact on rewrite.
enabled.txt semantics: empty, presence-only; the runtime derives the UE4SS Lua
startup list from the preloaded enabled.txt file row, so a Lua mod is only
runtime-disabled once its rows are gone from loadlist.txt.
"""

import hashlib
import json
import os
import struct
import subprocess
import sys
import time

LOADLIST_HEADER = "modforge-loadlist\t1"
RECORDS_HEADER = "modforge-records\t1"
MARKER = ".modforge-root"
LOADLIST_NAME = "loadlist.txt"
MFB_MAGIC = b"MFBUNDL1"

OVERLAY_DIR = "modmgr"  # under <state>/: app sidecar (disabled.tsv, backups, log)


class ModMgrError(Exception):
    """Refusal with a specific, printable reason (never a guess)."""

    def __init__(self, kind, message):
        super().__init__(message)
        self.kind = kind  # bad_request | not_found | conflict | io_error


# --------------------------------------------------------------------------- #
# format parsers (read-only; grammar per FORMATS.md, unknown kinds preserved) #
# --------------------------------------------------------------------------- #

class Row:
    __slots__ = ("kind", "fields", "raw")

    def __init__(self, kind, fields, raw):
        self.kind = kind    # title|engine|sdk|framework|mod|file|order|unknown|comment|blank
        self.fields = fields
        self.raw = raw      # exact original line (no newline)


class LoadList:
    def __init__(self, rows, header_ok):
        self.rows = rows
        self.header_ok = header_ok

    # -- parse ------------------------------------------------------------- #
    @classmethod
    def parse(cls, text):
        rows = []
        header_ok = False
        lines = text.split("\n")
        if lines and lines[-1] == "":
            lines.pop()  # trailing newline is a terminator, not a blank row
        for raw in lines:
            line = raw.rstrip("\r")
            if line == "":
                rows.append(Row("blank", [], line))
                continue
            if line.startswith("#"):
                rows.append(Row("comment", [], line))
                continue
            fields = line.split("\t")
            if fields[0] == "modforge-loadlist":
                header_ok = len(fields) == 2 and fields[1] == "1"
                rows.append(Row("header", fields, line))
                continue
            kind = fields[0]
            limits = {"title": 2, "engine": 2, "sdk": 2, "framework": 2,
                      "mod": 3, "file": 4, "order": 2}
            if kind in limits:
                if len(fields) > limits[kind]:
                    raise ModMgrError("bad_request",
                                      "loadlist row '%s' has %d fields (max %d)"
                                      % (kind, len(fields), limits[kind]))
                rows.append(Row(kind, fields, line))
            else:
                # FORMATS.md: readers must SKIP unknown record kinds.
                rows.append(Row("unknown", fields, line))
        return cls(rows, header_ok)

    @classmethod
    def load(cls, path):
        with open(path, "r", encoding="utf-8") as f:
            return cls.parse(f.read())

    def require_header(self):
        if not self.header_ok:
            raise ModMgrError("bad_request", "loadlist header missing or bad")

    # -- accessors ---------------------------------------------------------- #
    def mods(self):
        """[(id, version)] enabled mod rows, load order preserved."""
        return [(r.fields[1], r.fields[2]) for r in self.rows if r.kind == "mod"]

    def files(self):
        """[(role, path, size)] file rows."""
        out = []
        for r in self.rows:
            if r.kind == "file":
                out.append((r.fields[1], r.fields[2], int(r.fields[3])))
        return out

    def has_mod(self, mod_id):
        return any(r.kind == "mod" and r.fields[1] == mod_id for r in self.rows)

    def render(self):
        return "\n".join(r.raw for r in self.rows) + ("\n" if self.rows else "")


class Records:
    def __init__(self):
        self.mods = []   # {id,title,state,bundle}
        self.games = []  # {title,engine,sdk,generation,loadlist_sha}
        self.raw_text = ""

    @classmethod
    def parse(cls, text):
        rec = cls()
        rec.raw_text = text
        for line in text.split("\n"):
            if not line or line.startswith("#"):
                continue
            f = line.split("\t")
            if f[0] == "modforge-records":
                continue
            if f[0] == "mod" and len(f) == 5:
                rec.mods.append({"id": f[1], "title": f[2], "state": f[3],
                                 "bundle": f[4]})
            elif f[0] == "game" and len(f) == 6:
                rec.games.append({"title": f[1], "engine": f[2], "sdk": f[3],
                                  "generation": int(f[4]), "loadlist_sha": f[5]})
            else:
                # worker-8 rule: unknown rows/kinds are refused as corrupt,
                # never guessed.
                raise ModMgrError("bad_request",
                                  "corrupt records.txt row: %r" % line)
        return rec

    def game(self, title):
        for g in self.games:
            if g["title"] == title:
                return g
        return None

    def mods_for(self, title):
        return [m for m in self.mods if m["title"] == title]

    def mod(self, mod_id):
        for m in self.mods:
            if m["id"] == mod_id:
                return m
        return None


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def read_mfb_paths(path):
    """Read-only mfb index walk -> [final paths]. Format: FORMATS.md 'PS5 bundle'."""
    with open(path, "rb") as f:
        head = f.read(32)
        if len(head) != 32 or head[0:8] != MFB_MAGIC:
            raise ModMgrError("bad_request", "not a modforge bundle: %s" % path)
        ver, count = struct.unpack_from("<II", head, 8)
        idx_size, _res = struct.unpack_from("<QQ", head, 16)
        if ver != 1:
            raise ModMgrError("bad_request", "mfb version %d unsupported" % ver)
        idx = f.read(idx_size)
        if len(idx) != idx_size:
            raise ModMgrError("io_error", "mfb index truncated: %s" % path)
    paths, off = [], 0
    for _ in range(count):
        if off + 50 > idx_size:
            raise ModMgrError("bad_request", "mfb index record truncated: %s" % path)
        _off, _size = struct.unpack_from("<QQ", idx, off)
        plen = struct.unpack_from("<H", idx, off + 48)[0]
        off += 50
        if off + plen > idx_size:
            raise ModMgrError("bad_request", "mfb path truncated: %s" % path)
        paths.append(idx[off:off + plen].decode("utf-8"))
        off += plen
    return paths


# --------------------------------------------------------------------------- #
# small durable-write helpers                                                  #
# --------------------------------------------------------------------------- #

def fsync_dir(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def atomic_write(path, data: bytes):
    tmp = path + ".tmp-%d" % os.getpid()
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    try:
        os.write(fd, data)
        os.fsync(fd)
    finally:
        os.close(fd)
    os.rename(tmp, path)
    fsync_dir(os.path.dirname(path))


# --------------------------------------------------------------------------- #
# the proven installer binary wrapper                                          #
# --------------------------------------------------------------------------- #

class Installer:
    """Thin wrapper over the composed modinst CLI. No logic re-implemented."""

    def __init__(self, binary, state, roots, verify_sha=None):
        self.binary = binary
        self.state = os.path.abspath(state)
        self.roots = os.path.abspath(roots)
        if verify_sha:
            got = sha256_file(binary)
            if got != verify_sha:
                raise ModMgrError("bad_request",
                                  "installer binary sha %s != pinned %s" % (got, verify_sha))

    def run(self, args, check=True):
        cmd = [self.binary, "--state", self.state, "--roots", self.roots] + args
        p = subprocess.run(cmd, capture_output=True, text=True)
        if check and p.returncode != 0:
            raise ModMgrError("io_error",
                              "installer %s exit=%d: %s"
                              % (args[0], p.returncode, p.stdout.strip()))
        return p

    def run_raw(self, args, check=True):
        """Invoke the binary verbatim (make-bundle is a fixture verb that must
        be argv[1] and takes no --state/--roots)."""
        p = subprocess.run([self.binary] + args, capture_output=True, text=True)
        if check and p.returncode != 0:
            raise ModMgrError("io_error",
                              "installer %s exit=%d: %s"
                              % (args[0], p.returncode, p.stdout.strip()))
        return p

    # verb helpers -------------------------------------------------------- #
    def records_path(self):
        return os.path.join(self.state, "records.txt")

    def records(self):
        """Durable TSV truth (records.txt), same grammar the proven writer uses.
        Unknown row kinds are refused as corrupt, never guessed (worker-8 rule)."""
        path = self.records_path()
        if not os.path.exists(path):
            return Records()
        text = open(path, encoding="utf-8").read()
        rec = Records.parse(text)
        return rec

    def records_verb_sees(self, needle):
        """Reader-fidelity check: the binary's own `records` verb must report
        the given substring (e.g. 'game <title> ... loadlist=<sha>')."""
        p = self.run(["records"], check=False)
        return p.returncode == 0 and needle in p.stdout

    def register_mod(self, mod_id, title, bundle):
        return self.run(["register-mod", "--id", mod_id, "--title", title,
                         "--bundle", bundle])

    def prepare(self, title, mods):
        return self.run(["prepare", "--title", title, "--mods", ",".join(mods)])

    def prepare_archive(self, title, archive, quarantine, mod_id):
        return self.run(["prepare-archive", "--title", title, "--archive", archive,
                         "--quarantine", quarantine, "--mod", mod_id])

    def install(self, title, mods=None, remove=None):
        args = ["install", "--title", title]
        if mods:
            args += ["--mods", ",".join(mods)]
        if remove:
            args += ["--remove", remove]
        return self.run(args)

    def recover(self):
        return self.run(["recover"])

    def root_path(self, title):
        return os.path.join(self.roots, title)


# --------------------------------------------------------------------------- #
# the mod-manager core loop                                                    #
# --------------------------------------------------------------------------- #

class ModManager:
    def __init__(self, binary, state, roots, pinned_sha=None):
        self.inst = Installer(binary, state, roots, verify_sha=pinned_sha)
        self.state = self.inst.state
        self.roots = self.inst.roots
        self.overlay_root = os.path.join(self.state, OVERLAY_DIR)
        os.makedirs(self.overlay_root, exist_ok=True)
        self.inst.recover()  # CLI semantics: recovery runs on every open anyway

    # -- sidecar ------------------------------------------------------------ #
    def _overlay_path(self, title):
        return os.path.join(self.overlay_root, "disabled-%s.tsv" % title)

    def _overlay_rows(self, title):
        """[{index, row}] captured loadlist rows per disabled mod id."""
        path = self._overlay_path(title)
        out = {}
        if not os.path.exists(path):
            return out
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line:
                    continue
                mod_id, index, rowtext = line.split("\t", 2)
                out.setdefault(mod_id, []).append({"index": int(index),
                                                   "row": rowtext})
        return out

    def _overlay_put(self, title, mod_id, rows):
        path = self._overlay_path(title)
        lines = []
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                lines = [l.rstrip("\n") for l in f if l.strip()]
        lines = [l for l in lines if not l.startswith(mod_id + "\t")]
        lines += ["%s\t%d\t%s" % (mod_id, r["index"], r["row"]) for r in rows]
        atomic_write(path, ("\n".join(lines) + "\n").encode("utf-8"))

    def _overlay_drop(self, title, mod_id):
        path = self._overlay_path(title)
        if not os.path.exists(path):
            return
        with open(path, "r", encoding="utf-8") as f:
            lines = [l.rstrip("\n") for l in f if l.strip()]
        lines = [l for l in lines if not l.startswith(mod_id + "\t")]
        atomic_write(path, ("\n".join(lines) + "\n").encode("utf-8") if lines else b"")

    def disabled_ids(self, title):
        return sorted(self._overlay_rows(title).keys())

    def _audit(self, title, op, **kv):
        entry = {"ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                 "title": title, "op": op}
        entry.update(kv)
        with open(os.path.join(self.overlay_root, "log.jsonl"), "a",
                  encoding="utf-8") as f:
            f.write(json.dumps(entry, sort_keys=True) + "\n")

    # -- listing ------------------------------------------------------------ #
    def list_installed(self, title):
        rec = self.inst.records()
        game = rec.game(title)
        mods = rec.mods_for(title)
        root = self.inst.root_path(title)
        ll_path = os.path.join(root, LOADLIST_NAME)
        ll = None
        if os.path.exists(ll_path):
            ll = LoadList.load(ll_path)
            ll.require_header()
        live_sha = sha256_file(ll_path) if ll else None
        disabled = set(self.disabled_ids(title))
        out_mods = []
        ll_mod_ids = set(i for i, _v in (ll.mods() if ll else []))
        for m in mods:
            enabled = False
            files = 0
            fbytes = 0
            if ll:
                if m["id"] in ll_mod_ids and m["id"] not in disabled:
                    enabled = True
                # attribution via the mod's own bundle when readable
                if m["bundle"].endswith(".mfb") and os.path.exists(m["bundle"]):
                    try:
                        paths = set(read_mfb_paths(m["bundle"]))
                    except ModMgrError:
                        paths = None
                    if paths is not None:
                        for _role, p, size in ll.files():
                            if p in paths:
                                files += 1
                                fbytes += size
            note = None
            if m["state"] == "installed" and ll and not enabled \
                    and m["id"] not in disabled:
                note = "installed-but-absent-from-loadlist"
            out_mods.append({
                "id": m["id"], "state": m["state"], "enabled": enabled,
                "files": files, "bytes": fbytes,
                "bundle": m["bundle"], "note": note,
            })
        diverged = bool(game and live_sha is not None
                        and game["loadlist_sha"] != live_sha)
        return {
            "title": title,
            "installed": bool(game is not None and ll is not None),
            "generation": game["generation"] if game else 0,
            "records_sha": game["loadlist_sha"] if game else None,
            "live_sha": live_sha,
            "loadlist_diverged": diverged,
            "mods": out_mods,
            "disabled_overlay": sorted(disabled),
        }

    # -- install / uninstall ------------------------------------------------- #
    def _desired_set(self, title):
        """Everything that must stay in the root across an install: exactly the
        records-installed set (app-disabled mods are records-installed too, so
        the overlay keeps them present-but-row-stripped). `prepared` mods were
        uninstalled or never installed — they must NOT come back."""
        rec = self.inst.records()
        return set(m["id"] for m in rec.mods_for(title)
                   if m["state"] == "installed") | set(self.disabled_ids(title))

    def install(self, title, source, mod_id):
        if not os.path.isfile(source):
            raise ModMgrError("not_found", "source archive missing: %s" % source)
        if not mod_id or "\t" in mod_id or not mod_id.replace("-", "").replace("_", "").isalnum():
            raise ModMgrError("bad_request", "mod id must be a token: %r" % mod_id)
        with open(source, "rb") as f:
            magic = f.read(8)
        rec = self.inst.records()
        if rec.mod(mod_id):
            raise ModMgrError("conflict", "mod '%s' already registered" % mod_id)
        if magic == MFB_MAGIC:
            self.inst.register_mod(mod_id, title, os.path.abspath(source))
            self.inst.prepare(title, [mod_id])
        elif source.lower().endswith(".zip"):
            self.inst.register_mod(mod_id, title, os.path.abspath(source))
            q = os.path.join(self.state, "intake", mod_id + ".q")
            self.inst.prepare_archive(title, os.path.abspath(source), q, mod_id)
        else:
            raise ModMgrError("bad_request",
                              "unsupported source (need .zip or .mfb): %s" % source)
        desired = sorted(self._desired_set(title) | {mod_id})
        p = self.inst.install(title, mods=desired)
        self._apply_overlay(title)  # recomposition restores disabled rows; strip again
        self._audit(title, "install", mod=mod_id, source=os.path.basename(source),
                    desired=desired)
        return p.stdout.strip()

    def uninstall(self, title, mod_id):
        rec = self.inst.records()
        m = rec.mod(mod_id)
        if not m or m["title"] != title:
            raise ModMgrError("not_found", "mod '%s' not registered for %s"
                              % (mod_id, title))
        self.inst.install(title, remove=mod_id)
        self._overlay_drop(title, mod_id)
        self._apply_overlay(title)  # other disabled mods stay disabled
        self._audit(title, "uninstall", mod=mod_id)
        return True

    # -- enable / disable (loadlist row edit) --------------------------------- #
    def _attributed_paths(self, title, mod_id):
        """Exact final-path set for a mod: its own bundle index (records row).
        Falls back to the ue4ss/Mods/<id>/ prefix ONLY if the bundle is unreadable;
        refuses rather than guess when neither yields anything."""
        rec = self.inst.records()
        m = rec.mod(mod_id)
        if not m or m["title"] != title:
            raise ModMgrError("not_found", "mod '%s' not registered for %s"
                              % (mod_id, title))
        bundle = m["bundle"]
        if bundle.endswith(".mfb") and os.path.exists(bundle):
            paths = set(read_mfb_paths(bundle))
            if paths:
                return paths
        prefix = "ue4ss/Mods/%s/" % mod_id
        ll_path = os.path.join(self.inst.root_path(title), LOADLIST_NAME)
        fallback = {p for _r, p, _s in LoadList.load(ll_path).files()
                    if p.startswith(prefix)}
        if fallback:
            return fallback
        raise ModMgrError("bad_request",
                          "cannot attribute files for '%s' (bundle unreadable and "
                          "no %s* rows in loadlist); refusing to guess" % (mod_id, prefix))

    def _check_root_editable(self, title):
        root = self.inst.root_path(title)
        if not os.path.isdir(root):
            raise ModMgrError("not_found", "no installed root for %s" % title)
        if not os.path.exists(os.path.join(root, MARKER)):
            raise ModMgrError("bad_request",
                              "refusing to edit unmanaged root %s (missing %s)"
                              % (root, MARKER))
        ll_path = os.path.join(root, LOADLIST_NAME)
        if not os.path.exists(ll_path):
            raise ModMgrError("not_found", "no loadlist in %s" % root)
        ll = LoadList.load(ll_path)
        ll.require_header()
        return root, ll_path, ll

    def _loadlist_backup(self, title, ll_path):
        bdir = os.path.join(self.overlay_root, "backups", title)
        os.makedirs(bdir, exist_ok=True)
        dst = os.path.join(bdir, "%d-%s" % (int(time.time()), LOADLIST_NAME))
        with open(ll_path, "rb") as f:
            data = f.read()
        atomic_write(dst, data)
        return dst

    def _write_loadlist_and_resync(self, title, ll_path, ll):
        atomic_write(ll_path, ll.render().encode("utf-8"))
        sha = sha256_file(ll_path)
        self._resync_records_sha(title, sha)
        return sha

    def _resync_records_sha(self, title, sha):
        """Rewrite ONLY the game row's loadlist-sha field; grammar preserved.
        Then verify through the binary's own reader."""
        rpath = os.path.join(self.state, "records.txt")
        with open(rpath, "r", encoding="utf-8") as f:
            lines = f.read().split("\n")
        hit = False
        for i, line in enumerate(lines):
            f6 = line.split("\t")
            if f6[0] == "game" and len(f6) == 6 and f6[1] == title:
                f6[5] = sha
                lines[i] = "\t".join(f6)
                hit = True
        if not hit:
            raise ModMgrError("not_found", "no game row for %s in records" % title)
        atomic_write(rpath, ("\n".join(lines)).encode("utf-8"))
        if not self.inst.records_verb_sees("loadlist=%s" % sha):
            raise ModMgrError("io_error",
                              "records sha resync not visible through the installer "
                              "reader (expected %s)" % sha)

    def disable(self, title, mod_id):
        root, ll_path, ll = self._check_root_editable(title)
        if not ll.has_mod(mod_id):
            raise ModMgrError("conflict", "mod '%s' has no mod row in loadlist" % mod_id)
        paths = self._attributed_paths(title, mod_id)
        backup = self._loadlist_backup(title, ll_path)
        captured, kept = [], []
        got_mod_row = False
        for idx, r in enumerate(ll.rows):
            drop = (r.kind == "mod" and r.fields[1] == mod_id) \
                or (r.kind == "file" and r.fields[2] in paths)
            if drop:
                captured.append({"index": idx, "row": r.raw})
                got_mod_row = got_mod_row or r.kind == "mod"
            else:
                kept.append(r)
        if not got_mod_row:
            raise ModMgrError("io_error", "internal: mod row not captured")
        ll.rows = kept
        self._overlay_put(title, mod_id, captured)
        sha = self._write_loadlist_and_resync(title, ll_path, ll)
        self._audit(title, "disable", mod=mod_id, rows=len(captured),
                    sha=sha, backup=backup)
        return {"disabled": mod_id, "rows_removed": len(captured),
                "loadlist_sha": sha, "backup": backup}

    def enable(self, title, mod_id):
        root, ll_path, ll = self._check_root_editable(title)
        rows = self._overlay_rows(title).get(mod_id)
        if not rows:
            raise ModMgrError("not_found", "no disable overlay for '%s'" % mod_id)
        if ll.has_mod(mod_id):
            raise ModMgrError("conflict", "mod '%s' already enabled" % mod_id)
        backup = self._loadlist_backup(title, ll_path)
        # restore rows at their captured positions (clamped), original order
        for cap in sorted(rows, key=lambda c: c["index"]):
            orig = LoadList.parse(cap["row"]).rows[0]
            pos = min(cap["index"], len(ll.rows))
            ll.rows.insert(pos, orig)
        self._overlay_drop(title, mod_id)
        sha = self._write_loadlist_and_resync(title, ll_path, ll)
        self._audit(title, "enable", mod=mod_id, rows=len(rows), sha=sha,
                    backup=backup)
        return {"enabled": mod_id, "rows_restored": len(rows),
                "loadlist_sha": sha, "backup": backup}

    def _apply_overlay(self, title):
        """After any binary transaction: strip still-present disabled rows again."""
        overlay = self._overlay_rows(title)
        if not overlay:
            return
        ll_path = os.path.join(self.inst.root_path(title), LOADLIST_NAME)
        if not os.path.exists(ll_path):
            return
        ll = LoadList.load(ll_path)
        ll.require_header()
        changed = False
        for mod_id, caps in overlay.items():
            wanted = set(c["row"] for c in caps)
            kept, removed = [], []
            for r in ll.rows:
                if r.raw in wanted:
                    removed.append(r)
                    changed = True
                else:
                    kept.append(r)
            ll.rows = kept
        if changed:
            self._write_loadlist_and_resync(title, ll_path, ll)


# --------------------------------------------------------------------------- #
# CLI                                                                          #
# --------------------------------------------------------------------------- #

def _main(argv):
    import argparse
    ap = argparse.ArgumentParser(prog="modmgr",
                                 description="PS5 mod-manager core loop (host)")
    ap.add_argument("--binary", required=True, help="modinst_composed path")
    ap.add_argument("--state", required=True, help="<state> dir (records.txt)")
    ap.add_argument("--roots", required=True, help="roots BASE (/data/modbridge/roots)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("list", "install", "uninstall", "enable", "disable"):
        s = sub.add_parser(name)
        s.add_argument("--title", required=True)
        if name == "install":
            s.add_argument("--source", required=True)
            s.add_argument("--id", required=True)
        elif name != "list":
            s.add_argument("--id", required=True)
    a = ap.parse_args(argv)
    mm = ModManager(a.binary, a.state, a.roots)
    if a.cmd == "list":
        print(json.dumps(mm.list_installed(a.title), indent=2, sort_keys=True))
        return 0
    if a.cmd == "install":
        print(mm.install(a.title, a.source, a.id))
        return 0
    if a.cmd == "uninstall":
        mm.uninstall(a.title, a.id)
        print("uninstalled %s" % a.id)
        return 0
    if a.cmd == "disable":
        print(json.dumps(mm.disable(a.title, a.id), sort_keys=True))
        return 0
    if a.cmd == "enable":
        print(json.dumps(mm.enable(a.title, a.id), sort_keys=True))
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))

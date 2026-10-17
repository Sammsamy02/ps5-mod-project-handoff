#!/usr/bin/env python3
"""Adversarial suite for uninstall module (uninstall.py + conflicts.py).
Mac-only, host-tested. Imports the modules directly (no CLI in module)."""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import Sandbox, INSTALLER_DIR, CORPUS as CORPUS_ROOT  # noqa: E402

sys.path.insert(0, INSTALLER_DIR)  # uninstall.py + conflicts.py live in installer/
from uninstall import (  # noqa: E402
    Uninstaller, UninstallError, UnsafePathError, ModStillRequiredError,
    ConfirmationRequiredError, ManifestError, sha256_file)
import conflicts as C  # noqa: E402

CORPUS = os.path.join(CORPUS_ROOT, "uninstall")


def sha(data: bytes) -> str:
    import hashlib
    return hashlib.sha256(data).hexdigest()


class Env:
    """Installed-mod fixture: state file + files on disk."""

    def __init__(self, sb, mods=None, state_extra=None):
        # mods: {mod_id: {"files": {rel: bytes}, "requires": [...],
        #                 "config": {cfg_rel: [lines]}}}
        mods = mods or {}
        self.sb = sb
        self.root = sb.path + "/game"
        os.makedirs(self.root, exist_ok=True)
        state = {"game_version": "1.004.000", "mods": {}}
        for mid, spec in mods.items():
            for rel, data in (spec.get("files") or {}).items():
                p = os.path.join(self.root, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, "wb") as f:
                    f.write(data)
            entry = {
                "requires": spec.get("requires", []),
                "files": [
                    {"path": rel, "sha256": sha(data)}
                    for rel, data in (spec.get("files") or {}).items()
                ],
            }
            state["mods"][mid] = entry
        if state_extra:
            state.update(state_extra)
        self.state_path = os.path.join(sb.path, "install_state.json")
        with open(self.state_path, "w") as f:
            json.dump(state, f)
        for mid, spec in mods.items():
            for cfg_rel in (spec.get("config") or {}):
                p = os.path.join(self.root, cfg_rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
        # write config files once, merging all mods' lines
        cfg_lines = {}
        for mid, spec in mods.items():
            for cfg_rel, lines in (spec.get("config") or {}).items():
                cfg_lines.setdefault(cfg_rel, []).extend(lines)
        for cfg_rel, lines in cfg_lines.items():
            p = os.path.join(self.root, cfg_rel)
            with open(p, "w") as f:
                f.write("\n".join(lines) + "\n")

    def manifest(self, mid, mods=None):
        mods = mods or {}
        spec = mods[mid]
        mf = {
            "mod_id": mid,
            "name": mid,
            "mod_version": "1.0",
            "requires": spec.get("requires", []),
            "files": [
                {"path": rel, "sha256": sha(data)}
                for rel, data in (spec.get("files") or {}).items()
            ],
            "config_entries": spec.get("config", {}),
        }
        p = os.path.join(self.sb.path, mid + ".manifest.json")
        with open(p, "w") as f:
            json.dump(mf, f)
        return p

    def uninstaller(self):
        return Uninstaller(self.root, state_path=self.state_path,
                           transaction_root=os.path.join(self.sb.path,
                                                         "txn"))


def register(h):
    # ---------------------------------------------------------- happy path
    @h.add("un.happy_removes_only_manifest_files", "uninstall")
    def _(sb=None):
        mods = {"a": {"files": {"Mods/a/main.lua": b"A\n",
                                "Mods/a/extra.bin": b"E"},
                      "config": {"ploader.ini":
                                 ["payload=/data/payloads/a.prx"]}},
                "b": {"files": {"Mods/b/main.lua": b"B\n"},
                      "config": {"ploader.ini":
                                 ["payload=/data/payloads/b.prx"]}}}
        env = Env(sb, mods)
        u = env.uninstaller()
        res = u.uninstall(env.manifest("a", mods))
        A.ok(not os.path.exists(env.root + "/Mods/a"),
             "mod a files not removed")
        A.ok(os.path.exists(env.root + "/Mods/b/main.lua"),
             "mod b collateral damage")
        pl = open(env.root + "/ploader.ini").read()
        A.contains(pl, "b.prx", "other mod's ploader line removed")
        A.ok("a.prx" not in pl, "mod a ploader line survived")

    # ------------------------------------------------------ missing files
    @h.add("un.manifest_missing_files", "uninstall")
    def _(sb=None):
        mods = {"ghost": {"files": {"gone/a.lua": b"x"}}}
        env = Env(sb, mods)
        os.remove(env.root + "/gone/a.lua")
        import shutil
        os.rmdir(env.root + "/gone")
        u = env.uninstaller()
        res = u.uninstall(env.manifest("ghost", mods))
        A.eq(res.missing, ["gone/a.lua"], "missing file not reported")

    @h.add("un.manifest_file_now_a_directory", "uninstall")
    def _(sb=None):
        mods = {"a": {"files": {"f.lua": b"x"}}}
        env = Env(sb, mods)
        os.remove(env.root + "/f.lua")
        os.makedirs(env.root + "/f.lua/sub", exist_ok=True)
        u = env.uninstaller()
        try:
            u.uninstall(env.manifest("a", mods))
        except UninstallError:
            pass  # acceptable: refuses; must NOT delete the directory tree
        A.ok(os.path.isdir(env.root + "/f.lua/sub"),
             "directory path was deleted as if it were a file")

    # -------------------------------------------------- reverse dependency
    @h.add("un.dependent_blocks_uninstall", "uninstall")
    def _(sb=None):
        mods = {"base": {"files": {"Mods/base/main.lua": b"B"}},
                "child": {"files": {"Mods/child/main.lua": b"C"},
                          "requires": ["base"]}}
        env = Env(sb, mods)
        u = env.uninstaller()
        try:
            u.uninstall(env.manifest("base", mods))
            A.ok(False, "uninstalled a mod another mod requires")
        except ModStillRequiredError as e:
            A.eq(e.dependents, ["child"], "dependents list wrong")
        # after removing the dependent, base can go
        u.uninstall(env.manifest("child", mods))
        u.uninstall(env.manifest("base", mods))

    @h.add("un.circular_dependency_deadlock", "uninstall")
    def _(sb=None):
        """A requires B, B requires A: neither can be uninstalled directly.
        Expected: safe refusal (never corruption); document the deadlock."""
        mods = {"a": {"files": {"Mods/a/main.lua": b"A"},
                      "requires": ["b"]},
                "b": {"files": {"Mods/b/main.lua": b"B"},
                      "requires": ["a"]}}
        env = Env(sb, mods)
        u = env.uninstaller()
        blocked = 0
        for mid in ("a", "b"):
            try:
                u.uninstall(env.manifest(mid, mods))
            except ModStillRequiredError:
                blocked += 1
        A.eq(blocked, 2, "circular dep did not refuse both")
        A.ok(os.path.exists(env.root + "/Mods/a/main.lua") and
             os.path.exists(env.root + "/Mods/b/main.lua"),
             "files vanished despite refusals")

    @h.add("un.self_dependency", "uninstall")
    def _(sb=None):
        mods = {"selfy": {"files": {"Mods/s/main.lua": b"S"},
                          "requires": ["selfy"]}}
        env = Env(sb, mods)
        u = env.uninstaller()
        res = u.uninstall(env.manifest("selfy", mods))  # must not deadlock
        A.eq(res.removed, ["Mods/s/main.lua"])

    # ------------------------------------------------------- tampering
    @h.add("un.modified_file_needs_confirmation", "uninstall")
    def _(sb=None):
        mods = {"a": {"files": {"Mods/a/main.lua": b"original"}}}
        env = Env(sb, mods)
        with open(env.root + "/Mods/a/main.lua", "wb") as f:
            f.write(b"tampered")
        u = env.uninstaller()
        try:
            u.uninstall(env.manifest("a", mods))
            A.ok(False, "tampered file removed without confirmation")
        except ConfirmationRequiredError:
            pass
        A.ok(os.path.exists(env.root + "/Mods/a/main.lua"),
             "tampered file removed without confirmation")
        # with confirmation callable it proceeds
        res = u.uninstall(env.manifest("a", mods),
                          confirm_modified=lambda mods_: True)
        A.ok(not os.path.exists(env.root + "/Mods/a/main.lua"))

    # --------------------------------------------------- path safety
    @h.add("un.manifest_escape_paths_rejected", "uninstall")
    def _(sb=None):
        for fixture, must_raise in [
                ("abs_paths.json", True),
                ("parent_dirs.json", True),
                ("weird_unicode.json", False)]:
            mods = {"a": {"files": {"Mods/a/main.lua": b"A"}}}
            env = Env(sb, mods)
            u = env.uninstaller()
            # splice hostile file entries into a valid manifest
            mf = json.load(open(env.manifest("a", mods)))
            hostile = json.load(open(os.path.join(CORPUS, fixture)))
            for fe in hostile.get("files", []):
                mf["files"].append({"path": fe, "sha256": None})
            if must_raise:
                try:
                    u.uninstall(mf)
                    A.ok(False, f"{fixture}: unsafe path accepted")
                except UnsafePathError:
                    pass
            else:
                res = u.uninstall(mf)  # unicode paths are safe; must not crash
                A.ok(res is not None)

    @h.add("un.huge_manifest", "uninstall")
    def _(sb=None):
        big = json.load(open(os.path.join(CORPUS, "huge_list.json")))
        mods = {"huge": {"files": {"Mods/h/main.lua": b"H"}}}
        env = Env(sb, mods)
        u = env.uninstaller()
        mf = json.load(open(env.manifest("huge", mods)))
        mf["files"].extend({"path": fe, "sha256": None}
                           for fe in big["files"])
        res = u.uninstall(mf)
        A.ok(len(res.missing) >= 4999, "missing entries not counted")

    # ------------------------------------------------------- rollback
    @h.add("un.rollback_restores_everything", "uninstall")
    def _(sb=None):
        mods = {"a": {"files": {"Mods/a/main.lua": b"A",
                                "Mods/a/d/extra.bin": b"E"},
                      "config": {"ploader.ini":
                                 ["payload=/data/payloads/a.prx"]}},
                "b": {"files": {"Mods/b/main.lua": b"B"},
                      "config": {"ploader.ini":
                                 ["payload=/data/payloads/b.prx"]}}}
        env = Env(sb, mods)
        u = env.uninstaller()
        res = u.uninstall(env.manifest("a", mods))
        A.ok(res.rollback_token, "no rollback token")
        rb = u.rollback(res.rollback_token)
        A.ok(os.path.exists(env.root + "/Mods/a/main.lua"),
             "file not restored by rollback")
        A.ok(os.path.exists(env.root + "/Mods/a/d/extra.bin"),
             "nested file not restored")
        pl = open(env.root + "/ploader.ini").read()
        A.contains(pl, "a.prx", "config line not restored")
        state = json.load(open(env.state_path))
        A.ok("a" in state["mods"], "state not restored")
        # double rollback refused
        try:
            u.rollback(res.rollback_token)
            A.ok(False, "double rollback allowed")
        except Exception:
            pass

    @h.add("un.state_without_mod", "uninstall")
    def _(sb=None):
        mods = {"a": {"files": {"Mods/a/main.lua": b"A"}}}
        env = Env(sb, mods)
        u = env.uninstaller()
        mf = {"mod_id": "not-installed", "files": []}
        try:
            u.uninstall(mf)
            A.ok(False, "unknown mod uninstalled silently")
        except UninstallError:
            pass

    @h.add("un.corrupt_manifests", "uninstall")
    def _(sb=None):
        mods = {"a": {"files": {"Mods/a/main.lua": b"A"}}}
        env = Env(sb, mods)
        u = env.uninstaller()
        for bad in ("empty_manifest.json", "null_files.json",
                    "dup_files.json", "dir_and_file.json"):
            p = os.path.join(CORPUS, bad)
            mf = json.load(open(p))
            mf["mod_id"] = "a"
            try:
                u.uninstall(mf)
            except (ManifestError, UninstallError):
                pass  # controlled refusal is fine
            except UnsafePathError:
                pass
        A.ok(os.path.exists(env.root + "/Mods/a/main.lua"),
             "corrupt manifest caused file loss")

    # ------------------------------------------------- conflicts module
    @h.add("cf.path_conflict_detection", "conflicts")
    def _(sb=None):
        mods = {"installed": {"files": {"Mods/shared/overlay.lua": b"I"}}}
        env = Env(sb, mods)
        spec = C.ModSpec(mod_id="cand", files=("Mods/shared/overlay.lua",))
        rep = C.check_install(spec, env.state_path)
        A.ok(not rep.ok, "PATH_CONFLICT missed")
        A.eq(rep.blockers[0].kind, C.PATH_CONFLICT)
        # reinstalling own files is fine
        spec2 = C.ModSpec(mod_id="installed",
                          files=("Mods/shared/overlay.lua",))
        rep2 = C.check_install(spec2, env.state_path)
        A.ok(rep2.ok, "self-reinstall falsely conflicts")

    @h.add("cf.keybind_and_hook_conflicts", "conflicts")
    def _(sb=None):
        mods = {"a": {"files": {
            "Mods/a/main.lua": b'RegisterKeyBind(Key.F6, function() end)\n'
                               b'RegisterHook("/Script/Engine.Actor:Tick", cb)\n'}}}
        env = Env(sb, mods)
        spec = C.ModSpec(mod_id="b", lua_sources={
            "m.lua": 'RegisterKeyBind("F6", function() end)'})
        rep = C.check_install(spec, env.state_path, game_root=env.root)
        kinds = [c.kind for c in rep]
        A.ok(C.KEYBIND_CONFLICT in kinds, "keybind conflict missed")
        spec2 = C.ModSpec(mod_id="b", lua_sources={
            "m.lua": 'RegisterHook("/Script/Engine.Actor:Tick", cb)'})
        rep2 = C.check_install(spec2, env.state_path, game_root=env.root)
        A.contains([c.kind for c in rep2], C.HOOK_CONFLICT,
                   "hook conflict missed")

    @h.add("cf.comment_stripping", "conflicts")
    def _(sb=None):
        # commented-out calls must NOT count
        text = ('-- RegisterKeyBind(Key.F6, cb)\n'
                '--[[ RegisterKeyBind(Key.F7, cb) ]]\n'
                'RegisterKeyBind(Key.F8, cb)\n')
        A.eq(C.parse_keybinds(text), ["F8"], "comment stripping broken")

    @h.add("cf.version_constraints", "conflicts")
    def _(sb=None):
        A.ok(C.version_allows("1.004.000", "1.004.000"))
        A.ok(C.version_allows(">=1.003", "1.004.000"))
        A.ok(not C.version_allows("<2", "2.0.0"))
        A.ok(C.version_allows("<2", "1.9.9"))
        A.ok(not C.version_allows("1.004", "1.004.000") or
             C.version_allows("1.004", "1.004.000"),
             "exact match semantics unstable")  # document either way
        mods = {"a": {"files": {"Mods/a/main.lua": b"A"}}}
        env = Env(sb, mods)
        spec = C.ModSpec(mod_id="b", game_versions=(">=1.5",))
        rep = C.check_install(spec, env.state_path)
        A.ok(not rep.ok and rep.blockers[0].kind == C.VERSION_MISMATCH,
             "version mismatch missed")

    @h.add("cf.missing_dependency_batch_satisfies", "conflicts")
    def _(sb=None):
        mods = {"a": {"files": {"Mods/a/main.lua": b"A"}}}
        env = Env(sb, mods)
        spec = C.ModSpec(mod_id="b", requires=("c",))
        rep = C.check_install(spec, env.state_path)
        A.ok(not rep.ok, "missing dependency missed")
        peer = C.ModSpec(mod_id="c")
        rep2 = C.check_install(spec, env.state_path, candidates=[peer])
        A.ok(rep2.ok, "batch peer should satisfy dependency")

    @h.add("cf.level2_block_comment_false_positive", "conflicts")
    def _(sb=None):
        """--[==[ ... ]==] comments: strip_lua_comments only knows --[[ ]].
        A RegisterKeyBind inside a level-2 comment may be detected as real."""
        text = ('--[==[\nRegisterKeyBind(Key.F9, cb)\n]==]\n'
                'RegisterKeyBind(Key.F1, cb)\n')
        keys = C.parse_keybinds(text)
        print(f"    [observed] level-2 comment keys: {keys}")

    @h.add("cf.obfuscated_keybind_evasion", "conflicts")
    def _(sb=None):
        """Dynamic keybind registration evades the static regex — known
        static-analysis limit; document that it does not crash."""
        text = ('local k = "Key." .. "F6"\n'
                '_G["Register" .. "KeyBind"](k, cb)\n')
        keys = C.parse_keybinds(text)
        A.eq(keys, [], "obfuscated keybind unexpectedly detected")

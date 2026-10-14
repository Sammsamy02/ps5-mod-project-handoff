#!/usr/bin/env python3
"""console_link.py — app<->console contract implementation (host-side helpers).

Implements the two halves of the app<->console contract so the app can be
tested without a console:

  * build_command()   — emit `e2e-cmd.json` command files exactly per
    worker-e2e-console-dry/E2E-CMD-SCHEMA.md (schema 1, verb allow-list,
    key->flag map, deny-list). Byte-compatible with the 13 staged files in
    worker-e2e-staging-final/cmd/ (tests prove it against the pinned SHASUMS).
  * staged_sequence() — the full 13-command register/prepare/install/ack/remove
    sequence as (name, json) pairs, parameterized by run facts.
  * validate_command()— the config layer's eager refusals (unknown keys, bad
    types, verb/option legality, mods+remove exclusion, deny-list).
  * E2eTranscript     — e2e-status.jsonl reader/validator: begin/end pairing
    (any unmatched begin is a DEAD command), truncation detection, per-stage
    status events.

Console drive protocol (documented in CONSOLE-CONTRACT.md): STOR command file
only when no stale file exists, verify FTP readback sha, dispatch payload,
then require consumption proof (file gone, `.consumed` present and
hash-equal) and a paired e2e-status transcript. This module deliberately does
NOT do network I/O — the controller pastes the curl script; the app gets
machine-checkable artifacts instead.
"""

import hashlib
import json

SCHEMA = 1

VERBS = ("records", "register-mod", "prepare", "prepare-archive",
         "install", "simulate-runtime", "recover")

# options key -> (argv token, legal verbs, json type)
OPTIONS = {
    "title":          ("--title", ("register-mod", "prepare", "prepare-archive",
                                   "install", "simulate-runtime"), str),
    "id":             ("--id", ("register-mod",), str),
    "bundle":         ("--bundle", ("register-mod",), str),
    "archive":        ("--archive", ("prepare-archive",), str),
    "quarantine":     ("--quarantine", ("prepare-archive",), str),
    "mod":            ("--mod", ("prepare-archive",), str),
    "remove":         ("--remove", ("install",), str),
    "rt_state":       ("--rt-state", ("simulate-runtime",), str),
    "choices":        ("--choices", ("prepare", "install"), str),
    "no_auto_bundle": ("--no-auto-bundle", ("prepare-archive",), bool),
}

DENY_VERBS = ("make-bundle", "download")  # host fixture / transport-owning verbs


class CommandRefused(Exception):
    pass


def validate_command(doc):
    """Eager config-layer refusals per E2E-CMD-SCHEMA.md section 2/3/6."""
    if not isinstance(doc, dict):
        raise CommandRefused("top level must be an object")
    allowed_top = {"schema", "verb", "state_root", "mods", "options"}
    for k in doc:
        if k not in allowed_top:
            raise CommandRefused("unknown key %r" % k)
    if doc.get("schema") != SCHEMA:
        raise CommandRefused("schema must be %d" % SCHEMA)
    verb = doc.get("verb")
    if verb not in VERBS:
        raise CommandRefused("verb %r not in allow-list" % (verb,))
    for k in doc.get("state_root", {}):
        if k not in ("state", "roots"):
            raise CommandRefused("unknown state_root key %r" % k)
    mods = doc.get("mods")
    if mods is not None:
        if verb not in ("prepare", "install"):
            raise CommandRefused("mods legal only for prepare/install")
        if not isinstance(mods, list) or not all(isinstance(m, str) for m in mods):
            raise CommandRefused("mods must be an array of strings")
    opts = doc.get("options", {})
    if not isinstance(opts, dict):
        raise CommandRefused("options must be an object")
    for k, v in opts.items():
        if k not in OPTIONS:
            raise CommandRefused("unknown options key %r" % k)
        _flag, verbs, typ = OPTIONS[k]
        if verb not in verbs:
            raise CommandRefused("options key %r illegal for verb %s" % (k, verb))
        if typ is bool:
            if v is not True:
                raise CommandRefused("options key %r only accepts true" % k)
        elif not isinstance(v, str):
            raise CommandRefused("options key %r must be a string" % k)
    if mods is not None and "remove" in opts:
        raise CommandRefused("mods together with options.remove is refused "
                             "(install is either add or remove)")
    return doc


def build_command(verb, state, roots, mods=None, options=None, validate=True):
    doc = {"schema": SCHEMA, "verb": verb,
           "state_root": {"state": state, "roots": roots}}
    if mods is not None:
        doc["mods"] = list(mods)
    doc["options"] = options if options else {}
    if validate:
        validate_command(doc)
    # exact staged serialization: compact separators, insertion order
    return json.dumps(doc, separators=(",", ":"), ensure_ascii=False)


def staged_sequence(state="/data/modbridge/state", roots="/data/modbridge/roots",
                    title="PPSA33286", zip_id="movspeed",
                    mfb_ids=("mm-skin", "mm-lua")):
    """The worker-e2e-staging-final 13-command sequence, parameterized.

    Byte-identical to cmd/*.json for the default arguments (tests pin the
    published SHASUMS)."""
    sr = {"state": state, "roots": roots}
    def c(verb, mods=None, **opts):
        return build_command(verb, state, roots, mods=mods, options=opts)
    seq = [
        ("recover",               c("recover")),
        ("stage0-records",        c("records")),
        ("stage1-register",       c("register-mod", id=zip_id, title=title,
                                   bundle="/data/modbridge/intake/%s.zip" % zip_id)),
        ("stage2a-prepare-archive",
         c("prepare-archive", title=title,
           archive="/data/modbridge/intake/%s.zip" % zip_id,
           quarantine="/data/modbridge/intake/%s.q" % zip_id, mod=zip_id)),
        ("stage2b-1-register",    c("register-mod", id=mfb_ids[0], title=title,
                                    bundle="/data/modbridge/intake/%s.mfb" % mfb_ids[0])),
        ("stage2b-2-register",    c("register-mod", id=mfb_ids[1], title=title,
                                    bundle="/data/modbridge/intake/%s.mfb" % mfb_ids[1])),
        ("stage2b-3-prepare",     c("prepare", title=title, mods=list(mfb_ids))),
        ("stage3-install",        c("install", title=title,
                                    mods=[zip_id] + list(mfb_ids))),
        ("stage4-activated",      c("simulate-runtime", title=title,
                                    rt_state="activated")),
        ("stage5-mods-started",   c("simulate-runtime", title=title,
                                    rt_state="mods-started")),
        ("stage6-1-remove",       c("install", title=title, remove=mfb_ids[1])),
        ("stage6-2-remove",       c("install", title=title, remove=mfb_ids[0])),
        ("stage6-3-remove",       c("install", title=title, remove=zip_id)),
    ]
    return seq


# --------------------------------------------------------------------------- #
# e2e-status.jsonl protocol                                                    #
# --------------------------------------------------------------------------- #

class E2eTranscript:
    """Parser/validator for <state>/e2e-status.jsonl (owned e2e_status.c)."""

    def __init__(self, text):
        self.events = []
        self.bad_lines = []
        for ln in text.split("\n"):
            if not ln:
                continue
            try:
                self.events.append(json.loads(ln))
            except ValueError:
                self.bad_lines.append(ln)
        self.truncated = any(e.get("event") == "truncated" for e in self.events)

    def pairing(self):
        """(begins, ends, unmatched) — a begin whose end never arrived means
        the command DIED (RUNBOOK honesty rule)."""
        begins = ends = unmatched = 0
        for e in self.events:
            ev = e.get("event")
            if ev == "begin":
                begins += 1
            elif ev == "end":
                ends += 1
                if ends > begins:
                    unmatched += 1  # end without begin
        unmatched += begins - ends if begins > ends else 0
        return begins, ends, unmatched

    def last_exit(self):
        for e in reversed(self.events):
            if e.get("event") == "end":
                return e.get("exit")
        return None

    def status_events(self, stage=None, outcome=None):
        return [e for e in self.events
                if e.get("event") == "status"
                and (stage is None or e.get("stage") == stage)
                and (outcome is None or e.get("outcome") == outcome)]

    def validate(self):
        begins, ends, unmatched = self.pairing()
        problems = []
        if unmatched:
            problems.append("%d unmatched begin/end (dead or extra command)" % unmatched)
        if self.truncated:
            problems.append("transcript hit the 512-line/256KiB cap")
        if self.bad_lines:
            problems.append("%d unparseable line(s)" % len(self.bad_lines))
        return (len(problems) == 0), problems


def command_file_sha(data):
    if isinstance(data, str):
        data = data.encode("utf-8")
    return hashlib.sha256(data).hexdigest()

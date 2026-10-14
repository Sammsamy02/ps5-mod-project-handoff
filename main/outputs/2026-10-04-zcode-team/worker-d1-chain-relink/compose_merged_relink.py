#!/usr/bin/env python3
"""compose_console_bridge.py — worker-zip-bridge-console-link (PS5 mod-manager).

Controller-step worker: link the worker-zip-to-mfb F-R1 archive->bundle bridge
TU into the CONSOLE-flavor installer payload. worker-zip-to-mfb delivered the
bridge host-only ("the console link was NOT run here"); this worker closes
that step without touching any original file, by the same vendored-copy +
recorded exact-anchor patch chain discipline:

Chain (every step sha-pinned, applied to text, never to originals):
  1. worker-installer-composition/compose_installer.py (read-only root; sha
     cross-checked against worker-e2e-final e2e-receipts/script-patches.json
     vendored_script_sha256).
  2. worker-e2e-final/compose_e2e.py (sha cross-checked against
     worker-zip-to-mfb e2e-receipts/bridge-script-patches.json
     vendored_driver_sha256 — the exact input that produced the green 75/75
     delivery; NOTE: e2e-final's own older adapted_script_sha256 pin covers a
     pre-delivery revision and is intentionally not the pin used here).
  3. worker-zip-to-mfb's 4 recorded splices replayed VERBATIM; the
     intermediate adapted driver text must hash exactly to zip-to-mfb's
     adapted_driver_sha256 — a byte-exact replay proof.
  4. THIS worker's 2 corrective splices:
       - zb-console-inner-name: distinct inner script name in THIS directory
         (composition_zb_console_inner.py);
       - zb-console-target-bridge: add owned/modinst_zip_bridge.c to the
         CONSOLE target list. Recorded defect: the delivered chain adds the
         bridge TU to the HOST target list only, while the zb-dispatch-bridge
         modinst.c patch makes modinst_core.c reference mi_bridge_* on BOTH
         flavors — the delivered console link would fail with undefined
         mi_bridge_run/wants_auto/mod_arg. This splice is the minimal fix.
  5. vendor the delivered owned set from worker-zip-to-mfb/owned (11 files),
     every sha cross-checked against zip-to-mfb's owned-manifest.json, and
     the 9 pre-bridge files additionally against worker-e2e-final's
     owned-manifest.json.
  6. run the adapted inner script (subprocess, MODBRIDGE_ROOT pinned) with
     --flavor console (the deliverable payload) and/or --flavor host (the
     regression binary for the 75/75 bridge suite).

Run:
    python3 compose_console_bridge.py --flavor console   # payload step
    python3 compose_console_bridge.py --flavor host      # suite binary
    sh test/run_bridge_tests.sh                          # 75/75 regression

No network; no console; originals never modified; all writes under
outputs/2026-10-04-zcode-team/worker-zip-bridge-console-link/.
"""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = pathlib.Path(os.environ.get("MODBRIDGE_ROOT", HERE.parents[2]))
TEAM = ROOT / "outputs/2026-10-04-zcode-team"

INSTALLER = TEAM / "worker-installer-composition"
E2E = TEAM / "worker-e2e-final"
ZB = TEAM / "worker-zip-to-mfb"

ORIG_SCRIPT = INSTALLER / "compose_installer.py"
E2E_SCRIPT = E2E / "compose_e2e.py"
DRIVER = HERE / "composition_zb_console_inner.py"
OWNED = HERE / "owned"
REC = HERE / "receipts"

# ---- zip-to-mfb's 4 recorded splices, replayed VERBATIM (compose_bridge.py) --
S_INNER = ("bridge-script-inner-name",
           'INNER = HERE / "composition_e2e_inner.py"',
           'INNER = HERE / "composition_bridge_inner.py"   # worker-zip-to-mfb')

S_OWNED_LIST = ("bridge-script-owned-authored-list",
                '    for name in ("modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c"):',
                '    for name in ("modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c",\n'
                '                 "modinst_zip_bridge.c", "modinst_zip_bridge.h"):')

S_TARGETS = ("bridge-script-host-target",
             "             '        (\"mb_curl_stub_host\", HERE / \"owned/mb_curl_stub_host.c\",\\n'\n"
             "             '         core_flags + [\"-I\" + str(VEND / \"download\")]),\\n'",
             "             '        (\"mb_curl_stub_host\", HERE / \"owned/mb_curl_stub_host.c\",\\n'\n"
             "             '         core_flags + [\"-I\" + str(VEND / \"download\")]),\\n'\n"
             "             '        (\"modinst_zip_bridge\", HERE / \"owned/modinst_zip_bridge.c\", core_flags),\\n'")

S_PATCH_APPEND = ("bridge-script-patches-append",
                  '    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),\n]',
                  '    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),\n]\n' + '''
# ---- worker-zip-to-mfb (F-R1 bridge) patch tuples, applied after the e2e chain
ZB_INC_OLD = """#ifdef MI_HAS_DOWNLOAD
#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */
#endif"""
ZB_INC_NEW = ZB_INC_OLD + '\\n#include "modinst_zip_bridge.h"   /* worker-zip-to-mfb: F-R1 quarantine->mfb bridge */'

ZB_FLAG_OLD = "        else if (!strcmp(argv[i], \\"--job\\") && i + 1 < argc) ++i;          /* download (owned command parses it) */"
ZB_FLAG_NEW = ZB_FLAG_OLD + '\\n        else if (!strcmp(argv[i], \\"--no-auto-bundle\\")) ;         /* value-less opt-out (worker-zip-to-mfb bridge parses it) */'

ZB_DISP_OLD = "              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);"
ZB_DISP_NEW = """              if (e2e_pr == 0 && mi_bridge_wants_auto(argc, argv)) {
                  /* worker-zip-to-mfb (F-R1): compose a kind-1 mfb from the
                   * quarantine BEFORE the records transition; on success the
                   * record's bundle becomes the bridge output so install
                   * resolves a real modforge bundle. A refusal keeps the
                   * record at downloaded and the quarantine retained. */
                  char bridge_bundle[LINE_MAX_ * 2];
                  e2e_pr = mi_bridge_run(argc, argv, g_state_dir,
                                         bridge_bundle, sizeof bridge_bundle);
                  if (e2e_pr == 0) {
                      modrec *bm = find_mod(mi_bridge_mod_arg(argc, argv));
                      if (bm) snprintf(bm->bundle, sizeof bm->bundle, "%s", bridge_bundle);
                  }
              }
              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);"""

ZB_USAGE_OLD = '    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");'
ZB_USAGE_NEW = ZB_USAGE_OLD + '\\n    say("          [--no-auto-bundle]  (F-R1 bridge: with --mod, default composes bundles/<id>.mfb from the quarantine)");'

EXTRA_PATCHES += [
    ("zb-inc-bridge", ZB_INC_OLD, ZB_INC_NEW, 1),
    ("zb-flag-no-auto-bundle", ZB_FLAG_OLD, ZB_FLAG_NEW, 1),
    ("zb-dispatch-bridge", ZB_DISP_OLD, ZB_DISP_NEW, 1),
    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),
]
''')

ZB_SPLICES = [S_INNER, S_OWNED_LIST, S_TARGETS, S_PATCH_APPEND]

# ---- THIS worker's corrective splices (applied AFTER the byte-exact replay) --
# The console target list does not exist in driver text (it is produced at
# inner-generation time), so the console fix extends the driver's splice
# sequence with one more exact-anchor tuple over the vendored
# compose_installer.py text — the same mechanism the delivered chain uses.
MY_SPLICES = [
    ("zb-console-inner-name",
     'INNER = HERE / "composition_bridge_inner.py"   # worker-zip-to-mfb',
     'INNER = HERE / "composition_zb_console_inner.py"   # worker-zip-bridge-console-link'),
    ("zb-console-targets-seq",
     '    for pid, old, new in (S_DL_DEF, S_MKDIR, S_VENDOR, S_COREFLAGS, S_TARGETS):',
     '    for pid, old, new in (S_DL_DEF, S_MKDIR, S_VENDOR, S_COREFLAGS, S_TARGETS,\n'
     '                          S_CONSOLE_TARGETS):'),
    ("zb-console-target-def",
     '             \'        obj = out / (name + ".o")\')\n'
     'PATCHES_TAIL_HEAD =',
     '             \'        obj = out / (name + ".o")\')\n'
     "S_CONSOLE_TARGETS = (\"zb-console-target-bridge\",\n"
     "                     '        (\"posix_compat\", VEND / \"archive/private/posix_compat.c\", closure_flags),\\n'\n"
     "                     '    ]\\n'\n"
     "                     '    commands = []\\n'\n"
     "                     '    objects = []\\n'\n"
     "                     '    for name, src, flags in targets:\\n'\n"
     "                     '        obj = out / (name + \".ps5.o\")',\n"
     "                     '        (\"posix_compat\", VEND / \"archive/private/posix_compat.c\", closure_flags),\\n'\n"
     "                     '        (\"modinst_zip_bridge\", HERE / \"owned/modinst_zip_bridge.c\", core_flags),\\n'\n"
     "                     '    ]\\n'\n"
     "                     '    commands = []\\n'\n"
     "                     '    objects = []\\n'\n"
     "                     '    for name, src, flags in targets:\\n'\n"
     "                     '        obj = out / (name + \".ps5.o\")')\n"
     'PATCHES_TAIL_HEAD ='),
    # ---- worker-d1-chain-relink merged-chain splices (D1 fix FIRST,
    # F-R2 fix LAST; canonical 51-tuple order per worker-risk-e2e/
    # D1-PROPAGATION.md section 4). Block texts verified verbatim
    # against both fix-origin patches.json pins by the builder. ----
    ("d1-uf-block-append",
     '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]',
     '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]\n# ---- worker-uninstall-fault-expansion patch tuples, applied after the bridge\n# Defect D1 (found by adversarial window analysis + d1_window_probe.sh on the\n# baseline): cmd_install deleted the journal BEFORE committing records, so a\n# death in between left "journal durably gone + records stale" -- an end state\n# no recovery can repair (no journal == "nothing in flight"), and records\n# claimed installed while files were already gone. Fix: make records durable\n# FIRST in all three commit sites (cmd_install + both recover_title commit\n# paths). The residual window (records durable, journal present) is repaired\n# idempotently by recover_title via the staged load-list hash.\nUF_COMMIT_A_OLD = """    maybe_crash("prevdel");\n    journal_delete(title);\n\n    /* ---- commit records ---- */"""\nUF_COMMIT_A_NEW = """    maybe_crash("prevdel");\n\n    /* ---- commit records ----\n     * worker-uninstall-fault-expansion (uf-order-commit): records commit\n     * BEFORE journal_delete. Old order had a crash window leaving durable\n     * journal-delete + stale records: no journal means "nothing in flight",\n     * so recovery never repaired records that claimed installed while files\n     * were already gone. New order leaves records durable while the journal\n     * is still present, which recover_title repairs idempotently via the\n     * staged load-list hash. */"""\n\nUF_COMMIT_B_OLD = """        records_save();\n        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n    }\n}"""\nUF_COMMIT_B_NEW = """        records_save();\n        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n        maybe_crash("pre-journal-del"); /* uf probe: records durable, journal still present */\n    }\n    journal_delete(title); /* uf-order-commit: journal released only after records are durable */\n}"""\n\nUF_REC_FWD_OLD = """                rm_rf(prev);\n                journal_delete(title);\n                fsync_dir(g_roots);\n                records_commit_from_root(title, &j);\n                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);"""\nUF_REC_FWD_NEW = """                rm_rf(prev);\n                records_commit_from_root(title, &j); /* uf-order-commit: records first */\n                journal_delete(title);\n                fsync_dir(g_roots);\n                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);"""\n\nUF_REC_CMT_OLD = """                rm_rf(next);\n                rm_rf(prev);\n                journal_delete(title);\n                fsync_dir(g_roots);\n                records_commit_from_root(title, &j);\n                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);"""\nUF_REC_CMT_NEW = """                rm_rf(next);\n                rm_rf(prev);\n                records_commit_from_root(title, &j); /* uf-order-commit: records first */\n                journal_delete(title);\n                fsync_dir(g_roots);\n                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);"""\n\nEXTRA_PATCHES += [\n    ("uf-order-commit-a", UF_COMMIT_A_OLD, UF_COMMIT_A_NEW, 1),\n    ("uf-order-commit-b", UF_COMMIT_B_OLD, UF_COMMIT_B_NEW, 1),\n    ("uf-recover-forward", UF_REC_FWD_OLD, UF_REC_FWD_NEW, 1),\n    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]\n'),
    ("fr2-block-append",
     '    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]',
     '    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]\n# ---- worker-f-r2-fix (F-R2) patch tuples, applied after the zb chain\nR2_SHIM_OLD = \'#include "../vendored/modforge_activation.h"\'\nR2_SHIM_NEW = R2_SHIM_OLD + """\n\n/* worker-f-r2-fix (F-R2, worker-e2e-rehearsal REPORT.md section 3): route\n * every controlled exit() of the composed core through the status end path,\n * so a refused command (e.g. install rejecting a non-bundle archive) cannot\n * leave a dangling begin line in e2e-status.jsonl. Exit codes and all say()\n * output are unchanged; _exit(86) crash probes and real signals still leave\n * no end line, so "missing end == command died" honesty is preserved. This\n * sits after the LAST include so no header text is seen through the macro. */\ntypedef void (*r2_real_exit_fn)(int);\nstatic r2_real_exit_fn r2_real_exit = exit;\n#define exit r2_e2e_exit\nstatic void r2_e2e_exit(int code) {\n    e2e_status_end(code);\n    r2_real_exit(code);\n}"""\nR2_STATUS_OLD = \'    say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\'\nR2_STATUS_NEW = R2_STATUS_OLD + \'\\n    if (mods_out[0]) e2e_status_event("stage-3-install", "ok", mods_out);   /* worker-f-r2-fix (F-R2): terminal install status event; installs only — the remove commit shares this line with an empty mods list */\'\n\nEXTRA_PATCHES += [\n    ("r2-exit-shim", R2_SHIM_OLD, R2_SHIM_NEW, 1),\n    ("r2-install-status-ok", R2_STATUS_OLD, R2_STATUS_NEW, 1),\n]\n'),
    ("g1-block-append",
     '    ("r2-install-status-ok", R2_STATUS_OLD, R2_STATUS_NEW, 1),\n]',
     '    ("r2-install-status-ok", R2_STATUS_OLD, R2_STATUS_NEW, 1),\n]\n# ---- worker-e2e-config-entry (G1) config-file-driven entry patch\n# tuples, applied after the F-R2 chain. Promoted receipt tail\n# (patches.json.new, sha 63c28979...; endpoint 43ea66c1...). Tuples\n# extracted VERBATIM from worker-e2e-payload-build/g1_block.py via AST\n# and verified against the promoted pins by\n# worker-e2e-composer-splice/g1_composer_splice_builder.py. ----\nEXTRA_PATCHES += [\n    (\'g1-cmd-entry-parse\', \'int main(int argc, char **argv) {\', \'/* ---- worker-e2e-config-entry: config-file-driven entry (G1 fallback) --------\\n * Implements worker-e2e-console-dry/E2E-CMD-SCHEMA.md sec 1-6, mirroring the\\n * host harness worker-e2e-staging-final/host-test/host_cmd_entry_test.py\\n * line-for-line (same bounded JSON subset, same allow/deny lists, same\\n * synthesized argv). Controller deltas receipted in\\n * outputs/2026-10-04-zcode-team/worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md:\\n *   - EX_CONFIG (6): file present but refused - distinct from EX_BAD (2),\\n *     which stays "no file at the slot" (sec 1: identical to today\\\'s bare\\n *     usage()) and "consume rename failed" (sec 6);\\n *   - on refusals where state_root.state itself validated, exactly ONE\\n *     {"event":"status","stage":"e2e-cmd","outcome":"refused",...} line to the\\n *     existing e2e-status.jsonl report path (no begin/end pair, no store\\n *     mutation, verb never executed). Parse-level failures with no valid\\n *     state dir log one stdout line only.\\n * Exit paths here RETURN (never the exit() macro - F-R2 routes that through\\n * e2e_status_end and a refusal must not leave a dangling end line).\\n * Imports: read_file/say/malloc/free/snprintf/ps5io_rename and the e2e_status\\n * API only - all already in the 116-symbol census; census delta target ZERO.\\n * Parse/validation memory (one <=4096-byte file) is intentionally live until\\n * process exit: the synthesized argv points into it for the dispatch. */\\n\\n#define E2E_CMD_PATH "/data/modbridge/intake/e2e-cmd.json"\\n#define E2E_CMD_MAX 4096   /* file size cap, refused before parsing (sec 5) */\\n#define E2E_ARGV_MAX 64    /* synthesized vector cap (sec 5) */\\n#define EX_CONFIG 6        /* e2e-cmd present but refused (distinct code) */\\n\\ntypedef enum { EJ_STR, EJ_INT, EJ_BOOL, EJ_OBJ, EJ_ARR } ej_kind;\\n\\ntypedef struct ej_val ej_val;\\nstruct ej_val {\\n    ej_kind kind;\\n    char *s;        /* EJ_STR */\\n    long i;         /* EJ_INT */\\n    int b;          /* EJ_BOOL */\\n    char *key;      /* member name when linked under an EJ_OBJ */\\n    ej_val *child;  /* EJ_OBJ/EJ_ARR: first member/element */\\n    ej_val *next;   /* next sibling */\\n};\\n\\nstatic void ej_free(ej_val *v) {\\n    while (v) {\\n        ej_val *n = v->next;\\n        ej_free(v->child);\\n        free(v->s);\\n        free(v->key);\\n        free(v);\\n        v = n;\\n    }\\n}\\n\\ntypedef struct {\\n    const char *p;\\n    size_t i, n;\\n    const char *err;   /* one-line refusal reason, set at the failure site */\\n} ej_cur;\\n\\n/* Strict UTF-8 over the whole file (host harness: raw.decode("utf-8")). */\\nstatic int ej_utf8_ok(const char *p, size_t n) {\\n    size_t i = 0;\\n    while (i < n) {\\n        unsigned char b = (unsigned char)p[i];\\n        if (b < 0x80) { i++; continue; }\\n        if (b >= 0xc2 && b <= 0xdf) {\\n            if (i + 1 >= n || (unsigned char)p[i+1] >> 6 != 2) return 0;\\n            i += 2;\\n        } else if (b == 0xe0) {\\n            if (i + 2 >= n) return 0;\\n            if ((unsigned char)p[i+1] < 0xa0 || (unsigned char)p[i+1] > 0xbf ||\\n                (unsigned char)p[i+2] >> 6 != 2) return 0;\\n            i += 3;\\n        } else if ((b >= 0xe1 && b <= 0xec) || b == 0xee || b == 0xef) {\\n            if (i + 2 >= n || (unsigned char)p[i+1] >> 6 != 2 ||\\n                (unsigned char)p[i+2] >> 6 != 2) return 0;\\n            i += 3;\\n        } else if (b == 0xed) {\\n            if (i + 2 >= n || (unsigned char)p[i+1] < 0x80 || (unsigned char)p[i+1] > 0x9f ||\\n                (unsigned char)p[i+2] >> 6 != 2) return 0;\\n            i += 3;\\n        } else if (b == 0xf0) {\\n            if (i + 3 >= n) return 0;\\n            if ((unsigned char)p[i+1] < 0x90 || (unsigned char)p[i+1] > 0xbf ||\\n                (unsigned char)p[i+2] >> 6 != 2 || (unsigned char)p[i+3] >> 6 != 2) return 0;\\n            i += 4;\\n        } else if (b >= 0xf1 && b <= 0xf3) {\\n            if (i + 3 >= n || (unsigned char)p[i+1] >> 6 != 2 ||\\n                (unsigned char)p[i+2] >> 6 != 2 || (unsigned char)p[i+3] >> 6 != 2) return 0;\\n            i += 4;\\n        } else if (b == 0xf4) {\\n            if (i + 3 >= n || (unsigned char)p[i+1] < 0x80 || (unsigned char)p[i+1] > 0x8f ||\\n                (unsigned char)p[i+2] >> 6 != 2 || (unsigned char)p[i+3] >> 6 != 2) return 0;\\n            i += 4;\\n        } else return 0;   /* 0x80-0xbf continuation, 0xc0/0xc1 overlong, 0xf5-0xff */\\n    }\\n    return 1;\\n}\\n\\nstatic void ej_ws(ej_cur *c) {\\n    while (c->i < c->n && (c->p[c->i] == \\\' \\\' || c->p[c->i] == \\\'\\\\t\\\' ||\\n                           c->p[c->i] == \\\'\\\\r\\\' || c->p[c->i] == \\\'\\\\n\\\')) c->i++;\\n}\\n\\nstatic int ej_peek(ej_cur *c) {\\n    return c->i < c->n ? (unsigned char)c->p[c->i] : -1;\\n}\\n\\n/* Sec 5 string: escapes limited to \\\\" \\\\\\\\ \\\\/ ; \\\\uXXXX refuses; control <0x20\\n * refuses. Storage is one E2E_CMD_MAX+1 block - a string can never exceed\\n * the file it was parsed from. */\\nstatic ej_val *ej_string(ej_cur *c) {\\n    ej_val *v;\\n    char *s;\\n    if (ej_peek(c) != \\\'"\\\') { c->err = "expected \\\'\\\\"\\\'"; return 0; }\\n    c->i++;\\n    s = (char *)malloc(E2E_CMD_MAX + 1);\\n    if (!s) { c->err = "out of memory"; return 0; }\\n    {\\n        size_t len = 0;\\n        for (;;) {\\n            unsigned char ch;\\n            if (c->i >= c->n) { free(s); c->err = "unterminated string"; return 0; }\\n            ch = (unsigned char)c->p[c->i];\\n            if (ch == \\\'"\\\') {\\n                c->i++;\\n                s[len] = 0;\\n                break;\\n            }\\n            if (ch < 0x20) { free(s); c->err = "control char in string"; return 0; }\\n            if (ch == \\\'\\\\\\\\\\\') {\\n                unsigned char e;\\n                c->i++;\\n                if (c->i >= c->n) { free(s); c->err = "bad escape"; return 0; }\\n                e = (unsigned char)c->p[c->i];\\n                if (e == \\\'u\\\') { free(s); c->err = "\\\\\\\\uXXXX escape refuses (sec 5)"; return 0; }\\n                if (e != \\\'"\\\' && e != \\\'\\\\\\\\\\\' && e != \\\'/\\\') {\\n                    free(s); c->err = "bad escape (only \\\\\\\\\\\\" \\\\\\\\\\\\\\\\ \\\\\\\\/ legal)"; return 0;\\n                }\\n                ch = e;\\n                c->i++;\\n            } else c->i++;\\n            s[len++] = (char)ch;\\n        }\\n    }\\n    v = (ej_val *)calloc(1, sizeof *v);\\n    if (!v) { free(s); c->err = "out of memory"; return 0; }\\n    v->kind = EJ_STR;\\n    v->s = s;\\n    return v;\\n}\\n\\nstatic ej_val *ej_new(ej_kind k) {\\n    ej_val *v = (ej_val *)calloc(1, sizeof *v);\\n    if (!v) return 0;\\n    v->kind = k;\\n    return v;\\n}\\n\\nstatic ej_val *ej_value(ej_cur *c, int depth);\\n\\n/* Sec 5: one top-level object; object depth <= 2; arrays only as a top-level\\n * member value (mods); only true (false/null refuse); plain ints only;\\n * duplicate keys refuse. Mirrors rd_parse() byte-for-byte. */\\nstatic ej_val *ej_value(ej_cur *c, int depth) {\\n    int ch;\\n    ej_ws(c);\\n    ch = ej_peek(c);\\n    if (ch < 0) { c->err = "unexpected end of input"; return 0; }\\n    if (ch == \\\'"\\\') return ej_string(c);\\n    if (ch == \\\'{\\\') {\\n        ej_val *obj;\\n        if (depth >= 2) { c->err = "object depth > 2 refuses (sec 5)"; return 0; }\\n        c->i++;\\n        obj = ej_new(EJ_OBJ);\\n        if (!obj) { c->err = "out of memory"; return 0; }\\n        ej_ws(c);\\n        if (ej_peek(c) == \\\'}\\\') { c->i++; return obj; }\\n        for (;;) {\\n            ej_val *kn, *v, *tail;\\n            char *k;\\n            ej_ws(c);\\n            kn = ej_string(c);          /* key: parse a string node, steal its s */\\n            if (!kn) return 0;\\n            k = kn->s;\\n            free(kn);\\n            for (tail = obj->child; tail; tail = tail->next)\\n                if (!strcmp(tail->key, k)) {\\n                    free(k);\\n                    c->err = "duplicate key";\\n                    return 0;\\n                }\\n            ej_ws(c);\\n            if (ej_peek(c) != \\\':\\\') {\\n                free(k); c->err = "expected \\\':\\\'"; return 0;\\n            }\\n            c->i++;\\n            v = ej_value(c, depth + 1);\\n            if (!v) { free(k); return 0; }\\n            v->key = k;                 /* the value node IS the member */\\n            if (obj->child) {\\n                for (tail = obj->child; tail->next; tail = tail->next) ;\\n                tail->next = v;\\n            } else obj->child = v;\\n            ej_ws(c);\\n            if (ej_peek(c) == \\\',\\\') { c->i++; continue; }\\n            if (ej_peek(c) == \\\'}\\\') { c->i++; return obj; }\\n            c->err = "expected \\\',\\\' or \\\'}\\\'";\\n            return 0;\\n        }\\n    }\\n    if (ch == \\\'[\\\') {\\n        ej_val *arr;\\n        if (depth != 1) { c->err = "array only legal as a top-level member: mods (sec 5)"; return 0; }\\n        c->i++;\\n        arr = ej_new(EJ_ARR);\\n        if (!arr) { c->err = "out of memory"; return 0; }\\n        ej_ws(c);\\n        if (ej_peek(c) == \\\']\\\') { c->i++; return arr; }\\n        for (;;) {\\n            ej_val *el, *tail;\\n            el = ej_value(c, depth + 1);\\n            if (!el) return 0;\\n            if (arr->child) {\\n                for (tail = arr->child; tail->next; tail = tail->next) ;\\n                tail->next = el;\\n            } else arr->child = el;\\n            ej_ws(c);\\n            if (ej_peek(c) == \\\',\\\') { c->i++; continue; }\\n            if (ej_peek(c) == \\\']\\\') { c->i++; return arr; }\\n            c->err = "expected \\\',\\\' or \\\']\\\'";\\n            return 0;\\n        }\\n    }\\n    if (c->n - c->i >= 4 && !memcmp(c->p + c->i, "true", 4)) {\\n        ej_val *v = ej_new(EJ_BOOL);\\n        if (!v) { c->err = "out of memory"; return 0; }\\n        v->b = 1;\\n        c->i += 4;\\n        return v;\\n    }\\n    if (c->n - c->i >= 5 && !memcmp(c->p + c->i, "false", 5)) {\\n        c->err = "only bool true is allowed (sec 5)";\\n        return 0;\\n    }\\n    if (c->n - c->i >= 4 && !memcmp(c->p + c->i, "null", 4)) {\\n        c->err = "only bool true is allowed (sec 5)";\\n        return 0;\\n    }\\n    if (ch == \\\'-\\\' || (ch >= \\\'0\\\' && ch <= \\\'9\\\')) {\\n        size_t j = c->i + (ch == \\\'-\\\' ? 1 : 0);\\n        long v;\\n        if (j >= c->n || c->p[j] < \\\'0\\\' || c->p[j] > \\\'9\\\') { c->err = "bad int"; return 0; }\\n        while (j < c->n && c->p[j] >= \\\'0\\\' && c->p[j] <= \\\'9\\\') j++;\\n        /* bounded sign-aware decimal fold, saturating (NOT strtol: plain\\n         * strtol is absent from the console 116-import census and would\\n         * break the delta-zero target). Only schema==1 is ever accepted, so\\n         * a saturated value refuses with the same "schema must be exactly 1"\\n         * line - observably identical to the harness int() for all inputs.\\n         * Cap 0x0CCCCCCCCCCCCCCC keeps v*10+9 inside long with no UB. */\\n        {\\n            size_t k = c->i + (ch == \\\'-\\\' ? 1 : 0);\\n            v = 0;\\n            while (k < j && v < 0x0CCCCCCCCCCCCCCCL) v = v * 10 + (c->p[k++] - \\\'0\\\');\\n            if (ch == \\\'-\\\') v = -v;\\n        }\\n        c->i = j;\\n        ej_ws(c);\\n        if (c->i < c->n) {\\n            int a = ej_peek(c);\\n            if ((a >= \\\'0\\\' && a <= \\\'9\\\') || a == \\\'.\\\' || a == \\\'e\\\' || a == \\\'E\\\' ||\\n                a == \\\'+\\\' || a == \\\'-\\\') { c->err = "only plain ints (sec 5)"; return 0; }\\n        }\\n        {\\n            ej_val *n = ej_new(EJ_INT);\\n            if (!n) { c->err = "out of memory"; return 0; }\\n            n->i = v;\\n            return n;\\n        }\\n    }\\n    c->err = "unexpected byte";\\n    return 0;\\n}\\n\\nstatic ej_val *ej_member(ej_val *obj, const char *key) {\\n    ej_val *m;\\n    if (!obj || obj->kind != EJ_OBJ) return 0;\\n    for (m = obj->child; m; m = m->next)\\n        if (!strcmp(m->key, key)) return m;\\n    return 0;\\n}\\n\\n/* One-line refusal record on the existing report path (controller delta) plus\\n * the schema sec 6 stdout line and usage(). state_dir may be 0 (no validated\\n * report path yet). Returns `code` for the caller to return from main. */\\nint main(int argc, char **argv) {\', 1),\n    (\'g1-cmd-entry-allowlist\', \'int main(int argc, char **argv) {\', \'static int e2e_refuse(const char *state_dir, const char *reason, int code) {\\n    if (state_dir) {\\n        e2e_status_set_dir(state_dir);\\n        e2e_status_event("e2e-cmd", "refused", reason);\\n    }\\n    say("error bad_request e2e-cmd: %s", reason);\\n    usage();\\n    return code;\\n}\\n\\nstatic int e2e_verb_legal(const char *key, const char *verb) {\\n    /* schema sec 3 mapping: options key -> legal verbs (eager refusal) */\\n    if (!strcmp(key, "title"))\\n        return !strcmp(verb, "register-mod") || !strcmp(verb, "prepare") ||\\n               !strcmp(verb, "prepare-archive") || !strcmp(verb, "install") ||\\n               !strcmp(verb, "simulate-runtime");\\n    if (!strcmp(key, "id") || !strcmp(key, "bundle")) return !strcmp(verb, "register-mod");\\n    if (!strcmp(key, "archive") || !strcmp(key, "quarantine") ||\\n        !strcmp(key, "mod") || !strcmp(key, "no_auto_bundle"))\\n        return !strcmp(verb, "prepare-archive");\\n    if (!strcmp(key, "remove")) return !strcmp(verb, "install");\\n    if (!strcmp(key, "rt_state")) return !strcmp(verb, "simulate-runtime");\\n    if (!strcmp(key, "choices")) return !strcmp(verb, "prepare") || !strcmp(verb, "install");\\n    return 0;\\n}\\n\\nstatic const char *e2e_opt_flag(const char *key) {\\n    if (!strcmp(key, "title")) return "--title";\\n    if (!strcmp(key, "id")) return "--id";\\n    if (!strcmp(key, "bundle")) return "--bundle";\\n    if (!strcmp(key, "archive")) return "--archive";\\n    if (!strcmp(key, "quarantine")) return "--quarantine";\\n    if (!strcmp(key, "mod")) return "--mod";\\n    if (!strcmp(key, "remove")) return "--remove";\\n    if (!strcmp(key, "rt_state")) return "--rt-state";\\n    if (!strcmp(key, "choices")) return "--choices";\\n    if (!strcmp(key, "no_auto_bundle")) return "--no-auto-bundle";\\n    return 0;\\n}\\n\\nint main(int argc, char **argv) {\', 1),\n    (\'g1-cmd-entry-consume\', \'int main(int argc, char **argv) {\\n    const char *cmd = 0, *title = 0, *mods = 0, *remove = 0, *id = 0, *bundle = 0,\\n               *defaults = 0, *choices = 0, *statedir = 0, *rtstate = 0, *detail = 0,\\n               *crash = 0, *cancelat = 0;\\n    int code = 0, i;\\n    char vgroup[32][64], vopt[32][64], vbundle[32][LINE_MAX_];\\n    int nvar = 0;\\n    char state_dir[LINE_MAX_], roots[LINE_MAX_];\\n\\n    /* make-bundle owns its own flag set (fixture builder); bypass the store */\\n    if (argc > 1 && !strcmp(argv[1], "make-bundle")) {\\n        cmd_make_bundle(argc, argv);\\n        return EX_OK;\\n    }\\n\\n    for (i = 1; i < argc; ++i) {\', \'/* schema sec 6: consume BEFORE e2e_status_begin / verb execution */\\nstatic int e2e_consume(const char *path) {\\n    char consumed[LINE_MAX_ + 16];\\n    if (snprintf(consumed, sizeof consumed, "%s.consumed", path) <= 0) return -1;\\n    return ps5io_rename(path, consumed);   /* 0 ok; overwrites stale .consumed */\\n}\\n\\nstatic const char *e2e_cmd_override_path(int argc, char **argv) {\\n    int i;\\n    for (i = 1; i + 1 < argc; ++i)\\n        if (!strcmp(argv[i], "--e2e-cmd")) return argv[i + 1];   /* first wins */\\n    return 0;   /* valueless --e2e-cmd stays legacy grammar (unknown flag) */\\n}\\n\\n/* Returns -1 and replaces the caller\\\'s argc/argv with the synthesized vector\\n * on success; otherwise returns the exit code main must return (2 absent /\\n * consume failure, 6 refused). */\\nstatic int e2e_cmd_entry(const char *path, const char *prog,\\n                         int *out_argc, char ***out_argv) {\\n    char *raw = 0;\\n    size_t rawlen = 0;\\n    ej_val *root = 0, *sr, *mods, *opts, *m;\\n    const char *verb, *sd_report = 0;\\n    char *joinbuf = 0;\\n    char *synargv[E2E_ARGV_MAX];\\n    char **heap_argv;\\n    int n = 0, mods_present, i;\\n    ej_cur c;\\n\\n    if (read_file(path, &raw, &rawlen)) {\\n        /* sec 1/6: nothing executes, exit identical to today\\\'s bare usage() */\\n        say("error bad_request e2e-cmd: file absent");\\n        usage();\\n        return EX_BAD;\\n    }\\n    if (rawlen >= 3 && (unsigned char)raw[0] == 0xef &&\\n        (unsigned char)raw[1] == 0xbb && (unsigned char)raw[2] == 0xbf) {\\n        free(raw);\\n        return e2e_refuse(0, "BOM refuses (sec 5)", EX_CONFIG);\\n    }\\n    if (rawlen > E2E_CMD_MAX) {\\n        free(raw);\\n        return e2e_refuse(0, "file > 4096 bytes refuses before parsing (sec 5)", EX_CONFIG);\\n    }\\n    if (!ej_utf8_ok(raw, rawlen)) {\\n        free(raw);\\n        return e2e_refuse(0, "not valid UTF-8", EX_CONFIG);\\n    }\\n    c.p = raw; c.i = 0; c.n = rawlen; c.err = "parse error";\\n    root = ej_value(&c, 0);\\n    if (!root) {\\n        char why[128];\\n        snprintf(why, sizeof why, "parse error at byte %lu: %s",\\n                 (unsigned long)c.i, c.err ? c.err : "invalid");\\n        free(raw);\\n        return e2e_refuse(0, why, EX_CONFIG);\\n    }\\n    ej_ws(&c);\\n    if (c.i != c.n || root->kind != EJ_OBJ) {\\n        ej_free(root); free(raw);\\n        return e2e_refuse(0, root->kind != EJ_OBJ ? "top level must be one object (sec 5)"\\n                                                  : "trailing bytes after top-level value",\\n                          EX_CONFIG);\\n    }\\n\\n    /* -- validation, mirroring host validate() order exactly --------------- */\\n    for (m = root->child; m; m = m->next)\\n        if (strcmp(m->key, "schema") && strcmp(m->key, "verb") &&\\n            strcmp(m->key, "state_root") && strcmp(m->key, "mods") &&\\n            strcmp(m->key, "options")) {\\n            char why[192];\\n            snprintf(why, sizeof why, "unknown top-level key \\\'%s\\\' (sec 6)", m->key);\\n            ej_free(root); free(raw);\\n            return e2e_refuse(0, why, EX_CONFIG);\\n        }\\n    m = ej_member(root, "schema");\\n    if (!m || m->kind != EJ_INT || m->i != 1) {\\n        ej_free(root); free(raw);\\n        return e2e_refuse(0, "schema must be exactly 1 (sec 2)", EX_CONFIG);\\n    }\\n    m = ej_member(root, "verb");\\n    if (!m || m->kind != EJ_STR) {\\n        ej_free(root); free(raw);\\n        return e2e_refuse(0, "verb must be string", EX_CONFIG);\\n    }\\n    verb = m->s;\\n    if (!strcmp(verb, "make-bundle") || !strcmp(verb, "download")) {\\n        char why[64];\\n        snprintf(why, sizeof why, "deny-listed verb \\\'%s\\\' (sec 2)", verb);\\n        ej_free(root); free(raw);\\n        return e2e_refuse(0, why, EX_CONFIG);\\n    }\\n    if (strcmp(verb, "records") && strcmp(verb, "register-mod") &&\\n        strcmp(verb, "prepare") && strcmp(verb, "prepare-archive") &&\\n        strcmp(verb, "install") && strcmp(verb, "simulate-runtime") &&\\n        strcmp(verb, "recover")) {\\n        char why[192];\\n        snprintf(why, sizeof why, "verb \\\'%s\\\' not in allow-list (sec 2)", verb);\\n        ej_free(root); free(raw);\\n        return e2e_refuse(0, why, EX_CONFIG);\\n    }\\n    sr = ej_member(root, "state_root");\\n    {\\n        int cnt = 0;\\n        ej_val *e;\\n        if (sr && sr->kind == EJ_OBJ)\\n            for (e = sr->child; e; e = e->next) cnt++;\\n        if (!sr || sr->kind != EJ_OBJ || cnt != 2 ||\\n            !ej_member(sr, "state") || !ej_member(sr, "roots")) {\\n            ej_free(root); free(raw);\\n            return e2e_refuse(0, "state_root must be an object with exactly state,roots (sec 2)",\\n                              EX_CONFIG);\\n        }\\n    }\\n    for (m = sr->child; m; m = m->next) {\\n        if (m->kind != EJ_STR || m->s[0] != \\\'/\\\' || strlen(m->s) > LINE_MAX_) {\\n            ej_free(root); free(raw);\\n            return e2e_refuse(0, "state_root.state/.roots must be absolute strings <= LINE_MAX_ (sec 2)",\\n                              EX_CONFIG);\\n        }\\n    }\\n    /* state dir validated: report-path logging now possible on later refusals */\\n    sd_report = ej_member(sr, "state")->s;\\n\\n    mods = ej_member(root, "mods");\\n    mods_present = mods != 0;\\n    if (mods_present) {\\n        ej_val *e;\\n        if (mods->kind != EJ_ARR) {\\n            ej_free(root); free(raw);\\n            return e2e_refuse(sd_report, "mods must be array (sec 2)", EX_CONFIG);\\n        }\\n        if (strcmp(verb, "prepare") && strcmp(verb, "install")) {\\n            char why[128];\\n            snprintf(why, sizeof why, "mods present with verb \\\'%s\\\' refuses (sec 2)", verb);\\n            ej_free(root); free(raw);\\n            return e2e_refuse(sd_report, why, EX_CONFIG);\\n        }\\n        {\\n            int cnt = 0;\\n            for (e = mods->child; e; e = e->next) cnt++;\\n            if (cnt > 16) {\\n                ej_free(root); free(raw);\\n                return e2e_refuse(sd_report, "mods > 16 entries (sec 3)", EX_CONFIG);\\n            }\\n        }\\n        for (e = mods->child; e; e = e->next)\\n            if (e->kind != EJ_STR || !e->s[0] || strlen(e->s) > LINE_MAX_) {\\n                ej_free(root); free(raw);\\n                return e2e_refuse(sd_report, "mods entries must be non-empty strings <= LINE_MAX_",\\n                                  EX_CONFIG);\\n            }\\n    }\\n    opts = ej_member(root, "options");\\n    if (opts) {\\n        ej_val *e;\\n        int cnt = 0;\\n        if (opts->kind != EJ_OBJ) {\\n            ej_free(root); free(raw);\\n            return e2e_refuse(sd_report, "options must be object (sec 2)", EX_CONFIG);\\n        }\\n        for (e = opts->child; e; e = e->next) cnt++;\\n        if (cnt > 16) {\\n            ej_free(root); free(raw);\\n            return e2e_refuse(sd_report, "options > 16 keys (sec 3)", EX_CONFIG);\\n        }\\n        for (e = opts->child; e; e = e->next) {\\n            if (!strcmp(e->key, "crash_at") || !strcmp(e->key, "cancel_at") ||\\n                !strcmp(e->key, "cancel_after") || !strcmp(e->key, "variant") ||\\n                !strcmp(e->key, "defaults") || !strcmp(e->key, "state_name") ||\\n                !strcmp(e->key, "code") || !strcmp(e->key, "detail")) {\\n                char why[160];\\n                snprintf(why, sizeof why, "deny-listed option \\\'%s\\\' (sec 2)", e->key);\\n                ej_free(root); free(raw);\\n                return e2e_refuse(sd_report, why, EX_CONFIG);\\n            }\\n            if (!e2e_opt_flag(e->key)) {\\n                char why[192];\\n                snprintf(why, sizeof why, "unknown options key \\\'%s\\\' (sec 6)", e->key);\\n                ej_free(root); free(raw);\\n                return e2e_refuse(sd_report, why, EX_CONFIG);\\n            }\\n            if (!e2e_verb_legal(e->key, verb)) {\\n                char why[224];\\n                snprintf(why, sizeof why, "options.%s illegal for verb \\\'%s\\\' (eager refusal, sec 3)",\\n                         e->key, verb);\\n                ej_free(root); free(raw);\\n                return e2e_refuse(sd_report, why, EX_CONFIG);\\n            }\\n            if (!strcmp(e->key, "no_auto_bundle")) {\\n                if (e->kind != EJ_BOOL || !e->b) {\\n                    ej_free(root); free(raw);\\n                    return e2e_refuse(sd_report, "no_auto_bundle accepts bool true only (sec 5)",\\n                                      EX_CONFIG);\\n                }\\n            } else {\\n                if (e->kind != EJ_STR || !e->s[0] || strlen(e->s) > LINE_MAX_) {\\n                    char why[192];\\n                    snprintf(why, sizeof why, "options.%s must be a non-empty string <= LINE_MAX_",\\n                             e->key);\\n                    ej_free(root); free(raw);\\n                    return e2e_refuse(sd_report, why, EX_CONFIG);\\n                }\\n            }\\n        }\\n    }\\n    if (mods_present && opts && ej_member(opts, "remove")) {\\n        ej_free(root); free(raw);\\n        return e2e_refuse(sd_report, "mods together with options.remove refuses (sec 3)",\\n                          EX_CONFIG);\\n    }\\n\\n    /* -- consume, then synthesize the argv (sec 3 mechanical mapping) ------ */\\n    if (e2e_consume(path)) {\\n        ej_free(root); free(raw);\\n        say("error e2e-cmd consume failed");\\n        usage();\\n        return EX_BAD;   /* sec 6 verbatim; verb NOT executed */\\n    }\\n\\n    synargv[n++] = (char *)prog;\\n    synargv[n++] = (char *)verb;\\n    synargv[n++] = "--state";\\n    synargv[n++] = (char *)ej_member(sr, "state")->s;\\n    synargv[n++] = "--roots";\\n    synargv[n++] = (char *)ej_member(sr, "roots")->s;\\n    if (mods_present) {\\n        ej_val *e;\\n        size_t total = 0;\\n        char *w;\\n        for (e = mods->child; e; e = e->next) total += strlen(e->s) + 1;\\n        joinbuf = (char *)malloc(total + 1);\\n        if (!joinbuf) { ej_free(root); free(raw); return EX_IO; }\\n        w = joinbuf;\\n        for (e = mods->child; e; e = e->next) {\\n            size_t l = strlen(e->s);\\n            if (w != joinbuf) *w++ = \\\',\\\';\\n            memcpy(w, e->s, l);\\n            w += l;\\n        }\\n        *w = 0;\\n        synargv[n++] = "--mods";\\n        synargv[n++] = joinbuf;\\n    }\\n    /* fixed key order (host synth_argv): title,id,bundle,archive,quarantine,\\n     * mod,remove,rt_state,choices, then value-less --no-auto-bundle */\\n    {\\n        static const char *order[] = { "title", "id", "bundle", "archive", "quarantine",\\n                                       "mod", "remove", "rt_state", "choices",\\n                                       "no_auto_bundle" };\\n        size_t k;\\n        for (k = 0; k < sizeof order / sizeof order[0]; ++k) {\\n            ej_val *e = opts ? ej_member(opts, order[k]) : 0;\\n            if (!e) continue;\\n            if (!strcmp(order[k], "no_auto_bundle")) {\\n                synargv[n++] = "--no-auto-bundle";\\n            } else {\\n                synargv[n++] = (char *)e2e_opt_flag(order[k]);\\n                synargv[n++] = e->s;\\n            }\\n        }\\n    }\\n    synargv[n] = 0;\\n    heap_argv = (char **)malloc((size_t)(n + 1) * sizeof *heap_argv);\\n    if (!heap_argv) { free(joinbuf); ej_free(root); free(raw); return EX_IO; }\\n    for (i = 0; i <= n; ++i) heap_argv[i] = synargv[i];\\n    /* raw/tree/joinbuf stay live: heap_argv points into them (bounded by the\\n     * 4096-byte file); freed at process exit per schema sec 5 */\\n    *out_argc = n;\\n    *out_argv = heap_argv;\\n    return -1;\\n}\\n\\nint main(int argc, char **argv) {\\n    const char *cmd = 0, *title = 0, *mods = 0, *remove = 0, *id = 0, *bundle = 0,\\n               *defaults = 0, *choices = 0, *statedir = 0, *rtstate = 0, *detail = 0,\\n               *crash = 0, *cancelat = 0;\\n    int code = 0, i;\\n    char vgroup[32][64], vopt[32][64], vbundle[32][LINE_MAX_];\\n    int nvar = 0;\\n    char state_dir[LINE_MAX_], roots[LINE_MAX_];\\n\\n    /* make-bundle owns its own flag set (fixture builder); bypass the store */\\n    if (argc > 1 && !strcmp(argv[1], "make-bundle")) {\\n        cmd_make_bundle(argc, argv);\\n        return EX_OK;\\n    }\\n\\n    /* worker-e2e-config-entry: schema sec 1 entry rule. argc<2 (bare dispatch:\\n     * the console loader forwards no arguments) consults the compile-time\\n     * fixed path; the explicit override slot `--e2e-cmd FILE` (first\\n     * occurrence, value required) runs the IDENTICAL validate/consume/\\n     * synthesize rule against FILE — this is the host harness\\\'s entry point\\n     * and a controller-side manual trigger. Without the flag and with\\n     * argc>=2 the entry is never consulted: legacy argv wins and a leftover\\n     * cmd file stays untouched (schema sec 6). */\\n    { const char *e2e_path = e2e_cmd_override_path(argc, argv);\\n      if (argc < 2 || e2e_path) {\\n          int nargc; char **nargv;\\n          int erc = e2e_cmd_entry(argc < 2 ? E2E_CMD_PATH : e2e_path,\\n                                  argc > 0 ? argv[0] : "modbridge-installer",\\n                                  &nargc, &nargv);\\n          if (erc >= 0) return erc;   /* refused/absent: verb NOT executed */\\n          argc = nargc; argv = nargv;\\n      } }\\n\\n    for (i = 1; i < argc; ++i) {\', 1),\n]'),
    ("g1-receipt-promoted-form-def",
     '".ps5.o")\')\nPATCHES_TAIL_HEAD = ',
     '".ps5.o")\')\nS_RECEIPT_NEWLINE = ("g1-promoted-receipt-newline",\n                     \'    (REC / "patches.json").write_text(json.dumps(receipt, indent=2))\',\n                     \'    (REC / "patches.json").write_text(json.dumps(receipt, indent=2) + "\\\\n")   # worker-e2e-composer-splice: trailing newline matches the promoted receipt form (worker-e2e-payload-build/g1_tuples_build.py writes json.dumps(pj, indent=2) + newline)\')\nS_G1_PATCHED_LABEL = ("g1-promoted-patched-label",\n                      \'               "patched": "composition/vendored/modinst_core.c",\',\n                      \'               "patched": "worker-e2e-payload-build/src/modinst_core.c (config-entry revision; = worker-e2e-config-entry owned/modinst_core.c)",   # g1 splice: promoted label; the local composition/vendored/modinst_core.c is byte-identical\')\nS_G1_BYTE_COUNTS = ("g1-promoted-g1-byte-counts",\n                    \'                        "old_bytes": len(old), "new_bytes": len(new)})\',\n                    \'                        "old_bytes": len(old), "new_bytes": len(new.encode("utf-8")) if pid.startswith("g1-cmd-entry-") else len(new)})  # g1 splice: promoted tail records UTF-8 BYTE lengths (consume new = 12862 B); inherited prefix keeps the delivered char-length convention\')\nPATCHES_TAIL_HEAD = '),
    ("g1-receipt-promoted-form-seq",
     '                          S_CONSOLE_TARGETS):',
     '                          S_CONSOLE_TARGETS,\n                          S_RECEIPT_NEWLINE,\n                          S_G1_PATCHED_LABEL,\n                          S_G1_BYTE_COUNTS):'),
]

# owned files vendored from the delivered worker-zip-to-mfb/owned set
OWNED_ALL = ["ps5_io_dir.c", "ps5_io_dir.h", "e2e_status.c", "e2e_status.h",
             "modinst_archive_prepare.c", "modinst_archive_prepare.h",
             "modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c",
             "modinst_zip_bridge.c", "modinst_zip_bridge.h"]
OWNED_PREBRIDGE = set(OWNED_ALL) - {"modinst_zip_bridge.c", "modinst_zip_bridge.h"}


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def die(msg):
    print("compose_console_bridge: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


def check_pins():
    """Chain-root integrity: the files I vendor must be the exact bytes the
    delivered receipts pinned."""
    e2e_sp = json.loads((E2E / "e2e-receipts/script-patches.json").read_text())
    if digest(ORIG_SCRIPT) != e2e_sp["vendored_script_sha256"]:
        die("compose_installer.py diverges from the e2e-final chain-root pin")
    zb_bp = json.loads((ZB / "e2e-receipts/bridge-script-patches.json").read_text())
    if digest(E2E_SCRIPT) != zb_bp["vendored_driver_sha256"]:
        die("compose_e2e.py diverges from the zip-to-mfb vendored-driver pin")
    return e2e_sp, zb_bp


def stage_owned():
    OWNED.mkdir(parents=True, exist_ok=True)
    zb_pins = json.loads((ZB / "e2e-receipts/owned-manifest.json").read_text())
    e2e_pins = json.loads((E2E / "e2e-receipts/owned-manifest.json").read_text())
    log = {}
    for name in OWNED_ALL:
        src = ZB / "owned" / name
        if not src.is_file():
            die("owned vendoring input missing: " + str(src))
        got = digest(src)
        want = zb_pins.get(name, {}).get("sha256")
        if want and got != want:
            die("owned %s diverges from zip-to-mfb owned-manifest pin" % name)
        if name in OWNED_PREBRIDGE:
            want2 = e2e_pins.get(name, {}).get("sha256")
            if want2 and got != want2:
                die("owned %s diverges from e2e-final owned-manifest pin" % name)
        shutil.copyfile(src, OWNED / name)
        log[name] = {"source": str(src), "sha256": digest(OWNED / name),
                     "zb_pin_verified": bool(want),
                     "e2e_pin_verified": name in OWNED_PREBRIDGE}
    (REC / "vendored-owned.json").write_text(json.dumps(log, indent=2, sort_keys=True))
    return log


def adapt_driver():
    src = E2E_SCRIPT.read_text()
    original_sha = digest(E2E_SCRIPT)
    log = []
    for pid, old, new in ZB_SPLICES:
        n = src.count(old)
        if n != 1:
            die("replayed splice %s anchor found %d times (want 1)" % (pid, n))
        src = src.replace(old, new)
        log.append({"id": pid, "origin": "worker-zip-to-mfb (verbatim replay)",
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest()})
    replay_sha = hashlib.sha256(src.encode()).hexdigest()
    zb_bp = json.loads((ZB / "e2e-receipts/bridge-script-patches.json").read_text())
    if replay_sha != zb_bp["adapted_driver_sha256"]:
        die("verbatim replay diverges: intermediate sha %s != delivered "
            "adapted_driver_sha256 %s" % (replay_sha, zb_bp["adapted_driver_sha256"]))
    log.append({"id": "REPLAY-PROOF",
                "intermediate_sha256": replay_sha,
                "matches_delivered": True})
    for pid, old, new in MY_SPLICES:
        n = src.count(old)
        if n != 1:
            die("console splice %s anchor found %d times (want 1)" % (pid, n))
        src = src.replace(old, new)
        log.append({"id": pid, "origin": "worker-zip-bridge-console-link (correction)",
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest()})
    DRIVER.write_text(src)
    (REC / "console-bridge-script-patches.json").write_text(json.dumps({
        "chain_root": str(ORIG_SCRIPT),
        "chain_root_sha256": digest(ORIG_SCRIPT),
        "vendored_driver": str(E2E_SCRIPT),
        "vendored_driver_sha256": original_sha,
        "replay_proof": {"intermediate_sha256": replay_sha,
                         "delivered_pin": zb_bp["adapted_driver_sha256"],
                         "byte_exact": True},
        "adapted_driver": str(DRIVER),
        "adapted_driver_sha256": digest(DRIVER),
        "splices": log,
        "corrections": [
            {"id": "zb-console-target-bridge",
             "finding": "delivered chain wires modinst_zip_bridge into the "
                        "host target list only, while zb-dispatch-bridge "
                        "makes the patched modinst_core.c reference "
                        "mi_bridge_* on both flavors — the delivered console "
                        "link would abort on undefined mi_bridge_run/"
                        "mi_bridge_wants_auto/mi_bridge_mod_arg",
             "fix": "add (\"modinst_zip_bridge\", HERE / \"owned/"
                    "modinst_zip_bridge.c\", core_flags) to the console "
                    "target list (same core_flags as the other owned TUs)"}]},
        indent=2, sort_keys=True))
    print("adapted console-bridge driver: %s (replay proof ok, from %s)"
          % (DRIVER, original_sha[:12]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--flavor", choices=["host", "console", "all"], default="console")
    args = ap.parse_args()
    REC.mkdir(parents=True, exist_ok=True)
    for p in (ORIG_SCRIPT, E2E_SCRIPT, ZB / "owned/modinst_zip_bridge.c"):
        if not p.is_file():
            die("vendoring input missing: " + str(p))
    check_pins()
    stage_owned()
    adapt_driver()
    env = os.environ.copy()
    env["MODBRIDGE_ROOT"] = str(ROOT)
    r = subprocess.run([sys.executable, str(DRIVER), "--flavor", args.flavor],
                       env=env, cwd=str(HERE))
    if r.returncode:
        die("inner composition failed (see composition/receipts/)")
    print("CONSOLE-BRIDGE COMPOSITION OK (flavor=%s)" % args.flavor)
    return 0


if __name__ == "__main__":
    sys.exit(main())

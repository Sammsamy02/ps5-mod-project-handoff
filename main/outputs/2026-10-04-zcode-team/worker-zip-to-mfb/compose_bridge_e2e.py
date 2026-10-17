#!/usr/bin/env python3
"""compose_e2e.py — worker-e2e-final E2E wiring driver (PS5 mod-manager).

Origin: worker-e2e-final, 2026-10-04 (ZCode team). Host-side only.

Extends the delivered installer composition (worker-installer-composition)
WITHOUT touching any original file, by the same vendored-copy + recorded
exact-anchor patch chain discipline:

  1. vendors worker-installer-composition/compose_installer.py, verifies its
     sha256 against the pinned receipt value, and splices recorded additions
     into a copy (composition_e2e_inner.py) that builds into THIS directory:
       - vendored download ABI headers (download/runtime.h,
         download/curl_download.h) from outputs/ps5-mod-bridge/native-host/download
         (receipt callable_archive_download_transport @ ba0c4f1);
       - host targets: owned modinst_download.c (stage-1 command through the
         real curl-owner ABI v1), owned mb_curl_stub_host.c (HOST TEST
         transport stub — never console), and the REAL archive/private/
         file_sink.c (the genuine sink the transport drives);
       - host core define -DMI_HAS_DOWNLOAD=1;
       - additional modinst.c patch tuples (gap-1 records wiring + gap-2
         dispatch), appended to the original PATCHES chain so receipts/
         patches.json proves every anchor and hash.
  2. vendors the original owned/ adapters into this dir's owned/ and applies
     recorded patches there (prepare-archive --mod surfacing; e2e_status
     download stage mapping). My new owned files sit beside them.
  3. runs the inner script (a subprocess) with MODBRIDGE_ROOT pinned, so all
     its receipts land in ./composition/receipts.

Rebuild + host regression:
    python3 compose_e2e.py --flavor all
    sh test/smoke_host.sh      # the vendored 26/26 suite (original checks)
    sh test/smoke_e2e.sh       # the new download/records suite

The console flavor link is unchanged (gap-1 wiring compiles on both flavors;
the download TU is host-linked only — REPORT.md records the exact controller
step to add the pinned native download closure for the console payload).
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
COMP_SRC = TEAM / "worker-installer-composition"
ORIG_SCRIPT = COMP_SRC / "compose_installer.py"
ORIG_OWNED = COMP_SRC / "owned"
DL = ROOT / "outputs/ps5-mod-bridge/native-host/download"
OWNED = HERE / "owned"
INNER = HERE / "composition_bridge_inner.py"   # worker-zip-to-mfb
REC = HERE / "e2e-receipts"


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def die(msg):
    print("compose_e2e: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------------------
# extra modinst.c patch tuples (appended to the inner script's PATCHES chain;
# anchors are text the ORIGINAL chain produces, so order is preserved)
# ---------------------------------------------------------------------------
C_INC_OLD = '#include "modinst_archive_prepare.h"'
C_INC_NEW = (C_INC_OLD + '\n#ifdef MI_HAS_DOWNLOAD\n'
             '#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */\n#endif')

C_FLAGS_OLD = '        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) ++i;   /* prepare-archive */'
C_FLAGS_NEW = C_FLAGS_OLD + '\n' + (
    '        else if (!strcmp(argv[i], "--mod") && i + 1 < argc) ++i;          /* prepare-archive records wiring (worker-e2e-final) */\n'
    '        else if (!strcmp(argv[i], "--job") && i + 1 < argc) ++i;          /* download (owned command parses it) */')

C_HOOKS_ANCHOR = '/* running is derived, never stored: the runtime\'s own status file is the only'
C_MARK_FN = '''/* worker-e2e-final gap-1: a successful stage-2a prepare-archive is a durable
 * downloaded->prepared transition in this records line store, exactly the
 * transition cmd_prepare performs (worker-7's model: register=downloaded,
 * prepare=prepared, install=installed; the load-list sha256 remains the only
 * install/recovery commit truth and is never touched here). Without --mod the
 * run is reporting-only. The quarantine is never erased on refusal. */
static void records_mark_archive_prepared(int argc, char **argv) {
    const char *mod = 0;
    int i;
    modrec *m;
    for (i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--mod") && argv[i + 1][0]) mod = argv[i + 1];
    if (!mod) return;
    m = find_mod(mod);
    if (!m) {
        say("error bad_request unknown mod '%s' for prepare-archive (quarantine retained)", mod);
        exit(EX_BAD);
    }
    if (!strcmp(m->state, "prepared")) { say("records unchanged mod=%s already prepared", mod); return; }
    if (strcmp(m->state, "downloaded")) {
        say("error bad_request mod '%s' is '%s'; only downloaded->prepared is defined (quarantine retained)", mod, m->state);
        exit(EX_BAD);
    }
    snprintf(m->state, sizeof m->state, "prepared");
    records_save();
    say("records prepared mod=%s title=%s", mod, m->title);
}
'''
C_REG_FN = '''#ifdef MI_HAS_DOWNLOAD
/* worker-e2e-final gap-2: durable registration for a finished download job.
 * The mod row (state downloaded, bundle = the content-addressed intake
 * archive) is the download command\'s LAST durable write — the record is the
 * commit point, mirroring the load-list-sha rule for installs. */
static void records_register_download_job(int argc, char **argv) {
    const char *job = 0;
    int i;
    char *buf, *line, *save = 0;
    size_t len;
    char id[128] = "", title[64] = "", intake[LINE_MAX_] = "", final[LINE_MAX_ * 2];
    modrec *m;
    for (i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--job") && argv[i + 1][0]) job = argv[i + 1];
    if (!job) { say("error bad_request download needs --job"); exit(EX_BAD); }
    if (read_file(job, &buf, &len)) { say("error bad_request cannot read job file"); exit(EX_BAD); }
    for (line = strtok_r(buf, "\\n", &save); line; line = strtok_r(0, "\\n", &save)) {
        char *tab = strchr(line, \'\\t\');
        if (!tab) continue;
        *tab = 0;
        if (!strcmp(line, "id") && !id[0]) snprintf(id, sizeof id, "%s", tab + 1);
        else if (!strcmp(line, "title") && !title[0]) snprintf(title, sizeof title, "%s", tab + 1);
        else if (!strcmp(line, "intake") && !intake[0]) snprintf(intake, sizeof intake, "%s", tab + 1);
    }
    free(buf);
    if (!id[0] || !title[0] || !intake[0])
        { say("error bad_request job file lacks id/title/intake"); exit(EX_BAD); }
    snprintf(final, sizeof final, "%s/%s.zip", intake, id);
    m = find_mod(id);
    if (m) {
        if (strcmp(m->state, "downloaded") || strcmp(m->bundle, final))
            { say("error bad_request mod \'%s\' already registered differently", id); exit(EX_BAD); }
        say("records unchanged mod=%s state=downloaded", id);
        return;
    }
    if (g_nmods >= MAX_MODS) die("too many mods");
    m = &g_mods[g_nmods++];
    memset(m, 0, sizeof *m);
    snprintf(m->id, sizeof m->id, "%s", id);
    snprintf(m->title, sizeof m->title, "%s", title);
    snprintf(m->state, sizeof m->state, "downloaded");
    snprintf(m->bundle, sizeof m->bundle, "%s", final);
    records_save();
    say("registered mod=%s title=%s state=downloaded bundle=%s", id, title, final);
}
#endif
'''
C_HOOKS_NEW = C_MARK_FN + C_REG_FN + C_HOOKS_ANCHOR

C_PREPARC_OLD = '            return mi_cmd_prepare_archive(argc, argv);'
C_PREPARC_NEW = '''            { int e2e_pr = mi_cmd_prepare_archive(argc, argv);
              /* worker-e2e-final gap-1: durable downloaded->prepared records
               * transition on success (records_mark_archive_prepared); the
               * end line is emitted here, not skipped. */
              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);
              e2e_status_end(e2e_pr);
              return e2e_pr; }'''

C_DOWNLOAD_OLD = '        } else if (!strcmp(cmd, "simulate-runtime")) {'
C_DOWNLOAD_NEW = '''        } else if (!strcmp(cmd, "download")) {
#ifdef MI_HAS_DOWNLOAD
            /* worker-e2e-final gap-2: E2E stage 1 through the exclusive
             * curl-owner transport ABI v1 (mb_curl_runtime_start/download/
             * close); explicit job config only; the registration row is the
             * command\'s last durable write. */
            { int e2e_dr = mi_cmd_download(argc, argv, g_state_dir);
              if (e2e_dr == 0) records_register_download_job(argc, argv);
              e2e_status_end(e2e_dr);
              return e2e_dr; }
#else
            say("error bad_request download transport not linked into this flavor");
            return EX_BAD;
#endif
        } else if (!strcmp(cmd, "simulate-runtime")) {'''

C_USAGE_OLD = '    say("  recover");'
C_USAGE_NEW = ('    say("  download --job JOB.tsv   (stage 1: transport -> intake/<id>.zip + records downloaded)");\n'
               '    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");\n'
               '    say("  recover");')

EXTRA_PATCHES = [
    ("e2e-inc-download", C_INC_OLD, C_INC_NEW, 1),
    ("e2e-flags-mod-job", C_FLAGS_OLD, C_FLAGS_NEW, 1),
    ("e2e-records-hooks", C_HOOKS_ANCHOR, C_HOOKS_NEW, 1),
    ("e2e-dispatch-prepare-archive-wired", C_PREPARC_OLD, C_PREPARC_NEW, 1),
    ("e2e-dispatch-download", C_DOWNLOAD_OLD, C_DOWNLOAD_NEW, 1),
    ("e2e-usage", C_USAGE_OLD, C_USAGE_NEW, 1),
]

# ---- worker-zip-to-mfb (F-R1 bridge) patch tuples, applied after the e2e chain
ZB_INC_OLD = """#ifdef MI_HAS_DOWNLOAD
#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */
#endif"""
ZB_INC_NEW = ZB_INC_OLD + '\n#include "modinst_zip_bridge.h"   /* worker-zip-to-mfb: F-R1 quarantine->mfb bridge */'

ZB_FLAG_OLD = "        else if (!strcmp(argv[i], \"--job\") && i + 1 < argc) ++i;          /* download (owned command parses it) */"
ZB_FLAG_NEW = ZB_FLAG_OLD + '\n        else if (!strcmp(argv[i], \"--no-auto-bundle\")) ;         /* value-less opt-out (worker-zip-to-mfb bridge parses it) */'

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
ZB_USAGE_NEW = ZB_USAGE_OLD + '\n    say("          [--no-auto-bundle]  (F-R1 bridge: with --mod, default composes bundles/<id>.mfb from the quarantine)");'

EXTRA_PATCHES += [
    ("zb-inc-bridge", ZB_INC_OLD, ZB_INC_NEW, 1),
    ("zb-flag-no-auto-bundle", ZB_FLAG_OLD, ZB_FLAG_NEW, 1),
    ("zb-dispatch-bridge", ZB_DISP_OLD, ZB_DISP_NEW, 1),
    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),
]


# ---------------------------------------------------------------------------
# recorded splices into the vendored compose_installer.py source text
# ---------------------------------------------------------------------------
S_DL_DEF = ("e2e-script-dl-root",
            'ARCHIVE = ROOT / "outputs/ps5-mod-bridge/native-host/archive"',
            'ARCHIVE = ROOT / "outputs/ps5-mod-bridge/native-host/archive"\n'
            'DL = ROOT / "outputs/ps5-mod-bridge/native-host/download"')
S_VENDOR = ("e2e-script-vendor-abi-headers",
            '    (ARCHIVE / "link_layout.py", "archive/link_layout.py"),\n'
            '    (ARCHIVE / "dependency_build.py", "archive/dependency_build.py"),\n'
            ']',
            '    (ARCHIVE / "link_layout.py", "archive/link_layout.py"),\n'
            '    (ARCHIVE / "dependency_build.py", "archive/dependency_build.py"),\n'
            '    (DL / "runtime.h", "download/runtime.h"),          # exclusive curl-owner ABI v1\n'
            '    (DL / "curl_download.h", "download/curl_download.h"),\n'
            ']')
S_MKDIR = ("e2e-script-vendor-dirs",
           '    (VEND / "archive/private").mkdir(parents=True)',
           '    (VEND / "archive/private").mkdir(parents=True)\n'
           '    (VEND / "download").mkdir(parents=True, exist_ok=True)')
S_COREFLAGS = ("e2e-script-host-define",
               '    core_flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-DPS5IO_HOST=1",',
               '    core_flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-DPS5IO_HOST=1",\n'
               '                  "-DMI_HAS_DOWNLOAD=1",')
S_TARGETS = ("e2e-script-host-targets",
             '        ("posix_compat", VEND / "archive/private/posix_compat.c", closure_flags),\n'
             '    ]\n'
             '    commands = []\n'
             '    objects = []\n'
             '    for name, src, flags in targets:\n'
             '        obj = out / (name + ".o")',
             '        ("posix_compat", VEND / "archive/private/posix_compat.c", closure_flags),\n'
             '        ("file_sink", VEND / "archive/private/file_sink.c", closure_flags),\n'
             '        ("modinst_download", HERE / "owned/modinst_download.c",\n'
             '         core_flags + ["-I" + str(VEND / "download")]),\n'
             '        ("mb_curl_stub_host", HERE / "owned/mb_curl_stub_host.c",\n'
             '         core_flags + ["-I" + str(VEND / "download")]),\n'
             '        ("modinst_zip_bridge", HERE / "owned/modinst_zip_bridge.c", core_flags),\n'
             '    ]\n'
             '    commands = []\n'
             '    objects = []\n'
             '    for name, src, flags in targets:\n'
             '        obj = out / (name + ".o")')
PATCHES_TAIL_HEAD = "     '\\n    e2e_status_end(EX_OK);\\n    return EX_OK;\\n}\\n', 1),\n]"

# ---------------------------------------------------------------------------
# recorded patches to the vendored owned/ adapters
# ---------------------------------------------------------------------------
OWNED_VERBATIM = ["ps5_io_dir.c", "ps5_io_dir.h", "e2e_status.h",
                  "modinst_archive_prepare.h"]
OWNED_PREPARE_PATCHES = [
    ("owned-prepare-mod-decl",
     '    const char *archive = 0, *quarantine = 0, *title = 0;',
     '    const char *archive = 0, *quarantine = 0, *title = 0, *mod = 0;'),
    ("owned-prepare-mod-flag",
     '        else if (!strcmp(argv[i], "--title") && i + 1 < argc) title = argv[++i];',
     '        else if (!strcmp(argv[i], "--title") && i + 1 < argc) title = argv[++i];\n'
     '        else if (!strcmp(argv[i], "--mod") && i + 1 < argc) mod = argv[++i]; /* worker-e2e-final gap-1 */'),
    ("owned-prepare-mod-report-fmt",
     '"inventory=%s token=%s journal_retained=%u\\n",',
     '"inventory=%s token=%s journal_retained=%u mod=%s\\n",'),
    ("owned-prepare-mod-report-arg",
     '               sha_hex, tok_hex, res.journal_retained);',
     '               sha_hex, tok_hex, res.journal_retained, mod ? mod : "-");'),
]
OWNED_E2ESTATUS_PATCHES = [
    ("owned-e2estatus-download-stage",
     '    if (!strcmp(cmd, "register-mod")) return "stage-1-download";',
     '    if (!strcmp(cmd, "register-mod")) return "stage-1-download";\n'
     '    if (!strcmp(cmd, "download")) return "stage-1-download";   /* worker-e2e-final */'),
]


def apply_chain(what, text, patches, log):
    for pid, old, new in patches:
        n = text.count(old)
        if n != 1:
            die("%s: patch %s anchor found %d times (want 1) — vendored input "
                "differs from the reviewed anchor" % (what, pid, n))
        text = text.replace(old, new)
        log.append({"id": pid,
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest(),
                    "old_bytes": len(old), "new_bytes": len(new)})
    return text


def stage_owned():
    OWNED.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name in OWNED_VERBATIM:
        src = ORIG_OWNED / name
        if not src.is_file():
            die("owned vendoring input missing: " + str(src))
        shutil.copyfile(src, OWNED / name)
        manifest[name] = {"source": str(src), "sha256": digest(OWNED / name),
                          "patches": []}
    for name, patches in (("e2e_status.c", OWNED_E2ESTATUS_PATCHES),
                          ("modinst_archive_prepare.c", OWNED_PREPARE_PATCHES)):
        src = ORIG_OWNED / name
        if not src.is_file():
            die("owned vendoring input missing: " + str(src))
        text = src.read_text()
        before = hashlib.sha256(text.encode()).hexdigest()
        flog = []
        text = apply_chain("owned/" + name, text, patches, flog)
        (OWNED / name).write_text(text)
        manifest[name] = {"source": str(src), "source_sha256": before,
                          "sha256": digest(OWNED / name), "patches": flog}
    for name in ("modinst_download.c", "modinst_download.h", "mb_curl_stub_host.c",
                 "modinst_zip_bridge.c", "modinst_zip_bridge.h"):
        src = HERE / "owned" / name
        manifest[name] = {"source": "authored by worker-e2e-final (this dir)",
                          "sha256": digest(src), "patches": []}
    (REC / "owned-manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True))
    return manifest


def adapt_script():
    src = ORIG_SCRIPT.read_text()
    original_sha = digest(ORIG_SCRIPT)
    log = []
    for pid, old, new in (S_DL_DEF, S_MKDIR, S_VENDOR, S_COREFLAGS, S_TARGETS):
        n = src.count(old)
        if n != 1:
            die("script splice %s anchor found %d times (want 1)" % (pid, n))
        src = src.replace(old, new)
        log.append({"id": pid,
                    "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                    "new_sha256": hashlib.sha256(new.encode()).hexdigest()})
    # append the extra modinst.c patch tuples to the inner PATCHES chain
    n = src.count(PATCHES_TAIL_HEAD)
    if n != 1:
        die("script PATCHES tail anchor found %d times (want 1)" % n)
    head = PATCHES_TAIL_HEAD[:-1]  # drop the closing bracket
    extra = "".join("    %r,\n" % (p,) for p in EXTRA_PATCHES)
    src = src.replace(PATCHES_TAIL_HEAD, head + "\n" + extra + "]")
    log.append({"id": "e2e-script-patches-append",
                "appended": [p[0] for p in EXTRA_PATCHES]})
    INNER.write_text(src)
    (REC / "script-patches.json").write_text(json.dumps({
        "vendored_script": str(ORIG_SCRIPT),
        "vendored_script_sha256": original_sha,
        "adapted_script": str(INNER),
        "adapted_script_sha256": digest(INNER),
        "splices": log}, indent=2))
    print("adapted composition script: %s (from %s)" % (INNER, original_sha[:12]))


def main():
    ap = argparse.ArgumentParser(description="worker-e2e-final E2E wiring driver")
    ap.add_argument("--flavor", choices=["host", "console", "all"], default="all")
    args = ap.parse_args()
    REC.mkdir(parents=True, exist_ok=True)
    for p in (ORIG_SCRIPT, DL / "runtime.h", DL / "curl_download.h"):
        if not p.is_file():
            die("vendoring input missing: " + str(p))
    stage_owned()
    adapt_script()
    env = os.environ.copy()
    env["MODBRIDGE_ROOT"] = str(ROOT)
    r = subprocess.run([sys.executable, str(INNER), "--flavor", args.flavor],
                       env=env, cwd=str(HERE))
    if r.returncode:
        die("inner composition failed (see composition/receipts/)")
    print("E2E COMPOSITION OK (flavor=%s)" % args.flavor)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""compose_installer.py — controller-runnable installer composition build.

Origin: worker-installer-composition, 2026-10-04 (ZCode team, PS5 mod-manager).

Combines the delivered pieces into ONE native link configuration and emits an
owned payload ELF candidate plus a host-testable binary:

  vendored inputs (read-only originals, copies land in composition/vendored):
    - worker-7-installer: modinst.c + modforge_* (installer core, records,
      transactional install/rollback/recovery)
    - worker-installer-ps5io: ps5_io.{c,h} (pinned sceKernel file I/O),
      ps5_prepare_bridge.{c,h} (prepare -> archive intake ABI v1)
    - ps5-mod-bridge/native-host/archive: intake.c + private/posix_compat.c
      (real libarchive intake), link_layout.py, dependency_build.py
  owned adapters (this worker, composition inputs like any other source):
    - owned/ps5_io_dir.{c,h}        directory-enumeration gap binding
    - owned/e2e_status.{c,h}        bounded JSONL status interface (E2E stages)
    - owned/modinst_archive_prepare.{c,h}  `prepare-archive` stage-2a command

  console flavor: prospero-clang; the pinned 3.8.7 static closure
  (libarchive/liblzma/zlib, dependency_build.py selection, link order
  libarchive->liblzma->zlib, + the pinned native libmbedcrypto) as link
  inputs exactly as archive/verify.py assembles them; caller-local dynamic
  layout script from archive/link_layout.py; -Wl,--no-undefined,-z,now.
  Nothing is ever uploaded or executed on a target.

  host flavor (PS5IO_HOST=1): same adapted core over ps5_io's host mapping +
  the host closure, so semantics are testable (test/smoke_host.sh).

  import verification: every dynamic import of the emitted ELF must be
  defined by a pinned SDK target lib (llvm-nm provider check, the
  worker-installer-ps5io build_console.sh pattern). Any unresolvable symbol
  aborts the build loudly with the exact name.

The modinst.c adaptation is a recorded patch chain: exact unique anchors,
each verified for expected occurrence count; the patched copy is written to
composition/vendored/modinst_core.c and receipts/patches.json records every
replacement with hashes. The original file is never touched.
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

# ---- pinned roots (same bindings as archive/verify.py + build_console.sh) ----
SDK = ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
LLVM_BIN = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
RESOURCE = ROOT / ".superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21"
PROFILE = ROOT / ".superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1"
HOST_MBED = ROOT / "outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/host-a4768e3a50d32394616f27f8"
NATIVE_MBED = ROOT / "outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/native-038ce86825317d4d74716405"
MBED_SRC = "mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5"

W7 = ROOT / "outputs/2026-10-04-zcode-team/worker-7-installer"
PS5IO = ROOT / "outputs/2026-10-04-zcode-team/worker-installer-ps5io"
ARCHIVE = ROOT / "outputs/ps5-mod-bridge/native-host/archive"
DL = ROOT / "outputs/ps5-mod-bridge/native-host/download"

COMP = HERE / "composition"
VEND = COMP / "vendored"
BUILD = COMP / "build"
REC = COMP / "receipts"


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def run(argv, log_name, env=None):
    argv = [str(a) for a in argv]
    r = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    (REC / (log_name + ".stdout")).write_bytes(r.stdout)
    (REC / (log_name + ".stderr")).write_bytes(r.stderr)
    return {"argv": argv, "exit": r.returncode}


def die(msg):
    print("compose_installer: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------- vendoring
VENDOR_FILES = [
    # (source, relative destination)
    (W7 / "src/modinst.c", "modinst.c"),
    (PS5IO / "src/ps5_io.c", "ps5_io.c"),
    (PS5IO / "src/ps5_io.h", "ps5_io.h"),
    (PS5IO / "src/ps5_prepare_bridge.c", "ps5_prepare_bridge.c"),
    (PS5IO / "src/ps5_prepare_bridge.h", "ps5_prepare_bridge.h"),
    (PS5IO / "vendored/intake.h", "archive/intake.h"),
    (ARCHIVE / "intake.c", "archive/intake.c"),
    (ARCHIVE / "intake.h", "archive/intake.h.orig"),
    (ARCHIVE / "private/posix_compat.c", "archive/private/posix_compat.c"),
    (ARCHIVE / "private/posix_compat.h", "archive/private/posix_compat.h"),
    (ARCHIVE / "private/fd_io.c", "archive/private/fd_io.c"),
    (ARCHIVE / "private/fd_io.h", "archive/private/fd_io.h"),
    (ARCHIVE / "private/file_sink.c", "archive/private/file_sink.c"),
    (ARCHIVE / "private/file_sink.h", "archive/private/file_sink.h"),
    (ARCHIVE / "groups.c", "archive/groups.c"),
    (ARCHIVE / "groups.h", "archive/groups.h"),
    (ARCHIVE / "link_layout.py", "archive/link_layout.py"),
    (ARCHIVE / "dependency_build.py", "archive/dependency_build.py"),
    (DL / "runtime.h", "download/runtime.h"),          # exclusive curl-owner ABI v1
    (DL / "curl_download.h", "download/curl_download.h"),
]
for name in ("modforge_activation.c", "modforge_activation.h", "modforge_bundle.h",
             "modforge_catalog.h", "modforge_compose.h", "modforge_loadlist.h",
             "modforge_prx.h", "modforge_service.h"):
    VENDOR_FILES.append((W7 / "vendored" / name, name))


def vendor():
    if VEND.exists():
        shutil.rmtree(VEND)
    (VEND / "archive/private").mkdir(parents=True)
    (VEND / "download").mkdir(parents=True, exist_ok=True)
    manifest = {}
    for src, rel in VENDOR_FILES:
        if not src.is_file():
            die("vendoring input missing: " + str(src))
        dst = VEND / rel
        shutil.copyfile(src, dst)
        manifest[rel] = {"source": str(src), "sha256": digest(dst)}
    # provenance: the two vendored intake.h copies must be the same ABI bytes
    if manifest["archive/intake.h"]["sha256"] != manifest["archive/intake.h.orig"]["sha256"]:
        die("worker-installer-ps5io vendored intake.h differs from the archive "
            "closure intake.h — refusing to compose mismatched ABI headers")
    # provenance: ps5io worker's modforge copies must match worker-7's
    for name in ("modforge_activation.c", "modforge_activation.h", "modforge_bundle.h",
                 "modforge_compose.h"):
        theirs = PS5IO / "vendored" / name
        if theirs.is_file() and digest(theirs) != manifest[name]["sha256"]:
            die("worker-installer-ps5io vendored " + name + " differs from worker-7's")
    (REC / "vendor-manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True))
    return manifest


# ------------------------------------------------------------- patch chain
# (id, exact old text, new text, expected occurrence count)
def R(line_old, line_new):
    return (line_old, line_new.replace("ps5io_rename(", "ps5io_rename("))

PATCHES = [
    ("inc-ps5io",
     '#include "../vendored/modforge_compose.h"   /* includes modforge_bundle.h */',
     '#include "modforge_compose.h"   /* includes modforge_bundle.h */\n'
     '#include "ps5_io.h"\n'
     '#include "ps5_io_dir.h"\n'
     '#include "e2e_status.h"\n'
     '#include "modinst_archive_prepare.h"', 1),
    ("drop-write-all",
     '''static int write_all_fd(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, p + done, n - done);
        if (w <= 0) return -1;
        done += (size_t)w;
    }
    return 0;
}''',
     '/* composition: write-all is ps5io_write_full (pinned sceKernelWrite loop) */', 1),
    ("fsync-dir",
     '''static int fsync_dir(const char *path) {
    int fd = open(path, O_RDONLY);
    int rc;
    if (fd < 0) return -1;
    rc = fsync(fd);
    close(fd);
    return rc;
}''',
     '''static int fsync_dir(const char *path) {
    return ps5io_fsync_dir(path); /* composition: pinned sceKernelFsync */
}''', 1),
    ("atomic-write",
     '''static int atomic_write(const char *path, const void *buf, size_t n) {
    char tmp[LINE_MAX_ + 8];
    int fd, rc = 0;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (write_all_fd(fd, buf, n) || fsync(fd)) rc = -1;
    if (close(fd) && !rc) rc = -1;
    if (!rc && rename(tmp, path)) rc = -1;
    if (rc) unlink(tmp);''',
     '''static int atomic_write(const char *path, const void *buf, size_t n) {
    char tmp[LINE_MAX_ + 8];
    int fd, rc = 0;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fd = ps5io_open(tmp, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);
    if (fd < 0) return -1;
    if (ps5io_write_full(fd, buf, n) || ps5io_fsync(fd)) rc = -1;
    if (ps5io_close(fd) && !rc) rc = -1;
    if (!rc && ps5io_rename(tmp, path)) rc = -1;
    if (rc) ps5io_unlink(tmp);''', 1),
    ("rm-rf",
     '''static int rm_rf(const char *path) {
    struct stat st;
    if (lstat(path, &st)) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode)) return unlink(path);
    {
        DIR *d = opendir(path);
        struct dirent *e;
        int rc = 0;
        if (!d) return -1;
        while ((e = readdir(d))) {
            char child[LINE_MAX_ * 2];
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            joinpath(child, sizeof child, path, e->d_name);
            if (rm_rf(child)) rc = -1;
        }
        closedir(d);
        if (rmdir(path)) rc = -1;
        return rc;
    }
}''',
     '''static int rm_rf(const char *path) {
    /* composition: enumeration/removal through the owned ps5_io_dir binding
     * (pinned dirent + rmdir providers; sceKernelDopen/Dread/Dclose are not
     * exports of the pinned libs — see ps5_io_dir.h receipts). */
    return ps5io_dir_rm_rf(path);
}''', 1),
    ("mkdirs",
     '''static int mkdirs(const char *path) {
    char buf[LINE_MAX_ * 2];
    size_t i;
    snprintf(buf, sizeof buf, "%s", path);
    for (i = 1; buf[i]; ++i) {
        if (buf[i] == '/') {
            buf[i] = 0;
            if (mkdir(buf, 0755) && errno != EEXIST) return -1;
            buf[i] = '/';
        }
    }
    if (mkdir(buf, 0755) && errno != EEXIST) return -1;
    return 0;
}''',
     '''static int mkdirs(const char *path) {
    return ps5io_mkdir_p(path, 0755); /* composition: pinned sceKernelMkdir */
}''', 1),
    ("file-exists",
     '''static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}''',
     '''static int file_exists(const char *path) {
    ps5io_stat st;
    return ps5io_stat_path(path, &st) == 0 && st.exists;
}''', 1),
    ("read-file",
     '''static int read_file(const char *path, char **out, size_t *len) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return -1; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return -1; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return -1; }
    fclose(f);
    buf[n] = 0;
    *out = buf;
    *len = (size_t)n;
    return 0;
}''',
     '''static int read_file(const char *path, char **out, size_t *len) {
    ps5io_stat st;
    char *buf;
    size_t got = 0;
    int fd = ps5io_open(path, PS5IO_RDONLY, 0);
    if (fd < 0) return -1;
    if (ps5io_stat_path(path, &st) || st.is_dir == 1) { ps5io_close(fd); return -1; }
    buf = (char *)malloc((size_t)st.size + 1);
    if (!buf) { ps5io_close(fd); return -1; }
    if (st.size && ps5io_read_full(fd, buf, (size_t)st.size, &got)) {
        free(buf); ps5io_close(fd); return -1;
    }
    ps5io_close(fd);
    buf[got] = 0;
    *out = buf;
    *len = got;
    return 0;
}''', 1),
    ("bio-read-at",
     '''static int bio_read_at(void *vc, size_t bundle, uint64_t off, void *buf, size_t n) {
    bio_ctx *c = (bio_ctx *)vc;
    size_t got = 0;
    while (got < n) {
        ssize_t r = pread(c->fds[bundle], (char *)buf + got, n - got, (off_t)(off + got));
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}''',
     '''static int bio_read_at(void *vc, size_t bundle, uint64_t off, void *buf, size_t n) {
    bio_ctx *c = (bio_ctx *)vc;
    /* composition: exact pread over the pinned sceKernel set (lseek+read) */
    return ps5io_pread_exact(c->fds[bundle], off, buf, n);
}''', 1),
    ("bio-make-dirs",
     '''static int bio_make_dirs(void *vc, const char *rel_dir) {
    char path[LINE_MAX_ * 2];
    bio_ctx *c = (bio_ctx *)vc;
    joinpath(path, sizeof path, c->root, rel_dir);
    return mkdirs(path);
}''',
     '''static int bio_make_dirs(void *vc, const char *rel_dir) {
    char path[LINE_MAX_ * 2];
    bio_ctx *c = (bio_ctx *)vc;
    joinpath(path, sizeof path, c->root, rel_dir);
    return ps5io_mkdir_p(path, 0755) == 0 ? 0 : -1;
}''', 1),
    ("bio-open-write",
     '    joinpath(path, sizeof path, c->root, rel);\n    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);',
     '    joinpath(path, sizeof path, c->root, rel);\n    return ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);', 1),
    ("bio-write",
     '    return write_all_fd(h, buf, n);',
     '    return ps5io_write_full(h, buf, n) == 0 ? 0 : -1;', 1),
    ("bio-close",
     '    return close(h);',
     '    return ps5io_close(h);', 1),
    ("bio-remove",
     '    joinpath(path, sizeof path, ((bio_ctx *)vc)->root, rel);\n    return unlink(path);',
     '    joinpath(path, sizeof path, ((bio_ctx *)vc)->root, rel);\n    return ps5io_unlink(path);', 1),
    ("plan-load-open",
     '''        struct stat st;
        resolve_mod(ids[i], p->bundles[i], sizeof p->bundles[i], extra_choices);
        bc.fds[i] = open(p->bundles[i], O_RDONLY);
        if (bc.fds[i] < 0 || fstat(bc.fds[i], &st))''',
     '''        ps5io_stat st;
        resolve_mod(ids[i], p->bundles[i], sizeof p->bundles[i], extra_choices);
        bc.fds[i] = ps5io_open(p->bundles[i], PS5IO_RDONLY, 0);
        if (bc.fds[i] < 0 || ps5io_stat_path(p->bundles[i], &st))''', 1),
    ("plan-load-size",
     'if (mfx_load(&io, (size_t)i, (uint64_t)st.st_size, &p->mods[i], &ar, &err)) {',
     'if (mfx_load(&io, (size_t)i, st.size, &p->mods[i], &ar, &err)) {', 1),
    ("install-open",
     '''            struct stat st;
            bc.fds[i] = open(plan.bundles[i], O_RDONLY);
            if (bc.fds[i] < 0 || fstat(bc.fds[i], &st)) die("cannot reopen bundle");
            sizes[i] = (uint64_t)st.st_size;''',
     '''            ps5io_stat st;
            bc.fds[i] = ps5io_open(plan.bundles[i], PS5IO_RDONLY, 0);
            if (bc.fds[i] < 0 || ps5io_stat_path(plan.bundles[i], &st)) die("cannot reopen bundle");
            sizes[i] = st.size;''', 1),
    ("install-close",
     'for (i = 0; i < n; ++i) close(bc.fds[i]);',
     'for (i = 0; i < n; ++i) ps5io_close(bc.fds[i]);', 1),
    ("empty-root-open",
     '    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);',
     '    fd = ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0644);', 2),
    ("empty-root-marker",
     'if (fd < 0 || write_all_fd(fd, marker_text, sizeof marker_text - 1) || close(fd))',
     'if (fd < 0 || ps5io_write_full(fd, marker_text, sizeof marker_text - 1) || ps5io_close(fd))', 1),
    ("empty-root-loadlist",
     'if (fd < 0 || write_all_fd(fd, buf, (size_t)n) || fsync(fd) || close(fd))',
     'if (fd < 0 || ps5io_write_full(fd, buf, (size_t)n) || ps5io_fsync(fd) || ps5io_close(fd))', 1),
    ("journal-delete-unlink",
     '    unlink(path);',
     '    ps5io_unlink(path);', 1),
    ("rename-finish-swap",
     'if (rename(next, root)) die("recovery: cannot finish swap: %s", strerror(errno));',
     'if (ps5io_rename(next, root)) die("recovery: cannot finish swap: %s", strerror(errno));', 1),
    ("rename-rollback-swap",
     'if (rename(prev, root)) die("recovery: cannot roll back swap: %s", strerror(errno));',
     'if (ps5io_rename(prev, root)) die("recovery: cannot roll back swap: %s", strerror(errno));', 1),
    ("rename-aside",
     'if (rename(root, prev)) die("cannot move current root aside: %s", strerror(errno));',
     'if (ps5io_rename(root, prev)) die("cannot move current root aside: %s", strerror(errno));', 1),
    ("rename-cancel",
     'if (rename(prev, root)) die("cannot roll back first rename: %s", strerror(errno));',
     'if (ps5io_rename(prev, root)) die("cannot roll back first rename: %s", strerror(errno));', 1),
    ("rename-swap-in",
     'if (rename(next, root)) die("cannot swap new root in: %s", strerror(errno));',
     'if (ps5io_rename(next, root)) die("cannot swap new root in: %s", strerror(errno));', 1),
    ("simrt-clock",
     '''    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);''',
     '    if (ps5io_now_ms(&now)) now = 0; /* composition: sceKernelClockGettime */', 1),
    ("simrt-ts-decl",
     '    uint64_t now = 0;\n    struct timespec ts;',
     '    uint64_t now = 0;', 1),
    ("recover-all",
     '''static void recover_all(void) {
    char jdir[LINE_MAX_];
    DIR *d;
    struct dirent *e;
    snprintf(jdir, sizeof jdir, "%s/journal", g_state_dir);
    mkdirs(jdir);
    d = opendir(jdir);
    if (!d) return;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 5 && !strcmp(e->d_name + n - 5, ".json")) {
            char title[64];
            n -= 5;
            if (n >= sizeof title) continue;
            memcpy(title, e->d_name, n);
            title[n] = 0;
            recover_title(title);
        }
    }
    closedir(d);
}''',
     '''static void recover_all(void) {
    char jdir[LINE_MAX_];
    snprintf(jdir, sizeof jdir, "%s/journal", g_state_dir);
    mkdirs(jdir);
    /* composition: enumeration through the owned ps5_io_dir binding (pinned
     * dirent providers; the pure sceKernel D-op set is absent — see
     * ps5_io_dir.h). */
    ps5io_dir_each_json(jdir, recover_title);
}''', 1),
    ("e2e-set-dir",
     '    g_state_dir = state_dir;',
     '    g_state_dir = state_dir;\n    e2e_status_set_dir(g_state_dir);', 1),
    ("e2e-begin",
     '    /* every open recovers interrupted transactions first (mfs_open semantics) */',
     '    e2e_status_begin(cmd, title);\n\n'
     '    /* every open recovers interrupted transactions first (mfs_open semantics) */', 1),
    ("flag-prepare-archive",
     '''        else if (!strcmp(argv[i], "--rt-state") && i + 1 < argc) rtstate = argv[++i];''',
     '''        else if (!strcmp(argv[i], "--archive") && i + 1 < argc) ++i;      /* prepare-archive (owned command parses it) */
        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) ++i;   /* prepare-archive */
        else if (!strcmp(argv[i], "--rt-state") && i + 1 < argc) rtstate = argv[++i];''', 1),
    ("dispatch-prepare-archive",
     '        } else if (!strcmp(cmd, "simulate-runtime")) {',
     '        } else if (!strcmp(cmd, "prepare-archive")) {\n'
     '            /* composition: E2E stage 2a — archive -> quarantine via the\n'
     '             * pinned 3.8.7 intake closure through the vendored bridge. */\n'
     '            return mi_cmd_prepare_archive(argc, argv);\n'
     '        } else if (!strcmp(cmd, "simulate-runtime")) {', 1),
    ("e2e-end",
     '\n    return EX_OK;\n}\n',
     '\n    e2e_status_end(EX_OK);\n    return EX_OK;\n}\n', 1),

    ('e2e-inc-download', '#include "modinst_archive_prepare.h"', '#include "modinst_archive_prepare.h"\n#ifdef MI_HAS_DOWNLOAD\n#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */\n#endif', 1),
    ('e2e-flags-mod-job', '        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) ++i;   /* prepare-archive */', '        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) ++i;   /* prepare-archive */\n        else if (!strcmp(argv[i], "--mod") && i + 1 < argc) ++i;          /* prepare-archive records wiring (worker-e2e-final) */\n        else if (!strcmp(argv[i], "--job") && i + 1 < argc) ++i;          /* download (owned command parses it) */', 1),
    ('e2e-records-hooks', "/* running is derived, never stored: the runtime's own status file is the only", '/* worker-e2e-final gap-1: a successful stage-2a prepare-archive is a durable\n * downloaded->prepared transition in this records line store, exactly the\n * transition cmd_prepare performs (worker-7\'s model: register=downloaded,\n * prepare=prepared, install=installed; the load-list sha256 remains the only\n * install/recovery commit truth and is never touched here). Without --mod the\n * run is reporting-only. The quarantine is never erased on refusal. */\nstatic void records_mark_archive_prepared(int argc, char **argv) {\n    const char *mod = 0;\n    int i;\n    modrec *m;\n    for (i = 1; i + 1 < argc; ++i)\n        if (!strcmp(argv[i], "--mod") && argv[i + 1][0]) mod = argv[i + 1];\n    if (!mod) return;\n    m = find_mod(mod);\n    if (!m) {\n        say("error bad_request unknown mod \'%s\' for prepare-archive (quarantine retained)", mod);\n        exit(EX_BAD);\n    }\n    if (!strcmp(m->state, "prepared")) { say("records unchanged mod=%s already prepared", mod); return; }\n    if (strcmp(m->state, "downloaded")) {\n        say("error bad_request mod \'%s\' is \'%s\'; only downloaded->prepared is defined (quarantine retained)", mod, m->state);\n        exit(EX_BAD);\n    }\n    snprintf(m->state, sizeof m->state, "prepared");\n    records_save();\n    say("records prepared mod=%s title=%s", mod, m->title);\n}\n#ifdef MI_HAS_DOWNLOAD\n/* worker-e2e-final gap-2: durable registration for a finished download job.\n * The mod row (state downloaded, bundle = the content-addressed intake\n * archive) is the download command\'s LAST durable write — the record is the\n * commit point, mirroring the load-list-sha rule for installs. */\nstatic void records_register_download_job(int argc, char **argv) {\n    const char *job = 0;\n    int i;\n    char *buf, *line, *save = 0;\n    size_t len;\n    char id[128] = "", title[64] = "", intake[LINE_MAX_] = "", final[LINE_MAX_ * 2];\n    modrec *m;\n    for (i = 1; i + 1 < argc; ++i)\n        if (!strcmp(argv[i], "--job") && argv[i + 1][0]) job = argv[i + 1];\n    if (!job) { say("error bad_request download needs --job"); exit(EX_BAD); }\n    if (read_file(job, &buf, &len)) { say("error bad_request cannot read job file"); exit(EX_BAD); }\n    for (line = strtok_r(buf, "\\n", &save); line; line = strtok_r(0, "\\n", &save)) {\n        char *tab = strchr(line, \'\\t\');\n        if (!tab) continue;\n        *tab = 0;\n        if (!strcmp(line, "id") && !id[0]) snprintf(id, sizeof id, "%s", tab + 1);\n        else if (!strcmp(line, "title") && !title[0]) snprintf(title, sizeof title, "%s", tab + 1);\n        else if (!strcmp(line, "intake") && !intake[0]) snprintf(intake, sizeof intake, "%s", tab + 1);\n    }\n    free(buf);\n    if (!id[0] || !title[0] || !intake[0])\n        { say("error bad_request job file lacks id/title/intake"); exit(EX_BAD); }\n    snprintf(final, sizeof final, "%s/%s.zip", intake, id);\n    m = find_mod(id);\n    if (m) {\n        if (strcmp(m->state, "downloaded") || strcmp(m->bundle, final))\n            { say("error bad_request mod \'%s\' already registered differently", id); exit(EX_BAD); }\n        say("records unchanged mod=%s state=downloaded", id);\n        return;\n    }\n    if (g_nmods >= MAX_MODS) die("too many mods");\n    m = &g_mods[g_nmods++];\n    memset(m, 0, sizeof *m);\n    snprintf(m->id, sizeof m->id, "%s", id);\n    snprintf(m->title, sizeof m->title, "%s", title);\n    snprintf(m->state, sizeof m->state, "downloaded");\n    snprintf(m->bundle, sizeof m->bundle, "%s", final);\n    records_save();\n    say("registered mod=%s title=%s state=downloaded bundle=%s", id, title, final);\n}\n#endif\n/* running is derived, never stored: the runtime\'s own status file is the only', 1),
    ('e2e-dispatch-prepare-archive-wired', '            return mi_cmd_prepare_archive(argc, argv);', '            { int e2e_pr = mi_cmd_prepare_archive(argc, argv);\n              /* worker-e2e-final gap-1: durable downloaded->prepared records\n               * transition on success (records_mark_archive_prepared); the\n               * end line is emitted here, not skipped. */\n              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);\n              e2e_status_end(e2e_pr);\n              return e2e_pr; }', 1),
    ('e2e-dispatch-download', '        } else if (!strcmp(cmd, "simulate-runtime")) {', '        } else if (!strcmp(cmd, "download")) {\n#ifdef MI_HAS_DOWNLOAD\n            /* worker-e2e-final gap-2: E2E stage 1 through the exclusive\n             * curl-owner transport ABI v1 (mb_curl_runtime_start/download/\n             * close); explicit job config only; the registration row is the\n             * command\'s last durable write. */\n            { int e2e_dr = mi_cmd_download(argc, argv, g_state_dir);\n              if (e2e_dr == 0) records_register_download_job(argc, argv);\n              e2e_status_end(e2e_dr);\n              return e2e_dr; }\n#else\n            say("error bad_request download transport not linked into this flavor");\n            return EX_BAD;\n#endif\n        } else if (!strcmp(cmd, "simulate-runtime")) {', 1),
    ('e2e-usage', '    say("  recover");', '    say("  download --job JOB.tsv   (stage 1: transport -> intake/<id>.zip + records downloaded)");\n    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");\n    say("  recover");', 1),
    ('zb-inc-bridge', '#ifdef MI_HAS_DOWNLOAD\n#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */\n#endif', '#ifdef MI_HAS_DOWNLOAD\n#include "modinst_download.h"   /* worker-e2e-final: stage-1 owner transport */\n#endif\n#include "modinst_zip_bridge.h"   /* worker-zip-to-mfb: F-R1 quarantine->mfb bridge */', 1),
    ('zb-flag-no-auto-bundle', '        else if (!strcmp(argv[i], "--job") && i + 1 < argc) ++i;          /* download (owned command parses it) */', '        else if (!strcmp(argv[i], "--job") && i + 1 < argc) ++i;          /* download (owned command parses it) */\n        else if (!strcmp(argv[i], "--no-auto-bundle")) ;         /* value-less opt-out (worker-zip-to-mfb bridge parses it) */', 1),
    ('zb-dispatch-bridge', '              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);', '              if (e2e_pr == 0 && mi_bridge_wants_auto(argc, argv)) {\n                  /* worker-zip-to-mfb (F-R1): compose a kind-1 mfb from the\n                   * quarantine BEFORE the records transition; on success the\n                   * record\'s bundle becomes the bridge output so install\n                   * resolves a real modforge bundle. A refusal keeps the\n                   * record at downloaded and the quarantine retained. */\n                  char bridge_bundle[LINE_MAX_ * 2];\n                  e2e_pr = mi_bridge_run(argc, argv, g_state_dir,\n                                         bridge_bundle, sizeof bridge_bundle);\n                  if (e2e_pr == 0) {\n                      modrec *bm = find_mod(mi_bridge_mod_arg(argc, argv));\n                      if (bm) snprintf(bm->bundle, sizeof bm->bundle, "%s", bridge_bundle);\n                  }\n              }\n              if (e2e_pr == 0) records_mark_archive_prepared(argc, argv);', 1),
    ('zb-usage-bridge', '    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");', '    say("  prepare-archive --title T --archive F --quarantine D [--mod ID]   (stage 2a)");\n    say("          [--no-auto-bundle]  (F-R1 bridge: with --mod, default composes bundles/<id>.mfb from the quarantine)");', 1),
    ('uf-order-commit-a', '    maybe_crash("prevdel");\n    journal_delete(title);\n\n    /* ---- commit records ---- */', '    maybe_crash("prevdel");\n\n    /* ---- commit records ----\n     * worker-uninstall-fault-expansion (uf-order-commit): records commit\n     * BEFORE journal_delete. Old order had a crash window leaving durable\n     * journal-delete + stale records: no journal means "nothing in flight",\n     * so recovery never repaired records that claimed installed while files\n     * were already gone. New order leaves records durable while the journal\n     * is still present, which recover_title repairs idempotently via the\n     * staged load-list hash. */', 1),
    ('uf-order-commit-b', '        records_save();\n        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n    }\n}', '        records_save();\n        say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n        maybe_crash("pre-journal-del"); /* uf probe: records durable, journal still present */\n    }\n    journal_delete(title); /* uf-order-commit: journal released only after records are durable */\n}', 1),
    ('uf-recover-forward', '                rm_rf(prev);\n                journal_delete(title);\n                fsync_dir(g_roots);\n                records_commit_from_root(title, &j);\n                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);', '                rm_rf(prev);\n                records_commit_from_root(title, &j); /* uf-order-commit: records first */\n                journal_delete(title);\n                fsync_dir(g_roots);\n                say("recover title=%s finished swap forward (new set active, generation %ld)", title, j.generation);', 1),
    ('uf-recover-commit', '                rm_rf(next);\n                rm_rf(prev);\n                journal_delete(title);\n                fsync_dir(g_roots);\n                records_commit_from_root(title, &j);\n                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);', '                rm_rf(next);\n                rm_rf(prev);\n                records_commit_from_root(title, &j); /* uf-order-commit: records first */\n                journal_delete(title);\n                fsync_dir(g_roots);\n                say("recover title=%s committed (new set already active, generation %ld)", title, j.generation);', 1),
    ('r2-exit-shim', '#include "../vendored/modforge_activation.h"', '#include "../vendored/modforge_activation.h"\n\n/* worker-f-r2-fix (F-R2, worker-e2e-rehearsal REPORT.md section 3): route\n * every controlled exit() of the composed core through the status end path,\n * so a refused command (e.g. install rejecting a non-bundle archive) cannot\n * leave a dangling begin line in e2e-status.jsonl. Exit codes and all say()\n * output are unchanged; _exit(86) crash probes and real signals still leave\n * no end line, so "missing end == command died" honesty is preserved. This\n * sits after the LAST include so no header text is seen through the macro. */\ntypedef void (*r2_real_exit_fn)(int);\nstatic r2_real_exit_fn r2_real_exit = exit;\n#define exit r2_e2e_exit\nstatic void r2_e2e_exit(int code) {\n    e2e_status_end(code);\n    r2_real_exit(code);\n}', 1),
    ('r2-install-status-ok', '    say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);', '    say("installed title=%s generation=%ld mods=%s loadlist=%s", title, newgen, mods_out, sha);\n    if (mods_out[0]) e2e_status_event("stage-3-install", "ok", mods_out);   /* worker-f-r2-fix (F-R2): terminal install status event; installs only — the remove commit shares this line with an empty mods list */', 1),
    ('g1-cmd-entry-parse', 'int main(int argc, char **argv) {', '/* ---- worker-e2e-config-entry: config-file-driven entry (G1 fallback) --------\n * Implements worker-e2e-console-dry/E2E-CMD-SCHEMA.md sec 1-6, mirroring the\n * host harness worker-e2e-staging-final/host-test/host_cmd_entry_test.py\n * line-for-line (same bounded JSON subset, same allow/deny lists, same\n * synthesized argv). Controller deltas receipted in\n * outputs/2026-10-04-zcode-team/worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md:\n *   - EX_CONFIG (6): file present but refused - distinct from EX_BAD (2),\n *     which stays "no file at the slot" (sec 1: identical to today\'s bare\n *     usage()) and "consume rename failed" (sec 6);\n *   - on refusals where state_root.state itself validated, exactly ONE\n *     {"event":"status","stage":"e2e-cmd","outcome":"refused",...} line to the\n *     existing e2e-status.jsonl report path (no begin/end pair, no store\n *     mutation, verb never executed). Parse-level failures with no valid\n *     state dir log one stdout line only.\n * Exit paths here RETURN (never the exit() macro - F-R2 routes that through\n * e2e_status_end and a refusal must not leave a dangling end line).\n * Imports: read_file/say/malloc/free/snprintf/ps5io_rename and the e2e_status\n * API only - all already in the 116-symbol census; census delta target ZERO.\n * Parse/validation memory (one <=4096-byte file) is intentionally live until\n * process exit: the synthesized argv points into it for the dispatch. */\n\n#define E2E_CMD_PATH "/data/modbridge/intake/e2e-cmd.json"\n#define E2E_CMD_MAX 4096   /* file size cap, refused before parsing (sec 5) */\n#define E2E_ARGV_MAX 64    /* synthesized vector cap (sec 5) */\n#define EX_CONFIG 6        /* e2e-cmd present but refused (distinct code) */\n\ntypedef enum { EJ_STR, EJ_INT, EJ_BOOL, EJ_OBJ, EJ_ARR } ej_kind;\n\ntypedef struct ej_val ej_val;\nstruct ej_val {\n    ej_kind kind;\n    char *s;        /* EJ_STR */\n    long i;         /* EJ_INT */\n    int b;          /* EJ_BOOL */\n    char *key;      /* member name when linked under an EJ_OBJ */\n    ej_val *child;  /* EJ_OBJ/EJ_ARR: first member/element */\n    ej_val *next;   /* next sibling */\n};\n\nstatic void ej_free(ej_val *v) {\n    while (v) {\n        ej_val *n = v->next;\n        ej_free(v->child);\n        free(v->s);\n        free(v->key);\n        free(v);\n        v = n;\n    }\n}\n\ntypedef struct {\n    const char *p;\n    size_t i, n;\n    const char *err;   /* one-line refusal reason, set at the failure site */\n} ej_cur;\n\n/* Strict UTF-8 over the whole file (host harness: raw.decode("utf-8")). */\nstatic int ej_utf8_ok(const char *p, size_t n) {\n    size_t i = 0;\n    while (i < n) {\n        unsigned char b = (unsigned char)p[i];\n        if (b < 0x80) { i++; continue; }\n        if (b >= 0xc2 && b <= 0xdf) {\n            if (i + 1 >= n || (unsigned char)p[i+1] >> 6 != 2) return 0;\n            i += 2;\n        } else if (b == 0xe0) {\n            if (i + 2 >= n) return 0;\n            if ((unsigned char)p[i+1] < 0xa0 || (unsigned char)p[i+1] > 0xbf ||\n                (unsigned char)p[i+2] >> 6 != 2) return 0;\n            i += 3;\n        } else if ((b >= 0xe1 && b <= 0xec) || b == 0xee || b == 0xef) {\n            if (i + 2 >= n || (unsigned char)p[i+1] >> 6 != 2 ||\n                (unsigned char)p[i+2] >> 6 != 2) return 0;\n            i += 3;\n        } else if (b == 0xed) {\n            if (i + 2 >= n || (unsigned char)p[i+1] < 0x80 || (unsigned char)p[i+1] > 0x9f ||\n                (unsigned char)p[i+2] >> 6 != 2) return 0;\n            i += 3;\n        } else if (b == 0xf0) {\n            if (i + 3 >= n) return 0;\n            if ((unsigned char)p[i+1] < 0x90 || (unsigned char)p[i+1] > 0xbf ||\n                (unsigned char)p[i+2] >> 6 != 2 || (unsigned char)p[i+3] >> 6 != 2) return 0;\n            i += 4;\n        } else if (b >= 0xf1 && b <= 0xf3) {\n            if (i + 3 >= n || (unsigned char)p[i+1] >> 6 != 2 ||\n                (unsigned char)p[i+2] >> 6 != 2 || (unsigned char)p[i+3] >> 6 != 2) return 0;\n            i += 4;\n        } else if (b == 0xf4) {\n            if (i + 3 >= n || (unsigned char)p[i+1] < 0x80 || (unsigned char)p[i+1] > 0x8f ||\n                (unsigned char)p[i+2] >> 6 != 2 || (unsigned char)p[i+3] >> 6 != 2) return 0;\n            i += 4;\n        } else return 0;   /* 0x80-0xbf continuation, 0xc0/0xc1 overlong, 0xf5-0xff */\n    }\n    return 1;\n}\n\nstatic void ej_ws(ej_cur *c) {\n    while (c->i < c->n && (c->p[c->i] == \' \' || c->p[c->i] == \'\\t\' ||\n                           c->p[c->i] == \'\\r\' || c->p[c->i] == \'\\n\')) c->i++;\n}\n\nstatic int ej_peek(ej_cur *c) {\n    return c->i < c->n ? (unsigned char)c->p[c->i] : -1;\n}\n\n/* Sec 5 string: escapes limited to \\" \\\\ \\/ ; \\uXXXX refuses; control <0x20\n * refuses. Storage is one E2E_CMD_MAX+1 block - a string can never exceed\n * the file it was parsed from. */\nstatic ej_val *ej_string(ej_cur *c) {\n    ej_val *v;\n    char *s;\n    if (ej_peek(c) != \'"\') { c->err = "expected \'\\"\'"; return 0; }\n    c->i++;\n    s = (char *)malloc(E2E_CMD_MAX + 1);\n    if (!s) { c->err = "out of memory"; return 0; }\n    {\n        size_t len = 0;\n        for (;;) {\n            unsigned char ch;\n            if (c->i >= c->n) { free(s); c->err = "unterminated string"; return 0; }\n            ch = (unsigned char)c->p[c->i];\n            if (ch == \'"\') {\n                c->i++;\n                s[len] = 0;\n                break;\n            }\n            if (ch < 0x20) { free(s); c->err = "control char in string"; return 0; }\n            if (ch == \'\\\\\') {\n                unsigned char e;\n                c->i++;\n                if (c->i >= c->n) { free(s); c->err = "bad escape"; return 0; }\n                e = (unsigned char)c->p[c->i];\n                if (e == \'u\') { free(s); c->err = "\\\\uXXXX escape refuses (sec 5)"; return 0; }\n                if (e != \'"\' && e != \'\\\\\' && e != \'/\') {\n                    free(s); c->err = "bad escape (only \\\\\\" \\\\\\\\ \\\\/ legal)"; return 0;\n                }\n                ch = e;\n                c->i++;\n            } else c->i++;\n            s[len++] = (char)ch;\n        }\n    }\n    v = (ej_val *)calloc(1, sizeof *v);\n    if (!v) { free(s); c->err = "out of memory"; return 0; }\n    v->kind = EJ_STR;\n    v->s = s;\n    return v;\n}\n\nstatic ej_val *ej_new(ej_kind k) {\n    ej_val *v = (ej_val *)calloc(1, sizeof *v);\n    if (!v) return 0;\n    v->kind = k;\n    return v;\n}\n\nstatic ej_val *ej_value(ej_cur *c, int depth);\n\n/* Sec 5: one top-level object; object depth <= 2; arrays only as a top-level\n * member value (mods); only true (false/null refuse); plain ints only;\n * duplicate keys refuse. Mirrors rd_parse() byte-for-byte. */\nstatic ej_val *ej_value(ej_cur *c, int depth) {\n    int ch;\n    ej_ws(c);\n    ch = ej_peek(c);\n    if (ch < 0) { c->err = "unexpected end of input"; return 0; }\n    if (ch == \'"\') return ej_string(c);\n    if (ch == \'{\') {\n        ej_val *obj;\n        if (depth >= 2) { c->err = "object depth > 2 refuses (sec 5)"; return 0; }\n        c->i++;\n        obj = ej_new(EJ_OBJ);\n        if (!obj) { c->err = "out of memory"; return 0; }\n        ej_ws(c);\n        if (ej_peek(c) == \'}\') { c->i++; return obj; }\n        for (;;) {\n            ej_val *kn, *v, *tail;\n            char *k;\n            ej_ws(c);\n            kn = ej_string(c);          /* key: parse a string node, steal its s */\n            if (!kn) return 0;\n            k = kn->s;\n            free(kn);\n            for (tail = obj->child; tail; tail = tail->next)\n                if (!strcmp(tail->key, k)) {\n                    free(k);\n                    c->err = "duplicate key";\n                    return 0;\n                }\n            ej_ws(c);\n            if (ej_peek(c) != \':\') {\n                free(k); c->err = "expected \':\'"; return 0;\n            }\n            c->i++;\n            v = ej_value(c, depth + 1);\n            if (!v) { free(k); return 0; }\n            v->key = k;                 /* the value node IS the member */\n            if (obj->child) {\n                for (tail = obj->child; tail->next; tail = tail->next) ;\n                tail->next = v;\n            } else obj->child = v;\n            ej_ws(c);\n            if (ej_peek(c) == \',\') { c->i++; continue; }\n            if (ej_peek(c) == \'}\') { c->i++; return obj; }\n            c->err = "expected \',\' or \'}\'";\n            return 0;\n        }\n    }\n    if (ch == \'[\') {\n        ej_val *arr;\n        if (depth != 1) { c->err = "array only legal as a top-level member: mods (sec 5)"; return 0; }\n        c->i++;\n        arr = ej_new(EJ_ARR);\n        if (!arr) { c->err = "out of memory"; return 0; }\n        ej_ws(c);\n        if (ej_peek(c) == \']\') { c->i++; return arr; }\n        for (;;) {\n            ej_val *el, *tail;\n            el = ej_value(c, depth + 1);\n            if (!el) return 0;\n            if (arr->child) {\n                for (tail = arr->child; tail->next; tail = tail->next) ;\n                tail->next = el;\n            } else arr->child = el;\n            ej_ws(c);\n            if (ej_peek(c) == \',\') { c->i++; continue; }\n            if (ej_peek(c) == \']\') { c->i++; return arr; }\n            c->err = "expected \',\' or \']\'";\n            return 0;\n        }\n    }\n    if (c->n - c->i >= 4 && !memcmp(c->p + c->i, "true", 4)) {\n        ej_val *v = ej_new(EJ_BOOL);\n        if (!v) { c->err = "out of memory"; return 0; }\n        v->b = 1;\n        c->i += 4;\n        return v;\n    }\n    if (c->n - c->i >= 5 && !memcmp(c->p + c->i, "false", 5)) {\n        c->err = "only bool true is allowed (sec 5)";\n        return 0;\n    }\n    if (c->n - c->i >= 4 && !memcmp(c->p + c->i, "null", 4)) {\n        c->err = "only bool true is allowed (sec 5)";\n        return 0;\n    }\n    if (ch == \'-\' || (ch >= \'0\' && ch <= \'9\')) {\n        size_t j = c->i + (ch == \'-\' ? 1 : 0);\n        long v;\n        if (j >= c->n || c->p[j] < \'0\' || c->p[j] > \'9\') { c->err = "bad int"; return 0; }\n        while (j < c->n && c->p[j] >= \'0\' && c->p[j] <= \'9\') j++;\n        /* bounded sign-aware decimal fold, saturating (NOT strtol: plain\n         * strtol is absent from the console 116-import census and would\n         * break the delta-zero target). Only schema==1 is ever accepted, so\n         * a saturated value refuses with the same "schema must be exactly 1"\n         * line - observably identical to the harness int() for all inputs.\n         * Cap 0x0CCCCCCCCCCCCCCC keeps v*10+9 inside long with no UB. */\n        {\n            size_t k = c->i + (ch == \'-\' ? 1 : 0);\n            v = 0;\n            while (k < j && v < 0x0CCCCCCCCCCCCCCCL) v = v * 10 + (c->p[k++] - \'0\');\n            if (ch == \'-\') v = -v;\n        }\n        c->i = j;\n        ej_ws(c);\n        if (c->i < c->n) {\n            int a = ej_peek(c);\n            if ((a >= \'0\' && a <= \'9\') || a == \'.\' || a == \'e\' || a == \'E\' ||\n                a == \'+\' || a == \'-\') { c->err = "only plain ints (sec 5)"; return 0; }\n        }\n        {\n            ej_val *n = ej_new(EJ_INT);\n            if (!n) { c->err = "out of memory"; return 0; }\n            n->i = v;\n            return n;\n        }\n    }\n    c->err = "unexpected byte";\n    return 0;\n}\n\nstatic ej_val *ej_member(ej_val *obj, const char *key) {\n    ej_val *m;\n    if (!obj || obj->kind != EJ_OBJ) return 0;\n    for (m = obj->child; m; m = m->next)\n        if (!strcmp(m->key, key)) return m;\n    return 0;\n}\n\n/* One-line refusal record on the existing report path (controller delta) plus\n * the schema sec 6 stdout line and usage(). state_dir may be 0 (no validated\n * report path yet). Returns `code` for the caller to return from main. */\nint main(int argc, char **argv) {', 1),
    ('g1-cmd-entry-allowlist', 'int main(int argc, char **argv) {', 'static int e2e_refuse(const char *state_dir, const char *reason, int code) {\n    if (state_dir) {\n        e2e_status_set_dir(state_dir);\n        e2e_status_event("e2e-cmd", "refused", reason);\n    }\n    say("error bad_request e2e-cmd: %s", reason);\n    usage();\n    return code;\n}\n\nstatic int e2e_verb_legal(const char *key, const char *verb) {\n    /* schema sec 3 mapping: options key -> legal verbs (eager refusal) */\n    if (!strcmp(key, "title"))\n        return !strcmp(verb, "register-mod") || !strcmp(verb, "prepare") ||\n               !strcmp(verb, "prepare-archive") || !strcmp(verb, "install") ||\n               !strcmp(verb, "simulate-runtime");\n    if (!strcmp(key, "id") || !strcmp(key, "bundle")) return !strcmp(verb, "register-mod");\n    if (!strcmp(key, "archive") || !strcmp(key, "quarantine") ||\n        !strcmp(key, "mod") || !strcmp(key, "no_auto_bundle"))\n        return !strcmp(verb, "prepare-archive");\n    if (!strcmp(key, "remove")) return !strcmp(verb, "install");\n    if (!strcmp(key, "rt_state")) return !strcmp(verb, "simulate-runtime");\n    if (!strcmp(key, "choices")) return !strcmp(verb, "prepare") || !strcmp(verb, "install");\n    return 0;\n}\n\nstatic const char *e2e_opt_flag(const char *key) {\n    if (!strcmp(key, "title")) return "--title";\n    if (!strcmp(key, "id")) return "--id";\n    if (!strcmp(key, "bundle")) return "--bundle";\n    if (!strcmp(key, "archive")) return "--archive";\n    if (!strcmp(key, "quarantine")) return "--quarantine";\n    if (!strcmp(key, "mod")) return "--mod";\n    if (!strcmp(key, "remove")) return "--remove";\n    if (!strcmp(key, "rt_state")) return "--rt-state";\n    if (!strcmp(key, "choices")) return "--choices";\n    if (!strcmp(key, "no_auto_bundle")) return "--no-auto-bundle";\n    return 0;\n}\n\nint main(int argc, char **argv) {', 1),
    ('g1-cmd-entry-consume', 'int main(int argc, char **argv) {\n    const char *cmd = 0, *title = 0, *mods = 0, *remove = 0, *id = 0, *bundle = 0,\n               *defaults = 0, *choices = 0, *statedir = 0, *rtstate = 0, *detail = 0,\n               *crash = 0, *cancelat = 0;\n    int code = 0, i;\n    char vgroup[32][64], vopt[32][64], vbundle[32][LINE_MAX_];\n    int nvar = 0;\n    char state_dir[LINE_MAX_], roots[LINE_MAX_];\n\n    /* make-bundle owns its own flag set (fixture builder); bypass the store */\n    if (argc > 1 && !strcmp(argv[1], "make-bundle")) {\n        cmd_make_bundle(argc, argv);\n        return EX_OK;\n    }\n\n    for (i = 1; i < argc; ++i) {', '/* schema sec 6: consume BEFORE e2e_status_begin / verb execution */\nstatic int e2e_consume(const char *path) {\n    char consumed[LINE_MAX_ + 16];\n    if (snprintf(consumed, sizeof consumed, "%s.consumed", path) <= 0) return -1;\n    return ps5io_rename(path, consumed);   /* 0 ok; overwrites stale .consumed */\n}\n\nstatic const char *e2e_cmd_override_path(int argc, char **argv) {\n    int i;\n    for (i = 1; i + 1 < argc; ++i)\n        if (!strcmp(argv[i], "--e2e-cmd")) return argv[i + 1];   /* first wins */\n    return 0;   /* valueless --e2e-cmd stays legacy grammar (unknown flag) */\n}\n\n/* Returns -1 and replaces the caller\'s argc/argv with the synthesized vector\n * on success; otherwise returns the exit code main must return (2 absent /\n * consume failure, 6 refused). */\nstatic int e2e_cmd_entry(const char *path, const char *prog,\n                         int *out_argc, char ***out_argv) {\n    char *raw = 0;\n    size_t rawlen = 0;\n    ej_val *root = 0, *sr, *mods, *opts, *m;\n    const char *verb, *sd_report = 0;\n    char *joinbuf = 0;\n    char *synargv[E2E_ARGV_MAX];\n    char **heap_argv;\n    int n = 0, mods_present, i;\n    ej_cur c;\n\n    if (read_file(path, &raw, &rawlen)) {\n        /* sec 1/6: nothing executes, exit identical to today\'s bare usage() */\n        say("error bad_request e2e-cmd: file absent");\n        usage();\n        return EX_BAD;\n    }\n    if (rawlen >= 3 && (unsigned char)raw[0] == 0xef &&\n        (unsigned char)raw[1] == 0xbb && (unsigned char)raw[2] == 0xbf) {\n        free(raw);\n        return e2e_refuse(0, "BOM refuses (sec 5)", EX_CONFIG);\n    }\n    if (rawlen > E2E_CMD_MAX) {\n        free(raw);\n        return e2e_refuse(0, "file > 4096 bytes refuses before parsing (sec 5)", EX_CONFIG);\n    }\n    if (!ej_utf8_ok(raw, rawlen)) {\n        free(raw);\n        return e2e_refuse(0, "not valid UTF-8", EX_CONFIG);\n    }\n    c.p = raw; c.i = 0; c.n = rawlen; c.err = "parse error";\n    root = ej_value(&c, 0);\n    if (!root) {\n        char why[128];\n        snprintf(why, sizeof why, "parse error at byte %lu: %s",\n                 (unsigned long)c.i, c.err ? c.err : "invalid");\n        free(raw);\n        return e2e_refuse(0, why, EX_CONFIG);\n    }\n    ej_ws(&c);\n    if (c.i != c.n || root->kind != EJ_OBJ) {\n        ej_free(root); free(raw);\n        return e2e_refuse(0, root->kind != EJ_OBJ ? "top level must be one object (sec 5)"\n                                                  : "trailing bytes after top-level value",\n                          EX_CONFIG);\n    }\n\n    /* -- validation, mirroring host validate() order exactly --------------- */\n    for (m = root->child; m; m = m->next)\n        if (strcmp(m->key, "schema") && strcmp(m->key, "verb") &&\n            strcmp(m->key, "state_root") && strcmp(m->key, "mods") &&\n            strcmp(m->key, "options")) {\n            char why[192];\n            snprintf(why, sizeof why, "unknown top-level key \'%s\' (sec 6)", m->key);\n            ej_free(root); free(raw);\n            return e2e_refuse(0, why, EX_CONFIG);\n        }\n    m = ej_member(root, "schema");\n    if (!m || m->kind != EJ_INT || m->i != 1) {\n        ej_free(root); free(raw);\n        return e2e_refuse(0, "schema must be exactly 1 (sec 2)", EX_CONFIG);\n    }\n    m = ej_member(root, "verb");\n    if (!m || m->kind != EJ_STR) {\n        ej_free(root); free(raw);\n        return e2e_refuse(0, "verb must be string", EX_CONFIG);\n    }\n    verb = m->s;\n    if (!strcmp(verb, "make-bundle") || !strcmp(verb, "download")) {\n        char why[64];\n        snprintf(why, sizeof why, "deny-listed verb \'%s\' (sec 2)", verb);\n        ej_free(root); free(raw);\n        return e2e_refuse(0, why, EX_CONFIG);\n    }\n    if (strcmp(verb, "records") && strcmp(verb, "register-mod") &&\n        strcmp(verb, "prepare") && strcmp(verb, "prepare-archive") &&\n        strcmp(verb, "install") && strcmp(verb, "simulate-runtime") &&\n        strcmp(verb, "recover")) {\n        char why[192];\n        snprintf(why, sizeof why, "verb \'%s\' not in allow-list (sec 2)", verb);\n        ej_free(root); free(raw);\n        return e2e_refuse(0, why, EX_CONFIG);\n    }\n    sr = ej_member(root, "state_root");\n    {\n        int cnt = 0;\n        ej_val *e;\n        if (sr && sr->kind == EJ_OBJ)\n            for (e = sr->child; e; e = e->next) cnt++;\n        if (!sr || sr->kind != EJ_OBJ || cnt != 2 ||\n            !ej_member(sr, "state") || !ej_member(sr, "roots")) {\n            ej_free(root); free(raw);\n            return e2e_refuse(0, "state_root must be an object with exactly state,roots (sec 2)",\n                              EX_CONFIG);\n        }\n    }\n    for (m = sr->child; m; m = m->next) {\n        if (m->kind != EJ_STR || m->s[0] != \'/\' || strlen(m->s) > LINE_MAX_) {\n            ej_free(root); free(raw);\n            return e2e_refuse(0, "state_root.state/.roots must be absolute strings <= LINE_MAX_ (sec 2)",\n                              EX_CONFIG);\n        }\n    }\n    /* state dir validated: report-path logging now possible on later refusals */\n    sd_report = ej_member(sr, "state")->s;\n\n    mods = ej_member(root, "mods");\n    mods_present = mods != 0;\n    if (mods_present) {\n        ej_val *e;\n        if (mods->kind != EJ_ARR) {\n            ej_free(root); free(raw);\n            return e2e_refuse(sd_report, "mods must be array (sec 2)", EX_CONFIG);\n        }\n        if (strcmp(verb, "prepare") && strcmp(verb, "install")) {\n            char why[128];\n            snprintf(why, sizeof why, "mods present with verb \'%s\' refuses (sec 2)", verb);\n            ej_free(root); free(raw);\n            return e2e_refuse(sd_report, why, EX_CONFIG);\n        }\n        {\n            int cnt = 0;\n            for (e = mods->child; e; e = e->next) cnt++;\n            if (cnt > 16) {\n                ej_free(root); free(raw);\n                return e2e_refuse(sd_report, "mods > 16 entries (sec 3)", EX_CONFIG);\n            }\n        }\n        for (e = mods->child; e; e = e->next)\n            if (e->kind != EJ_STR || !e->s[0] || strlen(e->s) > LINE_MAX_) {\n                ej_free(root); free(raw);\n                return e2e_refuse(sd_report, "mods entries must be non-empty strings <= LINE_MAX_",\n                                  EX_CONFIG);\n            }\n    }\n    opts = ej_member(root, "options");\n    if (opts) {\n        ej_val *e;\n        int cnt = 0;\n        if (opts->kind != EJ_OBJ) {\n            ej_free(root); free(raw);\n            return e2e_refuse(sd_report, "options must be object (sec 2)", EX_CONFIG);\n        }\n        for (e = opts->child; e; e = e->next) cnt++;\n        if (cnt > 16) {\n            ej_free(root); free(raw);\n            return e2e_refuse(sd_report, "options > 16 keys (sec 3)", EX_CONFIG);\n        }\n        for (e = opts->child; e; e = e->next) {\n            if (!strcmp(e->key, "crash_at") || !strcmp(e->key, "cancel_at") ||\n                !strcmp(e->key, "cancel_after") || !strcmp(e->key, "variant") ||\n                !strcmp(e->key, "defaults") || !strcmp(e->key, "state_name") ||\n                !strcmp(e->key, "code") || !strcmp(e->key, "detail")) {\n                char why[160];\n                snprintf(why, sizeof why, "deny-listed option \'%s\' (sec 2)", e->key);\n                ej_free(root); free(raw);\n                return e2e_refuse(sd_report, why, EX_CONFIG);\n            }\n            if (!e2e_opt_flag(e->key)) {\n                char why[192];\n                snprintf(why, sizeof why, "unknown options key \'%s\' (sec 6)", e->key);\n                ej_free(root); free(raw);\n                return e2e_refuse(sd_report, why, EX_CONFIG);\n            }\n            if (!e2e_verb_legal(e->key, verb)) {\n                char why[224];\n                snprintf(why, sizeof why, "options.%s illegal for verb \'%s\' (eager refusal, sec 3)",\n                         e->key, verb);\n                ej_free(root); free(raw);\n                return e2e_refuse(sd_report, why, EX_CONFIG);\n            }\n            if (!strcmp(e->key, "no_auto_bundle")) {\n                if (e->kind != EJ_BOOL || !e->b) {\n                    ej_free(root); free(raw);\n                    return e2e_refuse(sd_report, "no_auto_bundle accepts bool true only (sec 5)",\n                                      EX_CONFIG);\n                }\n            } else {\n                if (e->kind != EJ_STR || !e->s[0] || strlen(e->s) > LINE_MAX_) {\n                    char why[192];\n                    snprintf(why, sizeof why, "options.%s must be a non-empty string <= LINE_MAX_",\n                             e->key);\n                    ej_free(root); free(raw);\n                    return e2e_refuse(sd_report, why, EX_CONFIG);\n                }\n            }\n        }\n    }\n    if (mods_present && opts && ej_member(opts, "remove")) {\n        ej_free(root); free(raw);\n        return e2e_refuse(sd_report, "mods together with options.remove refuses (sec 3)",\n                          EX_CONFIG);\n    }\n\n    /* -- consume, then synthesize the argv (sec 3 mechanical mapping) ------ */\n    if (e2e_consume(path)) {\n        ej_free(root); free(raw);\n        say("error e2e-cmd consume failed");\n        usage();\n        return EX_BAD;   /* sec 6 verbatim; verb NOT executed */\n    }\n\n    synargv[n++] = (char *)prog;\n    synargv[n++] = (char *)verb;\n    synargv[n++] = "--state";\n    synargv[n++] = (char *)ej_member(sr, "state")->s;\n    synargv[n++] = "--roots";\n    synargv[n++] = (char *)ej_member(sr, "roots")->s;\n    if (mods_present) {\n        ej_val *e;\n        size_t total = 0;\n        char *w;\n        for (e = mods->child; e; e = e->next) total += strlen(e->s) + 1;\n        joinbuf = (char *)malloc(total + 1);\n        if (!joinbuf) { ej_free(root); free(raw); return EX_IO; }\n        w = joinbuf;\n        for (e = mods->child; e; e = e->next) {\n            size_t l = strlen(e->s);\n            if (w != joinbuf) *w++ = \',\';\n            memcpy(w, e->s, l);\n            w += l;\n        }\n        *w = 0;\n        synargv[n++] = "--mods";\n        synargv[n++] = joinbuf;\n    }\n    /* fixed key order (host synth_argv): title,id,bundle,archive,quarantine,\n     * mod,remove,rt_state,choices, then value-less --no-auto-bundle */\n    {\n        static const char *order[] = { "title", "id", "bundle", "archive", "quarantine",\n                                       "mod", "remove", "rt_state", "choices",\n                                       "no_auto_bundle" };\n        size_t k;\n        for (k = 0; k < sizeof order / sizeof order[0]; ++k) {\n            ej_val *e = opts ? ej_member(opts, order[k]) : 0;\n            if (!e) continue;\n            if (!strcmp(order[k], "no_auto_bundle")) {\n                synargv[n++] = "--no-auto-bundle";\n            } else {\n                synargv[n++] = (char *)e2e_opt_flag(order[k]);\n                synargv[n++] = e->s;\n            }\n        }\n    }\n    synargv[n] = 0;\n    heap_argv = (char **)malloc((size_t)(n + 1) * sizeof *heap_argv);\n    if (!heap_argv) { free(joinbuf); ej_free(root); free(raw); return EX_IO; }\n    for (i = 0; i <= n; ++i) heap_argv[i] = synargv[i];\n    /* raw/tree/joinbuf stay live: heap_argv points into them (bounded by the\n     * 4096-byte file); freed at process exit per schema sec 5 */\n    *out_argc = n;\n    *out_argv = heap_argv;\n    return -1;\n}\n\nint main(int argc, char **argv) {\n    const char *cmd = 0, *title = 0, *mods = 0, *remove = 0, *id = 0, *bundle = 0,\n               *defaults = 0, *choices = 0, *statedir = 0, *rtstate = 0, *detail = 0,\n               *crash = 0, *cancelat = 0;\n    int code = 0, i;\n    char vgroup[32][64], vopt[32][64], vbundle[32][LINE_MAX_];\n    int nvar = 0;\n    char state_dir[LINE_MAX_], roots[LINE_MAX_];\n\n    /* make-bundle owns its own flag set (fixture builder); bypass the store */\n    if (argc > 1 && !strcmp(argv[1], "make-bundle")) {\n        cmd_make_bundle(argc, argv);\n        return EX_OK;\n    }\n\n    /* worker-e2e-config-entry: schema sec 1 entry rule. argc<2 (bare dispatch:\n     * the console loader forwards no arguments) consults the compile-time\n     * fixed path; the explicit override slot `--e2e-cmd FILE` (first\n     * occurrence, value required) runs the IDENTICAL validate/consume/\n     * synthesize rule against FILE — this is the host harness\'s entry point\n     * and a controller-side manual trigger. Without the flag and with\n     * argc>=2 the entry is never consulted: legacy argv wins and a leftover\n     * cmd file stays untouched (schema sec 6). */\n    { const char *e2e_path = e2e_cmd_override_path(argc, argv);\n      if (argc < 2 || e2e_path) {\n          int nargc; char **nargv;\n          int erc = e2e_cmd_entry(argc < 2 ? E2E_CMD_PATH : e2e_path,\n                                  argc > 0 ? argv[0] : "modbridge-installer",\n                                  &nargc, &nargv);\n          if (erc >= 0) return erc;   /* refused/absent: verb NOT executed */\n          argc = nargc; argv = nargv;\n      } }\n\n    for (i = 1; i < argc; ++i) {', 1),
]


def patch_modinst():
    src = (VEND / "modinst.c").read_text()
    before = hashlib.sha256(src.encode()).hexdigest()
    applied = []
    for pid, old, new, count in PATCHES:
        found = src.count(old)
        if found != count:
            die("patch %s: expected %d occurrence(s), found %d — vendored input "
                "differs from the reviewed anchor" % (pid, count, found))
        src = src.replace(old, new)
        applied.append({"id": pid, "occurrences": count,
                        "old_sha256": hashlib.sha256(old.encode()).hexdigest(),
                        "new_sha256": hashlib.sha256(new.encode()).hexdigest(),
                        "old_bytes": len(old), "new_bytes": len(new.encode("utf-8")) if pid.startswith("g1-cmd-entry-") else len(new)})  # g1 splice: promoted tail records UTF-8 BYTE lengths (consume new = 12862 B); inherited prefix keeps the delivered char-length convention
    (VEND / "modinst_core.c").write_text(src)
    receipt = {"source": "composition/vendored/modinst.c (copy of worker-7 src/modinst.c)",
               "source_sha256": before,
               "patched": "worker-e2e-payload-build/src/modinst_core.c (config-entry revision; = worker-e2e-config-entry owned/modinst_core.c)",   # g1 splice: promoted label; the local composition/vendored/modinst_core.c is byte-identical
               "patched_sha256": hashlib.sha256(src.encode()).hexdigest(),
               "patches": applied}
    # ---- vendored bridge correction (recorded, same anchor discipline) ----
    bridge = (VEND / "ps5_prepare_bridge.c").read_text()
    bridge_before = hashlib.sha256(bridge.encode()).hexdigest()
    gate = '''    if (!inventory.installation_supported) {
        out->status = PS5_PREPARE_E_SCAN; /* honest: inventoried, unusable */
        out->complete = inventory.complete;
        ps5io_close(qfd);
        out->quarantine_fd = -1;
        return -1;
    }
'''
    note = '''    /* composition correction (recorded patch, worker-installer-composition):
     * intake ABI v1 keeps inventory.installation_supported reserved-zero —
     * archive/groups.c rejects inventories where it is nonzero and
     * archive/tests/corpus_test.c asserts ==0 — so this vendored gate was
     * inverted against the real 3.8.7 closure and always refused. The
     * installation decision belongs to the caller's row selection (and the
     * groups stage), not this reserved field. */
'''
    if bridge.count(gate) != 1:
        die("bridge patch: reserved-field gate anchor not found exactly once")
    bridge = bridge.replace(gate, note)
    (VEND / "ps5_prepare_bridge.c").write_text(bridge)
    receipt["bridge"] = {
        "source": "composition/vendored/ps5_prepare_bridge.c (copy of worker-installer-ps5io)",
        "source_sha256": bridge_before,
        "patched_sha256": hashlib.sha256(bridge.encode()).hexdigest(),
        "patches": [{"id": "bridge-reserved-field-contract",
                     "occurrences": 1,
                     "old_bytes": len(gate), "new_bytes": len(note),
                     "finding": "gate on reserved-zero field inverted the real "
                                "intake contract; every real prepare would "
                                "have failed with PS5_PREPARE_E_SCAN"}]}
    (REC / "patches.json").write_text(json.dumps(receipt, indent=2) + "\n")   # worker-e2e-composer-splice: trailing newline matches the promoted receipt form (worker-e2e-payload-build/g1_tuples_build.py writes json.dumps(pj, indent=2) + newline)
    return src


# ---------------------------------------------------------------- builds
def console_build():
    sys.path.insert(0, str(VEND / "archive"))
    import dependency_build  # pinned codec selection (read-only validation)
    import link_layout       # caller-local dynamic layout (never edits the SDK)

    deps = dependency_build.prepare_archive("native", PROFILE, sdk=SDK, llvm_bin=LLVM_BIN)
    mbed = NATIVE_MBED
    mbed_include = mbed / "sources" / "mbedtls" / MBED_SRC / "include"
    crypto_config = mbed / "config"

    cc = SDK / "bin/prospero-clang"
    nm = SDK / "bin/llvm-nm"
    env = os.environ.copy()
    env["LLVM_CONFIG"] = str(LLVM_BIN / "llvm-config")

    out = BUILD / "console"
    out.mkdir(parents=True, exist_ok=True)

    profile = link_layout.write_linker_script(SDK, out)

    # closure TUs keep verify.py's exact compile contract; the adapted
    # installer TUs use the same pinned roots with -Wall/-Wextra only
    # (documented deviation: -Wpedantic/-Werror are not imposed on the
    # adapted composition sources).
    closure_flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                     "-resource-dir", RESOURCE,
                     "-DMB_ARCHIVE_BUILTIN_XZ=1",
                     '-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"',
                     "-I" + str(mbed_include), "-I" + str(crypto_config),
                     "-I" + str(VEND / "archive")]
    closure_flags += ["-I" + d for d in deps["include_dirs"]]
    core_flags = ["-std=c11", "-O2", "-Wall", "-Wextra",
                  "-resource-dir", RESOURCE,
                  "-DMB_ARCHIVE_BUILTIN_XZ=1",
                  "-I" + str(VEND), "-I" + str(HERE / "owned"),
                  "-I" + str(VEND / "archive"), "-I" + str(mbed_include),
                  "-I" + str(crypto_config)]

    targets = [
        ("modinst_core", VEND / "modinst_core.c", core_flags),
        ("ps5_io", VEND / "ps5_io.c", core_flags),
        ("ps5_io_dir", HERE / "owned/ps5_io_dir.c", core_flags),
        ("ps5_prepare_bridge", VEND / "ps5_prepare_bridge.c", core_flags),
        ("modinst_archive_prepare", HERE / "owned/modinst_archive_prepare.c", core_flags),
        ("e2e_status", HERE / "owned/e2e_status.c", core_flags),
        ("modforge_activation", VEND / "modforge_activation.c", core_flags),
        ("intake", VEND / "archive/intake.c", closure_flags),
        ("posix_compat", VEND / "archive/private/posix_compat.c", closure_flags),
        ("modinst_zip_bridge", HERE / "owned/modinst_zip_bridge.c", core_flags),
    ]
    commands = []
    objects = []
    for name, src, flags in targets:
        obj = out / (name + ".ps5.o")
        r = run([cc, *flags, "-MD", "-MF", out / (name + ".d"), "-c", src, "-o", obj],
                "console-compile-" + name, env)
        commands.append(r)
        if r["exit"]:
            die("console compile failed for " + name + " (see receipts/console-compile-%s.stderr)" % name)
        objects.append(obj)

    libs = [pathlib.Path(p) for p in deps["link_archives"]] + [mbed / "libmbedcrypto.a"]
    for lib in libs:
        if not lib.is_file():
            die("pinned native link input missing: " + str(lib))
    if [pathlib.Path(p).name for p in deps["link_archives"]] != ["libarchive.a", "liblzma.a", "libz.a"]:
        die("codec link order mismatch: " + str(deps["link_archives"]))

    elf = out / "modbridge-installer.payload.elf"
    caller_script = pathlib.Path(profile["caller_script"])
    r = run([cc, "-resource-dir", RESOURCE, *objects, *libs,
             "-Wl,--no-undefined,-z,now", "-Wl,-T," + str(caller_script),
             "-Wl,-Map=" + str(out / "modbridge-installer.payload.map"), "-o", elf],
            "console-link", env)
    commands.append(r)
    if r["exit"]:
        die("console link failed (see receipts/console-link.stderr)")

    # dynamic layout validation (same checks as archive/verify.py native)
    layout = link_layout.validate_dynamic_layout(elf)
    r = run([LLVM_BIN / "llvm-readobj", "--file-headers", "--program-headers",
             "--dynamic-table", elf], "console-elf-readobj", env)
    commands.append(r)
    if (REC / "console-elf-readobj.stderr").stat().st_size:
        die("llvm-readobj emitted stderr for the payload ELF")

    # ---- import provider check: refuse unresolvable symbols loudly ----
    r = run([nm, "-D", "--undefined-only", elf], "console-imports", env)
    commands.append(r)
    if r["exit"]:
        die("llvm-nm failed on the payload ELF")
    names = sorted({line.split()[-1] for line in r_stdout_text("console-imports").splitlines()
                    if line.strip()})
    providers = {}
    target_libs = sorted((SDK / "target/lib").glob("*.so"))
    for lib in target_libs:
        rr = run([nm, "-D", "--defined-only", lib], "console-provider-" + lib.stem, env)
        if rr["exit"]:
            die("llvm-nm failed on pinned lib " + str(lib))
        for line in r_stdout_text("console-provider-" + lib.stem).splitlines():
            parts = line.split()
            if parts:
                providers.setdefault(parts[-1], set()).add(lib.name)
    missing = [n for n in names if n not in providers]
    (REC / "console-import-providers.json").write_text(json.dumps({
        "elf": str(elf), "elf_sha256": digest(elf), "elf_bytes": elf.stat().st_size,
        "imports": {n: sorted(providers.get(n, [])) for n in names},
        "missing": missing,
        "note": "every dynamic import of the payload ELF resolved against the "
                "pinned SDK target libs (llvm-nm -D --defined-only), the "
                "worker-installer-ps5io build_console.sh provider pattern; "
                "stub_weak providers bind at load time to the console runtime"},
        indent=2, sort_keys=True))
    if missing:
        die("PAYLOAD ELF HAS UNRESOLVABLE IMPORTS — missing from pinned libs: "
            + ", ".join(missing))
    (REC / "console-build.json").write_text(json.dumps({
        "commands": commands,
        "link_inputs": {"objects": [str(o) for o in objects],
                        "static_closure": [str(p) for p in libs],
                        "link_order": "libarchive -> liblzma -> zlib -> libmbedcrypto "
                                      "(verify.py native assembly)",
                        "caller_script": profile},
        "dynamic_layout": layout}, indent=2))
    print("console: %s (%d bytes) imports=%d all resolved" % (elf, elf.stat().st_size, len(names)))
    return elf


def r_stdout_text(name):
    return (REC / (name + ".stdout")).read_text()


def host_build():
    sys.path.insert(0, str(VEND / "archive"))
    import dependency_build
    deps = dependency_build.prepare_archive("host", PROFILE)
    mbed = HOST_MBED
    mbed_include = mbed / "sources" / "mbedtls" / MBED_SRC / "include"
    crypto_config = mbed / "config"

    cc = "/usr/bin/clang"
    out = BUILD / "host"
    out.mkdir(parents=True, exist_ok=True)

    closure_flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                     "-DMB_ARCHIVE_BUILTIN_XZ=1",
                     '-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"',
                     "-I" + str(mbed_include), "-I" + str(crypto_config),
                     "-I" + str(VEND / "archive")]
    closure_flags += ["-I" + d for d in deps["include_dirs"]]
    core_flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-DPS5IO_HOST=1",
                  "-DMI_HAS_DOWNLOAD=1",
                  "-I" + str(VEND), "-I" + str(HERE / "owned"),
                  "-I" + str(VEND / "archive"), "-I" + str(mbed_include),
                  "-I" + str(crypto_config)]

    targets = [
        ("modinst_core", VEND / "modinst_core.c", core_flags),
        ("ps5_io", VEND / "ps5_io.c", core_flags),
        ("ps5_io_dir", HERE / "owned/ps5_io_dir.c", core_flags),
        ("ps5_prepare_bridge", VEND / "ps5_prepare_bridge.c", core_flags),
        ("modinst_archive_prepare", HERE / "owned/modinst_archive_prepare.c", core_flags),
        ("e2e_status", HERE / "owned/e2e_status.c", core_flags),
        ("modforge_activation", VEND / "modforge_activation.c", core_flags),
        ("intake", VEND / "archive/intake.c", closure_flags),
        ("posix_compat", VEND / "archive/private/posix_compat.c", closure_flags),
        ("file_sink", VEND / "archive/private/file_sink.c", closure_flags),
        ("modinst_download", HERE / "owned/modinst_download.c",
         core_flags + ["-I" + str(VEND / "download")]),
        ("mb_curl_stub_host", HERE / "owned/mb_curl_stub_host.c",
         core_flags + ["-I" + str(VEND / "download")]),
        ("modinst_zip_bridge", HERE / "owned/modinst_zip_bridge.c", core_flags),
    ]
    commands = []
    objects = []
    for name, src, flags in targets:
        obj = out / (name + ".o")
        r = run([cc, *flags, "-c", src, "-o", obj], "host-compile-" + name)
        commands.append(r)
        if r["exit"]:
            die("host compile failed for " + name + " (see receipts/host-compile-%s.stderr)" % name)
        objects.append(obj)

    libs = [pathlib.Path(p) for p in deps["link_archives"]] + [mbed / "libmbedcrypto.a"]
    binpath = out / "modinst_composed"
    r = run([cc, *objects, *libs, "-o", binpath], "host-link")
    commands.append(r)
    if r["exit"]:
        die("host link failed (see receipts/host-link.stderr)")
    (REC / "host-build.json").write_text(json.dumps({
        "commands": commands,
        "binary": str(binpath), "binary_sha256": digest(binpath),
        "static_closure": [str(p) for p in libs],
        "flavor": "PS5IO_HOST=1 (same adapted core as the console payload)"},
        indent=2))
    print("host: %s" % binpath)
    return binpath


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--flavor", choices=["host", "console", "all"], default="all")
    args = ap.parse_args()

    for d in (VEND, BUILD, REC):
        d.mkdir(parents=True, exist_ok=True)

    vendor()
    patch_modinst()
    ok = True
    if args.flavor in ("console", "all"):
        console_build()
    if args.flavor in ("host", "all"):
        host_build()
    print("COMPOSITION OK (flavor=%s)" % args.flavor)
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())

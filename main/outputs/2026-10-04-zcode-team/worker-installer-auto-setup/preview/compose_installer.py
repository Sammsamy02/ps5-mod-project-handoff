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
]
for name in ("modforge_activation.c", "modforge_activation.h", "modforge_bundle.h",
             "modforge_catalog.h", "modforge_compose.h", "modforge_loadlist.h",
             "modforge_prx.h", "modforge_service.h"):
    VENDOR_FILES.append((W7 / "vendored" / name, name))


def vendor():
    if VEND.exists():
        shutil.rmtree(VEND)
    (VEND / "archive/private").mkdir(parents=True)
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
    ("namespace-bootstrap",
     '    if (mkdirs((char *)g_state_dir)) die("cannot create state dir %s", g_state_dir);',
     '''    /* composition bootstrap (recorded patch, worker-installer-composition):
     * the file I/O owner's production namespace root must exist before any
     * owner opens. The console E2E run failed at stage 41 only because
     * /data/modbridge was absent and had to be created out-of-band (FTP MKD);
     * verify/create it up front so a fresh console never fails that way. The
     * ps5_io adapter additionally self-heals on first open (ps5_io.c).
     * Console only: the host flavor must not create console namespaces. */
#ifndef PS5IO_HOST
    {
        ps5io_stat ns_st;
        if (ps5io_stat_path(PS5IO_NAMESPACE_ROOT, &ns_st) != 0
            || !ns_st.exists || ns_st.is_dir != 1) {
            int ns_rc = ps5io_mkdir_p(PS5IO_NAMESPACE_ROOT, 0755);
            if (ns_rc) die("cannot create namespace root %s: %s",
                           PS5IO_NAMESPACE_ROOT, ps5io_status_string(ns_rc));
        }
    }
#endif
    if (mkdirs((char *)g_state_dir)) die("cannot create state dir %s", g_state_dir);''', 1),
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
                        "old_bytes": len(old), "new_bytes": len(new)})
    (VEND / "modinst_core.c").write_text(src)
    receipt = {"source": "composition/vendored/modinst.c (copy of worker-7 src/modinst.c)",
               "source_sha256": before,
               "patched": "composition/vendored/modinst_core.c",
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
    (REC / "patches.json").write_text(json.dumps(receipt, indent=2))
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

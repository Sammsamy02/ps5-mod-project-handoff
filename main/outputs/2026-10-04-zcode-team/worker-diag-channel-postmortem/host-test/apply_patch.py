#!/usr/bin/env python3
"""apply_patch.py — applies the FAILOPEN-PATCH.md edits to a COPY of module_entry.c.

The delivered source file is never touched. Each edit asserts its exact
baseline string (unique match) so this script doubles as a machine-checkable
patch spec. Baseline sha256 must match the SOURCES.sha256-pinned value.
"""
import hashlib
import sys
from pathlib import Path

ROOT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
BASELINE = ROOT / "outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c"
OUT_DIR = Path(__file__).resolve().parent
PATCHED = OUT_DIR / "module_entry_patched.c"
DIFF_OUT = OUT_DIR / "module_entry_patched.diff"
BASELINE_SHA = "bca1338b4aee984eaab4502d009cc8dcabcbf8bda28a783e9457a6c694066eae"

EDITS = [
    # (edit id, old, new) — old must occur exactly once in the file at time of apply.
    (
        "E1-includes-and-kernel-externs",
        "#include \"lifecycle.h\"\n"
        "#include \"../probe/probe_api.h\"\n"
        "#include \"../diagnostic.h\"\n"
        "#include <stdint.h>\n",
        "#include \"lifecycle.h\"\n"
        "#include \"../probe/probe_api.h\"\n"
        "#include \"../diagnostic.h\"\n"
        "#include <stdint.h>\n"
        "#include <sys/fcntl.h>\n"
        "\n"
        "/* Fail-open entry diagnostics (composition-5): the three frozen bootstrap\n"
        "   imports, declared exactly as in the pinned diagnostic.c implementation. */\n"
        "extern int sceKernelOpen(const char *, int, uint16_t);\n"
        "extern int64_t sceKernelWrite(int, const void *, size_t);\n"
        "extern int sceKernelClose(int);\n",
    ),
    (
        "E2-failover-helper",
        "static struct probe_lifecycle lifecycle = PROBE_LIFECYCLE_INIT;\n"
        "static unsigned char entry_guard;\n"
        "static unsigned constructor_index;\n",
        "static struct probe_lifecycle lifecycle = PROBE_LIFECYCLE_INIT;\n"
        "static unsigned char entry_guard;\n"
        "static unsigned constructor_index;\n"
        "\n"
        "/* ---- fail-open entry diagnostics (composition-5) ----------------------\n"
        "   module_start must never return -20/-21/-22 without leaving exactly one\n"
        "   line in /data/plugins/UE4SS.log (O_WRONLY|O_CREAT|O_APPEND, 0644 — the\n"
        "   exact flag set proven writable in-game by the modbridge_path_test\n"
        "   receipt \"PATH_TEST: /data/plugins/UE4SS.log writable ...\").\n"
        "   C only, fixed stack buffer, no allocation, no variadic formatting. */\n"
        "#define ENTRY_FAILOVER_PATH \"/data/plugins/UE4SS.log\"\n"
        "#define ENTRY_FAILOVER_LINE_CAPACITY 192u\n"
        "\n"
        "static void failover_append(char *buffer, unsigned *used, unsigned capacity,\n"
        "                            const char *text)\n"
        "{\n"
        "    while (*text && *used < capacity)\n"
        "        buffer[(*used)++] = *text++;\n"
        "}\n"
        "\n"
        "static void failover_append_hex(char *buffer, unsigned *used, unsigned capacity,\n"
        "                                int value)\n"
        "{\n"
        "    static const char digits[] = \"0123456789abcdef\";\n"
        "    unsigned v = (unsigned)value;\n"
        "    failover_append(buffer, used, capacity, \"0x\");\n"
        "    for (int shift = 28; shift >= 0; shift -= 4)\n"
        "        if (*used < capacity)\n"
        "            buffer[(*used)++] = digits[(v >> shift) & 15u];\n"
        "}\n"
        "\n"
        "static void entry_failover_write(const char *reason, int code)\n"
        "{\n"
        "    /* Volatile copies, exactly like diagnostic.c:19-22: guards against\n"
        "       elision and models unresolved-import NULL bindings. */\n"
        "    int (*volatile open_binding)(const char *, int, uint16_t) = sceKernelOpen;\n"
        "    int64_t (*volatile write_binding)(int, const void *, size_t) = sceKernelWrite;\n"
        "    int (*volatile close_binding)(int) = sceKernelClose;\n"
        "    if (open_binding == NULL || write_binding == NULL || close_binding == NULL)\n"
        "        return; /* no kernel imports: nothing can be written at all */\n"
        "    char line[ENTRY_FAILOVER_LINE_CAPACITY];\n"
        "    unsigned used = 0;\n"
        "    unsigned limit = (unsigned)sizeof(line) - 1; /* last byte reserved for '\\n' */\n"
        "    /* Reason stage reserves 15 bytes for \" rc=0x????????\\n\". */\n"
        "    failover_append(line, &used, limit - 15, \"modbridge-entry-failover/v1 \");\n"
        "    failover_append(line, &used, limit - 15, reason);\n"
        "    failover_append(line, &used, limit, \" rc=\");\n"
        "    failover_append_hex(line, &used, limit, code);\n"
        "    line[used++] = '\\n';\n"
        "    int fd = open_binding(ENTRY_FAILOVER_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);\n"
        "    if (fd < 0)\n"
        "        return; /* last-resort channel refused: no further fallback exists */\n"
        "    size_t offset = 0;\n"
        "    while (offset < used) {\n"
        "        int64_t written = write_binding(fd, line + offset, used - offset);\n"
        "        if (written <= 0 || (size_t)written > used - offset)\n"
        "            break;\n"
        "        offset += (size_t)written;\n"
        "    }\n"
        "    (void)close_binding(fd);\n"
        "}\n",
    ),
    (
        "E3-guard-busy-marker",
        "    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))\n"
        "        return -20;\n"
        "    if (lifecycle.state == PROBE_ACTIVE)\n",
        "    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE)) {\n"
        "        entry_failover_write(\"entry_guard_busy\", -20);\n"
        "        return -20;\n"
        "    }\n"
        "    if (lifecycle.state == PROBE_ACTIVE)\n",
    ),
    (
        "E4-wrong-state-marker",
        "    if (lifecycle.state != PROBE_NEW)\n"
        "        return leave_entry(-21);\n",
        "    if (lifecycle.state != PROBE_NEW) {\n"
        "        entry_failover_write(\"wrong_state\", -21);\n"
        "        return leave_entry(-21);\n"
        "    }\n",
    ),
    (
        "E5-channel-fail-marker",
        "    if (!diagnostic_ready() || diagnostic_open_channel() < 0)\n"
        "        return leave_entry(-22);\n",
        "    if (!diagnostic_ready()) {\n"
        "        entry_failover_write(\"diag_bindings_null\", -22);\n"
        "        return leave_entry(-22);\n"
        "    }\n"
        "    int channel = diagnostic_open_channel();\n"
        "    if (channel < 0) {\n"
        "        entry_failover_write(\"channel_open_failed\", channel);\n"
        "        return leave_entry(-22);\n"
        "    }\n",
    ),
]


def main() -> int:
    data = BASELINE.read_text()
    sha = hashlib.sha256(data.encode()).hexdigest()
    if sha != BASELINE_SHA:
        print(f"FAIL baseline sha256 {sha} != pinned {BASELINE_SHA}")
        return 1
    for edit_id, old, new in EDITS:
        count = data.count(old)
        if count != 1:
            print(f"FAIL {edit_id}: baseline anchor occurs {count} times (want 1)")
            return 1
        data = data.replace(old, new)
    PATCHED.write_text(data)
    new_sha = hashlib.sha256(data.encode()).hexdigest()
    import difflib

    diff = difflib.unified_diff(
        BASELINE.read_text().splitlines(keepends=True),
        data.splitlines(keepends=True),
        fromfile="lifecycle/module_entry.c (baseline bca1338b)",
        tofile="lifecycle/module_entry.c (fail-open, composition-5)",
    )
    DIFF_OUT.write_text("".join(diff))
    print(f"OK baseline {BASELINE_SHA[:16]} -> patched {new_sha[:16]}")
    print(f"OK wrote {PATCHED}")
    print(f"OK wrote {DIFF_OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

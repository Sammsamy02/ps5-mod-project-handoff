/*
 * test_activation_final.c — worker-loadlist-final host test, 2026-10-04.
 *
 * Loads the FINAL package (../final-package) through the vendored modforge
 * activation path (worker-7-installer/vendored modforge_activation.c +
 * modforge_loadlist.h) — the same mfa_activate the runtime chain performs
 * first in PrepareActivatedUE4SSRuntime. NO mod Lua is executed; NO engine
 * code runs; nothing outside this worker's directory is written.
 *
 * DERIVED by worker-engineversion-ini (2026-10-04) from
 * worker-loadlist-final/host-test/test_activation_final.c. ONLY delta: settings
 * row size expectation 834 -> 1448 (worker-settings-v3-package: [Hooks] append, +614 bytes) and preloaded payload 35945 -> 36559
 * (+7 NUL => 35952): the settings file gains the appended
 * [EngineVersionOverride] section (55 bytes). All other checks unchanged.
 * This supersedes worker-itemspawner-bundle's test_activation.c expectations,
 * which are pinned to the old 4-row bundle (settings 221, layout pair absent).
 * Final expectations:
 *   T1  loadlist.txt parses; header consumed; 1 title/engine/framework/mod;
 *       exactly 7 file rows with exact role/path/size; zero order rows;
 *       zero unknown rows.
 *   T2  mfa_activate(root, PPSA33286) OK; engine unreal; framework
 *       ue4ss-ps5; mod list [TownfallItemSpawner 1.0.0]; all 7 files
 *       preloaded (roles config/lua_mod); preloaded bytes == 36559 payload
 *       + 7 NUL terminators == 35952.
 *   T3  every activated file's data != NULL, listed size matches, enabled.txt
 *       empty; layout pair texts end NUL-terminated at exactly 588/186.
 *   T4  NativeModSource enumeration oracle: exactly one Lua mod
 *       TownfallItemSpawner with Scripts/ dir; shared/ never enumerated;
 *       enabled.txt -> startup list [TownfallItemSpawner].
 *   T5  the REQUIRED layout keys are PRESENT via mfa_find with exact sizes
 *       588 / 186 (the fix this package delivers).
 *   T6  negative roots: wrong title -> MFA_E_TITLE; truncated layout file ->
 *       MFA_E_SIZE; missing loadlist -> MFA_E_NO_LOADLIST.
 *
 * Usage: test_activation_final <final-root> <root-badtitle> <root-badsize> <root-noll>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modforge_activation.h"
#include "modforge_loadlist.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("PASS %s\n", msg); } \
    else { printf("FAIL %s\n", msg); ++failures; } \
} while (0)

static char *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n ? (size_t)n : 1);
    if (!buf) { fclose(f); return NULL; }
    if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f); free(buf); return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static int lit_eq(mf_str s, const char *lit) { return mf_str_eq(s, lit); }

struct expect_file { const char *role; const char *path; unsigned long long size; };
static const struct expect_file EXPECTED[] = {
    {"config",  "ue4ss/UE4SS-settings.ini", 1448},
    {"config",  "ue4ss/MemberVariableLayout.ini", 588},
    {"config",  "ue4ss/VTableLayout.ini", 186},
    {"config",  "ue4ss/PS5Pad.ini", 2960},
    {"lua_mod", "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", 21415},
    {"lua_mod", "ue4ss/Mods/TownfallItemSpawner/enabled.txt", 0},
    {"lua_mod", "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", 9962},
};
#define N_EXPECTED (sizeof EXPECTED / sizeof EXPECTED[0])

/* Mirror NativeModSource ctor enumeration: ue4ss/Mods/<name>/{Scripts,scripts}/
 * main.lua, fold-case "shared" skipped, then enabled.txt markers. */
static void mods_oracle(const mfa_activation *a, char *name, size_t name_cap,
                        char *scripts, size_t scripts_cap, int *count,
                        char *startup, int *startup_count) {
    static const char prefix[] = "ue4ss/Mods/";
    *count = 0; *startup_count = 0;
    for (size_t i = 0; i < mfa_file_count(a); ++i) {
        const mfa_file *f = mfa_file_at(a, i);
        if (strncmp(f->path, prefix, sizeof prefix - 1) != 0) continue;
        const char *suffix = f->path + sizeof prefix - 1;
        const char *slash = strchr(suffix, '/');
        if (!slash) continue;
        size_t nlen = (size_t)(slash - suffix);
        if (nlen == 6 && strncmp(suffix, "shared", 6) == 0) continue; /* fold-case shared */
        char lower[8] = {0};
        for (size_t k = 0; k < nlen && k < 7; ++k)
            lower[k] = (char)((suffix[k] >= 'A' && suffix[k] <= 'Z') ? suffix[k] + 32 : suffix[k]);
        if (strcmp(lower, "shared") == 0) continue;
        const char *rest = slash + 1;
        if ((strncmp(rest, "Scripts/main.lua", 16) == 0) ||
            (strncmp(rest, "scripts/main.lua", 16) == 0)) {
            if (*count == 0) {
                snprintf(name, name_cap, "%.*s", (int)nlen, suffix);
                snprintf(scripts, scripts_cap, "%.*s/%s",
                         (int)nlen, suffix,
                         strncmp(rest, "Scripts/", 8) == 0 ? "Scripts" : "scripts");
            }
            ++*count;
        }
        if (strcmp(rest, "enabled.txt") == 0) {
            snprintf(startup, 256, "%.*s", (int)nlen, suffix);
            ++*startup_count;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <final-root> <root-badtitle> <root-badsize> <root-noll>\n", argv[0]);
        return 2;
    }
    const char *root = argv[1];

    /* ---- T1: loadlist parse + exact record set ---- */
    size_t ll_len = 0;
    char *ll;
    {
        char p[1024];
        snprintf(p, sizeof p, "%s/loadlist.txt", root);
        ll = slurp(p, &ll_len);
    }
    CHECK(ll != NULL, "T1 loadlist.txt readable");
    int n_title = 0, n_engine = 0, n_framework = 0, n_mod = 0, n_file = 0,
        n_order = 0, n_unknown = 0, n_header = 0, t1_parse_ok = 1;
    if (ll) {
        mf_reader r;
        mf_record rec;
        mf_reader_init(&r, ll, ll_len);
        int rc;
        while ((rc = mf_next(&r, &rec)) > 0) {
            switch (rec.kind) {
            case MF_REC_TITLE:
                ++n_title;
                CHECK(lit_eq(rec.f[0], "PPSA33286"), "T1 title == PPSA33286");
                break;
            case MF_REC_ENGINE:
                ++n_engine;
                CHECK(lit_eq(rec.f[0], "unreal"), "T1 engine == unreal");
                break;
            case MF_REC_FRAMEWORK:
                ++n_framework;
                CHECK(lit_eq(rec.f[0], "ue4ss-ps5"), "T1 framework == ue4ss-ps5");
                break;
            case MF_REC_MOD:
                ++n_mod;
                CHECK(lit_eq(rec.f[0], "TownfallItemSpawner") && lit_eq(rec.f[1], "1.0.0"),
                      "T1 mod == TownfallItemSpawner 1.0.0");
                break;
            case MF_REC_FILE: {
                ++n_file;
                int found = 0;
                for (size_t i = 0; i < N_EXPECTED; ++i) {
                    if (lit_eq(rec.f[1], EXPECTED[i].path)) {
                        found = 1;
                        char msg[160];
                        snprintf(msg, sizeof msg, "T1 file row %s role/size", EXPECTED[i].path);
                        CHECK(lit_eq(rec.f[0], EXPECTED[i].role) && rec.size == EXPECTED[i].size, msg);
                        break;
                    }
                }
                if (!found) {
                    printf("FAIL T1 unexpected file row\n");
                    ++failures;
                }
                break;
            }
            case MF_REC_ORDER: ++n_order; break;
            default: ++n_unknown; break;
            }
        }
        n_header = r.seen_header;
        if (rc < 0) {
            printf("FAIL T1 loadlist malformed at line %d\n", r.line);
            ++failures;
            t1_parse_ok = 0;
        }
    }
    CHECK(n_header == 1, "T1 header modforge-loadlist 1 consumed");
    CHECK(n_title == 1 && n_engine == 1 && n_framework == 1 && n_mod == 1,
          "T1 exactly one title/engine/framework/mod record");
    CHECK(t1_parse_ok && n_file == (int)N_EXPECTED,
          "T1 exactly seven file rows (role/path/size)");
    CHECK(n_order == 0, "T1 zero order rows (Lua mod ships none)");
    CHECK(n_unknown == 0, "T1 zero unknown rows");
    free(ll);

    /* ---- T2: mfa_activate over the real package ---- */
    mfa_activation *a = NULL;
    mfa_error err;
    memset(&err, 0, sizeof err);
    mfa_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.root = root;
    cfg.title_id = "PPSA33286";
    int rc2 = mfa_activate(&cfg, &a, &err);
    CHECK(rc2 == MFA_OK && a != NULL, "T2 mfa_activate(final package) OK");
    if (rc2 != MFA_OK || !a) {
        printf("FAIL T2 abort: code=%d line=%d path=%s message=%s\n",
               err.code, err.line, err.path, err.message);
        return 1;
    }
    CHECK(strcmp(mfa_title(a), "PPSA33286") == 0, "T2 mfa_title == PPSA33286");
    CHECK(strcmp(mfa_engine(a), "unreal") == 0, "T2 mfa_engine == unreal (activated_mod_source.cpp:27 gate)");
    CHECK(mfa_wants_framework(a, "ue4ss-ps5"), "T2 wants_framework ue4ss-ps5 (activated_mod_source.cpp:28 gate)");
    CHECK(mfa_mod_count(a) == 1 && strcmp(mfa_mod_at(a, 0)->id, "TownfallItemSpawner") == 0 &&
          strcmp(mfa_mod_at(a, 0)->version, "1.0.0") == 0,
          "T2 load-order mod list == [TownfallItemSpawner 1.0.0]");
    CHECK(mfa_file_count(a) == 7, "T2 seven files in activation");
    int all_preloaded = 1;
    for (size_t i = 0; i < mfa_file_count(a); ++i) {
        const mfa_file *f = mfa_file_at(a, i);
        if (f->data == NULL ||
            (strcmp(f->role, "lua_mod") != 0 && strcmp(f->role, "config") != 0))
            all_preloaded = 0;
    }
    CHECK(all_preloaded, "T2 all files preloaded (roles lua_mod/config)");
    CHECK(mfa_preloaded_bytes(a) == 36559 + 7,
          "T2 preloaded bytes == 36566 (36559 payload + 7 NUL terminators)");
    CHECK(mfa_order_count(a) == 0, "T2 no data-plugin order entries");

    /* ---- T3: per-file activation facts ---- */
    for (size_t i = 0; i < N_EXPECTED; ++i) {
        const mfa_file *f = mfa_find(a, EXPECTED[i].path);
        char msg[192];
        snprintf(msg, sizeof msg, "T3 %s present, size==%llu, data NUL-terminated at [size]",
                 EXPECTED[i].path, EXPECTED[i].size);
        CHECK(f != NULL && f->size == EXPECTED[i].size &&
              f->data != NULL && f->data[f->size] == 0,
              msg);
    }
    {
        const mfa_file *en = mfa_find(a, "ue4ss/Mods/TownfallItemSpawner/enabled.txt");
        CHECK(en != NULL && en->size == 0 && en->data && en->data[0] == 0,
              "T3 enabled.txt empty, presence-only");
        const mfa_file *mv = mfa_find(a, "ue4ss/MemberVariableLayout.ini");
        CHECK(mv != NULL && mv->size == 588 && mv->data && mv->data[588] == 0,
              "T3 MemberVariableLayout.ini 588 bytes, NUL-terminated");
        const mfa_file *vt = mfa_find(a, "ue4ss/VTableLayout.ini");
        CHECK(vt != NULL && vt->size == 186 && vt->data && vt->data[186] == 0,
              "T3 VTableLayout.ini 186 bytes, NUL-terminated");
    }

    /* ---- T4: NativeModSource enumeration oracle ---- */
    {
        char name[128] = {0}, scripts[256] = {0}, startup[256] = {0};
        int count = 0, startup_count = 0;
        mods_oracle(a, name, sizeof name, scripts, sizeof scripts, &count,
                    startup, &startup_count);
        CHECK(count == 1 && strcmp(name, "TownfallItemSpawner") == 0,
              "T4 lua_mods == [TownfallItemSpawner], shared/ not a mod");
        CHECK(strcmp(scripts, "TownfallItemSpawner/Scripts") == 0,
              "T4 scripts dir == TownfallItemSpawner/Scripts");
        CHECK(startup_count == 1 && strcmp(startup, "TownfallItemSpawner") == 0,
              "T4 startup_mod_names == [TownfallItemSpawner] via enabled.txt");
        CHECK(mfa_find(a, "ue4ss/Mods/mods.txt") == NULL,
              "T4 no mods.txt (enabled.txt alone, BUNDLE-SPEC 1.3)");
    }

    /* ---- T5: REQUIRED layout pair PRESENT (the fix) ---- */
    {
        const mfa_file *mv = mfa_find(a, "ue4ss/MemberVariableLayout.ini");
        const mfa_file *vt = mfa_find(a, "ue4ss/VTableLayout.ini");
        CHECK(mv != NULL, "T5 MemberVariableLayout.ini PRESENT (mfa_find != NULL)");
        CHECK(vt != NULL, "T5 VTableLayout.ini PRESENT (mfa_find != NULL)");
        CHECK(mv && mv->size == 588 && vt && vt->size == 186,
              "T5 layout pair sizes 588/186 as emitted");
    }
    mfa_release(a);

    /* ---- T6: negative roots ---- */
    {
        mfa_activation *b = NULL;
        mfa_config c2;
        memset(&c2, 0, sizeof c2);
        c2.root = argv[2];
        c2.title_id = "PPSA33286";
        int rc = mfa_activate(&c2, &b, &err);
        CHECK(rc == MFA_E_TITLE && err.code == -4, "T6 wrong title -> MFA_E_TITLE (rc=-4)");
        if (b) mfa_release(b);
    }
    {
        mfa_activation *b = NULL;
        mfa_config c2;
        memset(&c2, 0, sizeof c2);
        c2.root = argv[3];
        c2.title_id = "PPSA33286";
        int rc = mfa_activate(&c2, &b, &err);
        CHECK(rc == MFA_E_SIZE && err.code == -8, "T6 truncated file -> MFA_E_SIZE (rc=-8)");
        if (b) mfa_release(b);
    }
    {
        mfa_activation *b = NULL;
        mfa_config c2;
        memset(&c2, 0, sizeof c2);
        c2.root = argv[4];
        c2.title_id = "PPSA33286";
        int rc = mfa_activate(&c2, &b, &err);
        CHECK(rc == MFA_E_NO_LOADLIST && err.code == -2,
              "T6 missing loadlist -> MFA_E_NO_LOADLIST (rc=-2)");
        if (b) mfa_release(b);
    }

    printf("%s: %d failure(s)\n", "test_activation_final", failures);
    return failures ? 1 : 0;
}

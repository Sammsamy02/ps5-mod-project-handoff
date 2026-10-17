/*
 * test_activation.c — worker-itemspawner-bundle host test, 2026-10-04.
 *
 * Loads the ACTUAL bundle (this worker's bundle/ directory) through the
 * vendored modforge activation path (worker-7-installer/vendored
 * modforge_activation.c + modforge_loadlist.h) and validates it the way the
 * runtime chain would: mfa_activate is the first thing
 * PrepareActivatedUE4SSRuntime performs. Bytes are checked against the pinned
 * sources. NO mod Lua is executed; NO engine code runs.
 *
 * Checks:
 *   T1  loadlist.txt parses with the vendored reader; exact record set
 *       (header consumed, 1 title, 1 engine, 1 framework, 1 mod, 4 file rows
 *       with exact role/path/size, zero order rows, zero unknown rows).
 *   T2  mfa_activate(root, title=PPSA33286) succeeds; title/engine/framework/
 *       mod facts; all 4 preloaded (roles config/lua_mod), preloaded bytes
 *       == 221+21415+0+9962.
 *   T3  every activated file byte-identical to its pinned source: intake
 *       member-00000000.bin (main.lua), member-00000001.bin (enabled.txt),
 *       distro UEHelpers.lua; settings byte-identical to worker-activation-
 *       bundle's validated 221 B build (cross-worker pin f6d06cad...).
 *   T4  NativeModSource enumeration oracle (NativeModSource.cpp:79-135):
 *       exactly one Lua mod "TownfallItemSpawner" with Scripts/ dir, shared/
 *       never enumerated, enabled.txt marker -> startup list [same].
 *   T5  the two REQUIRED layout keys are ABSENT (mfa_find == NULL) — the
 *       honest awaiting-staged-pair state; activation itself still valid.
 *   T6  negative roots: wrong title -> MFA_E_TITLE; truncated file ->
 *       MFA_E_SIZE; missing loadlist -> MFA_E_NO_LOADLIST.
 *
 * Usage: test_activation <bundle-root> <intake-main.lua> <intake-enabled.txt>
 *                        <distro-UEHelpers.lua> <prior-validated-settings.ini>
 *                        <root-badtitle> <root-badsize> <root-noll>
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

static int bytes_equal(const unsigned char *a, const char *b, size_t n) {
    return n == 0 || memcmp(a, b, n) == 0;
}

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
        char fold[128];
        if (nlen == 0 || nlen >= sizeof fold) continue;
        memcpy(fold, suffix, nlen); fold[nlen] = 0;
        for (size_t k = 0; k < nlen; ++k)
            if (fold[k] >= 'A' && fold[k] <= 'Z') fold[k] = (char)(fold[k] + 32);
        if (strcmp(fold, "shared") == 0) continue;           /* :100 */
        const char *rest = slash + 1;
        const char *se = strchr(rest, '/');
        if (!se) continue;
        if ((size_t)(se - rest) != 7) continue;
        if (strncmp(rest, "Scripts", 7) != 0 && strncmp(rest, "scripts", 7) != 0)
            continue;                                        /* :91 */
        if (strcmp(se + 1, "main.lua") != 0) continue;
        if (*count == 0) {
            snprintf(name, name_cap, "%.*s", (int)nlen, suffix);
            snprintf(scripts, scripts_cap, "%.*s", (int)(se - suffix), suffix);
            *count = 1;
        } else {
            *count = 2; /* duplicate mod dir: fail the oracle */
        }
    }
    char marker[512];
    snprintf(marker, sizeof marker, "ue4ss/Mods/%s/enabled.txt", name);
    if (*count == 1 && mfa_find(a, marker)) {
        snprintf(startup, name_cap, "%s", name);
        *startup_count = 1;
    }
}

static int expect_error(const char *root, const char *title, int want_code,
                        const char *label) {
    mfa_config cfg;
    mfa_activation *a = NULL;
    mfa_error err;
    memset(&cfg, 0, sizeof cfg);
    cfg.root = root; cfg.title_id = title;
    int rc = mfa_activate(&cfg, &a, &err);
    int ok = (rc != MFA_OK) && (err.code == want_code) && (a == NULL);
    printf("%s %s (rc=%d err.code=%d %s)\n", ok ? "PASS" : "FAIL", label,
           rc, err.code, mfa_result_name(err.code));
    if (!ok) ++failures;
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 9) {
        fprintf(stderr, "usage: %s <bundle-root> <intake-main.lua> "
                "<intake-enabled.txt> <distro-UEHelpers.lua> "
                "<prior-validated-settings.ini> <root-badtitle> <root-badsize> "
                "<root-noll>\n", argv[0]);
        return 2;
    }
    const char *root = argv[1];
    const char *src_main = argv[2], *src_enabled = argv[3];
    const char *src_uehelpers = argv[4], *src_settings = argv[5];

    /* T1: vendored loadlist reader */
    char llp[1024];
    snprintf(llp, sizeof llp, "%s/loadlist.txt", root);
    size_t ll_len = 0;
    char *ll = slurp(llp, &ll_len);
    CHECK(ll != NULL, "T1 loadlist.txt readable");
    if (!ll) return 1;
    mf_reader r; mf_record rec;
    mf_reader_init(&r, ll, ll_len);
    int n_title = 0, n_engine = 0, n_fw = 0, n_mod = 0, n_file = 0;
    int n_order = 0, n_unknown = 0, rc;
    int f_settings = 0, f_main = 0, f_enabled = 0, f_uehelpers = 0;
    while ((rc = mf_next(&r, &rec)) > 0) {
        switch (rec.kind) {
        case MF_REC_TITLE:
            ++n_title;
            CHECK(mf_str_eq(rec.f[0], "PPSA33286"), "T1 title == PPSA33286");
            break;
        case MF_REC_ENGINE:
            ++n_engine;
            CHECK(mf_str_eq(rec.f[0], "unreal"), "T1 engine == unreal");
            break;
        case MF_REC_FRAMEWORK:
            ++n_fw;
            CHECK(mf_str_eq(rec.f[0], "ue4ss-ps5"), "T1 framework == ue4ss-ps5");
            break;
        case MF_REC_MOD:
            ++n_mod;
            CHECK(mf_str_eq(rec.f[0], "TownfallItemSpawner") &&
                  mf_str_eq(rec.f[1], "1.0.0"), "T1 mod == TownfallItemSpawner 1.0.0");
            break;
        case MF_REC_FILE: {
            ++n_file;
            if (mf_str_eq(rec.f[1], "ue4ss/UE4SS-settings.ini") &&
                mf_str_eq(rec.f[0], "config") && rec.size == 221) ++f_settings;
            else if (mf_str_eq(rec.f[1], "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua") &&
                     mf_str_eq(rec.f[0], "lua_mod") && rec.size == 21415) ++f_main;
            else if (mf_str_eq(rec.f[1], "ue4ss/Mods/TownfallItemSpawner/enabled.txt") &&
                     mf_str_eq(rec.f[0], "lua_mod") && rec.size == 0) ++f_enabled;
            else if (mf_str_eq(rec.f[1], "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua") &&
                     mf_str_eq(rec.f[0], "lua_mod") && rec.size == 9962) ++f_uehelpers;
            break;
        }
        case MF_REC_ORDER: ++n_order; break;
        default: ++n_unknown; break;
        }
    }
    CHECK(rc == 0 && r.seen_header, "T1 header modforge-loadlist 1 consumed");
    CHECK(n_title == 1 && n_engine == 1 && n_fw == 1 && n_mod == 1,
          "T1 exactly one title/engine/framework/mod record");
    CHECK(n_file == 4 && f_settings == 1 && f_main == 1 && f_enabled == 1 &&
          f_uehelpers == 1, "T1 exact four file rows (role/path/size)");
    CHECK(n_order == 0, "T1 zero order rows (Lua mod ships none)");
    CHECK(n_unknown == 0, "T1 zero unknown rows");

    /* T2: mfa_activate over the actual bundle */
    mfa_config cfg;
    mfa_activation *a = NULL;
    mfa_error err;
    memset(&cfg, 0, sizeof cfg);
    cfg.root = root;
    cfg.title_id = "PPSA33286";
    rc = mfa_activate(&cfg, &a, &err);
    CHECK(rc == MFA_OK && a != NULL, "T2 mfa_activate(actual bundle) OK");
    if (rc != MFA_OK) {
        printf("  activate failed: %s os=%d line=%d path=%s msg=%s\n",
               mfa_result_name(err.code), err.os_error, err.line, err.path,
               err.message);
        return 1;
    }
    CHECK(strcmp(mfa_title(a), "PPSA33286") == 0, "T2 mfa_title == PPSA33286");
    CHECK(strcmp(mfa_engine(a), "unreal") == 0, "T2 mfa_engine == unreal "
          "(activated_mod_source.cpp:27 gate)");
    CHECK(mfa_wants_framework(a, "ue4ss-ps5") == 1,
          "T2 wants_framework ue4ss-ps5 (activated_mod_source.cpp:28 gate)");
    CHECK(mfa_mod_count(a) == 1 &&
          strcmp(mfa_mod_at(a, 0)->id, "TownfallItemSpawner") == 0 &&
          strcmp(mfa_mod_at(a, 0)->version, "1.0.0") == 0,
          "T2 load-order mod list == [TownfallItemSpawner 1.0.0]");
    CHECK(mfa_file_count(a) == 4, "T2 four files in activation");
    int all_preloaded = 1;
    for (size_t i = 0; i < mfa_file_count(a); ++i)
        if (mfa_file_at(a, i)->data == NULL) all_preloaded = 0;
    CHECK(all_preloaded, "T2 all files preloaded (roles lua_mod/config)");
    /* Each preloaded blob is size+1 (data[size] == 0 NUL, modforge_activation.h:101;
     * modforge_activation.c:383 preload_total += rec.size + 1). */
    CHECK(mfa_preloaded_bytes(a) == (221ULL + 1) + (21415ULL + 1) + (0ULL + 1) + (9962ULL + 1),
          "T2 preloaded bytes == 31602 (31598 payload + 4 NUL terminators)");
    CHECK(mfa_order_count(a) == 0, "T2 no data-plugin order entries");

    /* T3: byte identity against pinned sources */
    {
        size_t n = 0; char *want = slurp(src_main, &n);
        const mfa_file *f = mfa_find(a, "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua");
        int ok = want && f && f->size == 21415 && f->size == n &&
                 bytes_equal(f->data, want, n);
        printf("%s T3 main.lua byte-identical to intake member-00000000.bin "
               "(21415 B)\n", ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
        free(want);
    }
    {
        size_t n = 0; char *want = slurp(src_enabled, &n);
        const mfa_file *f = mfa_find(a, "ue4ss/Mods/TownfallItemSpawner/enabled.txt");
        int ok = want && f && f->size == 0 && n == 0;
        printf("%s T3 enabled.txt empty, presence-only (intake member-00000001.bin)\n",
               ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
        free(want);
    }
    {
        size_t n = 0; char *want = slurp(src_uehelpers, &n);
        const mfa_file *f = mfa_find(a, "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua");
        int ok = want && f && f->size == 9962 && f->size == n &&
                 bytes_equal(f->data, want, n);
        printf("%s T3 UEHelpers.lua byte-identical to distro asset (9962 B)\n",
               ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
        free(want);
    }
    {
        size_t n = 0; char *want = slurp(src_settings, &n);
        const mfa_file *f = mfa_find(a, "ue4ss/UE4SS-settings.ini");
        int ok = want && f && f->size == 221 && f->size == n &&
                 bytes_equal(f->data, want, n);
        printf("%s T3 settings.ini byte-identical to worker-activation-bundle's "
               "validated 221 B build (cross-worker pin)\n", ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
        free(want);
    }

    /* T4: NativeModSource enumeration oracle */
    {
        char name[128] = {0}, scripts[256] = {0}, startup[128] = {0};
        int count = 0, scount = 0;
        mods_oracle(a, name, sizeof name, scripts, sizeof scripts, &count,
                    startup, &scount);
        CHECK(count == 1 && strcmp(name, "TownfallItemSpawner") == 0,
              "T4 lua_mods == [TownfallItemSpawner], shared/ not a mod");
        CHECK(strcmp(scripts, "TownfallItemSpawner/Scripts") == 0,
              "T4 scripts dir == TownfallItemSpawner/Scripts");
        CHECK(scount == 1 && strcmp(startup, "TownfallItemSpawner") == 0,
              "T4 startup_mod_names == [TownfallItemSpawner] via enabled.txt");
        CHECK(mfa_find(a, "ue4ss/Mods/mods.txt") == NULL,
              "T4 no mods.txt (enabled.txt alone, BUNDLE-SPEC 1.3)");
    }

    /* T5: layout keys honestly absent (awaiting staged pair) */
    CHECK(mfa_find(a, "ue4ss/MemberVariableLayout.ini") == NULL,
          "T5 MemberVariableLayout.ini absent (awaiting staged pair, no synthetic bytes)");
    CHECK(mfa_find(a, "ue4ss/VTableLayout.ini") == NULL,
          "T5 VTableLayout.ini absent (awaiting staged pair, no synthetic bytes)");

    mfa_release(a);

    /* T6: negative roots (mutation testing of the validation path) */
    expect_error(argv[6], "PPSA33286", MFA_E_TITLE, "T6 wrong title -> MFA_E_TITLE");
    expect_error(argv[7], "PPSA33286", MFA_E_SIZE, "T6 truncated file -> MFA_E_SIZE");
    expect_error(argv[8], "PPSA33286", MFA_E_NO_LOADLIST, "T6 missing loadlist -> MFA_E_NO_LOADLIST");

    printf("%s: %d failure(s)\n", failures ? "FAIL" : "OK", failures);
    free(ll);
    return failures ? 1 : 0;
}

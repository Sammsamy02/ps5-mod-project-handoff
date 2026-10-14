/*
 * test_activation.c — host test: the EXISTING runtime's activation inputs for
 * the unchanged Item Spawner mod, exercised through the vendored modforge
 * components (worker-7-installer/vendored/modforge_activation.c +
 * modforge_loadlist.h). No engine execution, no mod Lua execution.
 *
 * Checks:
 *   T1  loadlist.txt parses with the vendored reader; exact record set.
 *   T2  mfa_activate(stage-root-full) OK; title/engine/framework/preload facts.
 *   T3  every file byte-identical to its pinned source (intake members,
 *       distro UEHelpers, generated settings); sizes as listed.
 *   T4  NativeModSource enable-logic oracle (mirrors NativeModSource.cpp:79-135):
 *       lua_mods == [TownfallItemSpawner], startup_mod_names == same via
 *       enabled.txt marker; shared/ never a mod; mods.txt absent.
 *   T5  strict root (no layout keys) activates; mfa_find(layout keys) == NULL
 *       -> the runtime's REQUIRED layout keys are absent (hard stop traced in
 *       LAYOUT-GATE.md; placeholders in stage-root-full are NOT layout facts).
 *   T6  validation failures: wrong title -> MFA_E_TITLE; truncated/long file
 *       -> MFA_E_SIZE; missing loadlist -> MFA_E_NO_LOADLIST.
 *
 * Usage: test_activation <stage-root-full> <strict-root> <intake-main.lua>
 *                        <intake-enabled.txt> <distro-UEHelpers.lua>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* mkdtemp (POSIX; host test only) */

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
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

/* Oracle mirroring RC::Platform::NativeModSource ctor (NativeModSource.cpp:79-135):
 * enumerate ue4ss/Mods/<name>/{Scripts,scripts}/main.lua (skip case-folded
 * "shared"), then enable from ue4ss/Mods/mods.txt rows ("Name : 1") and
 * per-mod enabled.txt markers. */
static void mods_oracle(const mfa_activation *a,
                        char names[][128], char scripts[][256], int *count,
                        char startup[][128], int *startup_count) {
    static const char prefix[] = "ue4ss/Mods/";
    *count = 0; *startup_count = 0;
    for (size_t i = 0; i < mfa_file_count(a); ++i) {
        const mfa_file *f = mfa_file_at(a, i);
        if (strncmp(f->path, prefix, sizeof prefix - 1) != 0) continue;
        const char *suffix = f->path + sizeof prefix - 1;
        const char *slash = strchr(suffix, '/');
        if (!slash) continue;
        size_t name_len = (size_t)(slash - suffix);
        char name[128];
        if (name_len == 0 || name_len >= sizeof name) continue;
        memcpy(name, suffix, name_len); name[name_len] = 0;
        char folded[128];
        for (size_t k = 0; k <= name_len; ++k)
            folded[k] = (name[k] >= 'A' && name[k] <= 'Z') ? (char)(name[k] + 32) : name[k];
        if (strcmp(folded, "shared") == 0) continue; /* NativeModSource.cpp:100 */
        const char *rest = slash + 1;
        const char *scripts_end = strchr(rest, '/');
        if (!scripts_end) continue;
        size_t scripts_len = (size_t)(scripts_end - rest);
        if (scripts_len != 7 || (strncmp(rest, "Scripts", 7) != 0 && strncmp(rest, "scripts", 7) != 0))
            continue; /* NativeModSource.cpp:91 */
        if (strcmp(scripts_end + 1, "main.lua") != 0) continue;
        int dup = 0;
        for (int k = 0; k < *count; ++k) if (strcmp(names[k], name) == 0) dup = 1;
        if (!dup) {
            snprintf(names[*count], 128, "%s", name);
            snprintf(scripts[*count], 256, "%.*s/%.*s", (int)name_len, suffix,
                     (int)(strlen(f->path) - (size_t)(suffix - f->path) - name_len - 1), rest);
            ++*count;
        }
    }
    /* mods.txt rows first (NativeModSource.cpp:113-132), then enabled.txt
     * markers (NativeModSource.cpp:134-135). Our bundle ships no mods.txt. */
    if (mfa_find(a, "ue4ss/Mods/mods.txt"))
        printf("note: mods.txt present (not expected in this bundle)\n");
    for (int i = 0; i < *count; ++i) {
        char marker[512];
        snprintf(marker, sizeof marker, "ue4ss/Mods/%s/enabled.txt", names[i]);
        if (mfa_find(a, marker)) snprintf(startup[(*startup_count)++], 128, "%s", names[i]);
    }
}

static int file_equals(const mfa_activation *a, const char *key,
                       const char *path, const char *label) {
    size_t len = 0;
    char *want = slurp(path, &len);
    const mfa_file *f = mfa_find(a, key);
    int ok = want && f && f->data && f->size == len &&
             (len == 0 || memcmp(f->data, want, len) == 0);
    printf("%s %s (key=%s size=%llu src=%zu)\n", ok ? "PASS" : "FAIL", label, key,
           f ? (unsigned long long)f->size : 0ULL, len);
    if (!ok) ++failures;
    free(want);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 6) { fprintf(stderr, "usage: %s <root-full> <root-strict> <main.lua> <enabled.txt> <UEHelpers.lua>\n", argv[0]); return 2; }
    const char *root_full = argv[1], *root_strict = argv[2];
    const char *src_main = argv[3], *src_enabled = argv[4], *src_uehelpers = argv[5];
    char path[1024];

    /* T1: vendored loadlist reader over the load list itself */
    snprintf(path, sizeof path, "%s/loadlist.txt", root_full);
    size_t ll_len = 0;
    char *ll = slurp(path, &ll_len);
    CHECK(ll != NULL, "T1 loadlist.txt readable");
    mf_reader r; mf_record rec;
    mf_reader_init(&r, ll, ll_len);
    int title_ok = 0, engine_ok = 0, fw_ok = 0, mod_ok = 0, files = 0, rc;
    int main_listed = 0, enabled_listed = 0, uehelpers_listed = 0, settings_listed = 0;
    int layout_listed = 0;
    while ((rc = mf_next(&r, &rec)) > 0) {
        switch (rec.kind) {
        case MF_REC_TITLE: title_ok = mf_str_eq(rec.f[0], "PPSA33286"); break;
        case MF_REC_ENGINE: engine_ok = mf_str_eq(rec.f[0], "unreal"); break;
        case MF_REC_FRAMEWORK: fw_ok = mf_str_eq(rec.f[0], "ue4ss-ps5"); break;
        case MF_REC_MOD: mod_ok = mf_str_eq(rec.f[0], "TownfallItemSpawner") && mf_str_eq(rec.f[1], "1.0.0"); break;
        case MF_REC_FILE: {
            ++files;
            if (mf_str_eq(rec.f[1], "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua")) {
                main_listed = mf_str_eq(rec.f[0], "lua_mod") && rec.size == 21415;
            } else if (mf_str_eq(rec.f[1], "ue4ss/Mods/TownfallItemSpawner/enabled.txt")) {
                enabled_listed = mf_str_eq(rec.f[0], "lua_mod") && rec.size == 0;
            } else if (mf_str_eq(rec.f[1], "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua")) {
                uehelpers_listed = mf_str_eq(rec.f[0], "lua_mod");
            } else if (mf_str_eq(rec.f[1], "ue4ss/UE4SS-settings.ini")) {
                settings_listed = mf_str_eq(rec.f[0], "config");
            } else if (mf_str_eq(rec.f[1], "ue4ss/MemberVariableLayout.ini") ||
                       mf_str_eq(rec.f[1], "ue4ss/VTableLayout.ini")) {
                layout_listed = mf_str_eq(rec.f[0], "config");
            }
            break;
        }
        default: break;
        }
    }
    CHECK(rc == 0, "T1 loadlist parsed to end without malformation");
    CHECK(title_ok && engine_ok && fw_ok && mod_ok, "T1 title/engine/framework/mod records");
    CHECK(files == 6 && main_listed && enabled_listed && uehelpers_listed && settings_listed && layout_listed,
          "T1 six file records with expected roles/sizes");

    /* T2: mfa_activate over the full root */
    mfa_config cfg; memset(&cfg, 0, sizeof cfg);
    cfg.root = root_full; cfg.title_id = "PPSA33286";
    mfa_activation *act = NULL; mfa_error err;
    int res = mfa_activate(&cfg, &act, &err);
    if (res != MFA_OK)
        printf("FAIL mfa_activate: %d %s (line %d path %s)\n", res, err.message, err.line, err.path);
    CHECK(res == MFA_OK, "T2 mfa_activate(stage-root-full) == MFA_OK");
    CHECK(act && strcmp(mfa_title(act), "PPSA33286") == 0, "T2 mfa_title == PPSA33286");
    CHECK(act && strcmp(mfa_engine(act), "unreal") == 0, "T2 mfa_engine == unreal");
    CHECK(act && mfa_wants_framework(act, "ue4ss-ps5") == 1, "T2 mfa_wants_framework(ue4ss-ps5)");
    CHECK(act && mfa_mod_count(act) == 1 && strcmp(mfa_mod_at(act, 0)->id, "TownfallItemSpawner") == 0,
          "T2 one mod record: TownfallItemSpawner 1.0.0");
    int all_preloaded = 1;
    for (size_t i = 0; act && i < mfa_file_count(act); ++i)
        if (!mfa_file_at(act, i)->data) all_preloaded = 0;
    CHECK(act && all_preloaded && mfa_file_count(act) == 6, "T2 all six files preloaded (roles lua_mod/config)");

    /* T3: byte-identity with pinned sources */
    if (act) {
        file_equals(act, "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", src_main,
                    "T3 main.lua bytes == retained intake member (f586043a...658)");
        file_equals(act, "ue4ss/Mods/TownfallItemSpawner/enabled.txt", src_enabled,
                    "T3 enabled.txt bytes == retained empty member (e3b0c442...855)");
        file_equals(act, "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", src_uehelpers,
                    "T3 UEHelpers.lua bytes == retained distro asset (5a8f6609...15a7)");
        const mfa_file *s = mfa_find(act, "ue4ss/UE4SS-settings.ini");
        int has_engine_tick = s && memmem(s->data, s->size, "DefaultExecuteInGameThreadMethod=EngineTick", 40) != NULL;
        int overrides_empty = s && memmem(s->data, s->size, "ModsFolderPath=\n", 15) != NULL;
        CHECK(has_engine_tick, "T3 settings: DefaultExecuteInGameThreadMethod=EngineTick present");
        CHECK(overrides_empty, "T3 settings: Overrides.ModsFolderPath empty (native activation constraint)");
    }

    /* T4: enable-logic oracle */
    if (act) {
        char names[16][128], scripts[16][256], startup[16][128];
        int count = 0, startup_count = 0;
        mods_oracle(act, names, scripts, &count, startup, &startup_count);
        CHECK(count == 1 && strcmp(names[0], "TownfallItemSpawner") == 0,
              "T4 lua_mods == [TownfallItemSpawner] (shared/ excluded)");
        CHECK(startup_count == 1 && strcmp(startup[0], "TownfallItemSpawner") == 0,
              "T4 startup_mod_names == [TownfallItemSpawner] via enabled.txt marker");
        CHECK(mfa_find(act, "ue4ss/Mods/mods.txt") == NULL, "T4 no mods.txt shipped (matches census)");
    }

    /* T5: strict root — no layout keys at all */
    mfa_activation *strict = NULL;
    mfa_config scfg; memset(&scfg, 0, sizeof scfg);
    scfg.root = root_strict; scfg.title_id = "PPSA33286";
    int sres = mfa_activate(&scfg, &strict, &err);
    CHECK(sres == MFA_OK, "T5 strict root (no layout files) still activates");
    CHECK(strict && mfa_find(strict, "ue4ss/MemberVariableLayout.ini") == NULL &&
          mfa_find(strict, "ue4ss/VTableLayout.ini") == NULL,
          "T5 strict root lacks both REQUIRED layout keys -> runtime hard-stops at UE4SSProgram.cpp:903");
    if (strict) {
        const mfa_file *ph = mfa_find(act, "ue4ss/MemberVariableLayout.ini");
        int ph_is_placeholder = ph && memmem(ph->data, ph->size, "NOT validated layout facts", 26) != NULL;
        CHECK(ph_is_placeholder,
              "T5 full-root layout key carries placeholder bytes explicitly marked NOT layout facts");
    }

    /* T6: validation failures */
    mfa_activation *bad = NULL;
    mfa_config wcfg; memset(&wcfg, 0, sizeof wcfg);
    wcfg.root = root_full; wcfg.title_id = "PPSA99999";
    CHECK(mfa_activate(&wcfg, &bad, &err) == MFA_E_TITLE && bad == NULL,
          "T6 wrong title refused (MFA_E_TITLE)");

    char tmp[] = "/tmp/mbundle-tamper-XXXXXX";
    if (mkdtemp(tmp)) {
        char cmd[2048];
        snprintf(cmd, sizeof cmd, "cp -R '%s/.' '%s/' && truncate -s 100 '%s/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua'",
                 root_full, tmp, tmp);
        if (system(cmd) == 0) {
            mfa_config tcfg; memset(&tcfg, 0, sizeof tcfg);
            tcfg.root = tmp; tcfg.title_id = "PPSA33286";
            CHECK(mfa_activate(&tcfg, &bad, &err) == MFA_E_SIZE && bad == NULL,
                  "T6 truncated main.lua refused (MFA_E_SIZE)");
        }
        snprintf(cmd, sizeof cmd, "rm -rf '%s/loadlist.txt'", tmp);
        if (system(cmd) == 0) {
            mfa_config ncfg; memset(&ncfg, 0, sizeof ncfg);
            ncfg.root = tmp; ncfg.title_id = "PPSA33286";
            CHECK(mfa_activate(&ncfg, &bad, &err) == MFA_E_NO_LOADLIST && bad == NULL,
                  "T6 missing loadlist refused (MFA_E_NO_LOADLIST)");
        }
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmp);
        system(cmd);
    }

    if (act) mfa_release(act);
    if (strict) mfa_release(strict);
    free(ll);
    printf(failures ? "\nRESULT: %d FAILURE(S)\n" : "\nRESULT: ALL PASS\n", failures);
    return failures ? 1 : 0;
}

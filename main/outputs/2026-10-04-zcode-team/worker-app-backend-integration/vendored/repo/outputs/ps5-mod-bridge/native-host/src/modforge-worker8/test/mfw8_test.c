/* mfw8_test.c — host tests for the worker-8 drop-in vertical.
 *
 * Runs the records store, the job actions and the modforge-service adapter
 * against the REAL composed installer binary (proven lineage binary on disk)
 * and, for the archive path, a REAL downloaded corpus zip. No network, no
 * console; everything in a fresh work root supplied by run_tests.sh.
 *
 * argv: mfw8_test <installer_path> <work_root> <corpus_zip>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../mfw8_actions.h"
#include "../mfw8_caps.h"
#include "../mfw8_records.h"
#include "../mfw8_service.h"

static int passed, failed;

#define CHECK(cond, name) do { \
    if (cond) { ++passed; printf("PASS %s\n", name); } \
    else { ++failed; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)

static void mkdirs(const char *p) {
    char cmd[512];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s'", p);
    if (system(cmd)) { /* best effort for test scaffolding */ }
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

static int dir_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char *reply_field(const char *json, const char *key, char *out, size_t cap) {
    char needle[128];
    const char *hit, *end;
    size_t len;
    out[0] = 0;
    snprintf(needle, sizeof needle, "\"%s\":", key);
    hit = strstr(json, needle);
    if (!hit) return out;
    hit += strlen(needle);
    while (*hit == ' ') ++hit;
    if (*hit == '"') {
        ++hit;
        end = strchr(hit, '"');
        if (!end) return out;
        len = (size_t)(end - hit);
    } else {
        end = hit;
        while (*end && *end != ',' && *end != '}') ++end;
        len = (size_t)(end - hit);
    }
    if (len >= cap) len = cap - 1;
    memcpy(out, hit, len);
    out[len] = 0;
    return out;
}

static mfs_owned call(mfs_service *svc, const char *method, const char *params) {
    char req[2048];
    mfs_view view;
    if (params)
        snprintf(req, sizeof req, "{\"method\":\"%s\",\"params\":%s}", method, params);
    else
        snprintf(req, sizeof req, "{\"method\":\"%s\",\"params\":{}}", method);
    view.bytes = req;
    view.length = strlen(req);
    return mfs_request(svc, view);
}

int main(int argc, char **argv) {
    const char *installer, *work, *corpus;
    char state[MFW8_PATH_MAX], roots[MFW8_PATH_MAX], intake[MFW8_PATH_MAX];
    char title[64];
    mfw8_actions_config cfg;
    mfs_service *svc = NULL;

    if (argc != 4) { fprintf(stderr, "usage: mfw8_test <installer> <work_root> <corpus_zip>\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);   /* unbuffered: progress visible even when stuck */
    installer = argv[1];
    work = argv[2];
    corpus = argv[3];
    snprintf(state, sizeof state, "%s/state", work);
    snprintf(roots, sizeof roots, "%s/roots", work);
    snprintf(intake, sizeof intake, "%s/state/intake", work);
    snprintf(title, sizeof title, "PPSA33286");
    mkdirs(state);
    mkdirs(roots);
    mkdirs(intake);
    memset(&cfg, 0, sizeof cfg);
    cfg.installer_path = installer;
    cfg.state_dir = state;
    cfg.roots_base = roots;

    /* ---- 1. records store unit semantics ---- */
    {
        static mfw8_records recs;
        mfw8_records_load(&recs, state);
        CHECK(recs.nmods == 0 && recs.ngames == 0, "missing records.txt = empty store");
        CHECK(mfw8_register_download(&recs, "dl-a", "PPSA33286", NULL) == 0, "register download");
        CHECK(mfw8_register_download(&recs, "dl-a", "PPSA33286", NULL) < 0, "duplicate register refused");
        CHECK(mfw8_register_download(&recs, "bad id", "PPSA33286", NULL) < 0, "non-token id refused");
        CHECK(mfw8_records_store(&recs, state) == 0, "records store atomic write");
        {
            static mfw8_records again;
            CHECK(mfw8_records_load(&again, state) == 0 && again.nmods == 1, "records reload");
            CHECK(!strcmp(again.mods[0].state, "downloaded"), "new row state downloaded");
            CHECK(!strcmp(again.mods[0].bundle, "-"), "bundleless row uses proven '-' placeholder");
        }
        CHECK(mfw8_mark_prepared(&recs, "nope", NULL, NULL) < 0, "prepare unknown refused");
        CHECK(mfw8_mark_installed(&recs, (const char *[]){"dl-a"}, 1, title, 0, "x") < 0,
              "install before prepare refused");
        CHECK(mfw8_mark_prepared(&recs, "dl-a", "unreal", "5.3") == 0, "downloaded->prepared");
        CHECK(mfw8_mark_prepared(&recs, "dl-a", NULL, NULL) == 1, "re-prepare idempotent");
        CHECK(mfw8_records_store(&recs, state) == 0, "store prepared");
        CHECK(mfw8_mark_installed(&recs, (const char *const *)0, 0, title, 0, NULL) < 0,
              "install requires loadlist sha");
    }
    /* ---- 2. corrupt store refused ---- */
    {
        char path[MFW8_PATH_MAX];
        FILE *f;
        static mfw8_records recs;
        snprintf(path, sizeof path, "%s/records.txt", state);
        f = fopen(path, "w");
        fprintf(f, "modforge-records\t2\n");
        fclose(f);
        CHECK(mfw8_records_load(&recs, state) == -2, "unknown header refused as corrupt");
        f = fopen(path, "w");
        fprintf(f, "modforge-records\t1\nmod\tx\tPPSA33286\tflying\t-\n");
        fclose(f);
        CHECK(mfw8_records_load(&recs, state) == -2, "unknown state refused as corrupt");
        unlink(path);
    }
    /* ---- 3. actions against the REAL composed binary ---- */
    {
        char bundle[MFW8_PATH_MAX], out[MFW8_PATH_MAX];
        mfw8_outcome outcome;
        static mfw8_records recs;
        char cmd[MFW8_PATH_MAX * 2];
        /* build a real kind-1 mfb fixture with the binary's own make-bundle */
        snprintf(bundle, sizeof bundle, "%s/mm-skin.mfb", work);
        snprintf(cmd, sizeof cmd,
            "'%s' make-bundle --id mm-skin --title PPSA33286 --engine unreal "
            "--file pak,mods/~mods/MM_Skin_P.pak=FAKEDATA -o '%s' >/dev/null 2>&1",
            installer, bundle);
        CHECK(system(cmd) == 0 && file_exists(bundle), "real make-bundle fixture");

        memset(&outcome, 0, sizeof outcome);
        {
            mfw8_register_spec spec = {"mm-skin", title, bundle};
            CHECK(mfw8_act_register(&cfg, &spec, &outcome) == 0, "register action ran");
            CHECK(outcome.exit_code == 0 && !strcmp(outcome.state, "succeeded"),
                  "register succeeded (exit contract 0)");
        }
        mfw8_records_load(&recs, state);
        {
            const mfw8_mod *m = mfw8_find_mod(&recs, "mm-skin");
            CHECK(m && !strcmp(m->state, "downloaded"), "real binary wrote downloaded row");
        }
        memset(&outcome, 0, sizeof outcome);
        {
            mfw8_prepare_spec spec = {title, "mm-skin", NULL};
            CHECK(mfw8_act_prepare(&cfg, &spec, &outcome) == 0 && outcome.exit_code == 0,
                  "prepare action succeeded");
        }
        mfw8_records_load(&recs, state);
        CHECK(mfw8_find_mod(&recs, "mm-skin") &&
              !strcmp(mfw8_find_mod(&recs, "mm-skin")->state, "prepared"),
              "real binary committed downloaded->prepared");

        /* capability report BEFORE any install/ack: honest zeros */
        {
            char report[MFW8_CAPS_OUTPUT_MAX];
            mfw8_caps_input in = {0};
            char field[64];
            in.state_dir = state;
            in.roots_base = roots;
            in.installer_path = installer;
            in.title = title;
            CHECK(mfw8_caps_report(&in, report, sizeof report) == 0, "caps report built");
            CHECK(strstr(report, "\"installer\":{") && strstr(report, "\"present\":true"),
                  "caps reports installer present");
            CHECK(!strcmp(reply_field(report, "running", field, sizeof field), "false"),
                  "caps running=false before ack");
            CHECK(strstr(report, "\"state\":\"not-started\"") &&
                  strstr(report, "\"ack_present\":false"),
                  "caps runtime not-started before ack");
            CHECK(strstr(report, "\"installation_supported\":false"),
                  "caps installation_supported=false without evidence");
        }

        memset(&outcome, 0, sizeof outcome);
        {
            mfw8_install_spec spec;
            memset(&spec, 0, sizeof spec);
            spec.title = title;
            spec.mods_csv = "mm-skin";
            CHECK(mfw8_act_install(&cfg, &spec, &outcome) == 0 && outcome.exit_code == 0,
                  "install action succeeded");
        }
        mfw8_records_load(&recs, state);
        CHECK(mfw8_find_mod(&recs, "mm-skin") &&
              !strcmp(mfw8_find_mod(&recs, "mm-skin")->state, "installed"),
              "real binary committed prepared->installed");
        {
            const mfw8_game *g = mfw8_find_game(&recs, title);
            CHECK(g && g->generation == 1 && strlen(g->loadlist_sha) == 64,
                  "game row committed generation+loadlist sha");
        }
        snprintf(out, sizeof out, "%s/%s", roots, title);
        CHECK(dir_exists(out) && file_exists((snprintf(cmd, sizeof cmd, "%s/.modforge-root", out), cmd)),
              "staged root carries the ownership marker");

        /* ack gate: running ONLY from the runtime ack file */
        {
            char report[MFW8_CAPS_OUTPUT_MAX];
            mfw8_caps_input in = {0};
            mfw8_runtime_state rt;
            char cmd2[MFW8_PATH_MAX * 2];
            in.state_dir = state;
            in.roots_base = roots;
            in.installer_path = installer;
            in.title = title;
            CHECK(mfw8_runtime_probe(roots, title, &rt) == 0 && rt.ack_present == 0 &&
                  rt.running == 0, "no ack file -> not running");
            snprintf(cmd2, sizeof cmd2,
                "'%s' --state '%s' --roots '%s' simulate-runtime --title %s --rt-state mods-started >/dev/null 2>&1",
                installer, state, roots, title);
            CHECK(system(cmd2) == 0, "runtime ack written via proven hook");
            CHECK(mfw8_runtime_probe(roots, title, &rt) == 0 && rt.running == 1 &&
                  !strcmp(rt.state, "mods-started"), "ack mods-started -> running");
            CHECK(mfw8_caps_report(&in, report, sizeof report) == 0 &&
                  strstr(report, "\"running\":true"), "caps running=true only after ack");
        }

        /* cancel: queued safe-point cancel leaves the installed set untouched */
        {
            char bundle2[MFW8_PATH_MAX], cmd2[MFW8_PATH_MAX * 2];
            mfw8_outcome outcome2;
            static mfw8_records before, after;
            snprintf(bundle2, sizeof bundle2, "%s/mm-lua.mfb", work);
            snprintf(cmd2, sizeof cmd2,
                "'%s' make-bundle --id mm-lua --title PPSA33286 --engine unreal "
                "--file lua_mod,ue4ss/Mods/ML/Scripts/main.lua=X -o '%s' >/dev/null 2>&1",
                installer, bundle2);
            (void)system(cmd2);
            {
                mfw8_register_spec spec = {"mm-lua", title, bundle2};
                (void)mfw8_act_register(&cfg, &spec, &outcome2);
                mfw8_prepare_spec pspec = {title, "mm-lua", NULL};
                (void)mfw8_act_prepare(&cfg, &pspec, &outcome2);
            }
            mfw8_records_load(&before, state);
            memset(&outcome2, 0, sizeof outcome2);
            {
                mfw8_install_spec spec;
                memset(&spec, 0, sizeof spec);
                spec.title = title;
                spec.mods_csv = "mm-skin,mm-lua";
                spec.cancel_at = "pre-stage";
                CHECK(mfw8_act_install(&cfg, &spec, &outcome2) == 0 &&
                      outcome2.cancelled && !strcmp(outcome2.state, "cancelled"),
                      "cancel-at pre-stage -> cancelled outcome");
            }
            mfw8_records_load(&after, state);
            CHECK(before.nmods == after.nmods &&
                  mfw8_find_mod(&after, "mm-skin") &&
                  !strcmp(mfw8_find_mod(&after, "mm-skin")->state, "installed") &&
                  mfw8_find_mod(&after, "mm-lua") &&
                  !strcmp(mfw8_find_mod(&after, "mm-lua")->state, "prepared"),
                  "cancelled install left installed set exactly as before");
        }

        /* uninstall: installed -> prepared, files gone */
        memset(&outcome, 0, sizeof outcome);
        CHECK(mfw8_act_uninstall(&cfg, title, "mm-skin", &outcome) == 0 && outcome.exit_code == 0,
              "uninstall action succeeded");
        mfw8_records_load(&recs, state);
        CHECK(mfw8_find_mod(&recs, "mm-skin") &&
              !strcmp(mfw8_find_mod(&recs, "mm-skin")->state, "prepared"),
              "uninstall committed installed->prepared");
        snprintf(out, sizeof out, "%s/%s/mods/~mods/MM_Skin_P.pak", roots, title);
        CHECK(!file_exists(out), "uninstalled file removed from managed root");
    }
    /* ---- 4. service adapter (drop-in mfs ABI) ---- */
    {
        char config[MFW8_PATH_MAX * 2];
        mfs_owned err, reply;
        char field[128];

        snprintf(config, sizeof config,
            "{\"state_dir\":\"%s\",\"mod_root_base\":\"%s\",\"installer_path\":\"%s\","
            "\"data_dir\":\"%s/share\",\"limits\":{\"max_archive_entries\":10000}}",
            state, roots, installer, state);
        err.bytes = NULL;
        CHECK(mfs_open(NULL, &svc, &err) != 0, "mfs_open refuses bad abi");
        if (err.bytes) mfs_release(&err);
        {
            mfs_open_params p;
            memset(&p, 0, sizeof p);
            p.abi_version = MFS_ABI_VERSION;
            p.struct_size = sizeof p;
            p.config_json.bytes = config;
            p.config_json.length = strlen(config);
            CHECK(mfs_open(&p, &svc, &err) == 0 && svc, "mfs_open with app-shaped config");
            if (err.bytes) mfs_release(&err);
        }
        reply = call(svc, "service.describe", NULL);
        CHECK(reply.bytes && strstr(reply.bytes, MFS_CONTRACT) &&
              strstr(reply.bytes, "\"mod.register_download\"") &&
              strstr(reply.bytes, "\"records.list\""), "describe advertises contract+required methods");
        CHECK(reply.bytes && strstr(reply.bytes, "\"installer_present\":true"),
              "describe reports installer honestly");
        mfs_release(&reply);

        reply = call(svc, "unknown.thing", NULL);
        CHECK(reply.bytes && strstr(reply.bytes, "\"ok\":false") &&
              strstr(reply.bytes, "unknown_method"), "unknown method refused");
        mfs_release(&reply);
        {
            mfs_view bad = {"{\"method\":", 11};
            reply = mfs_request(svc, bad);
            CHECK(reply.bytes && strstr(reply.bytes, "\"ok\":false"), "malformed JSON refused");
            mfs_release(&reply);
        }

        /* app-path registration: dl- id + source identity -> records.list mods[] */
        {
            char params[1024];
            char arch[MFW8_PATH_MAX];
            snprintf(params, sizeof params,
                "{\"download_id\":\"dl-%064d\",\"title_id\":\"%s\",\"source\":"
                "{\"provider\":\"nexus\",\"domain\":\"townfall\",\"mod_id\":42763,\"file_id\":51234567}}",
                1, title);
            reply = call(svc, "mod.register_download", params);
            CHECK(reply.bytes && strstr(reply.bytes, "\"state\":\"downloaded\""),
                  "register_download accepted");
            CHECK(reply.bytes && strstr(reply.bytes, "\"mod_id\":42763"),
                  "register reply carries source identity");
            mfs_release(&reply);
            reply = call(svc, "records.list", NULL);
            CHECK(reply.bytes && strstr(reply.bytes, "\"download_id\":\"dl-") &&
                  strstr(reply.bytes, "\"provider\":\"nexus\"") &&
                  strstr(reply.bytes, "\"domain\":\"townfall\"") &&
                  strstr(reply.bytes, "\"state\":\"downloaded\""),
                  "records.list mods[] has register_bridge shape");
            CHECK(reply.bytes && strstr(reply.bytes, "\"installer_records\":[{\"id\":\"mm-skin\""),
                  "CLI-registered row reported in installer_records (not hidden)");
            mfs_release(&reply);

            /* duplicate identity refused */
            reply = call(svc, "mod.register_download", params);
            CHECK(reply.bytes && strstr(reply.bytes, "\"ok\":false"), "duplicate register refused");
            mfs_release(&reply);

            /* archive prepare through the proven auto-bridge with the REAL corpus zip */
            snprintf(arch, sizeof arch, "%s/dl-%064d.zip", intake, 1);
            {
                char cp[MFW8_PATH_MAX * 2];
                snprintf(cp, sizeof cp, "cp '%s' '%s'", corpus, arch);
                CHECK(system(cp) == 0 && file_exists(arch), "corpus zip staged into intake");
            }
            {
                char p2[128];
                snprintf(p2, sizeof p2, "{\"title_id\":\"%s\",\"mod_ids\":[\"dl-%064d\"]}", title, 1);
                reply = call(svc, "mod.prepare", p2);
                CHECK(reply.bytes && strstr(reply.bytes, "\"state\":\"succeeded\""),
                      "archive prepare via auto-bridge succeeded");
                mfs_release(&reply);
                snprintf(arch, sizeof arch, "%s/state/bundles/dl-%064d.mfb", work, 1);
                CHECK(file_exists(arch), "auto-bridge composed a kind-1 bundle");
                reply = call(svc, "records.list", NULL);
                CHECK(reply.bytes && strstr(reply.bytes, "\"state\":\"prepared\""),
                      "records show prepared after archive prepare");
                mfs_release(&reply);

                /* install the archive mod end-to-end through the service */
                reply = call(svc, "install.apply", p2);
                CHECK(reply.bytes && strstr(reply.bytes, "\"state\":\"succeeded\""),
                      "install.apply succeeded");
                mfs_release(&reply);
                reply = call(svc, "records.list", NULL);
                CHECK(reply.bytes && strstr(reply.bytes, "\"state\":\"installed\""),
                      "records show installed after apply");
                mfs_release(&reply);
                reply = call(svc, "mod.uninstall", p2);
                CHECK(reply.bytes && strstr(reply.bytes, "\"state\":\"succeeded\""),
                      "mod.uninstall succeeded");
                mfs_release(&reply);

                /* cancel: unknown job refused, terminal job replies state */
                reply = call(svc, "job.cancel", "{\"job_id\":\"j-999999\"}");
                CHECK(reply.bytes && strstr(reply.bytes, "unknown_job"), "cancel unknown job refused");
                mfs_release(&reply);
                reply = call(svc, "job.list", NULL);
                CHECK(reply.bytes && strstr(reply.bytes, "mod.prepare") &&
                      strstr(reply.bytes, "install.apply"), "job.list lists executed jobs");
                mfs_release(&reply);
                {
                    char p3[128];
                    snprintf(p3, sizeof p3, "{\"title_id\":\"%s\"}", title);
                    reply = call(svc, "game.status", p3);
                }
                CHECK(reply.bytes && strstr(reply.bytes, "\"running\":false"),
                      "game.status honest (no fabricated ack)");
                mfs_release(&reply);
            }
        }
        mfs_close(svc);
    }

    printf("\n==== mfw8 host tests: PASS=%d FAIL=%d ====\n", passed, failed);
    return failed ? 1 : 0;
}

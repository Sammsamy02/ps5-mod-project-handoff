/*
 * test_multi_mod.c — host tests for the M2 N-mod lifecycle prototype.
 *
 * Build: cc -std=c99 -O2 -Wall -Wextra -o /tmp/mm_test test_multi_mod.c multi_mod_lifecycle.c
 * Run:   /tmp/mm_test    (exit 0 = all checks pass; artifacts in /tmp)
 *
 * Scenario coverage (task item 5):
 *   T1  3-mod install with a conflict (refused, actionable reason)
 *   T2  dependency chain install in shuffled orders (deterministic order)
 *   T3  selective uninstall in the middle of a chain (refused, then forced)
 *   T4  enable/disable toggle + generation bumps
 *   T5  recovery against live-root truth (load list + per-mod dirs)
 *   T6  failure evidence hooks (mark/clear, actions refused while failed)
 *   T7  classification: all four states reachable with detection rules
 *   T8  dependency version gate
 *   T9  capacity gate
 *   T10 cycle -> no deterministic load order
 *   T11 records round-trip + legacy 5-column line compatibility
 *   T12 UI state surfacing + legal actions per state
 *   T13 plan purity (validation mutates nothing before commit)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "multi_mod_lifecycle.h"

static int g_checks, g_fails;

#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg); } \
} while (0)

#define CHECK_STR(a, b, msg) do { \
    g_checks++; \
    if (strcmp((a), (b)) != 0) { \
        g_fails++; \
        printf("FAIL %s:%d %s (got '%s' want '%s')\n", __FILE__, __LINE__, msg, (a), (b)); \
    } \
} while (0)

static void reset_store(mm_store *st)
{
    memset(st, 0, sizeof *st);
    snprintf(st->title, sizeof st->title, "PPSA33286");
    st->capacity = 0;
}

/* caps table seeded the way modinst.c seeds caps.tsv from engine facts */
static void seed_caps(mm_caps *caps)
{
    memset(caps, 0, sizeof *caps);
    snprintf(caps->caps[caps->n_caps].id, MM_ID_MAX, "ue4ss-lua"); 
    snprintf(caps->caps[caps->n_caps].maturity, 16, "ported");
    caps->n_caps++;
    snprintf(caps->caps[caps->n_caps].id, MM_ID_MAX, "pak-loader");
    snprintf(caps->caps[caps->n_caps].maturity, 16, "ported");
    caps->n_caps++;
    snprintf(caps->caps[caps->n_caps].id, MM_ID_MAX, "ue4ss-ps5");
    snprintf(caps->caps[caps->n_caps].maturity, 16, "integrating");
    caps->n_caps++;
    snprintf(caps->caps[caps->n_caps].id, MM_ID_MAX, "reshade-vk");
    snprintf(caps->caps[caps->n_caps].maturity, 16, "planned");
    caps->n_caps++;
    snprintf(caps->fams[caps->n_fams].family, MM_ID_MAX, "asset-override");
    snprintf(caps->fams[caps->n_fams].verdict, 16, "native");
    snprintf(caps->fams[caps->n_fams].shared_policy, 16, "last-wins");
    caps->n_fams++;
    snprintf(caps->fams[caps->n_fams].family, MM_ID_MAX, "container-repack");
    snprintf(caps->fams[caps->n_fams].verdict, 16, "native");
    snprintf(caps->fams[caps->n_fams].shared_policy, 16, "refuse");
    caps->n_fams++;
    snprintf(caps->fams[caps->n_fams].family, MM_ID_MAX, "bsa-archive");
    snprintf(caps->fams[caps->n_fams].verdict, 16, "conversion");
    snprintf(caps->fams[caps->n_fams].shared_policy, 16, "last-wins");
    caps->n_fams++;
}

/* the three coexisting mods of the M2 exit criterion */
static mm_manifest mf_a(void)   /* asset base, no deps */
{
    mm_manifest m;
    memset(&m, 0, sizeof m);
    snprintf(m.id, MM_ID_MAX, "asset-base");
    snprintf(m.title, sizeof m.title, "Asset Base");
    snprintf(m.version, sizeof m.version, "1.0.0");
    m.priority = 100; m.layout = 1;
    snprintf(m.bundle, sizeof m.bundle, "/mods/asset-base.mfb");
    snprintf(m.family, sizeof m.family, "asset-override");
    m.n_files = 2;
    snprintf(m.files[0], MM_PATH_MAX, "text/ids.txt");
    snprintf(m.files[1], MM_PATH_MAX, "content/pak/base.pak");
    return m;
}
static mm_manifest mf_b(void)   /* depends on a >= 1.0.0; shares text/ids.txt */
{
    mm_manifest m = mf_a();
    snprintf(m.id, MM_ID_MAX, "asset-overhaul");
    snprintf(m.title, sizeof m.title, "Asset Overhaul");
    snprintf(m.version, sizeof m.version, "1.2.0");
    m.priority = 50;
    snprintf(m.bundle, sizeof m.bundle, "/mods/asset-overhaul.mfb");
    m.n_files = 1;
    snprintf(m.files[0], MM_PATH_MAX, "text/ids.txt");
    m.n_deps = 1;
    snprintf(m.dep_id[0], MM_ID_MAX, "asset-base");
    snprintf(m.dep_min[0], MM_VER_MAX, "1.0.0");
    return m;
}
static mm_manifest mf_c(void)   /* ui menu, depends on b >= 1.0.0 */
{
    mm_manifest m = mf_a();
    snprintf(m.id, MM_ID_MAX, "ui-menu");
    snprintf(m.title, sizeof m.title, "UI Menu");
    snprintf(m.version, sizeof m.version, "2.0.1");
    m.priority = 200;
    snprintf(m.bundle, sizeof m.bundle, "/mods/ui-menu.mfb");
    m.n_files = 1;
    snprintf(m.files[0], MM_PATH_MAX, "ui/menu.txt");
    m.n_deps = 1;
    snprintf(m.dep_id[0], MM_ID_MAX, "asset-overhaul");
    snprintf(m.dep_min[0], MM_VER_MAX, "1.0.0");
    return m;
}
static mm_manifest mf_d(void)   /* container clash vs a's base.pak (refuse) */
{
    mm_manifest m = mf_a();
    snprintf(m.id, MM_ID_MAX, "container-clash");
    snprintf(m.version, sizeof m.version, "1.0.0");
    m.priority = 300;
    snprintf(m.family, sizeof m.family, "container-repack");
    m.n_files = 1;
    snprintf(m.files[0], MM_PATH_MAX, "content/pak/base.pak");
    return m;
}
static mm_manifest mf_e(void)   /* bsa family -> needs conversion */
{
    mm_manifest m = mf_a();
    snprintf(m.id, MM_ID_MAX, "legacy-bsa");
    snprintf(m.version, sizeof m.version, "1.0.0");
    snprintf(m.family, sizeof m.family, "bsa-archive");
    m.n_files = 0;
    return m;
}
static mm_manifest mf_f(void)   /* planned framework -> unsupported */
{
    mm_manifest m = mf_a();
    snprintf(m.id, MM_ID_MAX, "reshade-pack");
    snprintf(m.version, sizeof m.version, "1.0.0");
    m.n_files = 0;
    m.n_fw = 1;
    snprintf(m.fw[0], MM_ID_MAX, "reshade-vk");
    return m;
}

int main(void)
{
    mm_store st;
    mm_caps caps;
    mm_plan plan;
    char reason[MM_REASON_MAX];
    mm_result rc;
    int perm[6][3] = { {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0} };
    int p, i;
    const char *ids3[3] = { "asset-base", "asset-overhaul", "ui-menu" };
    mm_manifest base = mf_a(), over = mf_b(), menu = mf_c(),
                clash = mf_d(), bsa = mf_e(), reshadepk = mf_f();

    seed_caps(&caps);

    /* ---- T13/T2/T1: declare the chain, plan purity, shuffled installs ---- */
    reset_store(&st);
    CHECK(mm_declare(&st, &base) == MM_OK, "declare a");
    CHECK(mm_declare(&st, &over) == MM_OK, "declare b");
    CHECK(mm_declare(&st, &menu) == MM_OK, "declare c");
    CHECK(st.n_mods == 3, "three declared");
    CHECK(mm_state_name(st.recs[0].state), "state name non-null");
    CHECK_STR(mm_state_name(MM_DOWNLOADED), "downloaded", "downloaded name");
    CHECK_STR(mm_state_name(MM_INSTALLED_DISABLED), "installed_disabled",
              "disabled name");

    /* dependency chain refuses partial installs (b needs a, c needs b) */
    { const char *one[1] = { "ui-menu" };
      rc = mm_plan_install(&st, one, 1, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_ERR_BLOCKED_DEP, "c alone blocked on dep");
      CHECK(strstr(reason, "asset-overhaul") != 0, "reason names the missing dep"); }
    { const char *two[2] = { "ui-menu", "asset-overhaul" };
      rc = mm_plan_install(&st, two, 2, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_ERR_BLOCKED_DEP, "c+b blocked on a"); }

    /* plan purity: nothing mutated by refused or successful plans */
    for (p = 0; p < 6; ++p) {
        const char *shuf[3];
        for (i = 0; i < 3; ++i) shuf[i] = ids3[perm[p][i]];
        rc = mm_plan_install(&st, shuf, 3, &caps, &plan, reason, sizeof reason);
        CHECK(rc == MM_OK, "shuffled plan ok");
        if (rc != MM_OK) { printf("  reason: %s\n", reason); continue; }
        CHECK(plan.n == 3, "plan holds the target set");
        CHECK_STR(plan.ids[plan.order[0]], "asset-base", "order[0] a");
        CHECK_STR(plan.ids[plan.order[1]], "asset-overhaul", "order[1] b");
        CHECK_STR(plan.ids[plan.order[2]], "ui-menu", "order[2] c");
        CHECK(plan.new_generation == 1, "first generation");
    }
    CHECK(st.recs[0].state == MM_DOWNLOADED, "plan did not mutate a");
    CHECK(st.recs[2].state == MM_DOWNLOADED, "plan did not mutate c");

    CHECK(mm_install_commit(&st, &plan) == MM_OK, "commit");
    CHECK(st.recs[0].state == MM_INSTALLED_ENABLED, "a enabled after commit");
    CHECK(st.recs[1].state == MM_INSTALLED_ENABLED, "b enabled after commit");
    CHECK(st.recs[2].state == MM_INSTALLED_ENABLED, "c enabled after commit");
    CHECK(st.generation == 1, "generation 1");

    /* ---- T1: 3-mod install WITH a conflict -> refused with a reason ----- */
    CHECK(mm_declare(&st, &clash) == MM_OK, "declare d (container clash)");
    { const char *d[1] = { "container-clash" };
      rc = mm_plan_install(&st, d, 1, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_ERR_CONFLICT, "container clash refused");
      CHECK(strstr(reason, "content/pak/base.pak") != 0,
            "reason names the clashing path");
      CHECK(strstr(reason, "undefined semantics") != 0, "reason is actionable"); }
    CHECK(mm_declare(&st, &bsa) == MM_OK, "declare e (bsa)");
    CHECK(mm_declare(&st, &reshadepk) == MM_OK, "declare f (reshade)");
    { const char *e[1] = { "legacy-bsa" };
      rc = mm_plan_install(&st, e, 1, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_ERR_CONFLICT, "bsa refused");
      CHECK(strstr(reason, "conversion") != 0, "bsa reason says conversion"); }
    { const char *f[1] = { "reshade-pack" };
      rc = mm_plan_install(&st, f, 1, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_ERR_CONFLICT, "planned framework refused");
      CHECK(strstr(reason, "reshade-vk") != 0, "reason names the framework"); }
    { const char *abc3[3] = { "asset-base", "asset-overhaul", "ui-menu" };
      /* the surviving three still plan clean against the installed set */
      rc = mm_plan_install(&st, abc3, 3, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_OK, "re-select of the installed set stays supported"); }

    /* ---- T4: enable/disable toggle with dependents ---------------------- */
    /* current: a,b,c all enabled, generation 1 */
    rc = mm_disable(&st, "asset-overhaul", reason, sizeof reason);
    CHECK(rc == MM_ERR_BLOCKED_DEPENDENT, "disable b blocked by enabled c");
    CHECK(strstr(reason, "ui-menu") != 0, "disable reason names c");
    CHECK(mm_disable(&st, "ui-menu", reason, sizeof reason) == MM_OK,
          "disable c (leaf)");
    CHECK(st.generation == 2, "generation bumped on disable");
    CHECK_STR(mm_state_name(st.recs[2].state), "installed_disabled", "c disabled");
    CHECK(mm_disable(&st, "ui-menu", reason, sizeof reason) == MM_OK,
          "disable idempotent");
    CHECK(st.generation == 2, "idempotent disable does not bump");
    rc = mm_enable(&st, "asset-overhaul", &caps, reason, sizeof reason);
    CHECK(rc == MM_OK, "enable b idempotent (already enabled)");
    CHECK(st.generation == 2, "idempotent enable does not bump");
    CHECK(mm_disable(&st, "asset-overhaul", reason, sizeof reason) == MM_OK,
          "disable b ok now (its dependent c is disabled)");
    CHECK(st.generation == 3, "generation bumped on disable b");
    rc = mm_enable(&st, "ui-menu", &caps, reason, sizeof reason);
    CHECK(rc == MM_ERR_BLOCKED_DEP, "enable c blocked while b disabled");
    CHECK(strstr(reason, "asset-overhaul") != 0, "enable reason names b");
    CHECK(mm_enable(&st, "asset-overhaul", &caps, reason, sizeof reason) == MM_OK,
          "re-enable b (real transition)");
    CHECK(st.generation == 4, "generation bumped on real enable");
    CHECK(mm_enable(&st, "ui-menu", &caps, reason, sizeof reason) == MM_OK,
          "enable c after its dep returned");
    CHECK(st.generation == 5, "generation bumped again");
    CHECK(mm_enable(&st, "ui-menu", &caps, reason, sizeof reason) == MM_OK,
          "enable idempotent");
    CHECK(st.generation == 5, "idempotent enable does not bump");

    /* ---- T3: selective uninstall in the chain --------------------------- */
    /* current: a,b,c all enabled, generation 5 */
    rc = mm_uninstall(&st, "asset-base", 0, reason, sizeof reason);
    CHECK(rc == MM_ERR_BLOCKED_DEPENDENT, "uninstall a blocked by installed b");
    CHECK(strstr(reason, "asset-overhaul") != 0, "uninstall reason names b");
    CHECK(mm_uninstall(&st, "ui-menu", 0, reason, sizeof reason) == MM_OK,
          "uninstall leaf c (no dependents)");
    CHECK_STR(mm_state_name(st.recs[2].state), "prepared", "c back to prepared");
    CHECK(st.generation == 6, "generation bumped on uninstall");

    /* force-uninstall a after disabling c: removes disabled dependents too */
    reset_store(&st);
    CHECK(mm_declare(&st, &base) == MM_OK, "redeclare a");
    CHECK(mm_declare(&st, &over) == MM_OK, "redeclare b");
    CHECK(mm_declare(&st, &menu) == MM_OK, "redeclare c");
    { const char *abc3[3] = { "asset-base", "asset-overhaul", "ui-menu" };
      CHECK(mm_plan_install(&st, abc3, 3, &caps, &plan, reason, sizeof reason) == MM_OK,
            "chain plan ok");
      CHECK(mm_install_commit(&st, &plan) == MM_OK, "chain commit"); }
    CHECK(mm_disable(&st, "ui-menu", reason, sizeof reason) == MM_OK, "disable c");
    CHECK(mm_disable(&st, "asset-overhaul", reason, sizeof reason) == MM_OK,
          "disable b (its dependent c is already disabled)");
    CHECK(mm_uninstall(&st, "asset-base", 0, reason, sizeof reason)
              == MM_ERR_BLOCKED_DEPENDENT, "no-force uninstall still blocked");
    CHECK(mm_uninstall(&st, "asset-base", 1, reason, sizeof reason) == MM_OK,
          "force uninstall a with only disabled dependents");
    CHECK_STR(mm_state_name(st.recs[0].state), "prepared", "a back to prepared");
    CHECK_STR(mm_state_name(st.recs[1].state), "prepared",
              "disabled dependent b cascaded to prepared");
    CHECK_STR(mm_state_name(st.recs[2].state), "prepared",
              "transitive disabled dependent c cascaded to prepared");
    CHECK(strstr(st.recs[2].reason, "asset-base") != 0,
          "cascade reason references the uninstalled dep");

    /* ---- T11: records round-trip + legacy compatibility ----------------- */
    CHECK(mm_store_save(&st, "/tmp/mm_records.txt") == MM_OK, "save store");
    {
        mm_store loaded;
        CHECK(mm_store_load(&loaded, "/tmp/mm_records.txt") == MM_OK, "load store");
        CHECK(loaded.n_mods == 3, "three rows loaded");
        CHECK_STR(loaded.recs[0].id, "asset-base", "row id");
        CHECK_STR(mm_state_name(loaded.recs[0].state), "prepared", "row state");
        CHECK_STR(loaded.mfs[2].dep_id[0], "asset-overhaul", "dep id round-trip");
        CHECK_STR(loaded.mfs[2].dep_min[0], "1.0.0", "dep min round-trip");
        CHECK(loaded.generation == st.generation, "generation round-trip");
        CHECK_STR(loaded.mfs[1].version, "1.2.0", "version round-trip");
    }
    {
        FILE *f = fopen("/tmp/mm_legacy.txt", "wb");
        /* legacy single-mod store line: mod id title state bundle */
        fputs("mmstore\t1\n", f);
        fputs("game\tPPSA33286\t7\n", f);
        fputs("mod\told-mod\tOld Mod\tinstalled\t/mods/old.mfb\n", f);
        fclose(f);
        {
            mm_store legacy;
            CHECK(mm_store_load(&legacy, "/tmp/mm_legacy.txt") == MM_OK,
                  "legacy load");
            CHECK(legacy.n_mods == 1, "legacy row parsed");
            CHECK_STR(legacy.recs[0].id, "old-mod", "legacy id");
            CHECK_STR(mm_state_name(legacy.recs[0].state), "installed",
                      "legacy state");
            CHECK(legacy.generation == 7, "legacy generation");
            CHECK_STR(legacy.mfs[0].version, "0", "legacy version defaulted");
            CHECK(legacy.mfs[0].n_deps == 0, "legacy has no deps");
        }
    }

    /* ---- T5: recovery against live-root truth --------------------------- */
    reset_store(&st);
    CHECK(mm_declare(&st, &base) == MM_OK, "recover: declare a");
    CHECK(mm_declare(&st, &over) == MM_OK, "recover: declare b");
    CHECK(mm_declare(&st, &menu) == MM_OK, "recover: declare c");
    { const char *abc3[3] = { "asset-base", "asset-overhaul", "ui-menu" };
      CHECK(mm_plan_install(&st, abc3, 3, &caps, &plan, reason, sizeof reason) == MM_OK,
            "recover setup plan");
      CHECK(mm_install_commit(&st, &plan) == MM_OK, "recover setup commit"); }
    st.recs[2].state = MM_INSTALLING;           /* simulate crash residue */
    {
        const char *ll[2] = { "asset-base", "asset-overhaul" };
        const char *dirs[3] = { "asset-base", "asset-overhaul", "ui-menu" };
        CHECK(mm_recover(&st, ll, 2, dirs, 3, 9, reason, sizeof reason) == MM_OK,
              "recover ok");
        CHECK_STR(mm_state_name(st.recs[0].state), "installed", "a in loadlist");
        CHECK_STR(mm_state_name(st.recs[1].state), "installed", "b in loadlist");
        CHECK_STR(mm_state_name(st.recs[2].state), "installed_disabled",
                  "c dir present, not in loadlist -> disabled");
        CHECK(st.generation == 9, "generation adopted from root");
    }
    {
        const char *ll[1] = { "asset-base" };
        const char *dirs[1] = { "asset-base" };
        CHECK(mm_recover(&st, ll, 1, dirs, 1, 10, reason, sizeof reason) == MM_OK,
              "second recover ok");
        CHECK_STR(mm_state_name(st.recs[1].state), "prepared",
                  "b absent from root -> prepared");
        CHECK_STR(mm_state_name(st.recs[2].state), "prepared",
                  "c absent from root -> prepared");
    }

    /* ---- T6: failure evidence hooks ------------------------------------- */
    { const char *abc3[3] = { "asset-base", "asset-overhaul", "ui-menu" };
      CHECK(mm_plan_install(&st, abc3, 3, &caps, &plan, reason, sizeof reason) == MM_OK,
            "reinstall plan");
      CHECK(mm_install_commit(&st, &plan) == MM_OK, "reinstall commit"); }
    CHECK(mm_mark_failed(&st, "ui-menu", "runtime ack state=failed code=3 detail=spawn-loop") == MM_OK,
          "mark failed from runtime ack");
    CHECK_STR(mm_state_name(st.recs[2].state), "failed", "failed stored");
    CHECK(strstr(st.recs[2].reason, "code=3") != 0, "failure reason kept");
    { const char *c[1] = { "ui-menu" };
      CHECK(mm_plan_install(&st, c, 1, &caps, &plan, reason, sizeof reason)
                == MM_ERR_STATE, "install refused while failed");
      CHECK(strstr(reason, "re-prepare") != 0, "failed install reason actionable"); }
    CHECK(mm_enable(&st, "ui-menu", &caps, reason, sizeof reason) == MM_ERR_STATE,
          "enable refused while failed");
    CHECK(mm_clear_failure(&st, "ui-menu", reason, sizeof reason) == MM_OK,
          "clear failure");
    CHECK_STR(mm_state_name(st.recs[2].state), "prepared", "cleared -> prepared");

    /* ---- T7: all four classification states reachable ------------------- */
    {
        char cls_reason[MM_REASON_MAX];
        mm_class c;
        c = mm_classify_against_set(&st, &st.mfs[0], &caps, 0, 0,
                                    cls_reason, sizeof cls_reason);
        CHECK(c == MM_CLASS_SUPPORTED, "asset-override pair supported");
        CHECK_STR(mm_class_name(MM_CLASS_SUPPORTED), "supported", "class name");
        c = mm_classify_against_set(&st, &clash, &caps,
                                    (const char *[]){ "asset-base" }, 1,
                                    cls_reason, sizeof cls_reason);
        CHECK(c == MM_CLASS_UNSUPPORTED, "container clash unsupported");
        c = mm_classify_against_set(&st, &bsa, &caps, 0, 0,
                                    cls_reason, sizeof cls_reason);
        CHECK(c == MM_CLASS_NEEDS_CONVERSION, "bsa needs conversion");
        /* Failed is evidence-only: it enters through the merge, never statically */
        c = mm_class_merge(MM_CLASS_SUPPORTED, MM_CLASS_FAILED);
        CHECK(c == MM_CLASS_FAILED, "failed dominates via merge");
        c = mm_class_merge(MM_CLASS_NEEDS_CONVERSION, MM_CLASS_UNSUPPORTED);
        CHECK(c == MM_CLASS_UNSUPPORTED, "unsupported dominates conversion");
    }

    /* ---- T8: dependency version gate ------------------------------------ */
    {
        mm_manifest g9 = mf_a(), h1 = mf_a();
        snprintf(g9.id, MM_ID_MAX, "lib-g");
        snprintf(g9.version, sizeof g9.version, "0.9.0");
        g9.n_files = 0;
        snprintf(h1.id, MM_ID_MAX, "needs-g");
        snprintf(h1.version, sizeof h1.version, "1.0.0");
        h1.n_files = 0;
        h1.n_deps = 1;
        snprintf(h1.dep_id[0], MM_ID_MAX, "lib-g");
        snprintf(h1.dep_min[0], MM_VER_MAX, "1.0.0");
        reset_store(&st);
        CHECK(mm_declare(&st, &g9) == MM_OK, "declare g 0.9.0");
        CHECK(mm_declare(&st, &h1) == MM_OK, "declare needs-g");
        { const char *gh[2] = { "lib-g", "needs-g" };
          rc = mm_plan_install(&st, gh, 2, &caps, &plan, reason, sizeof reason);
          CHECK(rc == MM_ERR_BLOCKED_DEP, "old dep version blocked");
          CHECK(strstr(reason, "0.9.0") != 0 && strstr(reason, "1.0.0") != 0,
                "version reason cites both"); }
        CHECK(mm_ver_cmp("1.0.0", "1.0.0") == 0, "ver equal");
        CHECK(mm_ver_cmp("1.2.0", "1.0.9") > 0, "ver greater");
        CHECK(mm_ver_cmp("1", "1.0.0") == 0, "ver pad zero");
        CHECK(mm_ver_cmp("0.9.0", "1.0.0") < 0, "ver less");
    }

    /* ---- T9: capacity gate ---------------------------------------------- */
    reset_store(&st);
    CHECK(mm_declare(&st, &base) == MM_OK, "cap: a");
    CHECK(mm_declare(&st, &over) == MM_OK, "cap: b");
    CHECK(mm_declare(&st, &menu) == MM_OK, "cap: c");
    st.capacity = 2;
    { const char *abc3[3] = { "asset-base", "asset-overhaul", "ui-menu" };
      rc = mm_plan_install(&st, abc3, 3, &caps, &plan, reason, sizeof reason);
      CHECK(rc == MM_ERR_CAPACITY, "capacity gate blocks third install");
      CHECK(strstr(reason, "capacity") != 0, "capacity reason actionable"); }

    /* ---- T10: dependency cycle ------------------------------------------ */
    {
        mm_manifest p1 = mf_a(), q1 = mf_a();
        snprintf(p1.id, MM_ID_MAX, "mod-p");
        p1.n_files = 0; p1.n_deps = 1;
        snprintf(p1.dep_id[0], MM_ID_MAX, "mod-q");
        snprintf(p1.dep_min[0], MM_VER_MAX, "1.0.0");
        snprintf(q1.id, MM_ID_MAX, "mod-q");
        q1.n_files = 0; q1.n_deps = 1;
        snprintf(q1.dep_id[0], MM_ID_MAX, "mod-p");
        snprintf(q1.dep_min[0], MM_VER_MAX, "1.0.0");
        reset_store(&st);
        CHECK(mm_declare(&st, &p1) == MM_OK, "declare p");
        CHECK(mm_declare(&st, &q1) == MM_OK, "declare q");
        { const char *pq[2] = { "mod-p", "mod-q" };
          rc = mm_plan_install(&st, pq, 2, &caps, &plan, reason, sizeof reason);
          CHECK(rc == MM_ERR_ORDER, "cycle refuses with MM_ERR_ORDER");
          CHECK(strstr(reason, "cycle") != 0, "cycle reason honest"); }
    }

    /* ---- T12: UI state surfacing ---------------------------------------- */
    reset_store(&st);
    CHECK(mm_declare(&st, &base) == MM_OK, "ui: a");
    CHECK(mm_ui_state_of(&st, "asset-base") == MM_UI_NOT_INSTALLED, "ui downloaded");
    CHECK(mm_ui_actions_of(&st, "asset-base") == MM_ACT_INSTALL, "ui actions downloaded");
    CHECK_STR(mm_ui_state_name(MM_UI_NOT_INSTALLED), "not_installed", "ui name");
    CHECK(mm_prepare(&st, "asset-base", &caps, reason, sizeof reason) == MM_OK,
          "prepare a");
    CHECK(mm_ui_state_of(&st, "asset-base") == MM_UI_READY, "ui prepared");
    CHECK((mm_ui_actions_of(&st, "asset-base") & MM_ACT_INSTALL) != 0,
          "prepared offers install");
    { const char *a[1] = { "asset-base" };
      CHECK(mm_plan_install(&st, a, 1, &caps, &plan, reason, sizeof reason) == MM_OK,
            "ui install plan");
      CHECK(mm_install_commit(&st, &plan) == MM_OK, "ui install commit"); }
    CHECK(mm_ui_state_of(&st, "asset-base") == MM_UI_ENABLED, "ui enabled");
    CHECK((mm_ui_actions_of(&st, "asset-base") & MM_ACT_DISABLE) != 0,
          "enabled offers disable");
    CHECK(mm_disable(&st, "asset-base", reason, sizeof reason) == MM_OK, "ui disable");
    CHECK(mm_ui_state_of(&st, "asset-base") == MM_UI_DISABLED, "ui disabled row");
    CHECK((mm_ui_actions_of(&st, "asset-base") & MM_ACT_ENABLE) != 0,
          "disabled offers enable");
    CHECK(mm_mark_failed(&st, "asset-base", "runtime ack failed") == MM_OK,
          "ui fail");
    CHECK(mm_ui_state_of(&st, "asset-base") == MM_UI_FAILED, "ui failed row");
    CHECK((mm_ui_actions_of(&st, "asset-base") & MM_ACT_RETRY) != 0,
          "failed offers retry");
    CHECK(mm_prepare(&st, "asset-base", &caps, reason, sizeof reason) == MM_OK,
          "failed re-prepares");
    CHECK(mm_ui_state_of(&st, "asset-base") == MM_UI_READY, "back to ready");
    CHECK(mm_index_of(&st, "nope") < 0, "unknown id lookup");
    CHECK(mm_ui_state_of(&st, "nope") == MM_UI_NOT_INSTALLED, "unknown row state");

    printf("checks=%d failures=%d\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}

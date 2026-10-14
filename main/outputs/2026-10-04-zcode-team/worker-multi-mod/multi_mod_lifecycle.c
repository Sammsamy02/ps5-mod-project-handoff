/*
 * multi_mod_lifecycle.c — M2 N-mod lifecycle state machine (prototype).
 * See multi_mod_lifecycle.h for the design contract. Pure C99, no deps.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "multi_mod_lifecycle.h"

/* ---------------------------------------------------------------- names -- */

static const char *g_state_names[] = {
    "downloaded", "prepared", "installing",
    "installed", "installed_disabled", "failed"
};
static const char *g_class_names[] = {
    "supported", "needs_conversion", "unsupported", "failed"
};
static const char *g_result_names[] = {
    "ok", "bad_request", "state", "blocked_dependency",
    "blocked_dependent", "conflict", "capacity", "order", "io"
};
static const char *g_ui_names[] = {
    "not_installed", "ready", "updating", "enabled", "disabled", "failed"
};

const char *mm_state_name(mm_state s)
{
    if (s < 0 || s > MM_FAILED) return "?";
    return g_state_names[s];
}

int mm_state_from_name(const char *name)
{
    int i;
    if (!name) return -1;
    for (i = 0; i <= MM_FAILED; ++i)
        if (!strcmp(g_state_names[i], name)) return i;
    return -1;
}

const char *mm_class_name(mm_class c)
{
    if (c < 0 || c > MM_CLASS_FAILED) return "?";
    return g_class_names[c];
}

mm_class mm_class_merge(mm_class a, mm_class b)
{
    return a >= b ? a : b;
}

const char *mm_result_name(mm_result r)
{
    if (r < 0 || r > MM_ERR_IO) return "?";
    return g_result_names[r];
}

/* -------------------------------------------------------------- helpers -- */

static void set_reason(char *reason, size_t rn, const char *fmt, ...)
{
    va_list ap;
    if (!reason || rn == 0) return;
    va_start(ap, fmt);
    vsnprintf(reason, rn, fmt, ap);
    va_end(ap);
}

int mm_index_of(const mm_store *st, const char *id)
{
    int i;
    if (!st || !id) return -1;
    for (i = 0; i < st->n_mods; ++i)
        if (!strcmp(st->recs[i].id, id)) return i;
    return -1;
}

const mm_manifest *mm_manifest_of(const mm_store *st, const char *id)
{
    int i = mm_index_of(st, id);
    return i < 0 ? 0 : &st->mfs[i];
}

const mm_record *mm_record_of(const mm_store *st, const char *id)
{
    int i = mm_index_of(st, id);
    return i < 0 ? 0 : &st->recs[i];
}

int mm_is_enabled(const mm_store *st, const char *id)
{
    const mm_record *r = mm_record_of(st, id);
    return r && r->state == MM_INSTALLED_ENABLED;
}

int mm_is_installed(const mm_store *st, const char *id)
{
    const mm_record *r = mm_record_of(st, id);
    return r && (r->state == MM_INSTALLED_ENABLED ||
                 r->state == MM_INSTALLED_DISABLED);
}

static int is_token(const char *s)
{
    size_t i, n = strlen(s);
    if (!n || n > 200) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c <= 0x20 || c >= 0x7f) return 0;
    }
    return 1;
}

/* dot-separated version compare; numeric when both segments parse,
 * lexical otherwise; missing segments count as 0. */
static void ver_next_seg(const char **p, char *out, size_t n)
{
    const char *s = *p;
    const char *e;
    size_t l;
    if (!s) { snprintf(out, n, "0"); return; }
    e = strchr(s, '.');
    l = e ? (size_t)(e - s) : strlen(s);
    if (l >= n) l = n - 1;
    memcpy(out, s, l);
    out[l] = 0;
    *p = e ? e + 1 : 0;
}

int mm_ver_cmp(const char *a, const char *b)
{
    char sa[MM_VER_MAX], sb[MM_VER_MAX];
    const char *pa = a, *pb = b;
    while (pa || pb) {
        long na, nb;
        char *enda, *endb;
        int c;
        ver_next_seg(&pa, sa, sizeof sa);
        ver_next_seg(&pb, sb, sizeof sb);
        na = strtol(sa, &enda, 10);
        nb = strtol(sb, &endb, 10);
        if (!*enda && !*endb) {           /* both segments numeric */
            if (na != nb) return na < nb ? -1 : 1;
        } else if ((c = strcmp(sa, sb)) != 0) {
            return c;
        }
    }
    return 0;
}

static int set_contains(const char *const *ids, int n, const char *id)
{
    int i;
    for (i = 0; i < n; ++i)
        if (!strcmp(ids[i], id)) return 1;
    return 0;
}

static int mf_dep_index(const mm_manifest *mf, const char *id)
{
    int i;
    for (i = 0; i < mf->n_deps; ++i)
        if (!strcmp(mf->dep_id[i], id)) return i;
    return -1;
}

/* transitive dependents of id among *installed* mods (BFS over declared
 * edges, restricted to installed states); enabled_out flags which are on. */
static int installed_dependents_transitive(const mm_store *st, const char *id,
                                           char out[][MM_ID_MAX], int *enabled_out,
                                           int max)
{
    char frontier[MM_MAX_MODS][MM_ID_MAX];
    int nf = 0, nout = 0, i, k;
    snprintf(frontier[nf++], MM_ID_MAX, "%s", id);
    while (nf > 0) {
        char cur[MM_ID_MAX];
        char next[MM_MAX_MODS][MM_ID_MAX];
        int nn = 0;
        snprintf(cur, sizeof cur, "%s", frontier[--nf]);
        for (i = 0; i < st->n_mods; ++i) {
            const mm_manifest *mf = &st->mfs[i];
            const mm_record *rec = &st->recs[i];
            int is_dep;
            if (!strcmp(mf->id, cur)) continue;
            if (!(rec->state == MM_INSTALLED_ENABLED ||
                  rec->state == MM_INSTALLED_DISABLED)) continue;
            is_dep = mf_dep_index(mf, cur) >= 0;
            if (!is_dep) continue;
            /* already collected? */
            {
                int seen = 0;
                for (k = 0; k < nout; ++k) if (!strcmp(out[k], mf->id)) seen = 1;
                for (k = 0; k < nn; ++k) if (!strcmp(next[k], mf->id)) seen = 1;
                if (seen) continue;
                if (nout < max) {
                    snprintf(out[nout], MM_ID_MAX, "%s", mf->id);
                    enabled_out[nout] = (rec->state == MM_INSTALLED_ENABLED);
                    nout++;
                }
                if (nn < MM_MAX_MODS) snprintf(next[nn++], MM_ID_MAX, "%s", mf->id);
            }
        }
        for (i = 0; i < nn && nf < MM_MAX_MODS; ++i)
            snprintf(frontier[nf++], MM_ID_MAX, "%s", next[i]);
        if (nout >= max) break;
    }
    return nout;
}

/* -------------------------------------------------------------- declare -- */

mm_result mm_declare(mm_store *st, const mm_manifest *mf)
{
    if (!st || !mf) return MM_ERR_BAD_REQUEST;
    if (!is_token(mf->id)) return MM_ERR_BAD_REQUEST;
    if (mm_index_of(st, mf->id) >= 0) return MM_ERR_BAD_REQUEST;
    if (st->n_mods >= MM_MAX_MODS) return MM_ERR_CAPACITY;
    st->mfs[st->n_mods] = *mf;
    memset(&st->recs[st->n_mods], 0, sizeof st->recs[0]);
    snprintf(st->recs[st->n_mods].id, MM_ID_MAX, "%s", mf->id);
    st->recs[st->n_mods].state = MM_DOWNLOADED;
    snprintf(st->recs[st->n_mods].reason, MM_REASON_MAX, "-");
    st->n_mods++;
    return MM_OK;
}

/* --------------------------------------------------------- classification */

static int cap_find(const mm_caps *caps, const char *id, const mm_cap **out)
{
    int i;
    for (i = 0; caps && i < caps->n_caps; ++i)
        if (!strcmp(caps->caps[i].id, id)) { *out = &caps->caps[i]; return 1; }
    return 0;
}

static int fam_find(const mm_caps *caps, const char *family, const mm_family **out)
{
    int i;
    for (i = 0; caps && i < caps->n_fams; ++i)
        if (!strcmp(caps->fams[i].family, family)) { *out = &caps->fams[i]; return 1; }
    return 0;
}

/* self rules: frameworks + family verdict. Never returns Failed. */
mm_class mm_classify_mod(const mm_store *st, const mm_manifest *m,
                         const mm_caps *caps, char *reason, size_t rn)
{
    int i;
    const mm_cap *c;
    const mm_family *f;
    (void)st;
    for (i = 0; i < m->n_fw; ++i) {
        if (!cap_find(caps, m->fw[i], &c)) {
            set_reason(reason, rn,
                "framework '%s' required by mod '%s' has no PS5 runtime component",
                m->fw[i], m->id);
            return MM_CLASS_UNSUPPORTED;
        }
        if (!strcmp(c->maturity, "missing") || !strcmp(c->maturity, "planned")) {
            set_reason(reason, rn,
                "framework '%s' required by mod '%s' is %s (not ported to PS5)",
                m->fw[i], m->id, c->maturity);
            return MM_CLASS_UNSUPPORTED;
        }
        /* "integrating" is allowed with a warning (same as modinst blockers
         * vs warnings); surfaces in the reason of the last integrating fw. */
        if (!strcmp(c->maturity, "integrating"))
            set_reason(reason, rn,
                "framework '%s' (mod '%s') is integrating: installs, in-game execution not proven",
                m->fw[i], m->id);
    }
    if (fam_find(caps, m->family, &f)) {
        if (!strcmp(f->verdict, "unsupported")) {
            set_reason(reason, rn,
                "mod family '%s' (mod '%s') has no PS5 backend",
                m->family, m->id);
            return MM_CLASS_UNSUPPORTED;
        }
        if (!strcmp(f->verdict, "conversion")) {
            set_reason(reason, rn,
                "mod family '%s' (mod '%s') needs conversion through a verified on-console converter",
                m->family, m->id);
            return MM_CLASS_NEEDS_CONVERSION;
        }
    } else {
        set_reason(reason, rn,
            "mod family '%s' (mod '%s') is not in the capability matrix",
            m->family, m->id);
        return MM_CLASS_UNSUPPORTED;
    }
    return MM_CLASS_SUPPORTED;
}

/* pair rules: shared destination paths governed by the family shared policy.
 * "last-wins": supported, the later mod in load order wins (compose already
 * implements this for 2 mods). "refuse": unsupported semantics. */
mm_class mm_classify_pair(const mm_manifest *a, const mm_manifest *b,
                          const mm_caps *caps, char *reason, size_t rn)
{
    int i, j;
    const mm_family *fa = 0, *fb = 0;
    int fa_found = fam_find(caps, a->family, &fa);
    int fb_found = fam_find(caps, b->family, &fb);
    for (i = 0; i < a->n_files; ++i)
        for (j = 0; j < b->n_files; ++j) {
            if (strcmp(a->files[i], b->files[j])) continue;
            {
                const char *pa = fa_found ? fa->shared_policy : "refuse";
                const char *pb = fb_found ? fb->shared_policy : "refuse";
                if (!strcmp(pa, "refuse") || !strcmp(pb, "refuse")) {
                    set_reason(reason, rn,
                        "mods '%s' (%s) and '%s' (%s) both write '%s'; shared-container override has undefined semantics",
                        a->id, a->family, b->id, b->family, a->files[i]);
                    return MM_CLASS_UNSUPPORTED;
                }
                set_reason(reason, rn,
                    "mods '%s' and '%s' share '%s'; load order decides the winner",
                    a->id, b->id, a->files[i]);
            }
        }
    if (reason && rn) reason[0] = 0;
    return MM_CLASS_SUPPORTED;
}

mm_class mm_classify_against_set(const mm_store *st, const mm_manifest *m,
                                 const mm_caps *caps,
                                 const char *const *also_ids, int n_also,
                                 char *reason, size_t rn)
{
    mm_class worst = mm_classify_mod(st, m, caps, reason, rn);
    char why[MM_REASON_MAX];
    int i;
    if (worst != MM_CLASS_SUPPORTED) return worst;
    /* gate against the ACTIVE set only: disabled mods are not loading, so a
     * pair involving one cannot refuse an install; mm_enable() re-runs this
     * classification before a disabled mod re-enters the load list. */
    for (i = 0; i < st->n_mods; ++i) {
        if (!strcmp(st->mfs[i].id, m->id)) continue;
        if (st->recs[i].state != MM_INSTALLED_ENABLED) continue;
        {
            mm_class c = mm_classify_pair(m, &st->mfs[i], caps, why, sizeof why);
            if (c > worst) { worst = c; set_reason(reason, rn, "%s", why); }
        }
    }
    for (i = 0; i < n_also; ++i) {
        const mm_manifest *o = mm_manifest_of(st, also_ids[i]);
        if (!o || !strcmp(o->id, m->id)) continue;
        {
            mm_class c = mm_classify_pair(m, o, caps, why, sizeof why);
            if (c > worst) { worst = c; set_reason(reason, rn, "%s", why); }
        }
    }
    if (worst == MM_CLASS_SUPPORTED && reason && rn && !reason[0])
        snprintf(reason, rn, "-");
    return worst;
}

/* ------------------------------------------------------------- prepare --- */

mm_result mm_prepare(mm_store *st, const char *id, const mm_caps *caps,
                     char *reason, size_t rn)
{
    mm_record *r;
    const mm_manifest *mf;
    mm_class c;
    int i = mm_index_of(st, id);
    if (i < 0) { set_reason(reason, rn, "unknown mod '%s'", id); return MM_ERR_BAD_REQUEST; }
    r = &st->recs[i];
    mf = &st->mfs[i];
    if (r->state != MM_DOWNLOADED && r->state != MM_PREPARED &&
        r->state != MM_FAILED) {
        set_reason(reason, rn, "mod '%s' is %s; prepare expects downloaded",
                   id, mm_state_name(r->state));
        return MM_ERR_STATE;
    }
    c = mm_classify_mod(st, mf, caps, reason, rn);
    /* modinst semantics: prepare records the analysis and marks prepared even
     * when blockers exist (exit not_ready); install is what refuses. */
    r->state = MM_PREPARED;
    if (c == MM_CLASS_UNSUPPORTED)
        snprintf(r->reason, MM_REASON_MAX, "unsupported: %s", reason ? reason : "");
    else if (c == MM_CLASS_NEEDS_CONVERSION)
        snprintf(r->reason, MM_REASON_MAX, "needs_conversion: %s", reason ? reason : "");
    else
        snprintf(r->reason, MM_REASON_MAX, "%s", (reason && reason[0]) ? reason : "-");
    return MM_OK;
}

/* ----------------------------------------------------- dependency checks - */

/* every declared dep of mf must be in set (ids[]) with version >= min */
static mm_result check_deps_against_set(const mm_store *st, const mm_manifest *mf,
                                        const char *const *ids, int n,
                                        char *reason, size_t rn)
{
    int k;
    for (k = 0; k < mf->n_deps; ++k) {
        const mm_manifest *dm = mm_manifest_of(st, mf->dep_id[k]);
        if (!dm || !set_contains(ids, n, mf->dep_id[k])) {
            set_reason(reason, rn,
                "mod '%s' requires '%s' >= %s which is not in the enabled set",
                mf->id, mf->dep_id[k], mf->dep_min[k]);
            return MM_ERR_BLOCKED_DEP;
        }
        if (mm_ver_cmp(dm->version, mf->dep_min[k]) < 0) {
            set_reason(reason, rn,
                "mod '%s' requires '%s' >= %s but declared version is %s",
                mf->id, mf->dep_id[k], mf->dep_min[k], dm->version);
            return MM_ERR_BLOCKED_DEP;
        }
    }
    return MM_OK;
}

/* ------------------------------------------------------------- ordering -- */

mm_result mm_order_set(const mm_store *st, const char *const *ids, int n,
                       int *out_idx, char *reason, size_t rn)
{
    int placed[MM_MAX_MODS];
    int i, k;
    if (n > MM_MAX_MODS) return MM_ERR_BAD_REQUEST;
    for (i = 0; i < n; ++i) {
        if (!mm_manifest_of(st, ids[i])) {
            set_reason(reason, rn, "unknown mod '%s' in load-order set", ids[i]);
            return MM_ERR_BAD_REQUEST;
        }
        placed[i] = 0;
    }
    for (k = 0; k < n; ++k) {
        int best = -1;
        for (i = 0; i < n; ++i) {
            const mm_manifest *mf;
            int j, ready = 1;
            if (placed[i]) continue;
            mf = mm_manifest_of(st, ids[i]);
            for (j = 0; j < mf->n_deps && ready; ++j) {
                int d;
                for (d = 0; d < n; ++d)
                    if (!strcmp(ids[d], mf->dep_id[j])) {   /* dep is in-set */
                        if (!placed[d]) ready = 0;          /* not placed yet */
                        break;
                    }
            }
            if (!ready) continue;
            if (best < 0) { best = i; continue; }
            {
                const mm_manifest *bm = mm_manifest_of(st, ids[best]);
                long pa = mf->priority, pb = bm->priority;
                if (pa != pb) { if (pa < pb) best = i; }
                else if (strcmp(mf->id, bm->id) < 0) best = i;
            }
        }
        if (best < 0) {
            set_reason(reason, rn,
                "dependency cycle among the selected mods; no deterministic load order");
            return MM_ERR_ORDER;
        }
        placed[best] = 1;
        out_idx[k] = best;
    }
    return MM_OK;
}

/* ------------------------------------------------------- plan / install -- */

mm_result mm_plan_install(mm_store *st, const char *const *new_ids, int n,
                          const mm_caps *caps, mm_plan *out,
                          char *reason, size_t rn)
{
    char target[MM_MAX_MODS][MM_ID_MAX];
    const char *targetp[MM_MAX_MODS];   /* pointers into target: a 2D char
                                           array must never be cast to
                                           const char *const * */
    int nt = 0, i, j;
    mm_result rc;

    if (!st || !out || n < 0 || n > MM_MAX_MODS) return MM_ERR_BAD_REQUEST;
    memset(out, 0, sizeof *out);

    /* target enabled set = currently enabled + new (re-select = update) */
    for (i = 0; i < st->n_mods; ++i)
        if (st->recs[i].state == MM_INSTALLED_ENABLED) {
            snprintf(target[nt], MM_ID_MAX, "%s", st->recs[i].id);
            targetp[nt] = target[nt];
            nt++;
        }
    for (i = 0; i < n; ++i) {
        int ix = mm_index_of(st, new_ids[i]);
        const mm_record *r;
        if (ix < 0) {
            set_reason(reason, rn, "unknown mod '%s'", new_ids[i]);
            return MM_ERR_BAD_REQUEST;
        }
        r = &st->recs[ix];
        if (r->state == MM_FAILED) {
            set_reason(reason, rn,
                "mod '%s' is failed (%s); re-prepare before installing",
                new_ids[i], r->reason);
            return MM_ERR_STATE;
        }
        if (r->state == MM_INSTALLING) {
            set_reason(reason, rn, "mod '%s' has a transaction in flight", new_ids[i]);
            return MM_ERR_STATE;
        }
        if (!set_contains(targetp, nt, new_ids[i])) {
            if (nt >= MM_MAX_MODS) { set_reason(reason, rn, "too many mods"); return MM_ERR_CAPACITY; }
            snprintf(target[nt], MM_ID_MAX, "%s", new_ids[i]);
            targetp[nt] = target[nt];
            nt++;
        }
    }

    /* classification gate over each new mod vs the whole target set */
    for (i = 0; i < n; ++i) {
        const mm_manifest *mf = mm_manifest_of(st, new_ids[i]);
        const char *also[MM_MAX_MODS];
        int na = 0;
        mm_class c;
        for (j = 0; j < nt; ++j)
            if (strcmp(target[j], new_ids[i])) also[na++] = target[j];
        c = mm_classify_against_set(st, mf, caps, also, na, reason, rn);
        if (c == MM_CLASS_UNSUPPORTED) return MM_ERR_CONFLICT;
        if (c == MM_CLASS_NEEDS_CONVERSION) return MM_ERR_CONFLICT;
        /* supported: reason may carry the last-wins / integrating notes */
    }

    /* dependency rules against the target set */
    for (i = 0; i < n; ++i) {
        const mm_manifest *mf = mm_manifest_of(st, new_ids[i]);
        rc = check_deps_against_set(st, mf, targetp, nt, reason, rn);
        if (rc != MM_OK) return rc;
    }

    /* capacity gate (roadmap: reuse the acquisition group capacity gate) */
    if (st->capacity > 0 && nt > st->capacity) {
        set_reason(reason, rn,
            "capacity gate: %d mods enabled but the title allows %ld",
            nt, st->capacity);
        return MM_ERR_CAPACITY;
    }

    /* deterministic load order over the target set */
    for (i = 0; i < nt; ++i) targetp[i] = target[i];
    rc = mm_order_set(st, targetp, nt, out->order, reason, rn);
    if (rc != MM_OK) return rc;

    out->n = nt;
    for (i = 0; i < nt; ++i)
        snprintf(out->ids[i], MM_ID_MAX, "%s", target[i]);
    out->new_generation = st->generation + 1;
    return MM_OK;
}

mm_result mm_install_commit(mm_store *st, const mm_plan *plan)
{
    int i, k;
    if (!st || !plan) return MM_ERR_BAD_REQUEST;
    for (i = 0; i < st->n_mods; ++i) {
        int in_set = 0;
        for (k = 0; k < plan->n; ++k)
            if (!strcmp(st->recs[i].id, plan->ids[k])) { in_set = 1; break; }
        if (in_set) {
            st->recs[i].state = MM_INSTALLED_ENABLED;
            snprintf(st->recs[i].reason, MM_REASON_MAX, "-");
        }
    }
    st->generation = plan->new_generation;
    return MM_OK;
}

/* --------------------------------------------------- enable / disable ---- */

mm_result mm_enable(mm_store *st, const char *id, const mm_caps *caps,
                    char *reason, size_t rn)
{
    int ix = mm_index_of(st, id);
    const mm_manifest *mf;
    mm_record *r;
    const char *also[MM_MAX_MODS];
    int na = 0, i;
    mm_result rc;
    mm_class c;
    mm_plan plan;

    if (ix < 0) { set_reason(reason, rn, "unknown mod '%s'", id); return MM_ERR_BAD_REQUEST; }
    mf = &st->mfs[ix];
    r = &st->recs[ix];
    if (r->state == MM_INSTALLED_ENABLED) return MM_OK; /* idempotent */
    if (r->state != MM_INSTALLED_DISABLED) {
        set_reason(reason, rn, "mod '%s' is %s; enable expects installed_disabled",
                   id, mm_state_name(r->state));
        return MM_ERR_STATE;
    }
    /* classification vs the enabled set + itself-as-target */
    for (i = 0; i < st->n_mods; ++i)
        if (st->recs[i].state == MM_INSTALLED_ENABLED && i != ix)
            also[na++] = st->recs[i].id;
    c = mm_classify_against_set(st, mf, caps, also, na, reason, rn);
    if (c == MM_CLASS_UNSUPPORTED || c == MM_CLASS_NEEDS_CONVERSION)
        return MM_ERR_CONFLICT;
    /* deps must be enabled now (enable does not auto-enable) */
    {
        const char *enabled_ids[MM_MAX_MODS];
        int ne = 0;
        for (i = 0; i < st->n_mods; ++i)
            if (st->recs[i].state == MM_INSTALLED_ENABLED)
                enabled_ids[ne++] = st->recs[i].id;
        rc = check_deps_against_set(st, mf, enabled_ids, ne, reason, rn);
        if (rc != MM_OK) return rc;
    }
    if (st->capacity > 0) {
        int n_enabled = na + 1;
        if (n_enabled > st->capacity) {
            set_reason(reason, rn, "capacity gate: enabling '%s' exceeds %ld",
                       id, st->capacity);
            return MM_ERR_CAPACITY;
        }
    }
    /* re-order the resulting enabled set; refuses on cycles */
    {
        const char *target[MM_MAX_MODS];
        int nt = 0;
        for (i = 0; i < st->n_mods; ++i)
            if (st->recs[i].state == MM_INSTALLED_ENABLED || i == ix)
                target[nt++] = st->recs[i].id;
        memset(&plan, 0, sizeof plan);
        rc = mm_order_set(st, target, nt, plan.order, reason, rn);
        if (rc != MM_OK) return rc;
        plan.n = nt;
        for (i = 0; i < nt; ++i) snprintf(plan.ids[i], MM_ID_MAX, "%s", target[i]);
        plan.new_generation = st->generation + 1;
    }
    r->state = MM_INSTALLED_ENABLED;
    snprintf(r->reason, MM_REASON_MAX, "-");
    st->generation = plan.new_generation;
    return MM_OK;
}

mm_result mm_disable(mm_store *st, const char *id, char *reason, size_t rn)
{
    int ix = mm_index_of(st, id);
    mm_record *r;
    char deps[MM_MAX_MODS][MM_ID_MAX];
    int en[MM_MAX_MODS], nd, i, n_enabled_dep = 0;
    char enabled_names[MM_REASON_MAX];
    size_t used = 0;

    if (ix < 0) { set_reason(reason, rn, "unknown mod '%s'", id); return MM_ERR_BAD_REQUEST; }
    r = &st->recs[ix];
    if (r->state == MM_INSTALLED_DISABLED) return MM_OK; /* idempotent */
    if (r->state != MM_INSTALLED_ENABLED) {
        set_reason(reason, rn, "mod '%s' is %s; disable expects installed",
                   id, mm_state_name(r->state));
        return MM_ERR_STATE;
    }
    nd = installed_dependents_transitive(st, id, deps, en, MM_MAX_MODS);
    for (i = 0; i < nd; ++i)
        if (en[i]) {
            used += (size_t)snprintf(enabled_names + used, sizeof enabled_names - used,
                                     "%s%s", n_enabled_dep ? ", " : "", deps[i]);
            n_enabled_dep++;
        }
    if (n_enabled_dep) {
        set_reason(reason, rn,
            "cannot disable '%s': %d enabled mod(s) depend on it (%s); disable them first",
            id, n_enabled_dep, enabled_names);
        return MM_ERR_BLOCKED_DEPENDENT;
    }
    r->state = MM_INSTALLED_DISABLED;
    snprintf(r->reason, MM_REASON_MAX, "-");
    st->generation++;
    return MM_OK;
}

/* ---------------------------------------------------------- uninstall ---- */

mm_result mm_uninstall(mm_store *st, const char *id, int force,
                       char *reason, size_t rn)
{
    int ix = mm_index_of(st, id);
    mm_record *r;
    char deps[MM_MAX_MODS][MM_ID_MAX];
    int en[MM_MAX_MODS], nd, i;

    if (ix < 0) { set_reason(reason, rn, "unknown mod '%s'", id); return MM_ERR_BAD_REQUEST; }
    r = &st->recs[ix];
    if (!mm_is_installed(st, id)) {
        set_reason(reason, rn, "mod '%s' is %s; uninstall expects installed",
                   id, mm_state_name(r->state));
        return MM_ERR_STATE;
    }
    nd = installed_dependents_transitive(st, id, deps, en, MM_MAX_MODS);
    for (i = 0; i < nd; ++i)
        if (en[i]) {
            set_reason(reason, rn,
                "cannot uninstall '%s': enabled mod '%s' depends on it; disable it first",
                id, deps[i]);
            return MM_ERR_BLOCKED_DEPENDENT;
        }
    if (nd && !force) {
        char names[MM_REASON_MAX];
        size_t used = 0;
        for (i = 0; i < nd; ++i)
            used += (size_t)snprintf(names + used, sizeof names - used,
                                     "%s%s", i ? ", " : "", deps[i]);
        set_reason(reason, rn,
            "cannot uninstall '%s': %d installed mod(s) depend on it (%s); uninstall them first or retry with force",
            id, nd, names);
        return MM_ERR_BLOCKED_DEPENDENT;
    }
    /* target first, then its (disabled, installed) transitive dependents */
    for (i = 0; i < nd; ++i) {
        mm_record *dr = (mm_record *)mm_record_of(st, deps[i]);
        if (dr) {
            dr->state = MM_PREPARED;
            snprintf(dr->reason, MM_REASON_MAX, "uninstalled with dependency '%s'", id);
        }
    }
    r->state = MM_PREPARED;
    snprintf(r->reason, MM_REASON_MAX, "-");
    st->generation++;
    return MM_OK;
}

/* ------------------------------------------------------- failure hooks --- */

mm_result mm_mark_failed(mm_store *st, const char *id, const char *why)
{
    int ix = mm_index_of(st, id);
    mm_record *r;
    if (ix < 0) return MM_ERR_BAD_REQUEST;
    r = &st->recs[ix];
    if (r->state == MM_INSTALLING) return MM_ERR_STATE;
    r->state = MM_FAILED;
    snprintf(r->reason, MM_REASON_MAX, "%s", why ? why : "runtime reported failure");
    return MM_OK;
}

mm_result mm_clear_failure(mm_store *st, const char *id, char *reason, size_t rn)
{
    int ix = mm_index_of(st, id);
    mm_record *r;
    if (ix < 0) { set_reason(reason, rn, "unknown mod '%s'", id); return MM_ERR_BAD_REQUEST; }
    r = &st->recs[ix];
    if (r->state != MM_FAILED) {
        set_reason(reason, rn, "mod '%s' is %s; clear_failure expects failed",
                   id, mm_state_name(r->state));
        return MM_ERR_STATE;
    }
    r->state = MM_PREPARED;
    snprintf(r->reason, MM_REASON_MAX, "failure cleared; re-validate before enabling");
    return MM_OK;
}

/* ------------------------------------------------------------ recovery --- */

mm_result mm_recover(mm_store *st, const char *const *loadlist, int n_ll,
                     const char *const *permod_dirs, int n_pd,
                     long generation_seen, char *reason, size_t rn)
{
    int i;
    if (!st) return MM_ERR_BAD_REQUEST;
    for (i = 0; i < st->n_mods; ++i) {
        mm_record *r = &st->recs[i];
        int in_ll = set_contains(loadlist, n_ll, r->id);
        int in_dirs = set_contains(permod_dirs, n_pd, r->id);
        if (in_ll) {
            r->state = MM_INSTALLED_ENABLED;
            snprintf(r->reason, MM_REASON_MAX, "-");
        } else if (in_dirs &&
                   (r->state == MM_INSTALLED_ENABLED ||
                    r->state == MM_INSTALLED_DISABLED)) {
            r->state = MM_INSTALLED_DISABLED;
            snprintf(r->reason, MM_REASON_MAX, "-");
        } else if (in_dirs && r->state == MM_INSTALLING) {
            /* swap-phase crash with a committed root: files present but the
             * load list does not name the mod yet -> disabled, never half-on */
            r->state = MM_INSTALLED_DISABLED;
            snprintf(r->reason, MM_REASON_MAX, "recovered from interrupted transaction");
        } else {
            if (r->state == MM_INSTALLED_ENABLED ||
                r->state == MM_INSTALLED_DISABLED ||
                r->state == MM_INSTALLING) {
                r->state = MM_PREPARED;
                snprintf(r->reason, MM_REASON_MAX,
                         "recovered: root does not contain mod '%s'", r->id);
            }
            /* downloaded/failed/prepared records stay as they are */
        }
    }
    if (generation_seen >= 0) st->generation = generation_seen;
    set_reason(reason, rn, "recovered against live root truth");
    return MM_OK;
}

/* -------------------------------------------------------- serialization -- */

mm_result mm_store_save(const mm_store *st, const char *path)
{
    FILE *f;
    int i;
    if (!st || !path) return MM_ERR_BAD_REQUEST;
    f = fopen(path, "wb");
    if (!f) return MM_ERR_IO;
    fprintf(f, "mmstore\t1\n");
    fprintf(f, "game\t%s\t%ld\t%ld\n", st->title, st->generation, st->capacity);
    for (i = 0; i < st->n_mods; ++i) {
        const mm_manifest *mf = &st->mfs[i];
        const mm_record *r = &st->recs[i];
        char deps[512];
        size_t used = 0;
        int k;
        deps[0] = 0;
        for (k = 0; k < mf->n_deps; ++k)
            used += (size_t)snprintf(deps + used, sizeof deps - used, "%s%s>=%s",
                                     k ? ";" : "", mf->dep_id[k], mf->dep_min[k]);
        if (!used) snprintf(deps, sizeof deps, "-");
        fprintf(f, "mod\t%s\t%s\t%s\t%s\t%s\t%ld\t%d\t%s\t%s\n",
                r->id, mf->title, mm_state_name(r->state),
                mf->bundle[0] ? mf->bundle : "-", mf->version, mf->priority,
                mf->layout, r->reason, deps);
    }
    if (fclose(f)) return MM_ERR_IO;
    return MM_OK;
}

mm_result mm_store_load(mm_store *st, const char *path)
{
    FILE *f;
    char line[2048];
    if (!st || !path) return MM_ERR_BAD_REQUEST;
    memset(st, 0, sizeof *st);
    f = fopen(path, "rb");
    if (!f) return MM_ERR_IO;
    while (fgets(line, sizeof line, f)) {
        char *f2[16];
        int nf = 0;
        char *s = line, *t;
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n) continue;
        while (nf < 16) {
            t = strchr(s, '\t');
            f2[nf++] = s;
            if (!t) break;
            *t = 0;
            s = t + 1;
        }
        if (!strcmp(f2[0], "game") && nf >= 3) {
            snprintf(st->title, sizeof st->title, "%s", f2[1]);
            st->generation = atol(f2[2]);
            st->capacity = nf >= 4 ? atol(f2[3]) : 0;
        } else if (!strcmp(f2[0], "mod") && nf >= 5) {
            mm_manifest *mf;
            mm_record *r;
            int st_i;
            if (st->n_mods >= MM_MAX_MODS) { fclose(f); return MM_ERR_CAPACITY; }
            mf = &st->mfs[st->n_mods];
            r = &st->recs[st->n_mods];
            memset(mf, 0, sizeof *mf);
            memset(r, 0, sizeof *r);
            snprintf(r->id, MM_ID_MAX, "%s", f2[1]);
            snprintf(mf->id, MM_ID_MAX, "%s", f2[1]);
            /* column 3 is title in both the legacy and the extended format;
             * the legacy line is: mod id title state bundle  (nf==5), the
             * extended line is: mod id title state bundle version prio
             * layout reason deps (nf==10). */
            snprintf(mf->title, sizeof mf->title, "%s", f2[2]);
            st_i = mm_state_from_name(f2[3]);
            r->state = st_i < 0 ? (int)MM_DOWNLOADED : st_i;
            if (nf >= 5) snprintf(mf->bundle, sizeof mf->bundle, "%s", f2[4]);
            if (nf >= 6) snprintf(mf->version, sizeof mf->version, "%s", f2[5]);
            else snprintf(mf->version, sizeof mf->version, "0");
            mf->priority = nf >= 7 ? atol(f2[6]) : 100;
            mf->layout = nf >= 8 ? atoi(f2[7]) : 0;
            snprintf(r->reason, MM_REASON_MAX, "%s",
                     nf >= 9 ? f2[8] : "-");
            if (nf >= 10 && strcmp(f2[9], "-")) {
                char dbuf[512], *pair, *save = 0;
                snprintf(dbuf, sizeof dbuf, "%s", f2[9]);
                for (pair = strtok_r(dbuf, ";", &save); pair && mf->n_deps < MM_MAX_DEPS;
                     pair = strtok_r(0, ";", &save)) {
                    char *ge = strstr(pair, ">=");
                    if (!ge) continue;
                    *ge = 0;
                    snprintf(mf->dep_id[mf->n_deps], MM_ID_MAX, "%s", pair);
                    snprintf(mf->dep_min[mf->n_deps], MM_VER_MAX, "%s", ge + 2);
                    mf->n_deps++;
                }
            }
            st->n_mods++;
        }
        /* unknown lines ignored: forward compatible with future extensions */
    }
    fclose(f);
    return MM_OK;
}

/* ------------------------------------------------------------------ UI --- */

const char *mm_ui_state_name(mm_ui_state u)
{
    if (u < 0 || u > MM_UI_FAILED) return "?";
    return g_ui_names[u];
}

mm_ui_state mm_ui_state_of(const mm_store *st, const char *id)
{
    const mm_record *r = mm_record_of(st, id);
    if (!r) return MM_UI_NOT_INSTALLED;
    switch (r->state) {
    case MM_DOWNLOADED:         return MM_UI_NOT_INSTALLED;
    case MM_PREPARED:           return MM_UI_READY;
    case MM_INSTALLING:         return MM_UI_UPDATING;
    case MM_INSTALLED_ENABLED:  return MM_UI_ENABLED;
    case MM_INSTALLED_DISABLED: return MM_UI_DISABLED;
    case MM_FAILED:             return MM_UI_FAILED;
    }
    return MM_UI_NOT_INSTALLED;
}

unsigned mm_ui_actions_of(const mm_store *st, const char *id)
{
    const mm_record *r = mm_record_of(st, id);
    unsigned a = 0;
    if (!r) return MM_ACT_INSTALL; /* unknown row: registration affordance */
    switch (r->state) {
    case MM_DOWNLOADED:
        a |= MM_ACT_INSTALL;
        break;
    case MM_PREPARED:
        a |= MM_ACT_INSTALL | MM_ACT_UNINSTALL;
        break;
    case MM_INSTALLING:
        break;                       /* transaction in flight: no actions */
    case MM_INSTALLED_ENABLED:
        a |= MM_ACT_DISABLE | MM_ACT_UNINSTALL | MM_ACT_UPDATE;
        break;
    case MM_INSTALLED_DISABLED:
        a |= MM_ACT_ENABLE | MM_ACT_UNINSTALL | MM_ACT_UPDATE;
        break;
    case MM_FAILED:
        a |= MM_ACT_RETRY | MM_ACT_UNINSTALL;
        break;
    }
    return a;
}

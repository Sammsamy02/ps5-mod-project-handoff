/* axes_detector.c — in-probe BuildConfiguration / Stats detector.
 *
 * Faithful C port of the genuine patternsleuth resolvers the runtime's
 * UnrealInitializer consumes (results.build_configuration / results.stats):
 *   - fingerprint: UTF-16LE substring search for the marker table over the
 *     image's readable memory (raw byte stream, both 2-byte phases, no
 *     alignment assumption — same as the Rust pattern scan);
 *   - decision: cost model over candidate builds (version x config x stats),
 *     CONTRADICTION=5 / OMISSION=1, best candidate per axis must beat every
 *     axis-differing candidate by MIN_MARGIN=4, with >= MIN_MARKERS=10
 *     markers present, else the axis stays 0 (not detected).
 *
 * Freestanding C: no libc, no allocation. All reads go through the caller's
 * checked read callback in bounded chunks (refusal-halving for span edges).
 */
#include "axes_detector.h"

/* ------------------------------------------------------------------ */
/* Candidate enumeration — Rust order: versions [None] + VERSIONS outer,  */
/* config {Development, Test, Shipping} middle, stats {Off, On} inner.    */
void ig_axes_candidate(int index, int *version_idx, int *cfg, int *stats_on) {
    int si = index % 2;
    int ci = (index / 2) % 3;
    int vi = index / 6;               /* 0 = None, 1..N = ig_axes_versions */
    if (stats_on) *stats_on = si;
    if (cfg) *cfg = ci;
    if (version_idx) *version_idx = vi - 1;
}

/* Rust Marker::covers */
int ig_axes_marker_covers(int mi, int version_idx, int cfg, int stats_on) {
    const IgAxesMarker *m = &ig_axes_markers[mi];
    switch (m->gate) {
    case IG_AXES_GATE_NOT_SHIPPING:     if (cfg == 2) return 0; break;
    case IG_AXES_GATE_DEV_ONLY:         if (cfg != 0) return 0; break;
    case IG_AXES_GATE_TEST_OR_SHIPPING: if (cfg == 0) return 0; break;
    case IG_AXES_GATE_SHIPPING_ONLY:    if (cfg != 2) return 0; break;
    case IG_AXES_GATE_WITH_STATS:       if (!stats_on) return 0; break;
    default: break;
    }
    if (m->first == IG_AXES_VNONE && m->last == IG_AXES_VNONE) return 1;
    if (version_idx < 0) return 0;   /* windowed marker, unknown version */
    if (m->first != IG_AXES_VNONE && version_idx < (int)m->first) return 0;
    if (m->last != IG_AXES_VNONE && version_idx > (int)m->last) return 0;
    return 1;
}

uint32_t ig_axes_costs_of(const uint8_t *present, uint32_t *out_costs) {
    uint32_t present_count = 0;
    int mi;
    for (mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi)
        if (present[mi]) ++present_count;
    for (int c = 0; c < IG_AXES_CANDIDATES; ++c) {
        int vi, cfg, on;
        uint32_t cost = 0;
        ig_axes_candidate(c, &vi, &cfg, &on);
        for (mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi) {
            int covered = ig_axes_marker_covers(mi, vi, cfg, on);
            if (!covered && present[mi]) cost += IG_AXES_CONTRADICTION;
            else if (covered && !present[mi]) cost += IG_AXES_OMISSION;
        }
        out_costs[c] = cost;
    }
    return present_count;
}

/* Rust resolve_axis: best = first minimum; runner-up = minimum cost among
 * candidates whose value on the axis differs; resolved iff margin >= 4. */
static int resolve_axis(const uint32_t *costs, int axis /* 0=version 1=cfg 2=stats */,
                        uint32_t *best_cost_out, uint32_t *runner_cost_out,
                        int *best_index_out) {
    int best = 0;
    uint32_t best_cost = costs[0];
    for (int c = 1; c < IG_AXES_CANDIDATES; ++c) {
        if (costs[c] < best_cost) { best_cost = costs[c]; best = c; }
    }
    int best_v, best_c, best_s;
    ig_axes_candidate(best, &best_v, &best_c, &best_s);
    int axis_best = (axis == 0) ? best_v : (axis == 1) ? best_c : best_s;

    int runner = -1;
    uint32_t runner_cost = 0;
    for (int c = 0; c < IG_AXES_CANDIDATES; ++c) {
        int v, cf, s;
        ig_axes_candidate(c, &v, &cf, &s);
        int axis_v = (axis == 0) ? v : (axis == 1) ? cf : s;
        if (axis_v == axis_best) continue;
        if (runner < 0 || costs[c] < runner_cost) { runner_cost = costs[c]; runner = c; }
    }
    if (best_cost_out) *best_cost_out = best_cost;
    if (runner_cost_out) *runner_cost_out = runner_cost;
    if (best_index_out) *best_index_out = best;
    return (runner >= 0) &&
           (runner_cost >= best_cost) &&
           (runner_cost - best_cost >= IG_AXES_MIN_MARGIN);
}

/* Resolve all three axes from a cost vector. Upstream parity: the Rust
 * property test feeds costs() straight into resolve_axis(), while the
 * scan-time fingerprint() applies the MIN_MARKERS gate before resolving. */
void ig_axes_resolve_costs(const uint32_t *costs, IgAxesResult *out) {
    int bi_v, bi_c, bi_s, vi, cfg, on;
    if (resolve_axis(costs, 1, &out->config_best_cost, &out->config_runner_cost, &bi_c)) {
        ig_axes_candidate(bi_c, &vi, &cfg, &on);
        out->config_resolved = 1;
        out->config_best_cfg = cfg;
        out->build_configuration =
            (cfg == 2) ? IG_AXES_CONFIG_SHIPPING :
            (cfg == 0) ? IG_AXES_CONFIG_DEVELOPMENT : IG_AXES_CONFIG_TEST;
    }
    if (resolve_axis(costs, 2, &out->stats_best_cost, &out->stats_runner_cost, &bi_s)) {
        ig_axes_candidate(bi_s, &vi, &cfg, &on);
        out->stats_resolved = 1;
        out->stats_best_on = on;
        out->stats = on ? IG_AXES_STATS_ON : IG_AXES_STATS_OFF;
    }
    if (resolve_axis(costs, 0, &out->version_best_cost, &out->version_runner_cost, &bi_v)) {
        ig_axes_candidate(bi_v, &vi, &cfg, &on);
        if (vi >= 0) {   /* Rust: None -> "predates the oldest fingerprinted version" */
            out->version_resolved = 1;
            out->version_best_index = vi;
            out->version_known = 1;
            out->version_major = ig_axes_versions[vi][0];
            out->version_minor = ig_axes_versions[vi][1];
        }
    }
}

void ig_axes_from_presence(const uint8_t *present, IgAxesResult *out) {
    uint32_t costs[IG_AXES_CANDIDATES];

    out->build_configuration = IG_AXES_CONFIG_UNKNOWN;
    out->stats = IG_AXES_STATS_UNKNOWN;
    out->version_known = 0;
    out->version_major = 0;
    out->version_minor = 0;
    out->config_resolved = out->stats_resolved = out->version_resolved = 0;
    out->config_best_cost = out->config_runner_cost = 0;
    out->stats_best_cost = out->stats_runner_cost = 0;
    out->version_best_cost = out->version_runner_cost = 0;
    out->config_best_cfg = 0;
    out->stats_best_on = 0;
    out->version_best_index = -1;

    out->present_count = ig_axes_costs_of(present, costs);
    if (out->present_count < IG_AXES_MIN_MARKERS)
        return;   /* Rust fingerprint() bails: "too few marker strings" */

    ig_axes_resolve_costs(costs, out);
}

/* ------------------------------------------------------------------ */
/* Scan: first-code-unit bucket index over the marker table.              */
#define IG_AXES_BUCKETS 4096
static int16_t ig_axes_head[IG_AXES_BUCKETS];      /* first marker or -1 */
static int16_t ig_axes_next[IG_AXES_MARKER_COUNT]; /* chain */
static uint16_t ig_axes_max_marker_bytes;

static void ig_axes_build_index(void) {
    for (int i = 0; i < IG_AXES_BUCKETS; ++i) ig_axes_head[i] = -1;
    ig_axes_max_marker_bytes = 0;
    for (int mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi) {
        uint32_t u = ig_axes_pool[ig_axes_markers[mi].off];
        uint32_t h = ((u * 2654435761u) >> 20) & (IG_AXES_BUCKETS - 1);
        ig_axes_next[mi] = ig_axes_head[h];
        ig_axes_head[h] = (int16_t)mi;
        uint32_t bytes = (uint32_t)ig_axes_markers[mi].len * 2u;
        if (bytes > ig_axes_max_marker_bytes) ig_axes_max_marker_bytes = bytes;
    }
}

/* Verify a full marker match at buffer offset p (buffer must hold len*2). */
static int marker_at(const uint8_t *buf, uint32_t buflen, uint32_t p, int mi) {
    const IgAxesMarker *m = &ig_axes_markers[mi];
    uint32_t need = (uint32_t)m->len * 2u;
    if (p + need > buflen) return 0;
    const uint16_t *units = &ig_axes_pool[m->off];
    for (uint16_t k = 0; k < m->len; ++k) {
        uint16_t u = (uint16_t)(buf[p + 2u * k] | ((uint16_t)buf[p + 2u * k + 1] << 8));
        if (u != units[k]) return 0;
    }
    return 1;
}

#define IG_AXES_CARRY_MAX 512

void ig_axes_scan(const IgAxesReadFn read_fn, void *read_ud,
                  uint64_t image_base, uint64_t extent,
                  uint8_t *present_out, IgAxesResult *out) {
    static uint8_t combined[IG_AXES_CARRY_MAX + IG_AXES_CHUNK];
    uint8_t present[IG_AXES_MARKER_COUNT];

    for (int mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi) present[mi] = 0;
    out->bytes_scanned = 0;
    out->read_refused = 0;
    out->chunks_read = 0;
    ig_axes_build_index();
    if (ig_axes_max_marker_bytes > IG_AXES_CARRY_MAX)
        ig_axes_max_marker_bytes = IG_AXES_CARRY_MAX; /* cannot happen with this table */

    const uint64_t end = image_base + extent;
    uint64_t p = image_base;
    uint32_t carry_len = 0;   /* bytes at the head of combined[] kept from the
                                 previous chunk so markers straddling a chunk
                                 boundary are found exactly once more */

    while (p < end) {
        uint32_t want = (uint32_t)((end - p) < (uint64_t)IG_AXES_CHUNK)
                            ? (uint32_t)(end - p) : (uint32_t)IG_AXES_CHUNK;
        uint32_t got = 0;
        uint32_t try_len = want;
        for (;;) {   /* refusal-halving rides over span edges */
            if (read_fn(read_ud, p, combined + carry_len, try_len) == 0) {
                got = try_len;
                break;
            }
            ++out->read_refused;
            if (try_len <= 512) break;
            try_len /= 2;
        }
        if (!got) {
            p += (try_len < 4096) ? try_len : 4096;  /* guaranteed progress */
            carry_len = 0;
            continue;
        }
        ++out->chunks_read;
        out->bytes_scanned += got;

        const uint32_t clen = carry_len + got;
        if (clen >= 2) {
            const uint32_t last = clen - 1;
            for (uint32_t q = 0; q < last; ++q) {
                const uint32_t u = (uint32_t)combined[q] |
                                   ((uint32_t)combined[q + 1] << 8);
                int16_t mi = ig_axes_head[((u * 2654435761u) >> 20) &
                                          (IG_AXES_BUCKETS - 1)];
                while (mi >= 0) {
                    if (marker_at(combined, clen, q, mi)) present[mi] = 1;
                    mi = ig_axes_next[mi];
                }
            }
        }

        /* Carry the tail: any marker fully inside the image but straddling
         * this chunk boundary starts within the last max-marker bytes, so it
         * is contained (and found) in the next combined buffer. */
        carry_len = (got < ig_axes_max_marker_bytes)
                        ? got : ig_axes_max_marker_bytes;
        for (uint32_t i = 0; i < carry_len; ++i)
            combined[i] = combined[clen - carry_len + i];
        p += got;
    }

    ig_axes_from_presence(present, out);
    if (present_out)
        for (int mi = 0; mi < IG_AXES_MARKER_COUNT; ++mi)
            present_out[mi] = present[mi];
}

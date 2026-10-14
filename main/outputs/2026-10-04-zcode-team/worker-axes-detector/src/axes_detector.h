/* axes_detector.h — standalone in-probe BuildConfiguration / Stats detector.
 *
 * Ports the runtime's genuine axes classifiers (patternsleuth resolvers
 * `BuildConfiguration` / `Stats` in upstream engine_version.rs, consumed by
 * UnrealInitializer.cpp as results.build_configuration 0..3 and results.stats
 * 0..2) to freestanding C so an in-game probe (igf5/igf6 lineage) can derive
 * the axes from game evidence instead of settings.
 *
 * How the runtime detects (verified in source, see REPORT.md):
 *   The classifier fingerprints the image for a fixed table of UTF-16LE
 *   marker strings (CVar names / log strings), each tagged with the build
 *   gates that include it (#if !UE_BUILD_SHIPPING, #if STATS, ...) and an
 *   engine-version window. Marker presence votes over candidate builds
 *   (engine version x {Development,Test,Shipping} x {Stats Off,On}):
 *   present-but-not-covered costs CONTRADICTION(5), covered-but-missing
 *   costs OMISSION(1); the cheapest candidate wins per axis if it beats
 *   every candidate differing on that axis by >= MIN_MARGIN(4), and at
 *   least MIN_MARKERS_PRESENT(10) markers must be present at all. Anything
 *   else stays "not detected" (axis value 0) — the detector never guesses.
 *
 * Memory contract (same evidence the runtime reads): the caller supplies
 * the image base (0x400000 for Townfall eboot, proven by worker-vaddr-
 * semantics) and the vmmap readable-span snapshot; this unit reads ONLY
 * inside [image_base, image_base+extent) through the caller's checked read
 * callback, in bounded chunks with straddle overlap. No imports, no libc,
 * no allocation; static tables only.
 */
#ifndef IG_AXES_DETECTOR_H
#define IG_AXES_DETECTOR_H

#include <stdint.h>
#include <stddef.h>

/* Marker/version tables (generated from upstream engine_version.rs). */
#include "axes_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime axis conventions (UnrealInitializer.cpp / binding-port lib.rs):
 *   build_configuration: 0 not detected, 1 shipping, 2 development, 3 test.
 *   stats:               0 not detected, 1 off, 2 on.                    */
#define IG_AXES_CONFIG_UNKNOWN   0
#define IG_AXES_CONFIG_SHIPPING  1
#define IG_AXES_CONFIG_DEVELOPMENT 2
#define IG_AXES_CONFIG_TEST      3
#define IG_AXES_STATS_UNKNOWN    0
#define IG_AXES_STATS_OFF        1
#define IG_AXES_STATS_ON         2

/* Cost model constants — ported verbatim from engine_version.rs. */
#define IG_AXES_CONTRADICTION    5
#define IG_AXES_OMISSION         1
#define IG_AXES_MIN_MARKERS      10
#define IG_AXES_MIN_MARGIN       4

/* Gate ids (axes_table.h comments mirror these). */
#define IG_AXES_GATE_NONE             0
#define IG_AXES_GATE_NOT_SHIPPING     1
#define IG_AXES_GATE_DEV_ONLY         2
#define IG_AXES_GATE_TEST_OR_SHIPPING 3
#define IG_AXES_GATE_SHIPPING_ONLY    4
#define IG_AXES_GATE_WITH_STATS       5

/* Read callback: copy len bytes at addr into dst; 0 ok, nonzero refused.
 * In-probe wrapper: ig_range_readable() + ig_copy_checked() (factstore.h).
 * Host test: memcpy from the mapped image buffer. */
typedef int (*IgAxesReadFn)(void *ud, uint64_t addr, void *dst, uint32_t len);

typedef struct IgAxesResult {
    /* Resolved axes in the runtime's 0..3 / 0..2 convention. */
    int build_configuration;
    int stats;

    /* Best-effort engine version from the same fingerprint (evidence only;
     * the runtime detects version separately and this must not override it). */
    int version_known;
    int version_major;
    int version_minor;

    /* Fingerprint evidence. */
    uint32_t present_count;    /* markers found anywhere in the scanned image */
    uint64_t bytes_scanned;    /* image bytes actually read */
    uint32_t read_refused;     /* refused read calls (span edges / holes) */
    uint32_t chunks_read;      /* successful chunk reads */

    /* Per-axis decision detail (costs in candidate-cost units). */
    int config_resolved, stats_resolved, version_resolved;
    uint32_t config_best_cost, config_runner_cost;   /* margin = diff */
    uint32_t stats_best_cost, stats_runner_cost;
    uint32_t version_best_cost, version_runner_cost;
    int config_best_cfg;       /* 0..2 internal {dev,test,ship} */
    int stats_best_on;         /* 0/1 internal */
    int version_best_index;    /* -1 = None candidate */

    /* Synthetic-presence self-test support: raw costs per candidate. */
    /* (kept out of the struct; see ig_axes_costs_of) */
} IgAxesResult;

/* Candidate-count and enumeration helpers shared with tests. */
#define IG_AXES_CANDIDATES ((IG_AXES_VERSION_COUNT + 1) * 3 * 2)
void ig_axes_candidate(int index, int *version_idx /* -1..N-1, -1 = None */,
                       int *cfg /* 0=dev 1=test 2=ship */,
                       int *stats_on /* 0/1 */);

/* Does marker mi exist in the candidate build described by
 * (version_idx, cfg, stats_on)? Direct port of Rust Marker::covers. */
int ig_axes_marker_covers(int mi, int version_idx, int cfg, int stats_on);

/* Cost vector over all candidates for the given presence vector
 * (present[IG_AXES_MARKER_COUNT]). out_costs[IG_AXES_CANDIDATES].
 * Returns the number of present markers. Direct port of costs(). */
uint32_t ig_axes_costs_of(const uint8_t *present, uint32_t *out_costs);

/* Pure decision entry from a cost vector (ig_axes_costs_of output).
 * Upstream test parity: applies NO marker-count gate — the gate lives in
 * the scan path (ig_axes_from_presence / ig_axes_scan), mirroring the
 * Rust split between resolve_axis() and fingerprint(). */
void ig_axes_resolve_costs(const uint32_t *costs, IgAxesResult *out);

/* Pure decision entry: resolve axes from a presence vector without
 * scanning. Used by the host self-test (synthetic presence for every
 * candidate build) and internally after a scan. */
void ig_axes_from_presence(const uint8_t *present, IgAxesResult *out);

/* Full scan entry. Walks [image_base, image_base + extent) through read_fn
 * in IG_AXES_CHUNK chunks (+ straddle overlap), fingerprints marker presence,
 * then resolves. present_out (may be NULL) receives the raw presence vector
 * (evidence / test hook). extent is the image extent in runtime bytes
 * (e.g. IgCand.extent — the caller passes exactly the proven bounds). */
#define IG_AXES_CHUNK (64u * 1024u)
void ig_axes_scan(const IgAxesReadFn read_fn, void *read_ud,
                  uint64_t image_base, uint64_t extent,
                  uint8_t *present_out /* [IG_AXES_MARKER_COUNT] or NULL */,
                  IgAxesResult *out);

#ifdef __cplusplus
}
#endif
#endif /* IG_AXES_DETECTOR_H */

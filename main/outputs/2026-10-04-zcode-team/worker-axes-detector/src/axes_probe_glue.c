/* axes_probe_glue.c — drop-in adapter wiring the axes detector into the
 * worker-ingame-probe-v2 lineage (igf5/igf6). Compiles against the probe's
 * own factstore.h / record_sink.h; no probe file is modified by this
 * worker (integration diff is documented in REPORT.md).
 *
 * Contract:
 *   - Caller passes the PROVEN chosen image base and extent (IgFacts
 *     chosen_base / chosen_extent from the V2 scan; for Townfall
 *     mapped_start(LOAD0) = 0x400000, extent = max(p_vaddr+p_memsz)).
 *   - Every read goes through ig_copy_checked (span-validated); refusals
 *     are evidence, never faults.
 *   - Emits ONE bounded JSONL record: exp="axes".
 */
#include "axes_probe_glue.h"

#include "axes_detector.h"
#include "factstore.h"
#include "record_sink.h"

/* read_fn adapter over the probe's checked copy. Halving on refusal is
 * handled inside ig_axes_scan; a refusal here means "range not fully in
 * one readable span", which may be a span edge or a real hole. */
static int axes_read_checked(void *ud, uint64_t addr, void *dst, uint32_t len) {
    (void)ud;
    return ig_copy_checked(addr, dst, len) == (size_t)len ? 0 : -1;
}

int ig_axes_experiment(IgSink *sink, int seq, const char *build_id,
                       uint64_t image_base, uint64_t image_extent) {
    IgAxesResult r;
    ig_axes_scan(axes_read_checked, NULL, image_base, image_extent, NULL, &r);

    IgBuf b;
    ig_record_begin(&b, seq, "axes", 0, 0, build_id);
    ig_kv_key(&b, "build_configuration");
    ig_buf_i64_dec(&b, r.build_configuration);      /* 0..3 runtime axis */
    ig_kv_key(&b, "stats");
    ig_buf_i64_dec(&b, r.stats);                    /* 0..2 runtime axis */
    ig_kv_key(&b, "version_known");
    ig_buf_i64_dec(&b, r.version_known);
    if (r.version_known) {
        ig_kv_key(&b, "version_major");
        ig_buf_i64_dec(&b, r.version_major);
        ig_kv_key(&b, "version_minor");
        ig_buf_i64_dec(&b, r.version_minor);
    }
    ig_kv_key(&b, "markers_present");
    ig_buf_i64_dec(&b, (int64_t)r.present_count);
    ig_kv_key(&b, "markers_total");
    ig_buf_i64_dec(&b, IG_AXES_MARKER_COUNT);
    ig_kv_key(&b, "bytes_scanned");
    ig_buf_u64_dec(&b, r.bytes_scanned);
    ig_kv_key(&b, "chunks_read");
    ig_buf_u64_dec(&b, r.chunks_read);
    ig_kv_key(&b, "read_refused");
    ig_buf_u64_dec(&b, r.read_refused);
    ig_kv_key(&b, "config_resolved");
    ig_buf_i64_dec(&b, r.config_resolved);
    ig_kv_key(&b, "config_cost_best");
    ig_buf_u64_dec(&b, r.config_best_cost);
    ig_kv_key(&b, "config_cost_runner");
    ig_buf_u64_dec(&b, r.config_runner_cost);
    ig_kv_key(&b, "stats_resolved");
    ig_buf_i64_dec(&b, r.stats_resolved);
    ig_kv_key(&b, "stats_cost_best");
    ig_buf_u64_dec(&b, r.stats_best_cost);
    ig_kv_key(&b, "stats_cost_runner");
    ig_buf_u64_dec(&b, r.stats_runner_cost);
    ig_kv_key(&b, "version_resolved");
    ig_buf_i64_dec(&b, r.version_resolved);
    ig_kv_key(&b, "image_base");
    ig_buf_u64_hex(&b, image_base);
    ig_kv_key(&b, "image_extent");
    ig_buf_u64_hex(&b, image_extent);
    ig_record_flush(&b);
    return ig_sink_write_record(sink, &b);
}

// qh_driver.cpp — worker-record-sink-addon (qdrv1, team 2026-10-04). NEW FILE.
//
// MISSION: drive the vendored NativeQueryHarness from OUTSIDE the composition
// and emit its records onto /data/modbridge-ingame-facts.jsonl so rung-1/2
// evidence (ticks_accepted advancing, game_thread_id_key, guard refusals, the
// pinned FormatRecordJsonLine terminal record) can exist this launch.
//
// ROUTE FACTS (see DESIGN.md for full cites):
//  * Nothing in the composed PRX calls OnEngineTickDispatch/FormatRecordJsonLine
//    (WIRING.md:120-124 leaves the driver to the controller; tree-wide grep:
//    host tests only). A separate late-load PRX has the same in-process
//    privileges as the igf probe family, which read the live GUA and wrote
//    this exact JSONL from their own module.
//  * The harness's EngineBindings seam (hpp:155-173) is injectable by design;
//    this driver installs its OWN four probes (the RC::Unreal default
//    bindings live inside the composed runtime and cannot be linked here).
//  * HONESTY LIMIT (also in CLAIM.json): the driver thread is NOT the engine's
//    OS game thread. game_thread_id_key is the add-on's published driver-
//    thread identity fold (stable, nonzero), and "one admitted thread" is the
//    harness's own discipline applied to this driver. In-game-thread execution
//    stays with the composition's persistent-callback route (WIRING.md).
//
// PER-POLL FLOW (one dispatch per poll, 4s cadence, 30 polls, then self-end):
//   1. fresh ig_mapscan_run() (controller correction #1 discipline: a CURRENT
//      per-invocation VMMAP scan backs every foreign dereference);
//   2. GUA admission: span-coverage check (bit0 readable) for the pinned
//      runtime address IG_RT_GUOBJECT_ARRAY, then a 96-byte direct deref into
//      a stack copy (the igf6-proven read class: known-mapped address, no
//      syscall, no mprotect), then header-window admission checks (below);
//   3. wiring schedule: bindings install on the first poll AFTER the count
//      source first admitted (so the harness's own refused_bindings_absent
//      guard fires naturally at least once); the layout gate arms only after
//      QH_STABILITY_POLLS consecutive admitted polls with NumElements strictly
//      above the harness threshold (so the terminal pair can only be
//      Inconclusive by a genuine post-arm collapse, never by arming early);
//   4. dispatch: harness refuses through its own guards whenever admission
//      fails (game_thread_id_key returns 0 -> RefusedIdentityUnknown -> any
//      staged count is discarded; the terminal record can never be built from
//      a failed read);
//   5. emission: ONE ig-envelope counter line per poll (phase, admission
//      rcode, raw GUA header fields for offline corrected-predicate replay,
//      and the harness dispatch counters — fields named exactly so
//      rung_check.py reads ticks_accepted / refused_* inside .fields), and,
//      once TrySnapshot hands out a terminal record, the pinned
//      FormatRecordJsonLine output written as a STANDALONE JSONL line (the
//      checker classifies it by top-level "record":"native_query_harness";
//      enveloping it would hide that key — documented in DESIGN.md).
//
// ADMISSION CHECKS (header window; layout A = FUObjectArray outer):
//   cited provenance: worker-batch2-live-prep/src/live_gua_reads.h
//   (offsets from worker-predicate-gc-fix vendored-fixed
//   townfall_layout_probes.cpp:19-31) + the LIVE-VALIDATED observed set
//   (worker-staged-pair-live/CLAIM.json via worker-anchor-validation):
//   layout A at runtime 0x97ba110, NumElements=71959, MaxElements=2162688,
//   MaxChunks=33, NumChunks=2, first_gc=35477 > last_non_gc=35476,
//   Objects**=0x13800220a0, PreAllocated=NULL, engine 5.6, base 0x400000.
//   checks (all must pass; rcode recorded per poll):
//     1 GUA not covered by a current readable span      -> not admitted
//     2 covering span not readable (prot bit0 clear)
//     3 layout-B pointer read (@+0x00 as u64) CANONICAL -> ambiguous, refuse
//       (with live layout-A data this u64 concatenates two small i32s and is
//       non-canonical by the user-half rule, so exactly one interpretation)
//     4 Objects**/PreAllocated not canonical-or-null
//     5 both pointers null
//     6 counts insane: !(0 < NumElements <= MaxElements &&
//       1 <= NumChunks <= MaxChunks <= 512 && MaxElements <= MaxChunks*65536)
//     7 GC order: !(first_gc > last_non_gc && first_gc >= 0 &&
//       last_non_gc >= -1 && last_non_gc <= MaxElements)   [corrected
//       invariant from worker-predicate-gc-fix — the old form refuted
//       healthy arrays]
//   The full corrected-predicate chunk-extent chase (cfgL) is NOT performed
//   in-PRX; the raw fields ride every counter line for offline replay.

#include "qh_driver.h"

#include "NativeQueryHarness.hpp"

#ifndef QH_DRIVER_HOST_TEST
#include "anchor_store.h"
#include "generated/ingame_config.h"
#else
#include "anchor_store.h"  // host-safe: plain C header (IgSpan types)
#ifndef IG_BUILD_ID
#error "host builds must define IG_BUILD_ID"
#endif
#ifndef IG_RT_GUOBJECT_ARRAY
#error "host builds must define IG_RT_GUOBJECT_ARRAY"
#endif
#endif

#include <pthread.h>
#include <time.h>
#include <unistd.h>

// The POSIX rows (nanosleep, pthread_create, pthread_detach, pthread_self,
// getpid) are declared by the pinned SDK target headers (pthread.h, time.h,
// unistd.h) with the same identities the frozen 191-row contract carries
// (ingame_abi.h discipline: not redeclared to avoid conflicts). Both the SDK
// target and the macOS host agree on these signatures.

extern unsigned g_nqh_wiring_refusals;  // vendored harness labeled change 2
extern unsigned g_ctor_events;          // module_entry.c lineage counter

namespace {

using RC::Platform::QueryHarness::CountSourceKind;
using RC::Platform::QueryHarness::EngineBindings;
using RC::Platform::QueryHarness::FormatRecordJsonLine;
using RC::Platform::QueryHarness::Global;
using RC::Platform::QueryHarness::kJsonLineCapacity;
using RC::Platform::QueryHarness::LayoutGateFacts;
using RC::Platform::QueryHarness::Phase;
using RC::Platform::QueryHarness::QueryResultRecord;
using RC::Platform::QueryHarness::StorageLayout;

// ---- pinned mission constants (provenance above + generated config) ----
constexpr uint64_t kGuaRt = (uint64_t)(IG_RT_GUOBJECT_ARRAY);
constexpr unsigned kHeaderBytes = 96;
constexpr unsigned kPollSeconds = 4;
constexpr unsigned kPollCount = 30;
constexpr unsigned kStabilityPolls = 6;
constexpr int64_t kThreshold =
    (int64_t)RC::Platform::QueryHarness::kMinimumPlausibleElementCount;
constexpr uint32_t kEngineMajor = 5;  // live-validated identity (staged-pair-live)
constexpr uint32_t kEngineMinor = 6;
constexpr const char *kGateSource = "igu_layout_a_live_header";  // 24 chars

// Admission rcodes (recorded every poll; 0 = admitted).
enum {
    kAdmOk = 0,
    kAdmNotCovered = 1,
    kAdmNotReadable = 2,
    kAdmAmbiguousLayout = 3,
    kAdmPtrNonCanonical = 4,
    kAdmPtrsBothNull = 5,
    kAdmCountsInsane = 6,
    kAdmGcOrder = 7,
};

struct GuHeaderView {
    int32_t first_gc;
    int32_t last_non_gc;
    int32_t max_not_considered;
    int32_t open_for_dfgc;
    uint64_t objects;
    uint64_t prealloc;
    uint32_t max_elements;
    uint32_t num_elements;
    uint32_t max_chunks;
    uint32_t num_chunks;
};

struct Admission {
    int ok;
    int rcode;
    GuHeaderView view;
};

struct DriverState {
    IgSink *sink;
    unsigned seq;
    int map_trusted;
    int poll_admitted;
    int poll_rcode;
    int64_t poll_count;
    GuHeaderView poll_view;
    int ever_admitted;
    unsigned streak;
    int bindings_installed;
    int gate_armed;
    unsigned long self_word;
    uint64_t key;
    int qh_record_emitted;
    unsigned polls_run;
    IgBuf buf;
};

DriverState g_st;

inline uint32_t ld32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
inline uint64_t ld64(const uint8_t *p) {
    return (uint64_t)ld32(p) | ((uint64_t)ld32(p + 4) << 32);
}
// User-half canonicality (cited: live_gua_reads.h igu_ptr_canonical).
inline int ptr_canonical(uint64_t p) { return (p >> 47) == 0; }
inline int ptr_canonical_or_null(uint64_t p) { return p == 0 || ptr_canonical(p); }

// ---- injectable ops -------------------------------------------------------
struct Ops {
    int (*mapscan_run)(void);
    int (*cover)(uint64_t addr, uint64_t len, IgSpan *out);
    void (*deref)(void *dst, uint64_t src, unsigned n);
    unsigned long (*self)(void);
    int (*sleep_sec)(long sec);
};

#ifndef QH_DRIVER_HOST_TEST

int op_mapscan_run(void) { return ig_mapscan_run(); }
int op_cover(uint64_t addr, uint64_t len, IgSpan *out) {
    return ig_map_cover(addr, len, out);
}
// The igf6-proven read class: plain dereference of a span-checked known-mapped
// address. NO mprotect, NO raw syscall, NO gate.
void op_deref(void *dst, uint64_t src, unsigned n) {
    const uint8_t *p = (const uint8_t *)(uintptr_t)src;
    uint8_t *q = (uint8_t *)dst;
    for (unsigned i = 0; i < n; ++i) q[i] = p[i];
}
unsigned long op_self(void) { return (unsigned long)pthread_self(); }
int op_sleep_sec(long sec) {
    struct timespec req, rem;
    req.tv_sec = sec;
    req.tv_nsec = 0;
    rem.tv_sec = 0;
    rem.tv_nsec = 0;
    return nanosleep((const struct timespec *)&req, &rem);
}

const Ops g_ops = {op_mapscan_run, op_cover, op_deref, op_self, op_sleep_sec};

#else  // QH_DRIVER_HOST_TEST

Ops g_ops = {0, 0, 0, 0, 0};
static unsigned g_last_poll_count = 0;

#endif  // QH_DRIVER_HOST_TEST

// ---- engine-window admission ---------------------------------------------
Admission admit(void) {
    Admission a;
    a.ok = 0;
    a.rcode = kAdmNotCovered;
    a.view = GuHeaderView{};

    IgSpan span;
    span.start = span.end = 0;
    span.prot = 0;
    if (g_ops.cover(kGuaRt, kHeaderBytes, &span) != 1) return a;  // 1
    if (!(span.prot & 1u)) { a.rcode = kAdmNotReadable; return a; }  // 2

    uint8_t hdr[kHeaderBytes];
    for (unsigned i = 0; i < kHeaderBytes; ++i) hdr[i] = 0;
    g_ops.deref(hdr, kGuaRt, kHeaderBytes);

    GuHeaderView v;
    v.first_gc = (int32_t)ld32(hdr + 0x00);
    v.last_non_gc = (int32_t)ld32(hdr + 0x04);
    v.max_not_considered = (int32_t)ld32(hdr + 0x08);
    v.open_for_dfgc = (int32_t)ld32(hdr + 0x0C);
    v.objects = ld64(hdr + 0x10);
    v.prealloc = ld64(hdr + 0x18);
    v.max_elements = ld32(hdr + 0x20);
    v.num_elements = ld32(hdr + 0x24);
    v.max_chunks = ld32(hdr + 0x28);
    v.num_chunks = ld32(hdr + 0x2C);
    a.view = v;

    // Layout-B disambiguation: under layout A the first 8 bytes concatenate
    // two small int32 fields, which fails the user-half canonicality rule;
    // if it PASSES, the label address could be a bare TUObjectArray and the
    // two hypotheses are ambiguous in this header window -> refuse.
    if (ptr_canonical(ld64(hdr + 0x00))) { a.rcode = kAdmAmbiguousLayout; return a; }  // 3

    if (!ptr_canonical_or_null(v.objects) || !ptr_canonical_or_null(v.prealloc)) {
        a.rcode = kAdmPtrNonCanonical;  // 4
        return a;
    }
    if (v.objects == 0 && v.prealloc == 0) { a.rcode = kAdmPtrsBothNull; return a; }  // 5

    const int64_t num = (int64_t)v.num_elements;
    const int64_t max_el = (int64_t)v.max_elements;
    const int64_t max_ch = (int64_t)v.max_chunks;
    const int64_t num_ch = (int64_t)v.num_chunks;
    const int64_t counts_ok =
        (num > 0) & (num <= max_el) & (max_ch >= 1) & (max_ch <= 512) &
        (num_ch >= 1) & (num_ch <= max_ch) &
        (max_el <= max_ch * (int64_t)65536);
    if (!counts_ok) { a.rcode = kAdmCountsInsane; return a; }  // 6

    // Corrected GC invariant (worker-predicate-gc-fix): permanent objects
    // occupy [0, ObjLastNonGCIndex]; ObjFirstGCIndex == ObjLastNonGCIndex + 1
    // settled; a gap is legal transiently. The old inverted form refuted
    // healthy arrays (live: first=35477, last=35476).
    const int32_t fg = v.first_gc;
    const int32_t lg = v.last_non_gc;
    const int gc_ok = (fg > lg) & (fg >= 0) & (lg >= -1) &
                      ((int64_t)lg <= max_el);
    if (!gc_ok) { a.rcode = kAdmGcOrder; return a; }  // 7

    a.ok = 1;
    a.rcode = kAdmOk;
    return a;
}

// ---- the four injectable harness probes -----------------------------------
// HONESTY (CLAIM.json/DESIGN.md): "engine window" = current-map trusted AND
// this poll's admission passed; "the one admitted thread" = this driver thread
// (pthread_self fold); key returns 0 whenever admission fails so the harness
// REFUSES (never reads a count through a broken window).
bool b_engine_window() noexcept {
    return g_st.map_trusted != 0 && g_st.poll_admitted != 0;
}
bool b_admitted_thread() noexcept {
    return g_st.self_word != 0 && g_ops.self() == g_st.self_word &&
           g_st.poll_admitted != 0;
}
uint64_t b_key() noexcept {
    if (!b_engine_window()) return 0;
    return g_st.key;
}
int32_t b_count() noexcept { return (int32_t)g_st.poll_count; }

// ---- emission --------------------------------------------------------------
int flush_line(void) {
    if (ig_sink_write_record(g_st.sink, &g_st.buf) != 0) {
        g_st.sink->failed = 1;
        return -1;
    }
    return 0;
}

void emit_counter(int poll_idx, const char *phase) {
    const RC::Platform::QueryHarness::DispatchCounters counters =
        Global().SnapshotDispatchCounters();
    IgBuf *b = &g_st.buf;
    ig_record_begin(b, (int)g_st.seq++, "qh_counter", 0, 0, IG_BUILD_ID);
    ig_kv_int(b, "poll", poll_idx);
    ig_kv_str(b, "phase", phase, 24);
    ig_kv_int(b, "map_trusted", g_st.map_trusted);
    ig_kv_int(b, "admitted", g_st.poll_admitted);
    ig_kv_int(b, "admission_rcode", g_st.poll_rcode);
    ig_kv_int(b, "gua_num_elements", g_st.poll_count);
    ig_kv_int(b, "gua_max_elements", (int64_t)g_st.poll_view.max_elements);
    ig_kv_int(b, "gua_max_chunks", (int64_t)g_st.poll_view.max_chunks);
    ig_kv_int(b, "gua_num_chunks", (int64_t)g_st.poll_view.num_chunks);
    ig_kv_int(b, "gua_first_gc", (int64_t)g_st.poll_view.first_gc);
    ig_kv_int(b, "gua_last_non_gc", (int64_t)g_st.poll_view.last_non_gc);
    ig_kv_int(b, "ticks_dispatched", (int64_t)counters.ticks_dispatched);
    ig_kv_int(b, "ticks_accepted", (int64_t)counters.ticks_accepted);
    ig_kv_int(b, "refused_bindings_absent",
              (int64_t)counters.refused_bindings_absent);
    ig_kv_int(b, "refused_layout_gate_closed",
              (int64_t)counters.refused_layout_gate_closed);
    ig_kv_int(b, "refused_identity_unknown",
              (int64_t)counters.refused_identity_unknown);
    ig_kv_int(b, "refused_wrong_thread", (int64_t)counters.refused_wrong_thread);
    ig_kv_int(b, "staged_resets", (int64_t)counters.staged_resets);
    ig_kv_int(b, "qh_record_emitted", g_st.qh_record_emitted);
    ig_record_flush(b);
    (void)flush_line();
}

void maybe_emit_qh_record(void) {
    if (g_st.qh_record_emitted) return;
    QueryResultRecord rec;
    if (!Global().TrySnapshot(rec)) return;
    char line[kJsonLineCapacity];
    const size_t n = FormatRecordJsonLine(rec, line);
    IgBuf *b = &g_st.buf;
    if (n == 0) {
        // LOUD: the pinned formatter refused the buffer; do not substitute a
        // hand-rolled shape. The counter lines still carry the state.
        ig_record_begin(b, (int)g_st.seq++, "qh_format_refused", 0, 0,
                        IG_BUILD_ID);
        ig_record_flush(b);
        (void)flush_line();
        return;
    }
    // The qh record is written AS THE PINNED LINE (top-level
    // "record":"native_query_harness"), NOT inside the ig envelope:
    // rung_check.py classifies rung-1/2 evidence by that top-level key.
    ig_buf_reset(b);
    ig_buf_raw(b, line);
    ig_buf_ch(b, '\n');
    (void)flush_line();
    g_st.qh_record_emitted = 1;
}

void emit_summary(void) {
    const RC::Platform::QueryHarness::DispatchCounters counters =
        Global().SnapshotDispatchCounters();
    IgBuf *b = &g_st.buf;
    ig_record_begin(b, (int)g_st.seq++, "summary", 0, 0, IG_BUILD_ID);
    ig_kv_int(b, "polls_run", (int64_t)g_st.polls_run);
    ig_kv_int(b, "map_trusted_last", g_st.map_trusted);
    ig_kv_int(b, "admitted_last", g_st.poll_admitted);
    ig_kv_int(b, "qh_record_emitted", g_st.qh_record_emitted);
    ig_kv_int(b, "wiring_refusals", (int64_t)g_nqh_wiring_refusals);
    ig_kv_int(b, "write_failures", (int64_t)g_st.sink->failed);
    ig_kv_int(b, "ticks_dispatched", (int64_t)counters.ticks_dispatched);
    ig_kv_int(b, "ticks_accepted", (int64_t)counters.ticks_accepted);
    ig_kv_int(b, "staged_resets", (int64_t)counters.staged_resets);
    ig_kv_hex(b, "driver_key", g_st.key);
    ig_kv_int(b, "ctor_events", (int64_t)g_ctor_events);
    ig_record_flush(b);
    (void)flush_line();
}

// ---- the paced loop --------------------------------------------------------
void run_driver_loop(IgSink *sink) {
    g_st.sink = sink;
    g_st.seq = 3;  // 1 entry + 2 startup already written by module_start
    g_st.map_trusted = 0;
    g_st.poll_admitted = 0;
    g_st.poll_rcode = kAdmNotCovered;
    g_st.poll_count = 0;
    g_st.poll_view = GuHeaderView{};
    g_st.ever_admitted = 0;
    g_st.streak = 0;
    g_st.bindings_installed = 0;
    g_st.gate_armed = 0;
    g_st.self_word = g_ops.self();
    {
        const uint64_t pid = (uint64_t)(unsigned int)getpid();
        g_st.key = ((pid << 32) | ((uint64_t)g_st.self_word & 0xffffffffu)) | 1u;
        if (g_st.key == 0) g_st.key = 1;
    }
    g_st.qh_record_emitted = 0;
    g_st.polls_run = 0;

    // seq 3: map/context record (counts only, lineage discipline).
    g_st.map_trusted = g_ops.mapscan_run();
    {
        IgBuf *b = &g_st.buf;
        ig_record_begin(b, (int)g_st.seq++, "mapscan", 0, 0, IG_BUILD_ID);
        ig_kv_int(b, "trusted", g_st.map_trusted);
        ig_kv_hex(b, "gua_rt", kGuaRt);
        ig_kv_int(b, "header_bytes", (int64_t)kHeaderBytes);
        ig_kv_str(b, "note", "current-scan span gate; GUA header window; no "
                             "chunk chase in-PRX", 56);
        ig_record_flush(b);
        (void)flush_line();
    }

    for (unsigned poll = 0; poll < kPollCount; ++poll) {
        if (poll != 0) {
            (void)g_ops.sleep_sec((long)kPollSeconds);
        }
        // Fresh per-invocation scan + admission (bindings read this snapshot;
        // the harness consumes it in the single dispatch below).
        g_st.map_trusted = g_ops.mapscan_run();
        const Admission a = admit();
        g_st.poll_admitted = a.ok;
        g_st.poll_rcode = a.rcode;
        g_st.poll_view = a.view;
        g_st.poll_count = a.ok ? (int64_t)a.view.num_elements : 0;
        if (a.ok) g_st.ever_admitted = 1;

        // Wiring schedule (each function called at most once with checked
        // preconditions; g_nqh_wiring_refusals is the LOUD wiring-bug signal).
        const char *phase = "bindings_absent";
        if (g_st.bindings_installed == 0) {
            if (g_st.ever_admitted != 0 && poll >= 1) {
                EngineBindings eb;
                eb.is_game_thread_initialized = &b_engine_window;
                eb.is_in_game_thread_raw = &b_admitted_thread;
                eb.game_thread_id_key = &b_key;
                eb.read_num_elements = &b_count;
                eb.count_source_kind = CountSourceKind::EngineUObjectArray;
                eb.storage_layout = StorageLayout::ChunkedArray;
                Global().InstallEngineBindings(eb);
                g_st.bindings_installed = (g_nqh_wiring_refusals == 0) ? 1 : 0;
                phase = "gate_closed";
            }
        } else {
            if (a.ok != 0 && (int64_t)a.view.num_elements > kThreshold) {
                ++g_st.streak;
            } else {
                g_st.streak = 0;
            }
            if (g_st.gate_armed == 0 && g_st.streak >= kStabilityPolls) {
                LayoutGateFacts gf;
                gf.engine_major = kEngineMajor;
                gf.engine_minor = kEngineMinor;
                gf.non_shipping = 0;  // shipping title axes (labeled add-on
                gf.stats = 0;         // constants; the S2 scan detector is
                                      // inside the composition)
                gf.storage = StorageLayout::ChunkedArray;  // live NumChunks=2
                for (unsigned i = 0; i < sizeof(gf.source); ++i) gf.source[i] = 0;
                for (unsigned i = 0; kGateSource[i] != '\0' &&
                                     i + 1 < sizeof(gf.source); ++i) {
                    gf.source[i] = kGateSource[i];
                }
                const unsigned before = g_nqh_wiring_refusals;
                Global().ArmLayoutGate(gf);
                g_st.gate_armed = (g_nqh_wiring_refusals == before) ? 1 : 0;
            }
            if (g_st.gate_armed != 0) {
                phase = (Global().phase() == Phase::Terminal) ? "terminal"
                                                              : "armed";
            } else {
                phase = "gate_closed";
            }
        }

        (void)Global().OnEngineTickDispatch();
        ++g_st.polls_run;
        emit_counter((int)poll, phase);
        maybe_emit_qh_record();
    }
    emit_summary();
}

}  // namespace

extern "C" int qh_driver_start(IgSink *sink) {
#ifndef QH_DRIVER_HOST_TEST
    pthread_t thread = 0;
    const int rc = pthread_create(&thread, 0, [](void *arg) -> void * {
        IgSink *s = (IgSink *)arg;
        run_driver_loop(s);
        ig_sink_close(s);
        return 0;
    }, (void *)sink);
    if (rc != 0) return rc != 0 ? rc : -1;
    if (thread == 0) return -1;
    (void)pthread_detach(thread);
    return 0;
#else
    (void)sink;
    run_driver_loop(sink);
    g_last_poll_count = g_st.polls_run;
    return 0;
#endif
}

#ifdef QH_DRIVER_HOST_TEST
extern "C" void qd_install_ops(const QdOps *ops) {
    g_ops.mapscan_run = ops->mapscan_run;
    g_ops.cover = (int (*)(uint64_t, uint64_t, IgSpan *))ops->cover;
    g_ops.deref = ops->deref;
    g_ops.self = ops->self;
    g_ops.sleep_sec = ops->sleep_sec;
}
extern "C" unsigned qd_last_poll_count(void) { return g_last_poll_count; }
#endif

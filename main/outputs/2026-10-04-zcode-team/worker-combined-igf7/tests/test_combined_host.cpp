// test_combined_host.cpp — worker-combined-igf7 host suite.
//
// Exercises the COMBINED igf7- probe logic end-to-end on synthetic
// KERN_PROC_VMMAP buffers + fixture bytes (live discipline: the walk leaves
// identity data pointers; the test re-points span->data into fixture
// storage, exactly as the rung3 suite did). The sysctl collection itself is
// target-only and is NOT exercised here (macOS lacks KERN_PROC_VMMAP).
//
// Fixtures mirror the LIVE recorded values wherever they exist: GUA
// 0x97ba110, Objects** 0x13800220a0, NumElements 71959 / MaxChunks 33 /
// NumChunks 2 / first_gc 35477 / last_non_gc 35476 (worker-staged-pair-live,
// worker-batch2-live-prep). Chunk/object storage addresses beyond the live
// chunk-table pointer are SYNTHETIC and labeled as such.
//
// The C2 candidate fixture bytes are the OFFLINE expected bytes from
// worker-c2-probe/EXPECTED-VALUES-OFFLINE.json — used ONLY to prove the raw
// read plumbing (live_hex must come out byte-exact); the probe itself never
// sees them.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "record_sink.h"
}

extern "C" void ig_run_combined_probe(IgSink *sink);
extern "C" void igf7_host_set_map(const uint8_t *buf, uint64_t len);
extern "C" int igf7_host_repoint(uint64_t start, const uint8_t *data);
extern "C" void ig_run_combined_probe_no_map_reset(IgSink *sink);

namespace {

int g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { \
    std::printf("    FAIL line %d: %s\n", __LINE__, #cond); g_fail++; } } while (0)
static int g_fail = 0;

/* ---------------- synthetic vmmap buffer ---------------- */

struct Rec { uint64_t start, end; uint32_t prot; uint32_t ksz; };

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put64(uint8_t *p, uint64_t v) {
    put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32));
}

static size_t push_rec(uint8_t *buf, size_t off, const Rec &r) {
    const uint32_t ksz = r.ksz ? r.ksz : 144u;
    std::memset(buf + off, 0, ksz);
    put32(buf + off + 0, ksz);        /* kve_structsize */
    put32(buf + off + 4, 2);          /* kve_type (VM_OBJ) */
    put64(buf + off + 8, r.start);    /* kve_start */
    put64(buf + off + 16, r.end);     /* kve_end */
    put32(buf + off + 56, r.prot);    /* kve_protection (KVME bits) */
    return off + ksz;
}

/* ---------------- fixtures ---------------- */

static const uint8_t HEXB[17] = "0123456789abcdef";
static void hexbytes(const char *hex, uint8_t *out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const char *s = hex + 2 * i;
        int hi = 0, lo = 0;
        for (int k = 0; k < 16; ++k) { if (HEXB[k] == s[0]) hi = k;
                                      if (HEXB[k] == s[1]) lo = k; }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
}

/* C2 OFFLINE expected bytes (worker-c2-probe/EXPECTED-VALUES-OFFLINE.json). */
static const char *EXP_B =
    "5500450035000000";
static const char *EXP_C =
    "4200750069006c006400490064004f0076006500720072006900640065000000";
static const char *EXP_D =
    "43757272656e74204368616e67656c69737400";
static const char *EXP_E =
    "637c777bf26b6fc53001672bfed7ab76ca82c97dfa5947f0add4a2af9ca472c0"
    "b7fd9326363ff7cc34a5e5f171d8311504c723c31896059a071280e2eb27b275";
static const char *EXP_A =
    "040000001400000003000000474e550004bb4330a7d71dcefe1cc2c4d19769d5"
    "00000000";

static uint8_t fB[0x40], fC[0x2A], fD[0x30], fE[0x40], fA[0x24];
static uint8_t fGUA[0x200], fOBJ[0x18], fC0[0x40], fC1[0x40];
static uint8_t fOBJW[0x1000], fVTW[0x1000];

static void init_fixtures() {
    hexbytes(EXP_B, fB, 8);
    hexbytes(EXP_C, fC, 32);
    hexbytes(EXP_D, fD, 19);
    hexbytes(EXP_E, fE, 64);
    hexbytes(EXP_A, fA, 36);

    /* GUA header (layout A) at fixture offset 0x110 (vaddr 0x97BA110 in
       span 0x97BA000..). LIVE header values. */
    std::memset(fGUA, 0, sizeof fGUA);
    put32(fGUA + 0x110 + 0x00, 35477);   /* first_gc           */
    put32(fGUA + 0x110 + 0x04, 35476);   /* last_non_gc        */
    put32(fGUA + 0x110 + 0x08, 1024);    /* max_not_gc         */
    put32(fGUA + 0x110 + 0x0C, 0);       /* open_dfgc          */
    put64(fGUA + 0x110 + 0x10, 0x13800220a0ull); /* Objects** LIVE */
    put32(fGUA + 0x110 + 0x20, 2162688); /* MaxElements = 33*65536 */
    put32(fGUA + 0x110 + 0x24, 71959);   /* NumElements LIVE   */
    put32(fGUA + 0x110 + 0x28, 33);      /* MaxChunks LIVE     */
    put32(fGUA + 0x110 + 0x2C, 2);       /* NumChunks LIVE     */

    /* chunk table: two chunk pointers (SYNTHETIC storage addresses). */
    put64(fOBJ + 0x00, 0x1381000000ull);
    put64(fOBJ + 0x08, 0x1382000000ull);

    /* FUObjectItem slot 0 of each chunk (0x18 stride). */
    put64(fC0 + 0x00, 0x1383000100ull); put32(fC0 + 0x08, 0);
    put32(fC0 + 0x0C, 0); put32(fC0 + 0x10, 1);
    put64(fC1 + 0x00, 0x1383000100ull); put32(fC1 + 0x08, 0);
    put32(fC1 + 0x0C, 0); put32(fC1 + 0x10, 2);

    /* UObjectBase (0x28) at 0x1383000100 + ClassPrivate window at
       0x1383000800 (offset 0x800). */
    std::memset(fOBJW, 0, sizeof fOBJW);
    put64(fOBJW + 0x100, 0x1383100080ull);      /* vptr (identity only) */
    put32(fOBJW + 0x108, 0x10);                 /* ObjectFlags          */
    put32(fOBJW + 0x10C, 0);                    /* InternalIndex 0      */
    put64(fOBJW + 0x110, 0x1383000800ull);      /* ClassPrivate         */
    fOBJW[0x118] = 0x01;                        /* NamePrivate RAW      */
    /* OuterPrivate stays 0 */
    put64(fOBJW + 0x800, 0x1383100080ull);      /* class vptr-ish       */

    std::memset(fVTW, 0, sizeof fVTW);
    put64(fVTW + 0x80, 0x400000deadbeefull & 0x0000ffffffffffffull);
}

/* Map variants: 0 happy, 1 A W-only, 2 E exec-overlap, 3 non-canonical
   Objects**, 4 untrusted walk, 5 candidate C uncovered. */
static size_t build_map(uint8_t *buf, int variant) {
    size_t off = 0;
    if (variant == 4) {
        /* one insane record first: structsize 140 (< 136? no — 140 >= 136,
           so use 132 to be insane) -> walk stops immediately. */
        off = push_rec(buf, off, Rec{1, 2, 1, 132});
    }
    off = push_rec(buf, off, Rec{0x400000, 0x787c000, 4, 0});      /* exec */
    off = push_rec(buf, off, Rec{0x7C26782, 0x7C2678A, 1, 0});     /* B    */
    if (variant != 5)
        off = push_rec(buf, off, Rec{0x79CFFDA, 0x79CFFFA, 1, 0}); /* C    */
    off = push_rec(buf, off, Rec{0x7F8845D, 0x7F88470, 1, 0});     /* D    */
    off = push_rec(buf, off, Rec{0x81D2A40, 0x81D2A80, variant == 2 ? 4u : 1u, 0}); /* E */
    off = push_rec(buf, off, Rec{0xC05F250, 0xC05F274, variant == 1 ? 2u : 1u, 0}); /* A */
    off = push_rec(buf, off, Rec{0x97BA000, 0x97BA200, 3, 0});     /* GUA  */
    off = push_rec(buf, off, Rec{0x13800220A0, 0x13800220B8, 3, 0});
    off = push_rec(buf, off, Rec{0x1381000000, 0x1381000040, 3, 0});
    off = push_rec(buf, off, Rec{0x1382000000, 0x1382000040, 3, 0});
    off = push_rec(buf, off, Rec{0x1383000000, 0x1383001000, 3, 0});
    off = push_rec(buf, off, Rec{0x1383100000, 0x1383101000, 3, 0});
    off = push_rec(buf, off, Rec{0x1390000000, 0x1390001000, 2, 0}); /* W-only */
    off = push_rec(buf, off, Rec{0x1380000000, 0x1380001000, 3, 0}); /* dummies */
    off = push_rec(buf, off, Rec{0x1380001000, 0x1380002000, 3, 0});
    off = push_rec(buf, off, Rec{0x1380002000, 0x1380003000, 3, 0});
    return off;
}

/* Live discipline: re-point span->data of the walked table into fixtures. */
struct Repoint { uint64_t start; const uint8_t *data; };

static void repoint_all(const Repoint *map, size_t n) {
    /* reaches into the probe's static table through the declared accessor */
    for (size_t i = 0; i < n; ++i) (void)igf7_host_repoint(map[i].start, map[i].data);
}

static void repoint_standard(int variant) {
    if (variant == 3) put64(fGUA + 0x110 + 0x10, 0x8000000000000000ull);
    const Repoint m[] = {
        {0x7C26782, fB}, {0x79CFFDA, fC}, {0x7F8845D, fD},
        {0x81D2A40, fE}, {0xC05F250, fA}, {0x97BA000, fGUA},
        {0x13800220A0, fOBJ}, {0x1381000000, fC0}, {0x1382000000, fC1},
        {0x1383000000, fOBJW}, {0x1383100000, fVTW},
    };
    repoint_all(m, sizeof m / sizeof m[0]);
}

/* ---------------- run one full probe into a file ---------------- */

static std::string run_to_file(const char *path, int variant) {
    std::remove(path);  /* the sink opens O_APPEND; start each run clean */
    IgSink sink;
    sink.fd = -1; sink.failed = 0; sink.open_errno = 0;
    if (ig_sink_open(&sink, path) != 0) { CHECK(!"sink open"); return ""; }
    (void)variant;
    uint8_t *buf = new uint8_t[4096];
    size_t len = build_map(buf, variant);
    igf7_host_set_map(buf, (uint64_t)len);
    /* set_map resets state; repoint AFTER it (walk re-assigns identity). */
    repoint_standard(variant);
    ig_run_combined_probe_no_map_reset(&sink);
    (void)ig_sink_close(&sink);
    delete[] buf;
    FILE *f = std::fopen(path, "rb");
    std::string out;
    char tmp[4096];
    size_t n;
    while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) out.append(tmp, n);
    std::fclose(f);
    return out;
}

/* ---------------- minimal JSON value extraction ---------------- */

static bool json_value_after(const std::string &line, const std::string &key,
                             size_t from, size_t *val_begin,
                             size_t *val_end) {
    const std::string pat = "\"" + key + "\":";
    size_t p = line.find(pat, from);
    if (p == std::string::npos) return false;
    p += pat.size();
    *val_begin = p;
    if (line[p] == '"') {
        p = line.find('"', p + 1);
        *val_end = (p == std::string::npos) ? std::string::npos : p + 1;
    } else {
        while (p < line.size() && line[p] != ',' && line[p] != '}' &&
               line[p] != ']') ++p;
        *val_end = p;
    }
    return true;
}

static std::string jstr(const std::string &line, const std::string &key,
                        size_t from = 0) {
    size_t b, e;
    if (!json_value_after(line, key, from, &b, &e) || line[b] != '"')
        return "";
    return line.substr(b + 1, e - b - 2);
}

static int64_t jint(const std::string &line, const std::string &key,
                    size_t from = 0) {
    size_t b, e;
    if (!json_value_after(line, key, from, &b, &e)) return -999999;
    return std::strtoll(line.substr(b, e - b).c_str(), nullptr, 10);
}

static int64_t jhex(const std::string &line, const std::string &key,
                    size_t from = 0) {
    size_t b, e;
    if (!json_value_after(line, key, from, &b, &e)) return -999999;
    return std::strtoll(line.substr(b + 3, e - b - 4).c_str(), nullptr, 16);
}

static std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> v;
    size_t p = 0;
    while (p < s.size()) {
        size_t q = s.find('\n', p);
        if (q == std::string::npos) q = s.size();
        v.push_back(s.substr(p, q - p + 1));
        p = q + 1;
    }
    return v;
}

static std::string find_rec(const std::string &file, const char *exp,
                            const char *cand = nullptr, int nth = 0) {
    for (const auto &l : split_lines(file)) {
        if (l.find("\"exp\":\"" + std::string(exp) + "\"") == std::string::npos)
            continue;
        if (cand && jstr(l, "cand") != cand) continue;
        if (nth-- > 0) continue;
        return l;
    }
    return "";
}

static int count_rec(const std::string &file, const char *exp) {
    int n = 0;
    for (const auto &l : split_lines(file))
        if (l.find("\"exp\":\"" + std::string(exp) + "\"") !=
            std::string::npos) ++n;
    return n;
}

}  // namespace

/* ---------------- the suite ---------------- */

int main() {
    init_fixtures();
    int tests = 0, passed = 0;
#define DIR "tests/out"

    /* t01 — happy path: everything gated-and-read, rung3 validated,
       bridge runs, summary flags clean. */
    {
        std::printf("[t01] happy path: 5/5 candidates read, rung3 Validated, "
                    "batch2 runs\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t01.jsonl", 0);
        CHECK(!f.empty());
        std::string ms = find_rec(f, "mapscan");
        CHECK(!ms.empty());
        CHECK(jint(ms, "map_trusted") == 1);
        CHECK((uint64_t)jhex(ms, "derived_base") == 0x400000ull);
        CHECK((uint64_t)jhex(ms, "exec_span_lo") == 0x400000ull);
        CHECK((uint64_t)(jhex(ms, "exec_span_hi") - jhex(ms, "exec_span_lo")) ==
              0x747c000ull);
        CHECK(jint(ms, "base_eq_expected") == 1);
        CHECK(jint(ms, "exec_extent_eq_expected") == 1);
        CHECK(jint(ms, "stop_reason") == 0);  /* full bounded walk, no stop */
        CHECK(count_rec(f, "mapspans") > 0);

        const char *ids[5] = {"B", "C", "D", "E", "A"};
        const char *exp_hex[5] = {EXP_B, EXP_C, EXP_D, EXP_E, EXP_A};
        for (int i = 0; i < 5; ++i) {
            std::string r = find_rec(f, "c2cand", ids[i]);
            CHECK(!r.empty());
            CHECK(jint(r, "rc") == 0);
            CHECK(jstr(r, "refusal").empty());
            CHECK(jstr(r, "live_hex") == exp_hex[i]);   /* RAW plumbing */
            CHECK((jint(r, "span_prot") & 1) == 1);
        }
        /* B before C before D before E before A (A last is load-bearing). */
        size_t p = 0, last = 0; bool order_ok = true;
        for (int i = 0; i < 5; ++i) {
            size_t q = f.find("\"cand\":\"" + std::string(ids[i]) + "\"");
            if (q == std::string::npos || q < last) { order_ok = false; break; }
            last = q; p = q;
        }
        CHECK(order_ok);

        std::string r3 = find_rec(f, "rung3");
        CHECK(jint(r3, "cls") == 0);
        CHECK(jint(r3, "rc") == 0);
        CHECK(jhex(r3, "objects_ptr") == (int64_t)0x13800220a0ull);
        CHECK(jint(r3, "num_elements") == 71959);
        CHECK(jint(r3, "num_chunks") == 2);
        CHECK(jint(r3, "c0_st_ptr") == 0 && jint(r3, "c1_st_ptr") == 0);
        CHECK(jhex(r3, "c0_item_object") == (int64_t)0x1383000100ull);
        CHECK(jhex(r3, "c1_item_object") == (int64_t)0x1383000100ull);
        CHECK(jint(r3, "obj_source_chunk") == 0);
        CHECK(jhex(r3, "obj_addr") == (int64_t)0x1383000100ull);
        CHECK(jhex(r3, "vptr") == (int64_t)0x1383100080ull);
        CHECK(jint(r3, "obj_flags") == 0x10);
        CHECK(jint(r3, "internal_index") == 0);
        CHECK(jhex(r3, "class_ptr") == (int64_t)0x1383000800ull);
        CHECK(jstr(r3, "name_raw") == "0100000000000000");
        CHECK(jhex(r3, "outer_ptr") == 0);
        CHECK(jstr(r3, "chk_vptr") == "pass");
        CHECK(jstr(r3, "chk_flags") == "pass");
        CHECK(jstr(r3, "chk_index_bounds") == "pass");
        CHECK(jstr(r3, "chk_roundtrip") == "pass");
        CHECK(jstr(r3, "chk_class") == "pass");
        CHECK(jstr(r3, "chk_outer") == "pass");
        CHECK(jstr(r3, "info_max_eq_chunks") == "pass");

        std::string b2 = find_rec(f, "batch2");
        CHECK(jint(b2, "ran") == 1);
        CHECK(jint(b2, "gate") == 0);   /* BRIDGE_GATE_OK */

        std::string sm = find_rec(f, "summary");
        CHECK(jint(sm, "exec_reads_attempted") == 0);
        CHECK(jint(sm, "game_memory_writes") == 0);
        CHECK(jint(sm, "write_failures") == 0);
        CHECK(jint(sm, "rung3_cls") == 0);
        CHECK(jint(sm, "batch2_ran") == 1);
        CHECK(sm.find("\"gated\":1") != std::string::npos);
        if (g_fail == before) ++passed;
    }

    /* t02 — candidate A covered but W-only: refusal, others fine. */
    {
        std::printf("[t02] A covered_not_readable (W-only span)\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t02.jsonl", 1);
        std::string a = find_rec(f, "c2cand", "A");
        CHECK(jstr(a, "refusal") == "covered_not_readable");
        CHECK(find_rec(f, "c2cand", "B").find("live_hex") !=
              std::string::npos);
        std::string sm = find_rec(f, "summary");
        CHECK(sm.find("\"outcome\":5") != std::string::npos);
        if (g_fail == before) ++passed;
    }

    /* t03 — candidate E inside a derived exec span: refusal BEFORE any
       readability decision (igf6 crash class, never deref). */
    {
        std::printf("[t03] E exec_span_intersection refusal\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t03.jsonl", 2);
        std::string e = find_rec(f, "c2cand", "E");
        CHECK(jstr(e, "refusal") == "exec_span_intersection");
        CHECK(e.find("live_hex") == std::string::npos);
        if (g_fail == before) ++passed;
    }

    /* t04 — non-canonical Objects**: rung3 Refuted, bridge gate
       STEP_FAILED, C2 reads unaffected (independence). */
    {
        std::printf("[t04] non-canonical Objects** -> rung3 Refuted, "
                    "bridge gated\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t04.jsonl", 3);
        std::string r3 = find_rec(f, "rung3");
        CHECK(jint(r3, "st_a") == -6);  /* IGU_R_NONCANONICAL */
        CHECK(jint(r3, "cls") == 2);
        std::string b2 = find_rec(f, "batch2");
        CHECK(jint(b2, "ran") == 0);
        CHECK(jint(b2, "gate") == 3);   /* BRIDGE_GATE_STEP_FAILED */
        CHECK(find_rec(f, "c2cand", "B").find("live_hex") !=
              std::string::npos);
        if (g_fail == before) ++passed;
    }

    /* t05 — untrusted walk: every read refused (needs-map), nothing read. */
    {
        std::printf("[t05] untrusted map -> global refusal, RC3_ST_NO_MAP\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t05.jsonl", 4);
        std::string ms = find_rec(f, "mapscan");
        CHECK(jint(ms, "map_trusted") == 0);
        CHECK(jint(ms, "insane") == 0); /* stop-never-guess: bad record not counted */
        CHECK(jint(ms, "stop_reason") == 1);
        for (const char *id : {"B", "C", "D", "E", "A"}) {
            CHECK(jstr(find_rec(f, "c2cand", id), "refusal") ==
                  "map_untrusted");
        }
        std::string r3 = find_rec(f, "rung3");
        CHECK(jint(r3, "rc") == 2);     /* RC3_ST_NO_MAP */
        CHECK(jint(r3, "st_hdr") == 2);
        std::string b2 = find_rec(f, "batch2");
        CHECK(jint(b2, "gate") == 1);   /* empty table -> BRIDGE_GATE_ARG (no spans to publish) */
        CHECK(jint(find_rec(f, "summary"), "spans_emitted") == 0);
        if (g_fail == before) ++passed;
    }

    /* t06 — candidate C uncovered by the map: refusal, others read. */
    {
        std::printf("[t06] C uncovered -> refusal, others read\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t06.jsonl", 5);
        CHECK(jstr(find_rec(f, "c2cand", "C"), "refusal") == "uncovered");
        CHECK(find_rec(f, "c2cand", "D").find("live_hex") !=
              std::string::npos);
        if (g_fail == before) ++passed;
    }

    /* t07 — two-run determinism of the whole combined record stream. */
    {
        std::printf("[t07] two-run determinism: byte-identical streams\n");
        const int before = g_fail;
        ++tests;
        std::string r1 = run_to_file(DIR "-run1.jsonl", 0);
        std::string r2 = run_to_file(DIR "-run2.jsonl", 0);
        CHECK(r1.size() == r2.size());
        CHECK(r1 == r2);
        CHECK(!r1.empty());
        if (g_fail == before) ++passed;
    }

    /* t08 — every non-truncated line is well-formed JSONL (shape). */
    {
        std::printf("[t08] record shape: marker prefix, }} tail, seq monotonic\n");
        const int before = g_fail;
        ++tests;
        std::string f = run_to_file(DIR "-t08.jsonl", 0);
        int last_seq = 2; bool shape_ok = true;
        for (const auto &l : split_lines(f)) {
            if (l.empty()) continue;
            if (l.rfind("{\"marker\":\"modbridge.ingamefacts\"", 0) != 0)
                shape_ok = false;
            if (l.size() < 3 || l[l.size() - 2] != '}' ||
                l[l.size() - 1] != '\n') shape_ok = false;
            const int sq = (int)jint(l, "seq");
            if (sq <= last_seq) shape_ok = false;
            last_seq = sq;
        }
        CHECK(shape_ok);
        if (g_fail == before) ++passed;
    }

    std::printf("\n%d/%d tests, %d asserts, %d failures\n",
                passed, tests, g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}

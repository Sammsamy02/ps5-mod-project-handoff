// firstlaunch-table-test.cpp — host-side logic contract test for the
// first-launch table loader + tier law (agent GT, Claude round-5,
// 2026-10-08). Battery-safe: pure parsing/logic, NO engine, NO console;
// the image-scan walk (admission spans, census arms) stays target-only.
//
// Build+run (NOT run this session — battery law: syntax check only):
//   c++ -std=c++23 -fsyntax-only host-tests/firstlaunch-table-test.cpp
//   c++ -std=c++23 -o /tmp/fl-table-test host-tests/firstlaunch-table-test.cpp \
//       && /tmp/fl-table-test
//
// Fixtures: the REAL shipped schemas, read from this repo:
//   ../tables/ue5.6-prospero-shipping-nostats.layout-table.example.json
//       (GI packed per-cell form)
//   ../blade-prep/per-version-table.json
//       (GJ authoring form; the measured 5.6 row carries the version-level
//       relative anchors the generator drives TODAY)
// plus a synthetic in-memory image fixture (planted utf-16le markers) for
// the scan primitive.

#include "../../2026-10-08-l26-runtime/typescan-r4/candidate/include/FirstLaunchProfile.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace rc::firstlaunch;

namespace
{
    int g_failures = 0;

    auto check(bool ok, const std::string& what) -> void
    {
        if (ok)
        {
            std::cout << "PASS " << what << "\n";
        }
        else
        {
            ++g_failures;
            std::cout << "FAIL " << what << "\n";
        }
    }

    auto read_text(const std::filesystem::path& p, std::string& out) -> bool
    {
        std::ifstream f(p, std::ios::binary);
        if (!f)
        {
            return false;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return !out.empty();
    }

    auto test_dir() -> std::filesystem::path
    {
        return std::filesystem::path(__FILE__).parent_path();
    }

    // ---------------- GA root-derivation law ----------------
    void test_derive_root()
    {
        check(derive_modbridge_root("/data/modbridge/roots/EXAMPLE/ue4ss/Mods") ==
                  "/data/modbridge/roots/EXAMPLE",
              "derive_root strips /ue4ss/Mods");
        check(derive_modbridge_root("/data/modbridge/roots/EXAMPLE/ue4ss") == "/data/modbridge/roots/EXAMPLE",
              "derive_root strips /ue4ss");
        check(derive_modbridge_root("/data/modbridge/roots/EXAMPLE/ue4ss/Mods/") ==
                  "/data/modbridge/roots/EXAMPLE",
              "derive_root tolerates trailing slash");
        check(derive_modbridge_root("C:\\mg\\ue4ss\\Mods").find("Mods") == std::string::npos,
              "derive_root normalizes backslashes and strips Mods");
        check(derive_modbridge_root("").empty(), "derive_root empty input -> empty (no title-id default)");
        // Rule 10: no title-id may ever appear in a derived root's LAW —
        // the function itself contains none (asserted by review, not here).
    }

    // ---------------- tier ladder (engine_match mirror) ----------------
    void test_classify_detect()
    {
        const Thresholds t{}; // engine_match.py constants

        // Astro Bot negative control: 0/16 triage -> cheap REFUSED, full
        // scan skipped (present == triage).
        DetectCounts astro{0, 16, 0};
        check(classify_detect(astro, t) == Verdict::Refused, "tier: 0/16 triage -> REFUSED (Astro path)");

        // triage passes but below the not-Unreal floor
        DetectCounts weak{4, 16, 5};
        check(classify_detect(weak, t) == Verdict::Refused, "tier: present 5 < 8 -> REFUSED");

        // family resolved, version not (below family-confident floor)
        DetectCounts fam{8, 16, 20};
        check(classify_detect(fam, t) == Verdict::FamilyOnly, "tier: present 20 (<25) -> FAMILY-ONLY");

        // tentative: margin = present - refused_max in [4,12)
        DetectCounts tent{8, 16, 12};
        check(classify_detect(tent, t) == Verdict::Tentative, "tier: present 12 (margin 4) -> TENTATIVE");

        // Townfall positive control: 54 markers -> margin 46 -> CONFIDENT
        DetectCounts tf{14, 16, 54};
        check(classify_detect(tf, t) == Verdict::Confident, "tier: present 54 (margin 46) -> CONFIDENT");

        // table-overridden thresholds degrade, never weaken, the ladder
        Thresholds cell_t{}; // the shipped 5.6 cell detection block
        cell_t.expected_present = 54;
        cell_t.margin_confident = 12;
        cell_t.margin_tentative = 4;
        cell_t.refused_max = 8;
        check(classify_detect(tf, cell_t) == Verdict::Confident, "tier: cell thresholds agree on 54");
    }

    // ---------------- utf-16le scan primitive ----------------
    void test_scan()
    {
        // ascii needle
        const auto needle = utf16le_needle("/Script/Engine");
        check(needle.size() == 14 * 2, "utf16le_needle: ascii size");
        check(needle[0] == '/' && needle[1] == 0 && needle[2] == 'S' && needle[3] == 0,
              "utf16le_needle: ascii byte order");

        // non-ascii (U+1D11E musical symbol -> surrogate pair)
        const auto wide = utf16le_needle("\xF0\x9D\x84\x9E");
        const uint8_t expected[] = {0x34, 0xD8, 0x1E, 0xDD}; // D834 DD1E LE
        check(wide.size() == 4 && std::memcmp(wide.data(), expected, 4) == 0, "utf16le_needle: surrogate pair");

        // synthetic image fixture: three markers planted, two absent
        std::vector<std::string> markers = {
            "/Script/Engine", "LogUObject", "FMallocBinned3", "NotPlantedA", "NotPlantedB",
        };
        std::vector<std::vector<uint8_t>> needles;
        for (const auto& m : markers)
        {
            needles.push_back(utf16le_needle(m));
        }
        std::vector<uint8_t> image;
        auto plant = [&](const std::string& m) {
            const auto n = utf16le_needle(m);
            image.insert(image.end(), n.begin(), n.end());
            image.push_back(0x00);
            image.push_back(0xFF); // separator noise
        };
        image.insert(image.end(), 4096, 0xCC); // leading noise
        plant("/Script/Engine");
        image.insert(image.end(), 123, 0xEE);
        plant("LogUObject");
        plant("FMallocBinned3");
        image.insert(image.end(), 8192, 0xAA);
        std::vector<bool> present;
        scan_present(image.data(), image.size(), needles, present);
        check(present.size() == 5 && present[0] && present[1] && present[2] && !present[3] && !present[4],
              "scan_present: planted found, absent missed");

        // presence accounting -> detect counts
        DetectCounts counts{};
        counts.triage_total = 5;
        counts.triage_present = 3;
        counts.present_count = 3;
        check(classify_detect(counts, Thresholds{}) == Verdict::Refused, "fixture counts: 3 < 8 -> REFUSED");
    }

    // ---------------- token/pattern parsers ----------------
    void test_parsers()
    {
        check(parse_u64_token("0x9365EC0") == 0x9365EC0, "parse_u64_token: hex");
        check(parse_u64_token("0x0 (8B)") == 0, "parse_u64_token: prose tail cut");
        check(parse_u64_token("8192") == 8192, "parse_u64_token: decimal");
        check(parse_u64_token("0x20000") == 0x20000, "parse_u64_token: block capacity");

        PatternBytes pb;
        check(parse_pattern("48 8D 05 ?? ?? ?? ?? 48 89 05", pb) && pb.size() == 11 && pb.wild[3] && !pb.wild[0],
              "parse_pattern: wildcards");
        check(pack_version("5.6.1-0+++UE5") == 5006 && pack_version("ue5.6") == 5006 && pack_version("4.27") == 4027,
              "pack_version: forms");
    }

    // ---------------- GI packed per-cell form ----------------
    void test_cell_table(const LayoutTable& cell)
    {
        check(cell.form == "cell", "cell: form");
        check(cell.table_id == "ue5.6-prospero-shipping-nostats", "cell: table_id");
        check(cell.family == "UE5" && cell.version == "5.6", "cell: engine axes");
        check(cell.thr.expected_present == 54, "cell: expected_present_count 54");
        check(cell.thr.margin_confident == 12 && cell.thr.margin_tentative == 4 && cell.thr.refused_max == 8,
              "cell: margins 12/4/8");
        check(cell.max_window_bytes == 67108864 && cell.max_candidates_per_anchor == 8, "cell: envelope");
        check(cell.markers_placeholder, "cell: placeholder marker list detected");
        check(cell.triage_markers.empty(), "cell: placeholder contributes no markers");

        check(cell.rows.size() == 1 && cell.rows[0].status == "measured", "cell: one measured row");
        bool saw_gua = false, saw_gmalloc = false, saw_ctor = false, saw_sco = false;
        unsigned labelless = 0;
        for (const auto& a : cell.rows[0].anchors)
        {
            if (a.id == "GUObjectArray")
            {
                saw_gua = true;
                check(a.min_arms == 2 && a.verify_kind == "pointer_probe", "cell: GUObjectArray agreement+verify");
            }
            if (a.id == "GMalloc") saw_gmalloc = true;
            if (a.id == "FNameCtorWchar")
            {
                saw_ctor = true;
                check(a.verify_kind == "behavioral_fnamector_byteproperty", "cell: ctor verify kind");
            }
            if (a.id == "StaticConstructObjectInternal") saw_sco = true;
            if (!a.has_label) ++labelless;
        }
        check(saw_gua && saw_gmalloc && saw_ctor && saw_sco, "cell: all four anchor ids present");
        // THE NOT-YET STUB (logic level): packed cells ship NO labels, so
        // every anchor refuses until the census-pool generator lands; the
        // runtime fails closed on the first required anchor.
        check(labelless == cell.rows[0].anchors.size(), "cell: no labels -> census-pool NOT-YET refusal path");
        bool any_probe = false;
        for (const auto& a : cell.rows[0].anchors)
        {
            any_probe = any_probe || !a.probe_patterns.empty();
        }
        check(any_probe, "cell: advisory probe patterns parsed");

        // structures: member rows + FMalloc vtable order
        const auto& row = cell.rows[0];
        auto off = [&](const char* s, const char* m, uint64_t& o) {
            for (const auto& r : row.members)
            {
                if (r.struct_name == s && r.member == m)
                {
                    o = r.offset;
                    return true;
                }
            }
            return false;
        };
        uint64_t v = 0;
        check(off("FUObjectArray", "ObjObjects", v) && v == 16, "cell: FUObjectArray::ObjObjects@16");
        check(off("FUObjectArray", "UObjectCreateListeners", v) && v == 88, "cell: listeners@88 (l25 correction)");
        check(off("TUObjectArray", "NumChunks", v) && v == 28, "cell: TUObjectArray::NumChunks@28");
        bool malloc48 = false;
        for (const auto& s : row.vtable_slots)
        {
            if (s.method == "Malloc" && s.byte_offset == 48) malloc48 = true;
        }
        check(malloc48, "cell: FMalloc::Malloc@48");
    }

    // ---------------- GJ authoring form ----------------
    void test_authoring_table(const LayoutTable& gj)
    {
        check(gj.form == "authoring", "gj: form");
        check(gj.thr.markers_total == 244, "gj: markers_total 244");
        check(gj.rows.size() == 7, "gj: seven version bands");

        const VersionRow* measured = nullptr;
        for (const auto& r : gj.rows)
        {
            if (r.version == "5.6")
            {
                measured = &r;
            }
        }
        check(measured != nullptr && measured->status == "measured" && measured->confidence == 1.0,
              "gj: 5.6 row measured/1.0");

        // row selection law: measured rows only (pack_rule)
        const VersionRow* selected = select_row(gj, pack_version("5.6"));
        check(selected == measured, "gj: select_row(5.6) -> measured row");
        check(select_row(gj, pack_version("5.4")) == nullptr, "gj: select_row(5.4) -> seed band refuses");
        check(select_row(gj, pack_version("4.27")) == nullptr, "gj: select_row(4.27) -> seed band refuses");
        check(select_row(gj, pack_version("5.8")) == nullptr, "gj: select_row(5.8) -> forward band refuses");

        // the version-level relative anchors the generator drives TODAY
        const AnchorRow* pool = nullptr;
        const AnchorRow* gua = nullptr;
        const AnchorRow* ctor = nullptr;
        const AnchorRow* sco = nullptr;
        const AnchorRow* gmalloc = nullptr;
        const AnchorRow* instance = nullptr;
        const AnchorRow* vtable = nullptr;
        const AnchorRow* tostring = nullptr;
        const AnchorRow* addl = nullptr;
        for (const auto& a : measured->anchors)
        {
            if (a.id == "FNamePool") pool = &a;
            else if (a.id == "GUObjectArray") gua = &a;
            else if (a.id == "FNameCtorWchar") ctor = &a;
            else if (a.id == "StaticConstructObjectInternal") sco = &a;
            else if (a.id == "GMalloc") gmalloc = &a;
            else if (a.id == "GMallocInstance") instance = &a;
            else if (a.id == "GMallocVtable") vtable = &a;
            else if (a.id == "FNameToStringInternal") tostring = &a;
            else if (a.id == "EngineAddDeleteListener") addl = &a;
        }
        check(pool && pool->has_label && pool->image_vaddr == 0x9365EC0, "gj: FNamePool label 0x9365EC0");
        check(gua && gua->has_label && gua->image_vaddr == 0x93BA110, "gj: GUObjectArray label 0x93BA110");
        check(ctor && ctor->has_label && ctor->image_vaddr == 0x23D590 && ctor->has_prefix && ctor->prefix.size() == 32,
              "gj: ctor label 0x23D590 + exact 32-byte prefix");
        check(ctor && ctor->prefix.bytes[0] == 0x55 && ctor->prefix.bytes[1] == 0x48 && ctor->prefix.bytes[2] == 0x89,
              "gj: ctor prefix starts 55 48 89");
        check(sco && sco->has_label && sco->image_vaddr == 0x490AA0 && sco->has_prefix && sco->prefix.size() == 25,
              "gj: SCO label 0x490AA0 + exact 25-byte prefix");
        check(gmalloc && gmalloc->has_label && gmalloc->image_vaddr == 0x930ADD8, "gj: GMalloc 0x930ADD8");
        check(instance && instance->image_vaddr == 0x93089B8, "gj: GMallocInstance 0x93089B8");
        check(vtable && vtable->image_vaddr == 0x7EC26E0, "gj: GMallocVtable 0x7EC26E0");
        check(tostring && tostring->image_vaddr == 0x23FA00 && tostring->has_prefix, "gj: ToString 0x23FA00");
        check(addl && addl->image_vaddr == 0x4841A0 && addl->has_prefix && addl->prefix.size() == 16,
              "gj: add-listener 0x4841A0 + 16B prefix");

        // pool shape (drives the FNamePool bounds arm)
        check(measured->pool.max_blocks == 8192 && measured->pool.block_capacity_bytes == 0x20000,
              "gj: pool decode bounds 8192/0x20000");
        check(measured->pool.current_block_off == 8 && measured->pool.current_byte_off == 12 &&
                  measured->pool.blocks_off == 16,
              "gj: pool header offsets 8/12/16");

        // members (two-source measured rows)
        uint64_t o = 0;
        bool found = false;
        for (const auto& m : measured->members)
        {
            if (m.struct_name == "FUObjectArray" && m.member == "ObjObjects")
            {
                o = m.offset;
                found = true;
            }
        }
        check(found && o == 16, "gj: FUObjectArray::ObjObjects@16");
        found = false;
        for (const auto& m : measured->members)
        {
            if (m.struct_name == "FUObjectArray" && m.member == "UObjectDeleteListeners")
            {
                o = m.offset;
                found = true;
            }
        }
        check(found && o == 104, "gj: FUObjectArray::UObjectDeleteListeners@104");
        found = false;
        for (const auto& m : measured->members)
        {
            if (m.struct_name == "FUObjectItem" && m.member == "total_size")
            {
                o = m.offset;
                found = true;
            }
        }
        check(found && o == 24, "gj: FUObjectItem total 24");

        // vtable order
        bool try_realloc = false;
        for (const auto& s : measured->vtable_slots)
        {
            if (s.method == "TryRealloc" && s.byte_offset == 72) try_realloc = true;
        }
        check(try_realloc, "gj: FMalloc::TryRealloc@72");

        // the required set is fully labeled in the measured row: candidate
        // arithmetic (base + label) reproduces the SeedTable receipts
        // (runtime 0x97ba110 = 0x93ba110 + base 0x400000; ctor 0x63d590).
        const uint64_t base = 0x400000;
        check(gua && base + gua->image_vaddr == 0x97BA110, "candidate: GUObjectArray base+label == 0x97BA110");
        check(ctor && base + ctor->image_vaddr == 0x63D590, "candidate: ctor base+label == 0x63D590");
        check(gmalloc && base + gmalloc->image_vaddr == 0x970ADD8, "candidate: GMalloc base+label == 0x970ADD8");
        for (size_t k = 0; k < kRequiredAnchorCount; ++k)
        {
            const char* id = kRequiredAnchors[k];
            bool labeled = false;
            for (const auto& a : measured->anchors)
            {
                if (a.id == id && a.has_label)
                {
                    labeled = true;
                }
            }
            check(labeled, std::string("required anchor labeled in measured row: ") + id);
        }
    }

    // ---------------- malformed input fail-closed ----------------
    void test_malformed()
    {
        LayoutTable t;
        std::string err;
        check(!parse_layout_table("{\"no\":\"schema\"}", t, err), "malformed: missing schema refuses");
        check(!parse_layout_table("{broken", t, err), "malformed: syntax error refuses");
        const std::string ph_json =
            "{\"schema\":\"ue-layout-table/1\",\"table_id\":\"x\",\"detection\":{\"triage_markers\":[\"<placeholder>\"]},"
            "\"anchors\":[]}";
        check(parse_layout_table(ph_json, t, err), "placeholder-only cell parses (flagged, not fatal)");
        check(t.markers_placeholder && t.triage_markers.empty(), "placeholder-only cell contributes no markers");
    }
} // namespace

auto main() -> int
{
    test_derive_root();
    test_classify_detect();
    test_scan();
    test_parsers();

    std::string cell_text;
    const auto cell_path = test_dir() / ".." / "tables" /
                            "ue5.6-prospero-shipping-nostats.layout-table.example.json";
    if (!read_text(cell_path, cell_text))
    {
        std::cout << "FAIL cannot read GI example cell at " << cell_path.string() << "\n";
        ++g_failures;
    }
    else
    {
        LayoutTable cell;
        std::string err;
        if (!parse_layout_table(cell_text, cell, err))
        {
            std::cout << "FAIL GI example cell parse: " << err << "\n";
            ++g_failures;
        }
        else
        {
            test_cell_table(cell);
        }
    }

    std::string gj_text;
    const auto gj_path = test_dir() / ".." / "blade-prep" / "per-version-table.json";
    if (!read_text(gj_path, gj_text))
    {
        std::cout << "FAIL cannot read GJ authoring table at " << gj_path.string() << "\n";
        ++g_failures;
    }
    else
    {
        LayoutTable gj;
        std::string err;
        if (!parse_layout_table(gj_text, gj, err))
        {
            std::cout << "FAIL GJ authoring table parse: " << err << "\n";
            ++g_failures;
        }
        else
        {
            test_authoring_table(gj);
        }
    }

    test_malformed();

    std::cout << (g_failures == 0 ? "ALL PASS" : "FAILURES: " + std::to_string(g_failures)) << "\n";
    return g_failures == 0 ? 0 : 1;
}

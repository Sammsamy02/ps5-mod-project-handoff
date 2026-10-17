#include "test.hpp"
#include "modbridge/core.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <span>
#include <string_view>
#include <vector>

using namespace modbridge;
namespace {
using Bytes=std::vector<std::uint8_t>;
void put(Bytes& b,std::size_t p,std::uint64_t value,unsigned n) {
    for(unsigned i=0;i<n;++i)b.at(p+i)=static_cast<std::uint8_t>(value>>(8*i));
}
void subrecord(Bytes& b,std::string_view tag,const Bytes& data) {
    CHECK_EQ(tag.size(),4u);CHECK(data.size()<=65535);
    b.insert(b.end(),tag.begin(),tag.end());
    b.push_back(static_cast<std::uint8_t>(data.size()));
    b.push_back(static_cast<std::uint8_t>(data.size()>>8));
    b.insert(b.end(),data.begin(),data.end());
}
Bytes hedr() {
    Bytes data(12);put(data,0,0x3f99999a,4);put(data,4,17,4);put(data,8,0x12345678,4);
    return data;
}
Bytes body() {Bytes b;subrecord(b,"HEDR",hedr());return b;}
void master(Bytes& b,const Bytes& name,std::uint64_t data=0) {
    subrecord(b,"MAST",name);Bytes value(8);put(value,0,data,8);subrecord(b,"DATA",value);
}
Bytes root(const Bytes& data,unsigned header=24,std::uint32_t flags=0) {
    CHECK(header==20||header==24);
    Bytes b(header);std::copy_n("TES4",4,b.begin());put(b,4,data.size(),4);
    put(b,8,flags,4);put(b,12,0x01020304,4);put(b,16,0xa0b0c0d0,4);
    if(header==24){put(b,20,43,2);put(b,22,0x9876,2);}
    b.insert(b.end(),data.begin(),data.end());return b;
}
const Json& header(const Json& j) {return j.at("facts").at("tes4_header");}
void complete(const Json& j) {
    CHECK_EQ(j.at("kind"),"bethesda_plugin");
    CHECK_EQ(j.at("facts").at("inspection_complete"),true);
    CHECK_EQ(j.at("facts").at("semantic_inventory_complete"),false);
    CHECK_EQ(j.at("facts").at("classification_confidence"),"structural_header");
    CHECK_EQ(j.at("facts").at("header_only"),true);
    CHECK(j.at("issues").empty());
    std::set<std::string> ids;
    for(const auto& r:j.at("requirements")){CHECK_EQ(r.at("status"),"unresolved");ids.insert(r.at("id"));}
    CHECK(ids.contains("translation.bethesda_plugin"));
    CHECK(ids.contains("dependencies.bethesda_masters"));
    CHECK(ids.contains("format.bethesda_plugin_records"));
    CHECK(!j.at("facts").contains("script_free"));
    CHECK(!j.at("facts").contains("installation_supported"));
}
void failed(const Json& j,const char* code=nullptr) {
    CHECK_EQ(j.at("facts").at("inspection_complete"),false);
    CHECK_EQ(j.at("facts").at("semantic_inventory_complete"),false);
    CHECK(!j.at("facts").contains("tes4_header"));
    CHECK(!j.at("facts").contains("masters"));
    CHECK(!j.at("issues").empty());
    if(code)CHECK_EQ(j.at("issues")[0].at("code"),code);
    (void)canonical_json(j);
}
}

// An extension branch must not hide a valid TES4 header or infer its layout.
TEST(bethesda_magic_precedes_extensions_for_both_verified_layouts) {
    for(unsigned size:{20u,24u})for(const char* name:{"renamed.lua","renamed.pak","data.bin","no_extension"}) {
        auto j=inspect_binary(root(body(),size),name);complete(j);
        CHECK_EQ(header(j).at("record_header_bytes"),size);
        CHECK_EQ(header(j).at("declared_body_bytes"),18);
        CHECK_EQ(header(j).at("hedr").at("version_bits"),0x3f99999a);
        CHECK_EQ(header(j).at("hedr").at("declared_record_count"),17);
        CHECK_EQ(header(j).at("hedr").at("next_object_id"),0x12345678);
        CHECK_EQ(header(j).at("form_id_bits"),0x01020304);
        CHECK_EQ(header(j).at("revision_bits"),0xa0b0c0d0);
        CHECK(header(j).at("masters").empty());
        CHECK_EQ(header(j).contains("form_version_bits"),size==24);
        if(size==24){CHECK_EQ(header(j).at("form_version_bits"),43);CHECK_EQ(header(j).at("reserved_bits"),0x9876);}
    }
}

// Root metadata does not validate later records, even if their bytes are captured.
TEST(bethesda_header_complete_does_not_claim_full_plugin_validity) {
    auto b=root(body());auto j=inspect_binary(b,"input",4096);complete(j);
    CHECK_EQ(header(j).at("root_end"),42);
    b.insert(b.end(),{0xff,0xfe,0xfd});j=inspect_binary(b,"input");complete(j);
    CHECK_EQ(header(j).at("root_end"),42);
}

// Dependencies must retain order and own their bytes without guessing encodings.
TEST(bethesda_master_pairs_preserve_owned_raw_names_and_opaque_data) {
    auto data=body();master(data,{'b','a','s','e','.','e','s','m',0},0x8877665544332211ULL);
    master(data,{0xff,0x80,0});auto b=root(data,20);
    auto j=inspect_binary(b,"input");complete(j);const auto saved=canonical_json(j);
    const auto& m=header(j).at("masters");CHECK_EQ(m.size(),2u);
    CHECK_EQ(m[0].at("name_ascii"),"base.esm");CHECK_EQ(m[0].at("name_bytes_hex"),"626173652e65736d");
    CHECK_EQ(m[0].at("opaque_data_bits"),0x8877665544332211ULL);
    CHECK_EQ(m[1].at("name_bytes_hex"),"ff80");CHECK(!m[1].contains("name_ascii"));
    std::fill(b.begin(),b.end(),0);b.clear();CHECK_EQ(canonical_json(j),saved);
}

// Forward strings may have opaque bytes after the terminator; none are filenames.
TEST(bethesda_master_terminator_is_bounded_to_its_subrecord) {
    auto data=body();master(data,{'a',0,0xff,0xfe});
    auto j=inspect_binary(root(data),"input");complete(j);
    CHECK_EQ(header(j).at("masters")[0].at("name_bytes_hex"),"61");
    data=body();master(data,{'a','b'});failed(inspect_binary(root(data),"input"));
    data=body();master(data,{0});failed(inspect_binary(root(data),"input"));
}

// The root body must begin with exactly one required twelve-byte HEDR.
TEST(bethesda_missing_nonfirst_wrong_size_and_duplicate_hedr_fail_closed) {
    Bytes data;subrecord(data,"CNAM",{'x',0});failed(inspect_binary(root(data),"input"));
    subrecord(data,"HEDR",hedr());failed(inspect_binary(root(data),"input"));
    data.clear();subrecord(data,"HEDR",Bytes(11));failed(inspect_binary(root(data),"input"));
    data=body();master(data,{'a',0});subrecord(data,"HEDR",hedr());
    failed(inspect_binary(root(data),"input"));
}

// Missing, delayed or incorrectly sized DATA cannot yield a dependency result.
TEST(bethesda_master_data_pairing_rejects_late_invalid_structure) {
    auto data=body();master(data,{'a',0});subrecord(data,"MAST",{'b',0});
    failed(inspect_binary(root(data),"input"));
    subrecord(data,"CNAM",{'x',0});subrecord(data,"DATA",Bytes(8));
    failed(inspect_binary(root(data),"input"));
    data=body();subrecord(data,"DATA",Bytes(8));failed(inspect_binary(root(data),"input"));
    data=body();subrecord(data,"MAST",{'a',0});subrecord(data,"DATA",Bytes(7));
    failed(inspect_binary(root(data),"input"));
    data=body();subrecord(data,"MAST",{'a',0});subrecord(data,"MAST",{'b',0});subrecord(data,"DATA",Bytes(8));
    failed(inspect_binary(root(data),"input"));
}

// Root-span and subrecord ranges must hold at late failures, with no partial facts.
TEST(bethesda_late_truncation_and_subrecord_overrun_publish_no_header) {
    auto data=body();master(data,{'a',0});subrecord(data,"CNAM",{'x','y','z',0});
    auto b=root(data);
    failed(inspect_binary(std::span(b).first(b.size()-1),"input",b.size()));
    failed(inspect_binary(std::span(b).first(25),"input",b.size()));
    failed(inspect_binary(std::span(b).first(4),"input",b.size()));
    b.back()=0;put(b,b.size()-10+4,10,2);failed(inspect_binary(b,"input"));
    data=body();data.push_back('X');failed(inspect_binary(root(data),"input"));
}

// Logical total size is independent of capture length and must bound the root.
TEST(bethesda_total_size_contradictions_do_not_accept_prefix_metadata) {
    auto b=root(body());
    failed(inspect_binary(b,"input",b.size()-1),"input_size_contradiction");
    auto prefix=std::span(b).first(b.size()-1);
    failed(inspect_binary(prefix,"input",b.size()-1));
    failed(inspect_binary(prefix,"input",b.size()));
    put(b,4,0xffffffffu,4);failed(inspect_binary(std::span(b).first(30),"input",UINT64_MAX));
}

// The declared body cap is enforced before walking or allocating from its size.
TEST(bethesda_root_body_cap_is_exact_and_rejects_oversize_declarations) {
    auto data=body();subrecord(data,"CNAM",Bytes(65512,0));
    CHECK_EQ(data.size(),65536u);complete(inspect_binary(root(data),"input"));
    auto b=root(data);put(b,4,65537,4);failed(inspect_binary(b,"input",b.size()+1),"bethesda_header_limit_exceeded");
}

// Finite dependency count and aggregate name bytes cannot be bypassed by pairing.
TEST(bethesda_master_count_and_aggregate_name_limits_are_enforced) {
    auto data=body();for(unsigned i=0;i<256;++i)master(data,{'a',0});
    auto j=inspect_binary(root(data),"input");complete(j);CHECK_EQ(header(j).at("masters").size(),256u);
    master(data,{'a',0});failed(inspect_binary(root(data),"input"),"bethesda_header_limit_exceeded");
    data=body();Bytes name(4096,'a');name.push_back(0);master(data,name);
    complete(inspect_binary(root(data),"input"));master(data,{'b',0});
    failed(inspect_binary(root(data),"input"),"bethesda_header_limit_exceeded");
}

// Small zero-size unknown fields still consume the finite subrecord work limit.
TEST(bethesda_subrecord_count_bounds_work_on_zero_size_fields) {
    auto data=body();for(unsigned i=0;i<1023;++i)subrecord(data,"ZZZZ",{});
    complete(inspect_binary(root(data),"input"));subrecord(data,"ZZZZ",{});
    failed(inspect_binary(root(data),"input"),"bethesda_header_limit_exceeded");
}

// Opaque flags and float bits must not become a platform, localization or script claim.
TEST(bethesda_unknown_flags_and_localized_version_bits_remain_opaque) {
    auto data=body();put(data,6,0x7fc12345,4);subrecord(data,"SNAM",{0xff,0});
    auto j=inspect_binary(root(data,24,0x80000080u),"input");complete(j);
    CHECK_EQ(header(j).at("declared_flags"),0x80000080u);
    CHECK_EQ(header(j).at("hedr").at("version_bits"),0x7fc12345);
    CHECK(!header(j).contains("localized"));CHECK(!header(j).contains("edition"));
    (void)canonical_json(j);
}

// Unsupported extension and compression mechanisms must be explicit failures.
TEST(bethesda_xxxx_and_compressed_roots_are_explicitly_unsupported) {
    auto data=body();subrecord(data,"XXXX",{1,0,0,0});subrecord(data,"CNAM",{});
    failed(inspect_binary(root(data),"input"),"bethesda_header_unsupported");
    failed(inspect_binary(root(body(),24,0x00040000),"input"),"bethesda_header_unsupported");
}

// File suffixes and unrelated signatures never identify a Bethesda plugin.
TEST(bethesda_fake_plugin_extensions_stay_opaque_and_unrelated_formats_preserved) {
    for(const char* path:{"fake.esp","fake.esm","fake.esl"}) {
        auto j=inspect_binary(Bytes{'N','O','P','E'},path);CHECK_EQ(j.at("kind"),"data");
        CHECK_EQ(j.at("facts").at("classification_confidence"),"opaque");CHECK(!j.at("facts").contains("tes4_header"));
    }
    CHECK_EQ(inspect_binary(Bytes{'x'},"script.lua").at("kind"),"lua");
    CHECK_EQ(inspect_binary(Bytes{'x'},"archive.pak").at("kind"),"unreal_pak");
    CHECK_EQ(inspect_binary(Bytes{0xc1,0x83,0x2a,0x9e},"renamed.esp").at("kind"),"unreal_asset");
}

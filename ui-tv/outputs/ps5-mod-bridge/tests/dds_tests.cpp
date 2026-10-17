#include "test.hpp"
#include "modbridge/core.hpp"
#include "dds_fixture.hpp"
#include <set>
using namespace modbridge;
using namespace dds_fixture;
// Dispatching by extension, or treating valid DDS as opaque, loses reusable layout.
TEST(dds_magic_dispatch_and_edge_block_pitches) {
    for(const auto& c:std::vector<std::vector<unsigned>>{{1,1,8,8},{4,4,8,8},{5,7,16,32}}) {
        auto j=inspect_binary(legacy(c[0],c[1],"DXT1",c[3]),"renamed.lua");
        CHECK_EQ(j.at("kind"),"dds_texture");
        auto f=j.at("facts");CHECK_EQ(f.at("layout_status"),"complete");
        CHECK_EQ(f.at("subresources")[0].at("row_pitch"),c[2]);
        CHECK_EQ(f.at("subresources")[0].at("size"),c[3]);
        CHECK_EQ(f.at("payload_expected_end"),128+c[3]);
        CHECK_EQ(f.at("subresources_hashed"),1);CHECK(f.at("normal_form_eligible").get<bool>());
        std::set<std::string> requirements;
        for(const auto& r:j.at("requirements")){requirements.insert(r.at("id"));CHECK_EQ(r.at("status"),"unresolved");}
        CHECK(requirements.contains("texture.ps5_writer"));CHECK(requirements.contains("texture.game_acceptance"));
    }
    CHECK_EQ(inspect_binary(std::vector<std::uint8_t>{1,2,3},"fake.dds").at("kind"),"data");
}
// Incorrect array expansion or face/mip interleaving shifts every following slice.
TEST(dds_cube_array_order_and_volume_depth) {
    auto j=inspect_binary(dx10(98,4,4,384,2,3,2,4),"x");auto f=j.at("facts");
    CHECK_EQ(f.at("subresources").size(),24u);CHECK_EQ(f.at("payload_expected_end"),532);
    auto s=f.at("subresources");CHECK_EQ(s[0].at("face"),"+X");CHECK_EQ(s[1].at("mip"),1);
    CHECK_EQ(s[2].at("face"),"-X");CHECK_EQ(s[12].at("cube"),1);CHECK_EQ(s[12].at("offset"),340);
    auto b=dx10(95,4,4,112,3,4);u32(b,8,0x800000);u32(b,24,4);u32(b,112,0x200000);
    f=inspect_binary(b,"v").at("facts");CHECK_EQ(f.at("subresources").size(),3u);
    CHECK_EQ(f.at("subresources")[0].at("size"),64);CHECK_EQ(f.at("subresources")[1].at("depth"),2);
    CHECK_EQ(f.at("subresources")[2].at("offset"),244);CHECK_EQ(f.at("payload_expected_end"),260);
}
// Rejecting too much or silently repairing declarations both misstate capabilities.
TEST(dds_unsupported_and_invalid_declarations) {
    auto check=[](std::vector<std::uint8_t> b,const char* status,const char* reason){
        auto j=inspect_binary(b,"x");CHECK_EQ(j.at("kind"),"dds_texture");
        CHECK_EQ(j.at("facts").at("layout_status"),status);CHECK_EQ(j.at("facts").at("reason"),reason);
        CHECK_EQ(j.at("facts").at("normal_form_eligible"),false);
    };
    check(legacy(4,4,"ZZZZ"),"unsupported_format","unsupported_format");
    check(dx10(28),"unsupported_format","unsupported_format");
    check(dx10(71,4,1,8,1,2),"unsupported_topology","bc_1d");
    auto b=legacy();u32(b,112,0x600);check(b,"unsupported_topology","partial_cube");
    b=legacy();u32(b,28,4);check(b,"invalid","invalid_mip_count");
    b=legacy();u32(b,16,0);check(b,"invalid","zero_extent");
    b=dx10();u32(b,140,0);check(b,"invalid","zero_array_size");
    b=dx10();u32(b,132,4);u32(b,136,4);check(b,"invalid","cube_dimension_conflict");
    b=legacy();u32(b,4,0);check(b,"unavailable","noncanonical_header_size");
    b=legacy();b.resize(100);check(b,"unavailable","header_truncated");
    b=dx10();b.resize(140);check(b,"unavailable","dx10_header_truncated");
    b=legacy();u32(b,28,0);CHECK_EQ(inspect_binary(b,"x").at("facts").at("mip_count"),1);
}
// Dropping type/alpha metadata would allow an unsafe reinterpretation.
TEST(dds_exact_bc_format_identity) {
    const std::vector<std::pair<unsigned,unsigned>> formats={{70,8},{71,8},{72,8},{73,16},{74,16},{75,16},
        {76,16},{77,16},{78,16},{79,8},{80,8},{81,8},{82,16},{83,16},{84,16},{94,16},{95,16},{96,16},{97,16},{98,16},{99,16}};
    for(const auto& [id,size]:formats){auto f=inspect_binary(dx10(id,4,4,size),"x").at("facts");
        CHECK_EQ(f.at("format").at("dxgi_format"),id);CHECK_EQ(f.at("format").at("block_bytes"),size);
        CHECK_EQ(f.at("format").at("color_tag"),(id==72||id==75||id==78||id==99)?"srgb":"no_explicit_srgb_tag");
        CHECK(f.at("normal_form_eligible").get<bool>());}
    for(const auto& [code,size]:std::vector<std::pair<std::string,unsigned>>{{"DXT1",8},{"DXT2",16},{"DXT3",16},{"DXT4",16},{"DXT5",16},{"ATI1",8},{"ATI2",16},{"BC4U",8},{"BC4S",8},{"BC5U",16},{"BC5S",16}}){
        auto f=inspect_binary(legacy(4,4,code,size),"x").at("facts");CHECK_EQ(f.at("format").at("fourcc"),code);CHECK_EQ(f.at("format").at("block_bytes"),size);
        CHECK_EQ(f.at("format").at("alpha_mode"),(code=="DXT2"||code=="DXT4")?"premultiplied":"unknown");}
    auto f=inspect_binary(dx10(71,4,4,8,1,3,1,0,0x80000001),"x").at("facts");
    CHECK_EQ(f.at("format").at("alpha_bits"),0x80000001);CHECK_EQ(f.at("normal_form_eligible"),false);
}
// Work must be rejected before producing an attacker-sized subresource list.
TEST(dds_layout_limits_and_checked_overflow) {
    auto b=dx10(71,4,4,8,1,3,65537);auto f=inspect_binary(b,"x").at("facts");
    CHECK_EQ(f.at("reason"),"subresource_limit");CHECK(f.at("subresources").empty());
    b=dx10(98,0xffffffff,0xffffffff,16,1,4);u32(b,8,0x800000);u32(b,24,0xffffffff);
    f=inspect_binary(b,"x").at("facts");CHECK_EQ(f.at("reason"),"arithmetic_overflow");CHECK(f.at("subresources").empty());
}

#include "modbridge/dds.hpp"
#include <type_traits>
// Prefix size is not true truncation; hashes exist only for complete slices/ranges.
TEST(dds_prefix_truncation_and_opaque_trailer) {
    auto b=legacy(5,7,"DXT1",40,2); // 32 + 8 bytes, end 168.
    b.insert(b.end(),{0xaa,0xbb,0xcc});
    auto f=inspect_dds(std::span(b).first(160),"x",171).at("facts");
    CHECK_EQ(f.at("payload_expected_end"),168);CHECK_EQ(f.at("payload_status"),"prefix_incomplete");
    CHECK_EQ(f.at("payload_truncated"),false);CHECK_EQ(f.at("subresources_hashed"),1);
    CHECK(!f.at("subresources")[1].contains("sha256"));CHECK(!f.contains("source_sha256"));
    CHECK_EQ(f.at("trailer").at("offset"),168);CHECK_EQ(f.at("trailer").at("size"),3);
    CHECK(!f.at("trailer").contains("sha256"));CHECK_EQ(f.at("normal_form_eligible"),false);
    f=inspect_dds(std::span(b).first(160),"x",160).at("facts");CHECK_EQ(f.at("payload_status"),"truncated");
    CHECK_EQ(f.at("payload_truncated"),true);CHECK(!f.contains("trailer"));
    f=inspect_dds(b,"x",171).at("facts");CHECK_EQ(f.at("subresources_hashed"),2);
    CHECK_EQ(f.at("trailer").at("sha256"),"fa22dfe1da9013b3c1145040acae9089e0c08bc1c1a0719614f4b73add6f6ef5");
    CHECK_EQ(f.at("normal_form_eligible"),true);
    f=inspect_binary(std::span(b).first(128),"x",171).at("facts");CHECK_EQ(f.at("payload_truncated"),false);
    CHECK_EQ(inspect_dds(b,"x",170).at("facts").at("reason"),"input_size_contradiction");
    CHECK_EQ(inspect_binary(std::vector<std::uint8_t>{1,2},"x",1).at("issues")[0].at("code"),"input_size_contradiction");
}
// Mutating/releasing source storage must not change retained blocks or trailer.
TEST(dds_neutral_owns_immutable_lossless_bytes) {
    auto b=legacy(5,7,"DXT4",80,2);b[32]=77;b.insert(b.end(),{3,2,1});const auto original=b;
    std::string reason;auto n=normalize_dds(b,&reason);CHECK(n.has_value());CHECK(reason.empty());
    static_assert(std::is_same_v<decltype(n->subresources()),const std::vector<DdsBlockPayload>&>);
    static_assert(std::is_same_v<decltype(n->header()),std::span<const std::uint8_t>>);
    CHECK_EQ(n->subresources().size(),2u);CHECK_EQ(n->subresources()[0].bytes.size(),64u);
    CHECK_EQ(n->subresources()[1].description.at("offset"),192);CHECK_EQ(n->header()[32],77);
    CHECK_EQ(n->trailer().size(),3u);b.clear();b.shrink_to_fit();CHECK(n->lossless_dds()==original);
    CHECK_EQ(n->metadata().at("source_sha256"),sha256_bytes(original));
    auto v=dx10(95,4,4,112,3,4);u32(v,8,0x800000);u32(v,24,4);
    auto volume=normalize_dds(v);CHECK(volume.has_value());CHECK(volume->lossless_dds()==v);
    auto c=dx10(98,4,4,384,2,3,2,4);CHECK(normalize_dds(c)->lossless_dds()==c);
    for(auto bad:std::vector<std::vector<std::uint8_t>>{legacy(4,4,"ZZZZ"),dx10(71,4,4,8,1,3,1,0,5),legacy(4,4,"DXT1",7)}) {
        CHECK(!normalize_dds(bad,&reason));CHECK(!reason.empty());
    }
}
// Conflicting topology/encoding flags must not normalize through a different branch.
TEST(dds_conflicting_topology_flags_rejected) {
    auto b=dx10(71,4,4,48,1,3,1,4);u32(b,112,0x600);
    CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"cube_declaration_conflict");
    b=legacy();u32(b,80,0x44); // FourCC plus RGB declaration is ambiguous.
    CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"pixel_format_declaration_conflict");
}
// Luminance and a BC FourCC declare incompatible encodings, just as RGB does.
TEST(dds_conflicting_luminance_declaration_rejected) {
    auto b=legacy();u32(b,80,0x20004);
    CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"pixel_format_declaration_conflict");
}

#include <sys/mman.h>
TEST(dds_byte_limit_precedes_header_read_and_storage) {
    // Sparse virtual storage is inaccessible: reading even the magic before the
    // size check crashes, without allocating or touching a 256 MiB test buffer.
    const std::size_t size=268435457;
    void* memory=::mmap(nullptr,size,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    CHECK(memory!=MAP_FAILED);
    struct Unmap { void* p;std::size_t size;~Unmap(){::munmap(p,size);} } guard{memory,size};
    std::string reason;
    CHECK(!normalize_dds(std::span(static_cast<const std::uint8_t*>(memory),size),&reason));
    CHECK_EQ(reason,"normal_byte_limit");
}
TEST(dds_complete_legacy_cube_and_2d_array) {
    auto b=legacy(4,4,"DXT1",48);u32(b,112,0xfe00);
    auto f=inspect_dds(b,"x",b.size()).at("facts");CHECK_EQ(f.at("subresources").size(),6u);
    CHECK_EQ(f.at("subresources")[5].at("face"),"-Z");CHECK_EQ(f.at("subresources")[5].at("offset"),168);
    CHECK(normalize_dds(b)->lossless_dds()==b);
    b=dx10(81,4,4,48,2,3,3);f=inspect_dds(b,"x",b.size()).at("facts");
    CHECK_EQ(f.at("subresources").size(),6u);CHECK_EQ(f.at("subresources")[4].at("item"),2);
    CHECK_EQ(f.at("subresources")[4].at("offset"),180);CHECK(!f.at("subresources")[4].contains("face"));
    CHECK(normalize_dds(b)->lossless_dds()==b);
}
TEST(dds_short_headers_are_distinct_from_capped_headers) {
    auto b=dx10();auto p=std::span(b).first(140);
    CHECK_EQ(inspect_dds(p,"x",b.size()).at("facts").at("reason"),"dx10_header_prefix_incomplete");
    p=std::span(b).first(100);CHECK_EQ(inspect_dds(p,"x",b.size()).at("facts").at("reason"),"header_prefix_incomplete");
    auto f=inspect_dds(p,"x",b.size()).at("facts");CHECK(!f.contains("header_sha256"));CHECK(!f.contains("source_sha256"));
    b=legacy();u32(b,76,24);CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"noncanonical_pixel_format_size");
    b=dx10();u32(b,132,0);CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"invalid_resource_dimension");
    b=dx10(95,4,4,16,1,4);u32(b,24,4);CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"volume_depth_declaration");
    u32(b,8,0x800000);u32(b,140,2);CHECK_EQ(inspect_dds(b,"x",b.size()).at("facts").at("reason"),"volume_array");
}
// Arbitrary FourCC bytes are binary evidence, never unchecked JSON UTF-8 text.
TEST(dds_binary_fourcc_serializes_as_opaque_evidence) {
    auto b=legacy(4,4,"ZZZZ");b[84]=0xff;b[85]=0xc0;b[86]=0x80;b[87]=0;
    auto j=inspect_dds(b,"x",b.size());
    auto serialized=canonical_json(j); // Before the fix: invalid UTF-8 exception.
    CHECK(!serialized.empty());CHECK_EQ(j.at("facts").at("layout_status"),"unsupported_format");
    CHECK_EQ(j.at("facts").at("format").at("fourcc_bytes"),Json::array({255,192,128,0}));
    CHECK(j.at("facts").at("format").at("fourcc").is_null());
    for(unsigned value=0;value<256;++value) {
        b=legacy(4,4,"ZZZZ");b[84]=static_cast<std::uint8_t>(value);
        j=inspect_dds(b,"x",b.size());auto parsed=Json::parse(canonical_json(j));
        CHECK_EQ(parsed.at("facts").at("reason"),"unsupported_format");
        CHECK_EQ(parsed.at("facts").at("format").at("fourcc_bytes")[0],value);
        CHECK_EQ(parsed.at("facts").at("format").at("fourcc").is_string(),value>=32&&value<=126);
    }
}

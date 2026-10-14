#include "test.hpp"
#include "modbridge/core.hpp"
#include <fstream>
#include <cstdlib>
#include <set>
#include <thread>
#include <atomic>
#include <chrono>
#include <archive.h>
#include <archive_entry.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace modbridge;
namespace {
std::filesystem::path fixtures(){static bool made=[](){return std::system("python3 tests/make_fixtures.py build/fixtures")==0;}();CHECK(made);return "build/fixtures";}
std::vector<std::uint8_t> bytes(std::string name){std::ifstream f(fixtures()/name,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
Json entry(const Json& j,std::string path){for(const auto&e:j.at("entries"))if(e.at("path")==path)return e;throw std::runtime_error("missing fixture entry "+path);}
bool issue(const Json&j,std::string code){for(const auto&i:j.at("issues"))if(i.at("code")==code)return true;return false;}
}
// Catches lost mixed entries, unchecked extension claims and invented preparation status.
TEST(mixed_inventory){for(auto name:{"mixed.zip","mixed.tar"}){auto j=analyze_package(fixtures()/name,Limits{});CHECK_EQ(j.at("status"),"analyzed");CHECK_EQ(j.at("schema_version"),1);CHECK_EQ(entry(j,"logic.lua").at("kind"),"lua");CHECK_EQ(entry(j,"native.dll").at("kind"),"native_pe");CHECK_EQ(entry(j,"managed.dll").at("kind"),"managed_pe");CHECK_EQ(entry(j,"native.elf").at("kind"),"elf");CHECK(!j.at("requirements").empty());CHECK(!j.contains("prepared"));CHECK(!j.contains("deployed"));CHECK_EQ(j.at("package_sha256"),sha256_file(fixtures()/name));}}
TEST(pe_structure_and_clr){auto j=inspect_binary(bytes("native.dll"),"renamed.bin");auto f=j.at("facts");CHECK_EQ(j.at("kind"),"native_pe");CHECK_EQ(f.at("machine"),0x8664);CHECK_EQ(f.at("imports").at(0).at("library"),"KERNEL32.dll");CHECK_EQ(f.at("imports").at(0).at("symbols").at(0).at("name"),"CreateFileW");CHECK_EQ(f.at("imports").at(0).at("symbols").at(1).at("ordinal"),7);CHECK_EQ(f.at("tls").at("callback_count"),1);CHECK_EQ(f.at("exception_entries"),1);CHECK_EQ(f.at("exception_functions").at(0).at("unwind").at("version"),1);CHECK_EQ(f.at("relocation_entries"),2);auto c=inspect_binary(bytes("managed.dll"),"x");CHECK_EQ(c.at("kind"),"managed_pe");CHECK_EQ(c.at("facts").at("clr").at("metadata_signature"),"BSJB");CHECK_EQ(c.at("facts").at("clr").at("runtime_version"),"v4.0.30319");CHECK(!inspect_binary(bytes("truncated.pe"),"x").at("issues").empty());}
TEST(elf_bounds){auto j=inspect_binary(bytes("native.elf"),"x");CHECK_EQ(j.at("kind"),"elf");CHECK_EQ(j.at("facts").at("machine"),62);CHECK_EQ(j.at("facts").at("segments").size(),1u);CHECK_EQ(j.at("facts").at("relocations").at(0).at("type"),8);CHECK(!inspect_binary(bytes("truncated.elf"),"x").at("issues").empty());CHECK(!inspect_binary(bytes("malformed.elf"),"x").at("issues").empty());}
TEST(extension_is_not_proof){auto j=inspect_binary(bytes("fake.dll"),"fake.dll");CHECK_EQ(j.at("kind"),"unknown");CHECK_EQ(j.at("facts").at("classification_confidence"),"extension_only");CHECK_EQ(inspect_binary(bytes("archive.pak"),"archive.pak").at("facts").at("classification_confidence"),"extension_only");}
TEST(limits_and_paths){for(auto name:{"unsafe.zip","collision.zip","duplicate.zip"}){auto j=analyze_package(fixtures()/name,Limits{});CHECK(!j.at("issues").empty());CHECK(j.at("entries").size()<4);}Limits l;l.max_entry_bytes=16;CHECK(issue(analyze_package(fixtures()/"overflow.zip",l),"max_entry_bytes"));l=Limits{};l.max_entries=1;CHECK(issue(analyze_package(fixtures()/"mixed.zip",l),"max_entries"));l=Limits{};l.max_total_bytes=24;CHECK(issue(analyze_package(fixtures()/"mixed.zip",l),"max_total_bytes"));l=Limits{};l.max_inspection_bytes=100;auto j=analyze_package(fixtures()/"mixed.zip",l);CHECK_EQ(entry(j,"native.dll").at("facts").at("inspection_complete"),false);CHECK_EQ(entry(j,"native.dll").at("sha256"),sha256_file(fixtures()/"native.dll"));}
TEST(symlink_preserved_not_followed){auto j=analyze_package(fixtures()/"mixed.tar",Limits{});auto e=entry(j,"link.lua");CHECK_EQ(e.at("kind"),"symlink");CHECK_EQ(e.at("facts").at("target"),"../outside.lua");test::TempDir d;std::ofstream(d.path/"file")<<"owned";std::filesystem::create_symlink(fixtures()/"native.dll",d.path/"link");auto a=analyze_package(d.path,Limits{});CHECK_EQ(entry(a,"link").at("kind"),"symlink");CHECK_EQ(entry(a,"file").at("size"),5);}
TEST(input_provenance_coherent){auto j=analyze_package(fixtures()/"mixed.zip",Limits{});CHECK_EQ(j.at("provenance").at("input_stable"),true);CHECK_EQ(j.at("package_sha256"),sha256_file(fixtures()/"mixed.zip"));CHECK(!analyze_package(fixtures()/"fake.dll",Limits{}).at("issues").empty());auto e=analyze_package(fixtures()/"empty.zip",Limits{});CHECK(e.at("entries").empty());CHECK(!issue(e,"archive_error"));}
TEST(manifest_dependencies){auto j=analyze_package(fixtures()/"mixed.zip",Limits{});CHECK_EQ(j.at("dependencies").size(),2u);CHECK_EQ(j.at("metadata").at("provider_id"),"fixture-only");CHECK_EQ(j.at("dependencies").at(0).at("from"),"logic.lua");CHECK_EQ(j.at("dependencies").at(0).at("type"),"runtime");}
// A flood of zero-byte directory headers must consume the entry budget too.
TEST(directory_headers_are_bounded){Limits l;l.max_entries=1;auto j=analyze_package(fixtures()/"directories.tar",l);CHECK(issue(j,"max_entries"));}
// Replacement/remapping of a source must invalidate coherent package provenance.
TEST(root_symlink_is_rejected){test::TempDir d;std::filesystem::create_symlink(std::filesystem::absolute(fixtures()/"mixed.zip"),d.path/"input");auto j=analyze_package(d.path/"input",Limits{});CHECK(issue(j,"input_error"));CHECK(j.at("package_sha256").is_null());}
TEST(manifest_rejected_atomically){test::TempDir d;std::ofstream(d.path/"modbridge.json")<<R"({"metadata":{"unsafe":"not accepted"},"requirements":[{"id":"bogus.validated","status":"validated"}],"dependencies":[{"from":"a","to":"b","type":"invalid"}]})";auto j=analyze_package(d.path,Limits{});CHECK(issue(j,"manifest_invalid"));CHECK(!j.contains("metadata"));for(const auto&r:j.at("requirements"))CHECK(r.at("id")!="bogus.validated");}

TEST(input_mutation_invalidates_provenance){test::TempDir d;auto path=d.path/"changing.zip";{std::ofstream f(path,std::ios::binary);std::string block(65536,'x');for(int i=0;i<128;i++)f.write(block.data(),block.size());}std::atomic<bool>stop=false;std::thread writer([&]{while(!stop){std::fstream f(path,std::ios::in|std::ios::out|std::ios::binary);f.put('q');f.flush();std::this_thread::sleep_for(std::chrono::milliseconds(1));}});Json j;try{j=analyze_package(path,Limits{});}catch(...){stop=true;writer.join();throw;}stop=true;writer.join();CHECK(issue(j,"input_mutation"));CHECK_EQ(j.at("provenance").at("input_stable"),false);CHECK(j.at("package_sha256").is_null());CHECK_EQ(j.at("member_inventory_complete"),false);CHECK(j.at("container_groups").empty());}
TEST(declared_directory_limit_reports_name){test::TempDir d;std::ofstream(d.path/"a")<<"a";std::ofstream(d.path/"b")<<"b";Limits l;l.max_entries=1;CHECK(issue(analyze_package(d.path,l),"max_entries"));}
TEST(pe_unaccessed_section_bounds){auto b=bytes("native.dll");b.resize(1900);auto j=inspect_binary(b,"x");CHECK(!j.at("issues").empty());}
TEST(binary_parser_work_is_bounded){auto b=bytes("native.dll");b.resize(30000);auto w=[&](std::size_t p,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)b[p+i]=std::uint8_t(v>>(i*8));};auto sh=0x188u;w(sh+8,29488,4);w(sh+16,29488,4);w(0x98+112+8+4,820,4);for(auto i:{3,5,9}){w(0x98+112+i*8,0,8);}for(unsigned i=0;i<40;i++){auto t=0x200+i*20;w(t,0x3000,4);w(t+4,0,4);w(t+8,0,4);w(t+12,0x6000,4);w(t+16,0x3000,4);}for(unsigned i=0;i<20;i++)b[0x200+800+i]=0;for(unsigned i=0;i<1000;i++)w(0x2200+i*8,0x6100,8);w(0x2200+8000,0,8);b[0x5200]='l';b[0x5201]=0;b[0x5302]='f';b[0x5303]=0;auto j=inspect_binary(b,"x");CHECK(!j.at("issues").empty());CHECK_EQ(j.at("facts").at("inspection_complete"),false);}
TEST(directory_hash_is_content_identity){test::TempDir d;auto b=bytes("native.dll");{std::ofstream f(d.path/"native.dll",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}Limits small;small.max_inspection_bytes=100;auto full=analyze_package(d.path,Limits{}),partial=analyze_package(d.path,small);CHECK_EQ(full.at("package_sha256"),partial.at("package_sha256"));}
namespace {
std::string nested_arrays(unsigned depth) {
    return std::string(depth, '[') + "0" + std::string(depth, ']');
}
void manifest_child_rejects(const std::string& document) {
    test::TempDir dir;
    std::ofstream(dir.path / "modbridge.json") << document;
    auto pid = ::fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        // A parser crash must fail one regression, rather than abort the suite.
        try {
            auto result = analyze_package(dir.path, Limits{});
            const bool rejected = issue(result, "manifest_invalid") &&
                result.at("inventory_complete") == false &&
                !result.contains("metadata") && result.at("dependencies").empty();
            auto serialized = canonical_json(result);
            ::_exit(rejected && !serialized.empty() ? 0 : 2);
        } catch (...) { ::_exit(3); }
    }
    int status = 0;
    CHECK_EQ(::waitpid(pid, &status, 0), pid);
    CHECK_EQ(status, 0);
}
Json manifest_result(const std::string& document) {
    test::TempDir dir;
    std::ofstream(dir.path / "modbridge.json") << document;
    return analyze_package(dir.path, Limits{});
}
}
// Removing the parse-time depth gate must make these subprocesses fail/crash.
TEST(manifest_deep_metadata_is_bounded) {
    manifest_child_rejects("{\"metadata\":{\"x\":" + nested_arrays(20000) + "}}");
}
TEST(manifest_deep_requirements_are_bounded) {
    manifest_child_rejects("{\"requirements\":[{\"id\":\"runtime.test\",\"payload\":" + nested_arrays(20000) + "}]}");
}
TEST(manifest_deep_dependencies_are_bounded) {
    manifest_child_rejects("{\"dependencies\":[{\"from\":\"a\",\"to\":\"b\",\"type\":\"reference\",\"payload\":" + nested_arrays(20000) + "}]}");
}
// Container depth counts the root and metadata object: 2 + 62 = 64.
TEST(manifest_depth_boundary) {
    auto allowed = manifest_result("{\"metadata\":{\"x\":" + nested_arrays(62) + "}}");
    CHECK(!issue(allowed, "manifest_invalid"));
    CHECK(allowed.contains("metadata"));
    CHECK_EQ(allowed.at("inventory_complete"), true);
    CHECK(!canonical_json(allowed).empty());
    auto rejected = manifest_result("{\"metadata\":{\"x\":" + nested_arrays(63) + "}}");
    CHECK(issue(rejected, "manifest_invalid"));
    CHECK_EQ(rejected.at("inventory_complete"), false);
    CHECK(!rejected.contains("metadata"));
}
// Node counting includes each JSON value/container and each object key.
TEST(manifest_node_boundary) {
    std::string values = "0";
    for (unsigned i = 1; i < 9995; ++i) values += ",0";
    auto allowed = manifest_result("{\"metadata\":{\"values\":[" + values + "]}}");
    CHECK(!issue(allowed, "manifest_invalid"));
    CHECK_EQ(allowed.at("metadata").at("values").size(), 9995u);
    auto rejected = manifest_result("{\"metadata\":{\"values\":[" + values + ",0]}}");
    CHECK(issue(rejected, "manifest_invalid"));
    CHECK_EQ(rejected.at("inventory_complete"), false);
    CHECK(!rejected.contains("metadata"));
}

#include "dds_fixture.hpp"
// Whole-file digest must stay stable while the DDS sees the real size under a cap.
TEST(dds_inventory_true_size_and_magic) {
    test::TempDir d;auto b=dds_fixture::legacy(5,7,"DXT1",40,2);
    {std::ofstream f(d.path/"renamed.lua",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
    Limits cap;cap.max_inspection_bytes=160;
    auto partial=analyze_package(d.path,cap),full=analyze_package(d.path,Limits{});
    auto e=entry(partial,"renamed.lua"),f=e.at("facts");CHECK_EQ(e.at("kind"),"dds_texture");
    CHECK_EQ(f.at("input_total_size"),168);CHECK_EQ(f.at("input_bytes_available"),160);
    CHECK_EQ(f.at("payload_status"),"prefix_incomplete");CHECK_EQ(f.at("payload_truncated"),false);
    CHECK_EQ(f.at("subresources_hashed"),1);CHECK_EQ(partial.at("inventory_complete"),false);
    CHECK_EQ(e.at("sha256"),sha256_bytes(b));CHECK_EQ(partial.at("package_sha256"),full.at("package_sha256"));
    CHECK_EQ(entry(full,"renamed.lua").at("facts").at("payload_complete"),true);
}

namespace {
using ContainerMembers = std::vector<std::pair<std::string, std::string>>;
void container_archive(const std::filesystem::path& path, const ContainerMembers& members,
                       bool zip = true) {
    auto* writer = archive_write_new();
    CHECK(writer != nullptr);
    CHECK_EQ(zip ? archive_write_set_format_zip(writer) : archive_write_set_format_pax_restricted(writer), ARCHIVE_OK);
    if (zip) CHECK_EQ(archive_write_set_format_option(writer, "zip", "compression", "store"), ARCHIVE_OK);
    CHECK_EQ(archive_write_open_filename(writer, path.c_str()), ARCHIVE_OK);
    for (const auto& [name, data] : members) {
        auto* item = archive_entry_new();
        CHECK(item != nullptr);
        archive_entry_set_pathname(item, name.c_str());
        archive_entry_set_filetype(item, AE_IFREG);
        archive_entry_set_perm(item, 0600);
        archive_entry_set_size(item, data.size());
        CHECK_EQ(archive_write_header(writer, item), ARCHIVE_OK);
        CHECK_EQ(archive_write_data(writer, data.data(), data.size()), static_cast<la_ssize_t>(data.size()));
        archive_entry_free(item);
    }
    CHECK_EQ(archive_write_close(writer), ARCHIVE_OK);
    CHECK_EQ(archive_write_free(writer), ARCHIVE_OK);
}
const Json& container_group(const Json& inventory, const std::string& parent, const std::string& stem) {
    for (const auto& group : inventory.at("container_groups"))
        if (group.at("parent") == parent && group.at("stem") == stem) return group;
    throw std::runtime_error("missing container candidate");
}
bool container_reason(const Json& group, const std::string& code) {
    for (const auto& value : group.at("reason_codes")) if (value == code) return true;
    return false;
}
void unpublished_containers(const Json& inventory) {
    CHECK_EQ(inventory.at("member_inventory_complete"), false);
    CHECK(inventory.at("container_groups").empty());
}
}

// Full streamed byte identities must survive a prefix cap without authorizing preparation.
TEST(container_large_member_separates_byte_and_inspection_completeness) {
    test::TempDir dir;
    ContainerMembers members{{"Wrapper/Bundle.PAK", "opaque pak"},
        {"Wrapper/Bundle.utoc", "opaque toc"},
        {"Wrapper/Bundle.ucas", std::string(8 * 1024 * 1024 + 1, 'u')}};
    container_archive(dir.path / "input.zip", members);
    const auto inventory = analyze_package(dir.path / "input.zip", Limits{});
    CHECK_EQ(inventory.at("member_inventory_complete"), true);
    CHECK_EQ(inventory.at("inventory_complete"), false);
    CHECK(issue(inventory, "inspection_cap"));
    CHECK_EQ(entry(inventory, "Wrapper/Bundle.ucas").at("facts").at("inspection_complete"), false);
    CHECK_EQ(entry(inventory, "Wrapper/Bundle.ucas").at("sha256"),
        sha256_bytes(std::span(reinterpret_cast<const std::uint8_t*>(members.back().second.data()), members.back().second.size())));
    CHECK_EQ(inventory.at("container_groups").size(), 1u);
    const auto& group = container_group(inventory, "Wrapper", "Bundle");
    CHECK_EQ(group.at("kind"), "unreal_iostore");
    CHECK_EQ(group.at("status"), "candidate");
    CHECK_EQ(group.at("members_complete"), true);
    CHECK_EQ(group.at("validation_state"), "unverified");
    CHECK_EQ(group.at("installation_supported"), false);
    CHECK_EQ(group.at("source_package_sha256"), inventory.at("package_sha256"));
    CHECK_EQ(group.at("members").size(), 3u);
    CHECK_EQ(group.at("members").at(0).at("role"), "pak");
    CHECK_EQ(group.at("members").at(0).at("original_path"), "Wrapper/Bundle.PAK");
    CHECK_EQ(group.at("members").at(1).at("role"), "ucas");
    CHECK_EQ(group.at("members").at(1).at("size"), 8388609);
    CHECK_EQ(group.at("members").at(2).at("role"), "utoc");
    CHECK(!inventory.contains("prepared")); CHECK(!inventory.contains("deployed"));
    CHECK(!inventory.at("requirements").empty());
}

// Wrapper paths and equal stems in distinct parents cannot be merged or rewritten.
TEST(container_groups_are_independent_of_outer_archive_format_and_wrapper) {
    test::TempDir dir;
    const ContainerMembers members{{"Outer/First/same.pak", "one"},
        {"Outer/First/same.utoc", "two"}, {"Outer/First/same.ucas", "three"},
        {"Outer/Second/same.utoc", "four"}, {"Outer/Second/same.ucas", "five"},
        {"readme.txt", "not a container"}};
    std::string prior_id;
    for (bool zip : {true, false}) {
        auto path = dir.path / (zip ? "input.zip" : "input.tar");
        container_archive(path, members, zip);
        auto inventory = analyze_package(path, Limits{});
        CHECK_EQ(inventory.at("member_inventory_complete"), true);
        CHECK_EQ(inventory.at("container_groups").size(), 2u);
        const auto& first = container_group(inventory, "Outer/First", "same");
        const auto& second = container_group(inventory, "Outer/Second", "same");
        CHECK_EQ(first.at("members").size(), 3u); CHECK_EQ(second.at("members").size(), 2u);
        CHECK_EQ(second.at("members").at(0).at("path"), "Outer/Second/same.ucas");
        CHECK(first.at("group_id") != second.at("group_id"));
        CHECK_EQ(first.at("validation_state"), "unverified");
        CHECK_EQ(second.at("installation_supported"), false);
        const auto repeated = analyze_package(path, Limits{});
        CHECK_EQ(inventory.at("container_groups"), repeated.at("container_groups"));
        if (!prior_id.empty()) CHECK(first.at("group_id") != prior_id);
        prior_id = first.at("group_id").get<std::string>();
    }
}

// Case-only partner spellings and extra partition-shaped lanes must stay unresolved.
TEST(container_missing_case_and_partition_candidates_are_unresolved) {
    test::TempDir dir;
    container_archive(dir.path / "input.zip", {{"lost.utoc", "a"}, {"ORBIT.utoc", "b"},
        {"orbit.ucas", "c"}, {"shared.utoc", "d"}, {"shared.ucas", "e"}, {"shared_s1.ucas", "f"}});
    auto inventory = analyze_package(dir.path / "input.zip", Limits{});
    CHECK_EQ(inventory.at("member_inventory_complete"), true);
    CHECK_EQ(inventory.at("container_groups").size(), 5u);
    CHECK(container_reason(container_group(inventory, "", "lost"), "container_member_missing"));
    for (auto stem : {"ORBIT", "orbit"}) {
        const auto& group = container_group(inventory, "", stem);
        CHECK(container_reason(group, "container_case_ambiguity"));
        CHECK_EQ(group.at("members_complete"), false);
    }
    for (auto stem : {"shared", "shared_s1"}) {
        const auto& group = container_group(inventory, "", stem);
        CHECK(container_reason(group, "container_partition_unresolved"));
        CHECK_EQ(group.at("status"), "unresolved");
        CHECK_EQ(group.at("members_complete"), false);
    }
}

// Links cannot become container bytes, even when their names have recognized suffixes.
TEST(container_standalone_pak_excludes_link_metadata) {
    test::TempDir dir;
    std::ofstream(dir.path / "base.pak") << "unchanged opaque bytes";
    std::filesystem::create_symlink("base.pak", dir.path / "linked.pak");
    auto inventory = analyze_package(dir.path, Limits{});
    CHECK_EQ(inventory.at("member_inventory_complete"), true);
    CHECK_EQ(inventory.at("container_groups").size(), 1u);
    const auto& group = container_group(inventory, "", "base");
    CHECK_EQ(group.at("kind"), "unreal_pak"); CHECK_EQ(group.at("members_complete"), true);
    CHECK_EQ(group.at("installation_supported"), false);
    CHECK(!inventory.at("requirements").empty());
}

// Byte completeness cannot erase a malformed structural prefix or its requirements.
TEST(container_structural_inspection_failure_remains_blocked) {
    test::TempDir dir;
    container_archive(dir.path / "input.zip", {{"broken.pak", "MZinvalid"}});
    auto inventory = analyze_package(dir.path / "input.zip", Limits{});
    CHECK_EQ(inventory.at("member_inventory_complete"), true);
    CHECK_EQ(inventory.at("inventory_complete"), false);
    CHECK(issue(inventory, "binary_structure_incomplete"));
    CHECK_EQ(container_group(inventory, "", "broken").at("validation_state"), "unverified");
    CHECK(!inventory.at("requirements").empty());
}

// A bad unused late row or a resource cap suppresses an otherwise complete earlier trio.
TEST(container_global_late_path_and_limit_failures_publish_no_groups) {
    test::TempDir dir;
    const ContainerMembers trio{{"Wrapper/base.pak", "p"}, {"Wrapper/base.utoc", "t"}, {"Wrapper/base.ucas", "u"}};
    for (auto bad : {"../late.bin", "Wrapper\\base.PAK"}) {
        auto rows = trio; rows.push_back({bad, "late"});
        container_archive(dir.path / "input.zip", rows);
        auto inventory = analyze_package(dir.path / "input.zip", Limits{});
        unpublished_containers(inventory);
        CHECK_EQ(inventory.at("inventory_complete"), false);
    }
    auto rows = trio; rows.push_back({"late.bin", "late"});
    container_archive(dir.path / "input.zip", rows);
    Limits entries; entries.max_entries = 3;
    unpublished_containers(analyze_package(dir.path / "input.zip", entries));
    Limits total; total.max_total_bytes = 3;
    unpublished_containers(analyze_package(dir.path / "input.zip", total));
    Limits member; member.max_entry_bytes = 2;
    unpublished_containers(analyze_package(dir.path / "input.zip", member));
}

// A CRC failure/truncation in member four cannot publish a prefix group from members one-three.
TEST(container_late_archive_corruption_and_truncation_publish_no_groups) {
    test::TempDir dir;
    const std::string marker = "unique late original member bytes";
    container_archive(dir.path / "input.zip", {{"base.pak", "p"}, {"base.utoc", "t"},
        {"base.ucas", "u"}, {"late.bin", marker}});
    std::ifstream input(dir.path / "input.zip", std::ios::binary);
    std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto position = raw.find(marker); CHECK(position != std::string::npos);
    auto corrupt = raw; corrupt[position] ^= 1;
    { std::ofstream out(dir.path / "corrupt.zip", std::ios::binary); out.write(corrupt.data(), corrupt.size()); }
    auto rejected = analyze_package(dir.path / "corrupt.zip", Limits{});
    CHECK(issue(rejected, "archive_error")); unpublished_containers(rejected);
    raw.resize(position + 4);
    { std::ofstream out(dir.path / "truncated.zip", std::ios::binary); out.write(raw.data(), raw.size()); }
    unpublished_containers(analyze_package(dir.path / "truncated.zip", Limits{}));
}

TEST(container_empty_and_failed_inputs_have_coherent_byte_completeness) {
    auto empty = analyze_package(fixtures() / "empty.zip", Limits{});
    CHECK_EQ(empty.at("member_inventory_complete"), true); CHECK(empty.at("container_groups").empty());
    test::TempDir dir;
    unpublished_containers(analyze_package(dir.path / "absent.zip", Limits{}));
}

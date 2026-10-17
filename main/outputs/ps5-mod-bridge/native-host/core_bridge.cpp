#include "core_bridge.h"
#include "modbridge/dds.hpp"
#include <cstring>
#include <stdexcept>

namespace {
using modbridge::Json;
int prepare(char* output, size_t capacity, size_t* length) noexcept {
    if (length) *length=0;
    if (output && capacity) output[0]=0;
    return !length || (!output && capacity) ? MB_CORE_INVALID_ARGUMENT : MB_CORE_OK;
}
int copy_json(const Json& value, char* output, size_t capacity, size_t* length) {
    const std::string owned=value.dump();
    *length=owned.size();
    if (capacity<=owned.size()) return MB_CORE_BUFFER_TOO_SMALL;
    std::memcpy(output,owned.c_str(),owned.size()+1);
    return MB_CORE_OK;
}
void put(std::vector<std::uint8_t>& bytes, size_t offset, unsigned value) {
    for (unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::vector<std::uint8_t> legacy_fixture() {
    std::vector<std::uint8_t> bytes(163,0);
    std::memcpy(bytes.data(),"DDS ",4);
    put(bytes,4,124); put(bytes,12,7); put(bytes,16,5); put(bytes,76,32); put(bytes,80,4);
    std::memcpy(bytes.data()+84,"DXT1",4);
    for(size_t i=128;i<163;++i) bytes[i]=static_cast<std::uint8_t>(i);
    return bytes;
}
std::vector<std::uint8_t> cube_fixture() {
    std::vector<std::uint8_t> bytes(342,0);
    std::memcpy(bytes.data(),"DDS ",4);
    put(bytes,4,124); put(bytes,12,4); put(bytes,16,4); put(bytes,28,2);
    put(bytes,76,32); put(bytes,80,4); std::memcpy(bytes.data()+84,"DX10",4);
    put(bytes,128,98); put(bytes,132,3); put(bytes,136,4); put(bytes,140,1);
    for(size_t i=148;i<342;++i) bytes[i]=static_cast<std::uint8_t>(i);
    return bytes;
}
// These are runtime probes: preserve accesses and separate TLS lookups at -Os.
thread_local volatile int tls_value=41;
[[gnu::noinline]] void tls_change() { tls_value=42; }
[[gnu::noinline]] int tls_observe() { return tls_value; }
[[gnu::noinline]] void tls_reset() { tls_value=41; }
[[gnu::noinline]] bool tls_check() {
    const bool initial=tls_observe()==41;
    tls_change(); const bool changed=tls_observe()==42;
    tls_reset();
    return initial && changed;
}
// Observable cleanup and read are required evidence, not just C++ semantics.
struct Guard {
    volatile int& destroyed;
    [[gnu::noinline]] ~Guard() { destroyed=destroyed+1; }
};
[[gnu::noinline]] int guard_observe(const volatile int& destroyed) noexcept { return destroyed; }
[[gnu::noinline]] bool exception_check() {
    volatile int destroyed=0;
    try {
        try {
            Guard guard{destroyed};
            throw std::runtime_error("owned core unwind");
        } catch (const std::runtime_error&) {
            throw;
        }
    } catch (const std::runtime_error& error) {
        return guard_observe(destroyed)==1 && std::strcmp(error.what(),"owned core unwind")==0;
    }
    return false;
}
Json selftest_report() {
    std::vector<std::uint8_t> allocation={'c','o','r','e'};
    const std::string text(allocation.begin(),allocation.end());
    const bool allocation_ok=allocation.size()==4 && text=="core";
    auto legacy=legacy_fixture();
    auto normal=modbridge::normalize_dds(legacy);
    auto facts=modbridge::inspect_dds(legacy,"builtin_legacy.dds",163).at("facts");
    const bool legacy_ok=normal && normal->header().size()==128 && normal->subresources().size()==1 &&
        normal->subresources()[0].bytes.size()==32 && normal->trailer().size()==3 &&
        facts.at("payload_expected_end")==160 && facts.at("subresources")[0].at("offset")==128 &&
        facts.at("trailer").at("offset")==160 && normal->lossless_dds()==legacy;
    auto cube=cube_fixture();
    auto cube_normal=modbridge::normalize_dds(cube);
    auto cube_facts=modbridge::inspect_dds(cube,"builtin_cube.dds",342).at("facts");
    bool cube_ok=cube.size()==342 && cube_normal && cube_normal->header().size()==148 &&
        cube_normal->trailer().size()==2 && cube_normal->subresources().size()==12 &&
        cube_facts.at("payload_expected_end")==340 && cube_facts.at("trailer").at("offset")==340 &&
        cube_normal->lossless_dds()==cube;
    if(cube_normal) for(size_t i=0;i<12 && cube_ok;++i) {
        const auto& item=cube_normal->subresources()[i];
        cube_ok=item.bytes.size()==16 && item.description.at("offset")==148+16*i &&
            item.description.at("mip")==i%2 && item.description.at("item")==i/2;
    }
    legacy.resize(159);
    auto incomplete=modbridge::inspect_dds(legacy,"builtin_truncated.dds",159);
    bool requirements=false, writer=false, acceptance=false;
    for(const auto& item:incomplete.at("requirements")) {
        if(item.at("status")!="unresolved") continue;
        writer=writer || item.at("id")=="texture.ps5_writer";
        acceptance=acceptance || item.at("id")=="texture.game_acceptance";
    }
    requirements=writer && acceptance;
    const bool truncated=!incomplete.at("facts").at("normal_form_eligible").get<bool>() &&
        incomplete.at("facts").at("payload_status")=="truncated" &&
        !modbridge::normalize_dds(legacy) && requirements;
    Json checks={{"sha256_known_vector",modbridge::sha256_bytes({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"allocator_roundtrip",allocation_ok},{"exception_raii",exception_check()},
        {"thread_local_single_thread",tls_check()},{"dds_legacy_roundtrip",legacy_ok},
        {"dds_cube_mip_roundtrip",cube_ok},{"dds_truncation_detected",truncated}};
    bool passed=true;
    for(const auto& check:checks) passed=passed && check.get<bool>();
    return {{"schema_version",1},{"core_version",modbridge::core_version},{"status",passed?"passed":"failed"},
        {"scope","owned_builtin_fixtures"},{"installation_supported",false},{"checks",std::move(checks)}};
}
}
extern "C" int mb_core_inspect_dds(const unsigned char* bytes, size_t available,
        uint64_t total_size, char* output, size_t capacity, size_t* length) noexcept {
    const int status=prepare(output,capacity,length);
    if(status!=MB_CORE_OK) return status;
    if(available>MB_CORE_INPUT_LIMIT_BYTES) return MB_CORE_INPUT_LIMIT;
    if(!bytes && available) return MB_CORE_INVALID_ARGUMENT;
    try {
        return copy_json(modbridge::inspect_dds({bytes,available},"native_input.dds",total_size),output,capacity,length);
    } catch(...) { *length=0; return MB_CORE_INTERNAL_ERROR; }
}
extern "C" int mb_core_selftest(char* output, size_t capacity, size_t* length) noexcept {
    const int status=prepare(output,capacity,length);
    if(status!=MB_CORE_OK) return status;
    try { return copy_json(selftest_report(),output,capacity,length); }
    catch(...) { *length=0; return MB_CORE_INTERNAL_ERROR; }
}

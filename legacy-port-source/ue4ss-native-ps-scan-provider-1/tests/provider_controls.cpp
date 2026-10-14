#include <OwnedMappedScanSource.hpp>
#include <MappedFixture.hpp>
#include <fixtures/fixture_layout.hpp>
#include <cassert>
#include <cstdio>
using namespace RC::Unreal::Platform;
static uint64_t callback_address;
static unsigned successes,failures;
static void normal(PsChar16* message) {
    assert(message && *message);
    // Real service reentry would deadlock if collect leaked its read lease.
    assert(PLH::OwnedMemory::query(callback_address));++successes;
}
static void error(PsChar16* message) {
    assert(message && *message);assert(PLH::OwnedMemory::query(callback_address));++failures;
}
static void quiet(PsChar16*){}
int main(int argc,char** argv) {
    assert(argc==2);MappedFixture fixture(argv[1]);fixture.protect();
    auto context=fixture.context();auto expected=context.module_source;
    callback_address=fixture.address(0);
    OwnedMappedImageFacts facts{PS_MAPPED_IMAGE_ELF64,fixture.address(fixture.text),0,
        fixture.frame_extent(),fixture.header_extent(),{}};
    std::shared_ptr<const OwnedMappedScanSource> source=std::make_shared<OwnedMappedScanSource>(expected,facts);
    PsScanConfig all{true,true,true,true,true,true,true,true,true,true,true,true,true};
    PsCtx ctx{quiet,normal,quiet,quiet,error,all};PsScanResults result{};
    assert(scan_owned_source(source,expected,&ctx,&result));
    assert(successes==1 && failures==0);
    assert(result.engine_version.major==5 && result.engine_version.minor==7);
    assert(result.g_uobject_array==reinterpret_cast<void*>(fixture.address(owned_object_slot_rva)));
    assert(result.gmalloc==reinterpret_cast<void*>(fixture.address(owned_allocator_slot_rva)));
    assert(result.fname_ctor_wchar==reinterpret_cast<void*>(fixture.address(owned_fname_ctor_rva)));
    assert(result.static_construct_object_internal==reinterpret_cast<void*>(fixture.address(owned_construct_rva)));
    assert(result.gameengine_tick==reinterpret_cast<void*>(fixture.address(owned_tick_rva)));
    assert(!result.fname_tostring_fstring && !result.ftext_fstring && !result.gnatives);
    assert(!result.fuobject_hash_tables_get && !result.console_manager_singleton);
    assert(result.build_configuration==0 && result.stats==0);
    auto same_bytes_other_owner=fixture.context().module_source;
    assert(!scan_owned_source(source,same_bytes_other_owner,&ctx,&result));
    assert(!result.g_uobject_array && result.engine_version.major==5 && result.engine_version.minor==7);
    result.g_uobject_array=reinterpret_cast<void*>(uintptr_t{17});result.engine_version={3,9};
    ctx.config={};ctx.config.engine_version=true;ctx.config.ftext_fstring=true;
    assert(scan_owned_source(source,expected,&ctx,&result));
    assert(result.g_uobject_array==reinterpret_cast<void*>(uintptr_t{17}) && result.engine_version.minor==7);
    assert(!scan_owned_source({},expected,&ctx,&result));
    assert(result.g_uobject_array==reinterpret_cast<void*>(uintptr_t{17}) && result.engine_version.minor==7);
    auto invalid=ctx;invalid.error=nullptr;const auto calls=successes+failures;
    assert(!scan_owned_source(source,expected,&invalid,&result));
    assert(!scan_owned_source(source,expected,nullptr,&result));
    assert(!scan_owned_source(source,expected,&ctx,nullptr));assert(successes+failures==calls);
    PLH::MemAccessor accessor;bool ok=false;
    accessor.mem_protect(fixture.address(fixture.text),1,PLH::R,ok);assert(ok);
    assert(!scan_owned_source(source,expected,&ctx,&result));
    assert(result.engine_version.minor==7 && result.g_uobject_array==reinterpret_cast<void*>(uintptr_t{17}));
    accessor.mem_protect(fixture.address(fixture.text),1,PLH::R|PLH::X,ok);assert(ok);
    assert(scan_owned_source(source,expected,&ctx,&result));
    std::weak_ptr<PLH::OwnedMemory::OwnedMapping> weak=fixture.owner;
    fixture.owner.reset();context.module_source.reset();same_bytes_other_owner.reset();expected.reset();
    assert(!weak.expired());
    assert(scan_owned_source(source,source->source_owner(),&ctx,&result));
    source.reset();assert(weak.expired());
    std::printf("native source controls passed: actual7leaf path, five required results, callbacks after lease, owner/permission refusal and retained lifetime (%u success/%u failure callbacks)\n",successes,failures);
    return 0;
}

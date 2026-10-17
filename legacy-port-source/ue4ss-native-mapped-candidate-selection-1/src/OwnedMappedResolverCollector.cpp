#include <OwnedMappedResolverCollector.hpp>
#include <patternsleuth_mapped_core.h>
#include <patternsleuth_mapped_properties.h>

namespace RC::Unreal::Platform {
static_assert(sizeof(PsMappedCoreResult)==16 && alignof(PsMappedCoreResult)==8);
static_assert(sizeof(void*)==8);

auto unavailable_owned_mapped(const PsScanConfig& config,const PsScanResults& previous)
    noexcept -> OwnedMappedScanReport {
    OwnedMappedScanReport report{};
    report.results=previous;
    const bool requested[]{config.g_uobject_array,config.fname_tostring_fstring,
        config.fname_ctor_wchar,config.gmalloc,config.static_construct_object_internal,
        config.ftext_fstring,config.engine_version,config.fuobject_hash_tables_get,
        config.gnatives,config.console_manager_singleton,config.gameengine_tick,
        config.build_configuration,config.stats};
    for(size_t slot=0;slot<13;++slot) {
        report.slots[slot].requested=requested[slot];
        if(requested[slot])report.requested_mask|=uint32_t{1}<<slot;
    }
    report.image_error=true;
    report.required_missing_mask=report.requested_mask & MappedRequiredSlots;
    auto clear=[&](size_t slot,void*& field){if(requested[slot])field=nullptr;};
    clear(0,report.results.g_uobject_array);clear(1,report.results.fname_tostring_fstring);
    clear(2,report.results.fname_ctor_wchar);clear(3,report.results.gmalloc);
    clear(4,report.results.static_construct_object_internal);clear(5,report.results.ftext_fstring);
    clear(7,report.results.fuobject_hash_tables_get);clear(8,report.results.gnatives);
    clear(9,report.results.console_manager_singleton);clear(10,report.results.gameengine_tick);
    if(requested[11])report.results.build_configuration=0;
    if(requested[12])report.results.stats=0;
    return report;
}

auto collect_owned_mapped(const OwnedMappedImageCaller& image,const PsScanConfig& config,
    const PsScanResults& previous) noexcept -> OwnedMappedScanReport {
    auto report=unavailable_owned_mapped(config,previous);
    report.image_error=false;
    constexpr size_t provider_slots[]{6,0,2,1,3,4,10};
    std::array<PsMappedCoreResult,13> values{};
    // Original resolver scheduling precedes config gating. Disabled fields stay
    // untouched even when their provider runs, misses or returns a candidate.
    for(uint32_t id=1;id<=7;++id) {
        const auto slot=provider_slots[id-1];
        auto& value=values[slot];auto& outcome=report.slots[slot];
        outcome.endpoint_status=ps_resolve_mapped_core_v1(&image.view(),id,&value);
        switch(outcome.endpoint_status) {
        case 0:
            if(value.reserved || (slot==6 ? (!value.major || value.address) : (value.major || value.minor))) {
                outcome.status=MappedSlotStatus::InvalidResult;report.image_error=true;
            } else if(slot==6 || ((slot==0 || slot==3) ? image.permits_pointer_slot(value.address) : image.permits_code(value.address))) {
                outcome.status=MappedSlotStatus::Available;
                report.available_mask|=uint32_t{1}<<slot;
            } else outcome.status=MappedSlotStatus::RejectedAddress;
            break;
        case 1:outcome.status=MappedSlotStatus::InvalidArguments;report.image_error=true;break;
        case 2:outcome.status=MappedSlotStatus::InvalidImage;report.image_error=true;break;
        case 3:outcome.status=MappedSlotStatus::NoMatch;break;
        case 4:outcome.status=MappedSlotStatus::Ambiguous;break;
        case 5:outcome.status=MappedSlotStatus::OperandOrMetadataError;break;
        default:outcome.status=MappedSlotStatus::InvalidResult;report.image_error=true;break;
        }
    }
    static_assert(sizeof(PsMappedPropertyResult)==8 && alignof(PsMappedPropertyResult)==4);
    std::array<PsMappedPropertyResult,2> properties{};
    for(uint32_t id=1;id<=2;++id) {
        const size_t slot=id+10;
        auto& value=properties[id-1];auto& outcome=report.slots[slot];
        outcome.endpoint_status=ps_resolve_mapped_property_v1(&image.view(),id,&value);
        switch(outcome.endpoint_status) {
        case 0:
            if(value.reserved || value.value<1 || value.value>(id==1?3u:2u)) {
                outcome.status=MappedSlotStatus::InvalidResult;report.image_error=true;
            } else {
                outcome.status=MappedSlotStatus::Available;
                report.available_mask|=uint32_t{1}<<slot;
            }
            break;
        case 1:outcome.status=MappedSlotStatus::InvalidArguments;report.image_error=true;break;
        case 2:outcome.status=MappedSlotStatus::InvalidImage;report.image_error=true;break;
        case 3:outcome.status=MappedSlotStatus::NoMatch;break;
        default:outcome.status=MappedSlotStatus::InvalidResult;report.image_error=true;break;
        }
    }
    report.required_missing_mask=report.requested_mask & MappedRequiredSlots & ~report.available_mask;
    auto pointer=[&](size_t slot,void*& field) {
        if(report.slots[slot].requested)field=(report.available_mask & (uint32_t{1}<<slot))
            ? reinterpret_cast<void*>(static_cast<uintptr_t>(values[slot].address)) : nullptr;
    };
    pointer(0,report.results.g_uobject_array);
    pointer(1,report.results.fname_tostring_fstring);
    pointer(2,report.results.fname_ctor_wchar);
    pointer(3,report.results.gmalloc);
    pointer(4,report.results.static_construct_object_internal);
    pointer(5,report.results.ftext_fstring);
    // Genuine EngineVersion failure preserves the previous value.
    if(report.slots[6].requested && (report.available_mask & (uint32_t{1}<<6)))
        report.results.engine_version={values[6].major,values[6].minor};
    pointer(7,report.results.fuobject_hash_tables_get);
    pointer(8,report.results.gnatives);
    pointer(9,report.results.console_manager_singleton);
    pointer(10,report.results.gameengine_tick);
    if(report.slots[11].requested)report.results.build_configuration=(report.available_mask & (uint32_t{1}<<11))?properties[0].value:0;
    if(report.slots[12].requested)report.results.stats=(report.available_mask & (uint32_t{1}<<12))?properties[1].value:0;
    return report;
}
}

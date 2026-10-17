#pragma once
#include <PsMappedImageCaller.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include <limits>
#include <vector>
namespace RC::Unreal::Platform {
// Synchronous mapped resolver caller. No discovery, registration or engine
// identity assertion. Context must already own actual described bytes.
class OwnedMappedImageCaller final {
    RC::Platform::NativeProgramContext context_;
    std::vector<PLH::OwnedMemory::ReadRange> ranges_;
    PLH::OwnedMemory::ReadLease lease_;
    PsMappedImageCaller caller_;
    static auto ranges(const RC::Platform::NativeProgramContext& context) {
        if (!context.module_source) throw std::invalid_argument("Mapped resolver requires module owner");
        std::vector<PLH::OwnedMemory::ReadRange> result;
        for (const auto& range : context.module_source->ReadableRanges().at(ScanTarget::MainExe))
            result.push_back({range.data, range.size});
        return result;
    }
    static auto segments(const PLH::OwnedMemory::ReadLease& lease) {
        std::vector<PsMappedSegment> result;
        for (const auto& span : lease.spans()) {
            // The PLH bit numbering differs from the wire flags.
            const auto flags = uint32_t((span.protection & PLH::R ? PS_MAPPED_READ : 0) |
                (span.protection & PLH::W ? PS_MAPPED_WRITE : 0) |
                (span.protection & PLH::X ? PS_MAPPED_EXECUTE : 0));
            result.push_back({span.data,span.size,reinterpret_cast<uintptr_t>(span.data),flags,0});
        }
        return result;
    }
    auto permits(uint64_t address,size_t size,uint32_t required) const noexcept -> bool {
        if (!address || !size || size > std::numeric_limits<uint64_t>::max()-address) return false;
        const auto end=address+size;auto current=address;
        const auto& image=caller_.view();
        for(size_t i=0;i<image.segment_count;++i) {
            const auto& segment=image.segments[i];const auto stop=segment.runtime_address+segment.size;
            if(stop<=current)continue;
            if(segment.runtime_address>current || (segment.permissions&required)!=required)return false;
            current=std::min(end,stop);if(current==end)return true;
        }
        return false;
    }
public:
    OwnedMappedImageCaller(const RC::Platform::NativeProgramContext& context,uint32_t format,
        uint64_t text_base,uint64_t image_base=0,PsMappedExtent eh_frame={},
        PsMappedExtent eh_frame_hdr={},PsMappedExtent pe_exception_directory={})
      :context_(context),ranges_(ranges(context_)),lease_(ranges_),
       caller_(context_,segments(lease_),format,text_base,image_base,eh_frame,eh_frame_hdr,pe_exception_directory) {
        if(!permits_code(text_base)) throw std::invalid_argument("Mapped text base must be registered readable executable bytes");
        const auto& main=context_.module_source->Views().array[static_cast<size_t>(ScanTarget::MainExe)];
        if(format==PS_MAPPED_IMAGE_PE64 && image_base!=reinterpret_cast<uintptr_t>(main.image_base))
            throw std::invalid_argument("Mapped PE base differs from retained module image");
    }
    OwnedMappedImageCaller(const OwnedMappedImageCaller&)=delete;
    auto operator=(const OwnedMappedImageCaller&)->OwnedMappedImageCaller& = delete;
    OwnedMappedImageCaller(OwnedMappedImageCaller&&)=delete;
    auto operator=(OwnedMappedImageCaller&&)->OwnedMappedImageCaller& = delete;
    // No owned-service/logging callback may run until this caller is destroyed.
    auto view()const noexcept->const PsMappedImageViewV1& { return caller_.view(); }
    auto source_owner()const noexcept->const std::shared_ptr<const ModuleSnapshot>& { return caller_.source_owner(); }
    auto permits_code(uint64_t address)const noexcept->bool { return permits(address,1,PS_MAPPED_READ|PS_MAPPED_EXECUTE); }
    auto permits_data(uint64_t address,size_t size)const noexcept->bool { return permits(address,size,PS_MAPPED_READ); }
    auto permits_pointer_slot(uint64_t address)const noexcept->bool {
        return address%alignof(void*)==0 && permits_data(address,sizeof(void*));
    }
};
}

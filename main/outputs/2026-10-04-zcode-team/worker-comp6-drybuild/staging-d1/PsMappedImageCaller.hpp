#pragma once
#include "PsScanAbi.h"
#include <Unreal/PlatformModuleSnapshot.hpp>
#include <Platform/NativeProgramContext.hpp>
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
namespace RC::Unreal::Platform {
// Retains exactly the snapshot supplied by a real caller, never discovers images.
// Geometry/declared lifetime checks only: root must pin actual owned read leases
// and derive permissions before exposing Rust slices. No permission hint authorizes.
class PsMappedImageCaller final {
    std::shared_ptr<const ModuleSnapshot> owner_;
    std::vector<PsMappedSegment> segments_;
    PsMappedImageViewV1 view_{};
    static uint64_t end(uint64_t address, size_t size) {
        if (!address || !size || size > std::numeric_limits<uint64_t>::max()-address)
            throw std::invalid_argument("Invalid supplied mapped extent");
        return address+size;
    }
    bool contains(PsMappedExtent extent) const {
        if (!extent.address && !extent.size) return true;
        if (!extent.address || !extent.size) return false;
        const auto last=end(extent.address,extent.size);auto cursor=extent.address;
        for (const auto& segment:segments_) {
            const auto finish=end(segment.runtime_address,segment.size);
            if (finish<=cursor) continue;
            if (segment.runtime_address>cursor) return false;
            cursor=std::min(last,finish);if(cursor==last)return true;
        }
        return false;
    }
public:
    PsMappedImageCaller(const RC::Platform::NativeProgramContext& context,
        std::vector<PsMappedSegment> segments, uint32_t format,
        uint64_t text_base,uint64_t image_base,PsMappedExtent eh_frame={},
        PsMappedExtent eh_frame_hdr={},PsMappedExtent pe_exception_directory={})
      : owner_(context.module_source),segments_(std::move(segments)) {
        if (!owner_ || segments_.empty() ||
            (format!=PS_MAPPED_IMAGE_ELF64 && format!=PS_MAPPED_IMAGE_PE64))
            throw std::invalid_argument("Mapped call requires snapshot, spans and known format");
        std::sort(segments_.begin(),segments_.end(),[](const auto&a,const auto&b){return a.runtime_address<b.runtime_address;});
        uint64_t previous=0;
        for (const auto& segment:segments_) {
            const auto finish=end(segment.runtime_address,segment.size);
            if (reinterpret_cast<uintptr_t>(segment.bytes)!=segment.runtime_address ||
                segment.runtime_address<previous || segment.reserved ||
                !(segment.permissions&PS_MAPPED_READ) || (segment.permissions&~uint32_t{7}))
                throw std::invalid_argument("Invalid supplied mapped span");
            previous=finish;
        }
        // Permit real page-permission splits, requiring exact supplied coverage.
        const auto& ranges=owner_->ReadableRanges().at(ScanTarget::MainExe);
        size_t index=0;
        for (const auto& range:ranges) {
            auto cursor=reinterpret_cast<uintptr_t>(range.data);
            const auto finish=end(cursor,range.size);
            while(cursor<finish) {
                if(index==segments_.size() || segments_[index].runtime_address!=cursor)
                    throw std::invalid_argument("Mapped spans must exactly cover supplied snapshot ranges");
                cursor=end(segments_[index].runtime_address,segments_[index].size);
                if(cursor>finish)throw std::invalid_argument("Mapped span extends beyond supplied range");
                ++index;
            }
        }
        // comp6 Diff B (worker-d2-textbase-design): PsScanAbi.h metadata rule
        // — text_base==0 is the ABI's absence spelling; containment required
        // only for a SUPPLIED text base.
        if(index!=segments_.size() ||
            (text_base!=0 && !contains({text_base,1})) ||
            !contains(eh_frame) || !contains(eh_frame_hdr) || !contains(pe_exception_directory))
            throw std::invalid_argument("Mapped metadata or spans exceed supplied readable bytes");
        if ((format==PS_MAPPED_IMAGE_ELF64 && (image_base || pe_exception_directory.address || pe_exception_directory.size)) ||
            (format==PS_MAPPED_IMAGE_PE64 && (!image_base || eh_frame.address || eh_frame.size || eh_frame_hdr.address || eh_frame_hdr.size)))
            throw std::invalid_argument("Mapped metadata does not match format");
        view_={PS_MAPPED_IMAGE_ABI_V1,format,segments_.data(),segments_.size(),text_base,image_base,eh_frame,eh_frame_hdr,pe_exception_directory};
    }
    PsMappedImageCaller(const PsMappedImageCaller&)=delete;
    PsMappedImageCaller& operator=(const PsMappedImageCaller&)=delete;
    PsMappedImageCaller(PsMappedImageCaller&&)=delete;
    PsMappedImageCaller& operator=(PsMappedImageCaller&&)=delete;
    const PsMappedImageViewV1& view()const noexcept{return view_;}
    const std::shared_ptr<const ModuleSnapshot>& source_owner()const noexcept{return owner_;}
};
}
namespace comp6_drybuild {
#define WCD_DRYBUILD_STAGING_PS_HEADER 1
extern const char WCD_PS_DIFF_B_STAGING_MARKER[] = "comp6-drybuild: PsMappedImageCaller.hpp staging override (D2 Diff B text_base-absence rule) compiled into this object";
}  // namespace comp6_drybuild (RC::Unreal::Platform closed above)

#include <OwnedMappedScanSource.hpp>
namespace RC::Unreal::Platform {
OwnedMappedScanSource::OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot> owner,OwnedMappedImageFacts facts)
  :owner_(std::move(owner)),facts_(facts) {
    // Validate using the same permission/lifetime path as the real scan.
    OwnedMappedImageCaller checked(owner_,facts_.format,facts_.text_base,facts_.image_base,
        facts_.eh_frame,facts_.eh_frame_hdr,facts_.pe_exception_directory);
}
auto OwnedMappedScanSource::collect(const PsScanConfig& config,const PsScanResults& previous) const noexcept -> OwnedMappedScanReport {
    try {
        OwnedMappedImageCaller image(owner_,facts_.format,facts_.text_base,facts_.image_base,
            facts_.eh_frame,facts_.eh_frame_hdr,facts_.pe_exception_directory);
        return collect_owned_mapped(image,config,previous);
    } catch(...) {
        return unavailable_owned_mapped(config,previous);
    }
}
auto scan_owned_source(const std::shared_ptr<const OwnedMappedScanSource>& source,
    const std::shared_ptr<const ModuleSnapshot>& expected,PsCtx* ctx,PsScanResults* results) -> bool {
    if(!ctx || !results || !ctx->default_ || !ctx->normal || !ctx->verbose || !ctx->warning || !ctx->error)
        return false;
    // Keep the source and owner alive across callbacks; startup is serialized.
    const auto retained=source;
    const auto retained_expected=expected;
    auto report=unavailable_owned_mapped(ctx->config,*results);
    if(retained && retained_expected && retained->source_owner()==retained_expected)
        report=retained->collect(ctx->config,*results);
    *results=report.results;
    // collect's scoped lease has ended before any caller callback runs.
#if defined(__PROSPERO__) || defined(_WIN32)
    PsChar16 success[]=L"Owned mapped resolver pass completed";
    PsChar16 failure[]=L"Owned mapped source or required resolver unavailable";
#else
    PsChar16 success[]=u"Owned mapped resolver pass completed";
    PsChar16 failure[]=u"Owned mapped source or required resolver unavailable";
#endif
    if(report.succeeded())ctx->normal(success);else ctx->error(failure);
    return report.succeeded();
}
}

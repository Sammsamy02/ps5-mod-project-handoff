        ops.scan_image_base =
            [](void *ctx, const lmf::OsVmMapping *mappings, size_t count,
               lmf::VmScanResult *out) -> int {
              if (out == nullptr || mappings == nullptr) return -1;
              const lmf::LiveModuleOsOps &base =
                  *static_cast<const lmf::LiveModuleOsOps *>(ctx);
              // First: the original header scan (unchanged behavior).
              if (base.scan_image_base != nullptr &&
                  base.scan_image_base(nullptr, mappings, count, out) == 0 &&
                  out->chosen_index >= 0 && out->chosen_base != 0)
                  return 0;
              // Fallback: data-window anchor (validated windows, readable
              // spans only, BOTH windows must verify at the candidate base).
              if (!window_anchor_verify(base, nullptr)) return -1;
              lmf::VmScanResult r{};
              r.chosen_index = 0;
              r.chosen_base = kCandidateBase;
              r.chosen_extent = kImageExtentVaddr;
              r.candidate_count = 1;
              r.truncated = 0;
              *out = r;
              return 0;
            };

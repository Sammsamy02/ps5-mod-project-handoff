// mock_owned_range_scanner.cpp — compiles the REAL UE4SS owned-range scanner
// (ue4ss_headers/SigScanner/OwnedRangeScanner.inl, sha256 59bd538d..., the
// exact unit the composition's "scanner" runtime unit builds from) inside a
// mocked environment, so the host test can run the genuine
// SinglePassScanner::start_scan(map, ReadableRanges()) seam that
// Signatures.cpp calls on non-Windows:
//
//     SinglePassScanner::start_scan(signature_container_map,
//                                   config.ModuleSource->ReadableRanges());
//
// Mocks (environment only — the scan algorithms are untouched, verbatim):
//   - ProfilerSetThreadName/ProfilerScope: no-op (the PRX build gets these
//     from <Profiler/Profiler.hpp>; they are observability, not semantics).
//   - The static data members normally defined by the full
//     SinglePassSigScanner.cpp translation unit.
//   - m_num_threads = 1: serialized callbacks, deterministic match order
//     (the "Callbacks are serialized" contract line of the header).
//   - m_scan_method = Scalar: the pinned scalar high/low-nibble algorithm.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <future>
#include <mutex>
#include <vector>

#include <SigScanner/SinglePassSigScanner.hpp>

namespace RC
{
    // Profiler mocks (observability only).
    inline void ProfilerSetThreadName(const char *) {}
    inline void ProfilerScope() {}

    // Static members normally defined by SinglePassSigScanner.cpp.
    ScanTargetArray SigScannerStaticData::m_modules_info{};
    bool SigScannerStaticData::m_is_modular{false};
    std::mutex SinglePassScanner::m_scanner_mutex;
    uint32_t SinglePassScanner::m_num_threads = 1;
    SinglePassScanner::ScanMethod SinglePassScanner::m_scan_method = SinglePassScanner::ScanMethod::Scalar;
    uint32_t SinglePassScanner::m_multithreading_module_size_threshold = 0x1000000;

    // The REAL owned-range scanner implementation, verbatim (textual include
    // inside namespace RC, exactly how SinglePassSigScanner.cpp includes it).
#include <SigScanner/OwnedRangeScanner.inl>
} // namespace RC

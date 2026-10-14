// live_module_base_scan.hpp
//
// In-memory ELF image-base scan over kernel-reported VM mappings. Vendored
// 2026-10-04 by worker-supplier-ingame-update from worker-ingame-probe-v2's
// V2 logic (outputs/2026-10-04-zcode-team/worker-ingame-probe-v2/src/
// experiments.c: scan_image_candidates / phdr_table_read / dyn_walk), which
// ran for real inside the game process (checkpoint zcode_ingame_v1_results).
//
// WHY THIS EXISTS (observed in-game facts, zcode_ingame_v1_results):
//   - sceKernelGetExecutableModuleHandle -> raw 0        (identity route dead)
//   - DT_DEBUG absent/unfilled, r_map 0                  (link-map route dead)
//   - sceKernelGetModuleInfo on module-list handles -> 0x7FFFFFFF, empty names
//   - KERN_PROC_VMMAP rc 0, 1252 entries                 (only proven source)
// Image identity is therefore derived from VMMAP spans + an in-memory ELF
// header/program-header scan. The ONLY reads this TU ever performs are
// span-validated: an address range is read solely after it has been proven
// to lie inside a reported readable (bit0) span, and the raw read itself is
// a caller-injected callback (host tests point it at fixtures; the SCE TU
// points it at memcpy).
//
// Self-contained by design: local minimal ELF64 definitions with the pinned
// SDK layouts (Elf64_Ehdr 64B, Elf64_Phdr 56B, Elf64_Dyn 16B), so the same
// TU compiles for host tests and for the target without SDK headers.
#ifndef LMF_LIVE_MODULE_BASE_SCAN_HPP
#define LMF_LIVE_MODULE_BASE_SCAN_HPP

#include <cstddef>
#include <cstdint>

namespace lmf {

// One kernel-reported mapping (identical layout to OsVmMapping; converted
// field-by-field by callers so the scan never depends on the ops header).
struct VmScanSpan {
    uint64_t start;       // inclusive
    uint64_t end;         // exclusive
    uint32_t protection;  // raw reported value; bit0 readable, bit2 exec (observed)
};

// Observed protection-bit assignments (worker-ingame-probe V1 histograms;
// values otherwise kept opaque).
static constexpr uint32_t kVmScanReadableBit = 1u;
static constexpr uint32_t kVmScanExecBit = 4u;

// Candidate bound carried over from the V2 probe (IG_CAND_MAX).
static constexpr size_t kVmScanCandidateCapacity = 64;

// One candidate image base. Every field is evidence: nothing is guessed,
// unreadable == 0/refused (field-for-field the V2 IgCand record).
struct VmScanCandidate {
    uint64_t base;         // span start where ELF magic was probed
    uint64_t span_start;   // owning span bounds (evidence)
    uint64_t span_end;
    uint32_t prot;
    char where[16];        // "rx_span" | "r_adjacent_exec"
    int magic_ok;          // 0x7fELF at base, span-validated read
    int class_ok;          // ELFCLASS64
    uint16_t e_type;       // raw e_type (2=EXEC, 3=DYN observed values)
    uint16_t phnum;
    int phdr_ok;           // phdr table fully span-readable, sane entsize
    uint64_t dyn_vaddr;    // PT_DYNAMIC p_vaddr (0 if none/unreadable)
    uint64_t dyn_filesz;
    int dyn_found;
    int dyn_mode;          // 0 none, 1 dyn@base+vaddr, 2 dyn@absolute vaddr
    int dyn_parses;        // dynamic walk saw entries (and DT_NULL or all-sane)
    int dyn_entries;       // entries actually scanned
    int dt_null_seen;
    int dt_debug_present;  // observed in-memory value recorded, never trusted
    uint64_t dt_debug_value;
    int has_exec_segment;  // some PT_LOAD carries PF_X
    uint64_t extent;       // max(p_vaddr+p_memsz) over PT_LOADs (vaddr space)
    int chosen;            // 1 for the single chosen main-image candidate
};

struct VmScanResult {
    uint32_t candidate_count;
    int truncated;          // candidate bound hit: further spans not probed
    int chosen_index;       // -1 = no candidate parsed (needs-base)
    uint64_t chosen_base;   // span-validated ELF header address of the chosen image
    uint64_t chosen_extent; // PT_LOAD vaddr extent of the chosen image
    uint64_t chosen_dyn_addr;  // resolved dynamic address under chosen mode
    int chosen_dyn_mode;
    uint16_t chosen_e_type;
    char provenance[24];    // "v2_elf_base_scan" | "needs-base"
};

// Raw read callback: copies len bytes from addr to dst, returns len on
// success, 0 (or < len) on failure. INVARIANT: lmf_scan_image_bases calls it
// ONLY for ranges already proven to lie inside one readable span.
using VmScanReadFn = size_t (*)(void *context, uint64_t addr, void *dst,
                                size_t len);

// Scans the span set for ELF64 image candidates and chooses the main image:
// the LARGEST executable image whose dynamic section parses (ties resolve to
// the first/lowest span). spans are read as integers; every byte read goes
// through read_fn after span validation. candidates/candidate_capacity is
// caller storage (use kVmScanCandidateCapacity).
VmScanResult lmf_scan_image_bases(const VmScanSpan *spans, size_t span_count,
                                  VmScanReadFn read_fn, void *read_context,
                                  VmScanCandidate *candidates,
                                  size_t candidate_capacity) noexcept;

}  // namespace lmf

#endif  // LMF_LIVE_MODULE_BASE_SCAN_HPP

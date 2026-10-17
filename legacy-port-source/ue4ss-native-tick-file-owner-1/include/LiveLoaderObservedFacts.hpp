#pragma once
#include <cstdint>

/* Controller-recorded loader behavior for this target, 2026-10-04, from TWO
 * environments (never mix them):
 *
 * 1) PAYLOAD ENVIRONMENT (LIVEFACTS_PROBE target run; receipt
 *    outputs/2026-10-04-zcode-controller/console/livefacts-probe-receipt.json,
 *    build lfp-84fdce4cba6a46b2ae85037cd0757548).
 * 2) IN-GAME (checkpoint zcode_ingame_v1_results; records
 *    outputs/2026-10-04-zcode-controller/console/ingame-v1-facts-2026-10-04.jsonl,
 *    build igf-f4a91d8123b0, INSIDE the running game process).
 *
 * IN-GAME facts (zcode_ingame_v1_results):
 * - g5_vmmap: KERN_PROC_VMMAP rc 0, 1252 entries, kve 1160B, 6 growth
 *   attempts to a 1452672B buffer -> the ONLY proven mapping/identity source
 *   in-game; promoted to PRIMARY route (supplier + base scan).
 * - g1_exec_handle: sceKernelGetExecutableModuleHandle raw 0x0 (errno 12)
 *   -> route DEAD in-game; supplier refuses with a distinct code.
 * - g2_dtdebug: DT_DEBUG entry absent/unfilled (value 0, r_map 0, 0 entries)
 *   -> link-map route DEAD on target; supplier refuses with a distinct code.
 * - g4_module_list: 3-arg sceKernelGetModuleList works (7 modules
 *   available), but sceKernelGetModuleInfo on those handles returns
 *   0x7FFFFFFF with empty names -> domain/ABI mismatch, UNRESOLVED; the
 *   supplier does not use this route.
 * - g7_fw_summary: kernel version string r229358/releases/13.60.
 *
 * Payload-environment facts are retained below unchanged; several were
 * REFUTED there and the in-game run has now decided the decisive cases.
 * Lifetime rule unchanged: a supplier that cannot retain an image yields zero
 * loader references; the conversion adapter decides what may be planted. */
namespace lmf {
struct LiveLoaderObservedFacts {
    // ---- payload environment (livefacts-probe, 2026-10-04) -------------
    static constexpr bool vmmap_works_payload_env = true;
    static constexpr bool dt_debug_present_payload_env = false;
    static constexpr bool rtld_noload_self_payload_env = false;
    static constexpr bool sysctl_proc_pathname_payload_env = false;
    static constexpr unsigned module_list_argc_payload_env = 3;
    static constexpr unsigned firmware_raw = 0x13600007u;  // 13.60 via crt1 (payload env)

    // ---- in-game (checkpoint zcode_ingame_v1_results, 2026-10-04) ------
    static constexpr bool vmmap_works_ingame = true;               // g5: rc 0
    static constexpr unsigned long vmmap_entries_ingame = 1252;    // g5
    static constexpr unsigned long vmmap_kve_size_ingame = 1160;   // g5
    static constexpr bool exec_handle_raw_zero_ingame = true;      // g1: raw 0 -> dead
    static constexpr bool dt_debug_present_ingame = false;         // g2: absent/unfilled
    static constexpr bool module_list_works_ingame = true;         // g4: 3-arg, rc 0
    static constexpr unsigned module_list_modules_ingame = 7;      // g4
    static constexpr bool module_info_on_list_handles_ok_ingame = false;  // g4: rc 0x7FFFFFFF, empty names
    static constexpr int64_t module_info_rc_on_list_handles_ingame = 0x7FFFFFFF;
    static constexpr char firmware_string_ingame[] = "r229358/releases/13.60";  // g7

    // ---- distinct provider refusal codes (runtime-gated demotion) ------
    // The two observed-dead routes stay in the source tree, runtime-gated
    // behind LMF_ENABLE_EXPERIMENTAL_INGAME_ROUTES, and refuse by default
    // with these DISTINCT codes so callers can cite the exact observed fact.
    // Codes are far below -1 and far apart; they never collide with errno
    // pass-throughs or the generic -1..-4 walk codes.
    static constexpr int32_t kRefusalExecHandleRawZeroIngame = -4001;  // g1
    static constexpr int32_t kRefusalDtDebugUnfilledIngame = -4002;    // g2

    // ---- VMMAP buffer sizing (primary route) ---------------------------
    // In-game 1252 entries at 1160B were observed; the SCE TU keeps a .bss
    // buffer of kVmMapBufferEntries to cover that with headroom (single-
    // threaded synchronous captures only, unchanged contract).
    static constexpr unsigned kVmMapBufferEntries = 1408;
    // Snapshot capacity for mappings (data bound; honest truncation flag
    // still records overflow). Matches the in-game probe's retained-span
    // bound (1024 of 1252 entries kept by worker-ingame-probe-v2 V1).
    static constexpr unsigned kVmMappingSnapshotCapacity = 1024;
};
}

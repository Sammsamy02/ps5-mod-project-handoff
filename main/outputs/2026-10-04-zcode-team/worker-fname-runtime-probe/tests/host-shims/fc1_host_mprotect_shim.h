/* sys_mprotect_shim.h — HOST-TEST override (worker-fname-runtime-probe).
 *
 * Selected by include-path order ONLY in the host test build
 * (-I tests/host-shims BEFORE -I src). The PRX/target build keeps the
 * REAL pinned-SDK inline-syscall shim at src/sys_mprotect_shim.h
 * (verified by cross-compile -Werror + the objdump syscall-instruction
 * gate, per the igf5 lineage convention — the raw syscall cannot run on
 * this macOS arm64 host).
 *
 * The host shim forwards to fc1_host_mprotect: a deterministic SOFTWARE
 * protection model over the synthetic test arenas (validates 16 KiB
 * page alignment, applies protection state, supports injectable failure
 * modes and a full call log) so the probe's ORDERING contract
 * (mprotect -> read -> restore) and refusal behavior are exercised
 * end-to-end without kernel behavior.
 */
#ifndef IGFC1_HOST_SYS_MPROTECT_SHIM_H
#define IGFC1_HOST_SYS_MPROTECT_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

long fc1_host_mprotect(const void *addr, size_t len, int prot);

static inline long igcar_sys_mprotect(const void *addr, size_t len, int prot) {
    return fc1_host_mprotect(addr, len, prot);
}

#ifdef __cplusplus
}
#endif

#endif /* IGFC1_HOST_SYS_MPROTECT_SHIM_H */

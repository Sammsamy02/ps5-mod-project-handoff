/*
 * runtime_start_bridge.h — C-callable API over the composed runtime's C++
 * startup chain (composition-4 object, sha256 45dd9901…075af5).
 *
 * This is the controller-owned wiring that composition-4 lacked: the ONLY
 * bridge between the C module_start lifecycle and
 *   lmf::RetainedLiveModuleFactsSupplier  (loader facts capture, VMMAP-first)
 *   RC::Platform::TryMakeNativeLoaderFactsFromSupplier
 *   RC::Platform::MakeNativeProgramContext
 *   modbridge::native::StartActivatedRuntimeWithAck
 *     (= InstallDefaultEngineBindings + MakeRuntimeAckConfig +
 *        PrepareActivatedUE4SSRuntime + RuntimeOwner::start)
 *
 * Every symbol the C++ side calls is DEFINED in the composed runtime object
 * (runtime.map run-3, MEASURED 2026-10-04). This TU adds no new OS imports.
 *
 * Threading: module_start/module_stop are serialized by the entry_guard in
 * runtime_module_entry.c, which is the only intended caller. The single
 * RuntimeOwner lives in a function-local static of the bridge for the process
 * lifetime (the activation contract: kept, never re-read).
 *
 * C99-visible. Compiled as C++ on target (prospero-clang++); the C entry only
 * ever sees this header.
 */
#ifndef MODBRIDGE_RUNTIME_START_BRIDGE_H
#define MODBRIDGE_RUNTIME_START_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque activation type (modforge_activation.h). */
struct mfa_activation;

/* Result codes returned by modbridge_runtime_start_with_activation. */
#define MRS_OK             0  /* owner started; runtime continues on its init thread */
#define MRS_ERR_CONVERSION 1  /* loader-facts capture/conversion refused (honest gate) */
#define MRS_ERR_CONTEXT    2  /* NativeProgramContext build/validation threw */
#define MRS_ERR_START      3  /* prepare/start threw or returned null */
#define MRS_ERR_INTERNAL   4  /* defensive: bad arguments / already started */
/* r13 (review P3-4): renumbered 2->5; the old value collided with
 * MRS_ERR_CONTEXT. Consumers only test zero/nonzero (the entry compares
 * MRS_OK / MRS_ERR_START at start and '!= 0' at stop); re-baseline any
 * future log reader that wants the exact code. */
#define MRS_ERR_STOP_EXN   5  /* modbridge_runtime_stop: exception escaped stop */

/*
 * Build the loader context from live module facts and start the composed
 * runtime with `activation`.
 *
 *   module_file_path  absolute normalized path of THIS PRX as staged
 *                     (ValidateNativeProgramContext requires an existing
 *                     regular file — /data is still reachable here).
 *   mod_root          the EXACT mod root string (WIRING-SPEC S3). Becomes
 *                     RuntimeAckConfig::status_root verbatim.
 *   title_id          expected title (load list must match it).
 *   activation        raw mfa_activation from mfa_activate. OWNERSHIP IS
 *                     ALWAYS TRANSFERRED on non-NULL input: the bridge adopts
 *                     it into ActivatedMods (ActivationDeleter -> mfa_release)
 *                     and releases it on EVERY exit path, success or failure.
 *                     The caller must NEVER mfa_release it afterwards.
 *   detail            optional buffer; receives a bounded printable-ASCII
 *                     failure reason ("" on success).
 *
 * Returns an MRS_* code.
 */
int modbridge_runtime_start_with_activation(const char *module_file_path,
                                            const char *mod_root,
                                            const char *title_id,
                                            struct mfa_activation *activation,
                                            char *detail,
                                            unsigned detail_capacity);

/*
 * Loader-control-thread stop (RUNTIME-ACK point 5): owner->stop() joins the
 * init thread and emits at most one 'stopped' line; the owner is then
 * destroyed (closing the retained status descriptor). Returns 0 when stopped
 * or when no owner exists; nonzero when stop() reported failure.
 */
int modbridge_runtime_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBRIDGE_RUNTIME_START_BRIDGE_H */

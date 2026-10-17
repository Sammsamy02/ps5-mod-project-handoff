// CAP13 managed route — frozen v1 managed<->native ABI.
// SOURCE STAGE ONLY: never compiled here, never applied to any tree.
// This header is the single shared contract between:
//   - the managed side (Mbm.ModSdk.cs P/Invokes, "__Internal", Cdecl, ExactSpelling)
//   - the native dl-fallback provider (MbmManagedBridge.cpp, exact-name matching)
//   - the discovery ImplMap whitelist (DESIGN/managed-dll-route.md section 2c.4)
// Growth policy: additions only, with an ABI version bump. Existing symbols
// never change shape. The provider answers ONLY symbols in the frozen set;
// any other "__Internal" entry point in an admitted image is refused at
// admission by name (precise diagnostic, not a mid-game DllNotFoundException).
#ifndef MBM_MANAGED_ABI_H
#define MBM_MANAGED_ABI_H

#define MBM_CS_ABI_VERSION 1

// i32 status codes. Negative = precise failure; 0 = success. Out-params are
// written only on success. No C++ or managed exception ever crosses this
// boundary (cap10 InvocationBoundary discipline).
#define MBM_CS_OK                    0
#define MBM_CS_BAD_ARGUMENT         -1  // null/out-of-range arguments
#define MBM_CS_UNSUPPORTED          -2  // symbol admitted but feature refused
#define MBM_CS_NOT_FOUND            -3  // named object/property/key missing
#define MBM_CS_MARSHAL              -4  // string/handling conversion failure
#define MBM_CS_ENGINE_UNBOUND       -5  // engine surface not bound (fail-closed)
#define MBM_CS_BUFFER_TOO_SMALL     -6  // caller buffer insufficient; *len = needed
#define MBM_CS_HOST_NOT_READY       -7  // resident host not in steady state

// mbm_cs_log levels (map onto the #9 host diagnostic_event severities).
#define MBM_LOG_TRACE 0
#define MBM_LOG_INFO  1
#define MBM_LOG_WARN  2
#define MBM_LOG_ERROR 3

// mbm_cs_host_query keys.
#define MBM_QUERY_TICK_COUNT 1  // i64 out: engine ticks since host start
#define MBM_QUERY_MOD_COUNT  2  // i64 out: admitted mod assemblies
#define MBM_QUERY_BOOT_ID    3  // i64 out: monotonic resident-host boot serial

// Frozen symbol set (C linkage on the native side):
//   int32_t mbm_cs_abi_version(void);
//   int32_t mbm_cs_log(int32_t level, const char* utf8_message);
//   int32_t mbm_cs_host_query(int32_t key, int64_t* out_value);
//   int32_t mbm_cs_mod_assembly(int32_t index, char* buffer, int32_t* len_inout);
//   int32_t mbm_cs_engine_find_object(const char* object_path, void** out_handle);
//   int32_t mbm_cs_engine_read_number(void* handle, const char* property_name,
//                                     double* out_value);
//   int32_t mbm_cs_engine_release(void** handle);
//
// Buffer contract (mbm_cs_mod_assembly): caller passes buffer + capacity via
// *len_inout; on MBM_CS_BUFFER_TOO_SMALL the required length (including NUL)
// is written back to *len_inout. Names are NUL-terminated ASCII
// (assembly short names; no paths — native owns all file access).

// Engine handle discipline (v1, read-only): handles are opaque native-side
// tokens owned by the engine-surface binding; NULL is never a valid handle;
// every find is paired with exactly one release; no handle outlives the
// shutdown phase. No engine writes exist in v1 by design.

#endif // MBM_MANAGED_ABI_H

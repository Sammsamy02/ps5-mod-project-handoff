#pragma once
#ifdef __cplusplus
extern "C" {
#define RC_NATIVE_RETIRE_NOEXCEPT noexcept
#else
#define RC_NATIVE_RETIRE_NOEXCEPT
#endif
// In-link ABI: 0=initialized owner retired/absent, 1=busy/self-wait refusal,
// -1=unknown before/after C++ lifetime. Every nonzero result retains resources.
int bridge_native_thread_authority_retire(void) RC_NATIVE_RETIRE_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef RC_NATIVE_RETIRE_NOEXCEPT

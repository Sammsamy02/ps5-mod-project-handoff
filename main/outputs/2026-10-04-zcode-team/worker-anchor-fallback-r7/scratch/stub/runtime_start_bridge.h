#pragma once
#include <cstdint>
struct mfa_activation;
enum { MRS_OK = 0, MRS_ERR_INTERNAL = 1, MRS_ERR_CONVERSION = 2, MRS_ERR_CONTEXT = 3, MRS_ERR_START = 4, MRS_ERR_STOP_EXN = 5 };
extern "C" int modbridge_runtime_start_with_activation(const char *, const char *, const char *, struct mfa_activation *, char *, unsigned);
extern "C" int modbridge_runtime_stop(void);

#include "call_exports.h"
#if !defined(__x86_64__)
#error "Owned fixture calls require AMD64"
#endif
typedef int32_t (__attribute__((ms_abi)) *exercise_fn)(uint64_t *);
typedef int32_t (__attribute__((ms_abi)) *unload_fn)(void *);
typedef void *(__attribute__((ms_abi)) *lookup_fn)(void);
int32_t mb_owned_call_exercise(void *address,uint64_t results[4]) {return ((exercise_fn)address)(results);}
int32_t mb_owned_call_unload(void *address,void *module) {return ((unload_fn)address)(module);}
void *mb_owned_call_lookup(void *address) {return ((lookup_fn)address)();}

#ifndef MB_OWNED_CALL_EXPORTS_H
#define MB_OWNED_CALL_EXPORTS_H
#include <stdint.h>
/* Native callbacks calling only the exact owned fixture export signatures. */
int32_t mb_owned_call_exercise(void *address,uint64_t results[4]);
int32_t mb_owned_call_unload(void *address,void *module);
void *mb_owned_call_lookup(void *address);
#endif

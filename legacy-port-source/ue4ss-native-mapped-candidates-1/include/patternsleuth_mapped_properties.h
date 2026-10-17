#ifndef PATTERNSLEUTH_MAPPED_PROPERTIES_H
#define PATTERNSLEUTH_MAPPED_PROPERTIES_H
#include "patternsleuth_mapped_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Same valid nonaliasing immutable caller-borrow contract as mapped_core.
 * property_id1 BuildConfiguration:0unknown,1Shipping,2Development,3Test.
 * property_id2 Stats:0unknown,1Off,2On. Off is a resolved enum, not unknown.
 * Exact upstream marker gates/costs/minimum-count/margin selection apply.
 * Status0resolved,1arguments,2descriptor,3insufficient/inconclusive evidence.
 * A successful property classifies supplied bytes only, not runtime identity. */
typedef struct PsMappedPropertyResult { uint32_t value;uint32_t reserved; } PsMappedPropertyResult;
int ps_resolve_mapped_property_v1(const PsMappedImageViewV1*,uint32_t,PsMappedPropertyResult*);
#ifdef __cplusplus
}
#endif
#endif

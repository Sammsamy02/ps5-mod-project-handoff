/* axes_probe_glue.h — in-probe axes experiment entry (see axes_probe_glue.c). */
#ifndef IG_AXES_PROBE_GLUE_H
#define IG_AXES_PROBE_GLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct IgSink;  /* record_sink.h */

/* Runs the axes detector over [image_base, image_base + image_extent) and
 * writes one exp="axes" record. Returns 0 on record written, -1 on sink
 * failure. seq continues the probe's record sequence. */
int ig_axes_experiment(struct IgSink *sink, int seq, const char *build_id,
                       uint64_t image_base, uint64_t image_extent);

#ifdef __cplusplus
}
#endif
#endif /* IG_AXES_PROBE_GLUE_H */

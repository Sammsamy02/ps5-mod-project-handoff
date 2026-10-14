/* fc1_host_env.h — host test environment API (worker-fname-runtime-probe). */
#ifndef IGFC1_HOST_ENV_H
#define IGFC1_HOST_ENV_H

#include "fc1_probe.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t addr;
    uint64_t len;
    int prot;
    int rc;
} Fc1HostCall;
extern Fc1HostCall fc1_host_calls[];
extern int fc1_host_ncalls;

/* Rebuild both arenas fresh; reset injections + call log; set map trust. */
void fc1_host_reset(int map_trusted);

/* The env the probe should run against (built by fc1_host_reset). */
const Fc1Env *fc1_host_env(void);

/* Injection + inspection. */
int fc1_host_fail_next_restore(int count);
int fc1_host_fail_next_mprotect(int count);
int fc1_host_exec_prot(void);
uint64_t fc1_host_exec_base(void);

/* Runtime-drift corruption helpers. */
void fc1_host_corrupt_target(int byte_off, uint8_t xor_mask);
void fc1_host_corrupt_site_edx_imm(int site, uint8_t new_imm);
void fc1_host_corrupt_site_call_target(int site, uint64_t new_target);
void fc1_host_corrupt_site_unsupported(int site);

#ifdef __cplusplus
}
#endif

#endif /* IGFC1_HOST_ENV_H */

/* mfw8_caps.h — honest capability report builder.
 *
 * worker-8-app-backend (2026-10-04). Emits a bounded JSON report of what is
 * ACTUALLY wired in this process, never what the product ambition is:
 *
 *   - records store state + per-state counts (proven records.txt store)
 *   - installer composition present/absent (mfw8_actions probe)
 *   - download stage link state (flavor-dependent: worker-e2e-final proved the
 *     host wiring; the console payload links the download TU only after the
 *     worker-ps5io-rdwr relink — the caller must say which flavor it is)
 *   - runtime state derived ONLY from the runtime ack file
 *     (<roots_base>/<title>/runtime-status.txt lines
 *     "state\tseq\tstate\tcode\tdetail", modinst.c read_runtime_status):
 *     running=1 requires the LAST ack state to be exactly "mods-started".
 *     No ack file -> state "not-started", running 0. Never synthesized.
 *   - framework capability table passthrough (state/caps.tsv rows
 *     "<id>\t<maturity>" with maturity available|integrating|planned,
 *     worker-7 seeds from engines/unreal.json facts)
 *   - installation_supported is reported false unless the caller passes
 *     explicit evidence flags; it NEVER defaults to true.
 *
 * Bounds: C11; output buffer caller-owned. */
#ifndef MFW8_CAPS_H
#define MFW8_CAPS_H

#include <stddef.h>
#include "mfw8_records.h"

#define MFW8_CAPS_OUTPUT_MAX 16384
#define MFW8_CAPS_FRAMEWORKS_MAX 64

typedef struct mfw8_caps_framework {
    char id[MFW8_TOKEN_MAX + 1];
    char maturity[32];            /* available | integrating | planned */
} mfw8_caps_framework;

typedef struct mfw8_caps_input {
    const char *state_dir;
    const char *roots_base;
    const char *installer_path;
    const char *title;            /* optional: include per-title runtime state */
    int console_flavor;           /* 1 = console payload target, 0 = host */
    int download_linked;          /* caller-asserted link state of the stage-1 TU */
    int install_evidence;         /* caller-asserted console install+ack evidence exists */
} mfw8_caps_input;

typedef struct mfw8_runtime_state {
    char state[32];               /* not-started | activated | engine-ready | mods-started | failed | stopped */
    char detail[192];
    int running;                  /* 1 iff last ack state == "mods-started" */
    int ack_present;
} mfw8_runtime_state;

/* Read the runtime ack file for one title. Missing file = not-started. */
int mfw8_runtime_probe(const char *roots_base, const char *title, mfw8_runtime_state *out);

/* Load caps.tsv from state_dir (missing file = empty table, not an error). */
int mfw8_caps_table_load(const char *state_dir, mfw8_caps_framework *out, int max, int *nout);

/* Build the JSON report. Returns 0 ok, -1 output truncated / bad input. */
int mfw8_caps_report(const mfw8_caps_input *in, char *output, size_t cap);

#endif

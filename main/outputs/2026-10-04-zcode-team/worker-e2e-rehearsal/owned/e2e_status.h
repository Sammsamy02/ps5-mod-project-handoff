/*
 * e2e_status.h — bounded JSONL status interface for the composed installer
 * payload, mapped to CONSOLE-E2E-PLAN.md stages.
 *
 * Origin: worker-installer-composition, 2026-10-04 (owned).
 *
 * One JSON object per line, appended to <state dir>/e2e-status.jsonl:
 *   {"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
 *   {"event":"end","exit":0}
 *   {"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"..."}
 * Bounds: at most 512 lines and 262144 bytes; past either bound exactly one
 * {"event":"truncated"} line is emitted and nothing more is written. A
 * command that dies or exits mid-way simply has no "end" line — the absence
 * is the crash signal (the same honesty rule the runtime status file uses).
 * Detail text is sanitized: anything outside [32,126] plus '"' and '\\'
 * becomes '_'; fields are fixed-size, no dynamic allocation.
 *
 * Stage mapping (CONSOLE-E2E-PLAN.md):
 *   records          -> stage-0-baseline
 *   register-mod     -> stage-1-download
 *   prepare          -> stage-2-prepare
 *   prepare-archive  -> stage-2-prepare
 *   install          -> stage-3-install (also covers stage-6 uninstall runs;
 *                       the run's --remove intent distinguishes them)
 *   simulate-runtime -> stage-4-5-runtime-ack (host simulation only; on
 *                       console the real runtime writes runtime-status.txt)
 *   recover          -> recovery
 */
#ifndef E2E_STATUS_H
#define E2E_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Must be called once (from the composed main) with the resolved state dir
 * before e2e_status_begin; copies the path. */
void e2e_status_set_dir(const char *state_dir);

void e2e_status_begin(const char *cmd, const char *title);
void e2e_status_end(int exit_code);
void e2e_status_event(const char *stage, const char *outcome, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* E2E_STATUS_H */

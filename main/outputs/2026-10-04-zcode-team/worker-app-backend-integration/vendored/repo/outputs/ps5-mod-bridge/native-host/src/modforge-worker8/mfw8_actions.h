/* mfw8_actions.h — prepare/install/uninstall/cancel job actions delegating to
 * the PROVEN composed installer CLI (modinst_composed lineage).
 *
 * worker-8-app-backend (2026-10-04). Every action shells the composed binary
 * with its documented argument surface (worker-installer-composition +
 * worker-e2e-final + worker-zip-to-mfb patch chain) and maps the PROVEN exit
 * contract (modinst.c:54-60: 0 ok, 1 io, 2 bad request, 3 not ready,
 * 4 needs choices, 5 cancelled) onto vendor SERVICE.md job/error semantics.
 *
 * Delegation, not reimplementation: this module NEVER mutates mod state on its
 * own. The durable transitions happen inside the installer transaction (root
 * swap + journal + records.txt commit). The caller re-reads records.txt after
 * each action (mfw8_records) — that read is the only state authority.
 *
 * Cancel semantics (proven): cooperative safe points only, via the installer's
 * --cancel-at pre-stage|post-stage|swap-journal|rename1 (modinst.c:91-93,
 * 1046-1089); a cancelled install leaves the previous installed set exactly
 * as it was (vendor SERVICE.md:72-73). A queued action that has not started
 * can be refused outright via mfw8_cancel_pending(). There is no hard
 * interrupt of a running child; that is the installer's own documented model.
 *
 * Console note (honest): the POSIX fork/exec driver below is the HOST driver.
 * The console payload process model does not fork; wiring the same action
 * surface to an in-process or payload-launched installer instance is a
 * controller-owned integration step (PATCHES.md documents the seam). The
 * action mapping, exit contract and records semantics are identical.
 *
 * Bounds: C11; POSIX used only by the host driver implementation. */
#ifndef MFW8_ACTIONS_H
#define MFW8_ACTIONS_H

#include <stddef.h>

#define MFW8_OUTCOME_OUTPUT_MAX 8192
#define MFW8_CANCEL_POINTS 4

typedef enum mfw8_action_kind {
    MFW8_ACT_REGISTER = 0,
    MFW8_ACT_PREPARE,
    MFW8_ACT_INSTALL,
    MFW8_ACT_UNINSTALL,
    MFW8_ACT_RECOVER
} mfw8_action_kind;

typedef struct mfw8_actions_config {
    const char *installer_path;   /* composed modinst binary */
    const char *state_dir;        /* --state */
    const char *roots_base;       /* --roots */
} mfw8_actions_config;

typedef struct mfw8_outcome {
    int exit_code;                /* raw installer exit code */
    int cancelled;                /* exit code 5 */
    const char *state;            /* "succeeded" | "failed" | "cancelled" */
    const char *error;            /* vendor SERVICE.md error code or "" */
    char detail[256];             /* first matched service error detail line */
    char output[MFW8_OUTCOME_OUTPUT_MAX]; /* captured stdout (bounded, NUL-term) */
} mfw8_outcome;

typedef struct mfw8_install_spec {
    const char *title;
    const char *mods_csv;         /* NULL = whole selected set */
    const char *remove_csv;       /* uninstall list (install --remove) */
    const char *choices_csv;      /* "group=option,..." or NULL */
    const char *cancel_at;        /* proven safe point or NULL */
} mfw8_install_spec;

typedef struct mfw8_prepare_spec {
    const char *title;
    const char *mods_csv;
    const char *choices_csv;
} mfw8_prepare_spec;

typedef struct mfw8_archive_spec {
    const char *title;
    const char *archive_path;     /* downloaded original (e.g. intake/<id>.zip) */
    const char *quarantine_dir;   /* intake quarantine dir */
    const char *mod_id;           /* when set: auto-bridge marks prepared */
} mfw8_archive_spec;

typedef struct mfw8_register_spec {
    const char *id;
    const char *title;
    const char *bundle_path;      /* kind-1 mfb (may be produced by auto-bridge) */
} mfw8_register_spec;

/* All actions return 0 when the child ran and produced a classified outcome
 * (inspect outcome->state / exit_code), -1 when the action could not be
 * attempted (bad config, spawn failure) with errno-class message in
 * outcome->error = "io_error". outcome must be zero-initialized. */
int mfw8_act_register(const mfw8_actions_config *, const mfw8_register_spec *, mfw8_outcome *);
int mfw8_act_prepare(const mfw8_actions_config *, const mfw8_prepare_spec *, mfw8_outcome *);
/* Stage-2a archive preparation (proven auto-bridge semantics, zip-to-mfb):
 * without mod_id the run is reporting-only; with it, success performs the
 * durable downloaded -> prepared transition and composes bundles/<id>.mfb. */
int mfw8_act_prepare_archive(const mfw8_actions_config *, const mfw8_archive_spec *,
                             mfw8_outcome *);
int mfw8_act_install(const mfw8_actions_config *, const mfw8_install_spec *, mfw8_outcome *);
int mfw8_act_uninstall(const mfw8_actions_config *, const char *title,
                       const char *remove_csv, mfw8_outcome *);
int mfw8_act_recover(const mfw8_actions_config *, mfw8_outcome *);

/* Returns the proven cancel safe point name at index i, NULL past the end. */
const char *mfw8_cancel_point(int i);

/* Installer binary presence/executability probe (capability reporting). */
int mfw8_installer_present(const mfw8_actions_config *);

#endif

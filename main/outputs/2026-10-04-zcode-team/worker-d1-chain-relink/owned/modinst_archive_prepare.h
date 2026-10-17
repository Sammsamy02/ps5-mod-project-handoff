/*
 * modinst_archive_prepare.h — E2E stage 2a command for the composed installer.
 *
 * Origin: worker-installer-composition, 2026-10-04 (owned).
 *
 * `prepare-archive` turns a downloaded archive into the installer's
 * "prepared" artifact (quarantine + journal) through the REAL pinned intake:
 * ps5_prepare_archive (vendored, unmodified worker-installer-ps5io bridge)
 * over the pinned libarchive 3.8.7 closure exactly as the archive module's
 * verify.py assembles it. On console the four intake functions come from the
 * statically linked closure; on host from the host closure.
 *
 * HONEST LIMIT (recorded in REPORT.md): this command does not yet write the
 * downloaded->prepared transition into worker-7's records line store; it
 * reports through stdout and the bounded e2e JSONL. Wiring it into the
 * records store is a remaining gap before the console E2E run.
 */
#ifndef MODINST_ARCHIVE_PREPARE_H
#define MODINST_ARCHIVE_PREPARE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Parses --archive FILE --quarantine DIR [--title T] from argv (main's flag
 * loop is left untouched). Returns the process exit code for the command. */
int mi_cmd_prepare_archive(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* MODINST_ARCHIVE_PREPARE_H */

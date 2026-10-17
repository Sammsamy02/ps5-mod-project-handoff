/* mfw8_records.h — install-record store over the PROVEN records.txt format.
 *
 * worker-8-app-backend (2026-10-04). This module maps the app side onto the
 * durable record semantics that are already proven end-to-end by the team
 * composition lineage (worker-7-installer -> worker-installer-composition ->
 * worker-e2e-final -> worker-zip-to-mfb). It is a READER/WRITER for that
 * exact format, not a new format:
 *
 *   modforge-records\t1
 *   mod\t<id>\t<title>\t<state>\t<bundle|->          (modinst.c records_save 340-362)
 *   variant\t<mod>\t<group>\t<option>\t<bundle>\t<def>
 *   choice\t<mod>\t<group>\t<option>
 *   game\t<title>\t<engine>\t<sdk>\t<generation>\t<loadlist_sha>
 *
 * Semantics that are PROVEN and therefore mandatory here:
 *   - states are exactly downloaded | prepared | installed (modinst.c:1176,1225,844)
 *   - the only forward transition store-side is downloaded -> prepared; a
 *     second prepare of a prepared record is idempotent; anything else refuses
 *     (worker-e2e-final REPORT §2, checks B1-B4b)
 *   - installed -> prepared on uninstall (modinst.c:844-846,1142-1144)
 *   - "running" is NEVER stored; it is derived only from the runtime ack file
 *     (modinst.c read_runtime_status 1240-1263; vendor SERVICE.md:53,190-191)
 *   - tokens are printable ASCII 0x21..0x7e, 1..128 bytes (is_token, 218-226)
 *   - writes are atomic (tmp + rename; atomic_write at modinst.c:361)
 *
 * Sidecar (worker-8's own file, NOT part of the proven format): identity.tsv
 *   download_id \t title \t provider \t domain \t mod_id \t file_id
 * Written only by mod.register_download, required so records.list can emit the
 * source{provider,domain,mod_id,file_id} shape that the app's
 * mod_service/register_bridge.cpp record() validation (214-221) demands.
 * records.txt stays byte-format identical to the proven lineage so the
 * composed installer keeps reading it unchanged.
 *
 * Bounds: C11, no dynamic global state, all storage caller-owned. */
#ifndef MFW8_RECORDS_H
#define MFW8_RECORDS_H

#include <stddef.h>

#define MFW8_MODS_MAX 256
#define MFW8_VARS_MAX 512
#define MFW8_GAMES_MAX 64
#define MFW8_TOKEN_MAX 128
#define MFW8_PATH_MAX 4096
#define MFW8_ID_MAX 67            /* dl- + 64 hex, register_bridge.cpp:48-52 */
#define MFW8_RECORDS_HEADER "modforge-records\t1"

typedef enum mfw8_state {
    MFW8_DOWNLOADED = 0,
    MFW8_PREPARED = 1,
    MFW8_INSTALLED = 2
} mfw8_state;

typedef struct mfw8_mod {
    char id[MFW8_TOKEN_MAX + 1];
    char title[MFW8_TOKEN_MAX + 1];
    char state[16];               /* canonical names, validated on load */
    char bundle[MFW8_PATH_MAX];   /* "-" when none */
} mfw8_mod;

typedef struct mfw8_var {
    char mod[MFW8_TOKEN_MAX + 1], group[MFW8_TOKEN_MAX + 1], option[MFW8_TOKEN_MAX + 1];
    char bundle[MFW8_PATH_MAX];
    int is_default;
} mfw8_var;

typedef struct mfw8_choice {
    char mod[MFW8_TOKEN_MAX + 1], group[MFW8_TOKEN_MAX + 1], option[MFW8_TOKEN_MAX + 1];
} mfw8_choice;

typedef struct mfw8_game {
    char title[MFW8_TOKEN_MAX + 1], engine[MFW8_TOKEN_MAX + 1], sdk[MFW8_TOKEN_MAX + 1];
    long generation;
    char loadlist_sha[65];
} mfw8_game;

typedef struct mfw8_records {
    mfw8_mod mods[MFW8_MODS_MAX];         int nmods;
    mfw8_var vars[MFW8_VARS_MAX];         int nvars;
    mfw8_choice choices[MFW8_VARS_MAX];   int nchoices;
    mfw8_game games[MFW8_GAMES_MAX];      int ngames;
    char error[160];                      /* diagnostics for load failures */
} mfw8_records;

typedef struct mfw8_identity {
    char download_id[MFW8_ID_MAX + 1];
    char title[MFW8_TOKEN_MAX + 1];
    char provider[65];
    char domain[81];
    char mod_id[21];              /* canonical decimal, <= INT64_MAX */
    char file_id[21];
} mfw8_identity;

/* Load records.txt from state_dir. A missing file yields an empty store (0).
 * Returns 0 ok, -1 I/O error, -2 malformed content (store refuses to guess;
 * error[] says why). Unknown row kinds or wrong field counts are malformed. */
int mfw8_records_load(mfw8_records *recs, const char *state_dir);

/* Atomic store: write records.txt.tmp in state_dir, fsync file, rename,
 * fsync directory. Returns 0 ok, -1 I/O error. */
int mfw8_records_store(const mfw8_records *recs, const char *state_dir);

const char *mfw8_state_name(const char *state); /* passthrough for valid names */
int mfw8_state_valid(const char *state);
int mfw8_token_valid(const char *s);            /* proven is_token rule */
int mfw8_state_enum(const char *state);         /* -> mfw8_state, -1 unknown */

/* Finders (NULL when absent). */
const mfw8_mod *mfw8_find_mod(const mfw8_records *recs, const char *id);
const mfw8_game *mfw8_find_game(const mfw8_records *recs, const char *title);

/* Transitions. All return 0 applied, 1 no-op (idempotent), -1 refused
 * (records->error says why). Refused transitions never touch storage. */
int mfw8_register_download(mfw8_records *recs, const char *id, const char *title,
                           const char *bundle_or_null);
int mfw8_mark_prepared(mfw8_records *recs, const char *id, const char *engine,
                       const char *sdk);
int mfw8_mark_installed(mfw8_records *recs, const char *const *ids, int nids,
                        const char *title, long generation, const char *loadlist_sha);
int mfw8_mark_uninstalled(mfw8_records *recs, const char *id);
int mfw8_forget(mfw8_records *recs, const char *id);

/* Identity sidecar: <state_dir>/identity.tsv. */
int mfw8_identity_load(const char *state_dir, mfw8_identity *out, int max, int *nout,
                       char *error, size_t error_cap);
int mfw8_identity_put(const char *state_dir, const mfw8_identity *id);
int mfw8_identity_get(const char *state_dir, const char *download_id, mfw8_identity *out);

#endif

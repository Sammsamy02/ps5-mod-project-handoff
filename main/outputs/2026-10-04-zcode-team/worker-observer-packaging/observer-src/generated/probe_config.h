#pragma once
#define PROBE_TITLE "PPSA33286"
/* New revision log path: distinct from the proven
   /data/townfall_runtime_aligned_entry.log so both modules can be staged
   alongside each other without interleaving each other's lines. */
#define PROBE_LOG_PATH "/data/townfall_runtime_aligned_observer.log"
#define PROBE_MARKER "modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556"
/* Observation targets. This module only ever READS these paths; it never
   writes, truncates, or creates them. */
#define OBSERVER_UE4SS_LOG_PATH "/data/plugins/UE4SS.log"
#define OBSERVER_MOD_ROOT_PATH "/data/modbridge/roots/PPSA33286/Mods/TownfallItemSpawner"
#define OBSERVER_MOD_ROOT_UE4SS_PATH "/data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner"
/* ~2 s cadence for up to ~60 s: 30 polls x 2 s sleep. */
#define OBSERVER_POLL_SECONDS 2
#define OBSERVER_POLL_COUNT 30u
/* Bounded tail read: one pread of at most this many bytes ending at EOF. */
#define OBSERVER_TAIL_BYTES 512u
/* Fallback forward-drain cap for the console-proven read() row. */
#define OBSERVER_TAIL_DRAIN_LIMIT (1u << 20)
#define OBSERVER_READ_EINTR_LIMIT 8u
/* Max sanitized tail text embedded in one observer log line. */
#define OBSERVER_TAIL_TEXT_MAX 256u

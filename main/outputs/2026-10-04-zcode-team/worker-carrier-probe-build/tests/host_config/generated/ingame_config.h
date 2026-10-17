// GENERATED-analog for HOST TESTS ONLY — tests/host_config/ingame_config.h
// (worker-carrier-probe-build, team 2026-10-04). The target build generates
// its own ingame_config.h via build_carrier_probe.py from the PINNED labels;
// this host copy carries the SAME label constants with host paths, and the
// runtime anchor moved above 4 GiB (macOS arm64 mmap constraint; the
// fname-runtime-probe / fc1pool1 lineage precedent). Labels are
// byte-identical facts:
//   GUA label 0x93ba110, pool label 0x9365ec0, init-flag label 0x9365ea0,
//   expected base 0x400000 (checkpoint zcode_v3_MAPPING_BREAKTHROUGH),
//   pool runtime = base + 0x9365ec0 = 0x9765ec0.
#pragma once
#define IG_BUILD_ID "cc1-host"
#define IG_LOG_PATH "/tmp/cc1-host/facts.jsonl"
#define IG_UE4SS_LOG_PATH "/tmp/cc1-host/UE4SS.log"
#define IG_VADDR_GUOBJECT_ARRAY 0x93ba110ull
#define IG_RT_GUOBJECT_ARRAY 0x97ba110ull
#define IG_VADDR_FNAME_POOL 0x9365ec0ull
#define IG_RT_FNAME_POOL 0x9765ec0ull
#define IG_VADDR_INIT_FLAG 0x9365ea0ull
#define IG_IMAGE_BASE 0x400000ull
#define IG_EXPECTED_BASE 0x400000ull
#define IG_EXEC_SPAN_START 0x400000ull
#define IG_EXEC_SPAN_END 0x787c000ull
#define IG_PAGE_SIZE 0x4000ull

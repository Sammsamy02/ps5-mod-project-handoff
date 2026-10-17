Planted-failure proof for cap09/tests/run.sh (Mac-only, scratch copy of the team folder, 2026-10-13).
Planted on the COPY only (the real folder is unchanged):
  1. test_host_core.cpp: one extra c.ok(false, "PLANTED failure")      -> test_host_core exit=1 failed=1
  2. api_usage_check.py: exits 0 but prints items_checked=0             -> suite FAILS on zero count
  3. NativeModHostCore.hpp: the reviewed form 'inline thread_local std::vector<uint64_t>' re-added and used
        -> check_sources lint FAIL, target_compile_check FAIL: new undefined __cxa_thread_atexit (the reviewer's B2)
run.sh exit=1, TOTAL result=FAIL (check_sources api_usage_check test_host_core target_compile_check). See run.sh.out.
The cap09 run state the real folder was then rerun to is in ../SUMMARY.txt (PASS).

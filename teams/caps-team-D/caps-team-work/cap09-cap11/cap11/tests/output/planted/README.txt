Planted-failure proof for cap11/tests/run.sh (Mac-only, scratch copy of the team folder, 2026-10-13).
Planted on the COPY only:
  1. NativeGuiHost.hpp: the reviewed (pre-fix) behaviour restored (block sent at open time, no
     atlas/block redo after a surface rebuild)  -> test_gui_host exit=1, failed=9 (all new F1/F2 checks)
  2. lint_patch.py: exits 0 but prints items_checked=0 -> suite FAILS on zero count
run.sh exit=1, TOTAL result=FAIL (lint_patch test_gui_host). See run.sh.out and test_gui_host.log.

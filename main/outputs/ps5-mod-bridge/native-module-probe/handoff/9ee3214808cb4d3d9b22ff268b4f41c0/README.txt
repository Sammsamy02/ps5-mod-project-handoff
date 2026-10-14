Visible startup diagnostic for the existing log-only module test

Build: 9ee3214808cb4d3d9b22ff268b4f41c0
This is the first probe with visible entry/error reporting, not UE4SS or the
full-import middle probe. It retains the same module path and mode-2 title entry.
Module path: /data/plugins/modbridge_log_probe.prx
Log path: /data/townfall_mod.log

Look for a notification beginning with modbridge-log-probe/v3 and this build ID,
then module_start start or init_array start. A subsequent open/write/close rc=
message reports the exact raw file-operation return value. A startup message
shows that this entry reached the notification call. A missing message cannot
prove that startup did not run, since notification delivery can itself fail.
The module_start still returns0 on logging failure. Loader OK alone is separate.

The probe adds one libkernel notification import to the previous open/write/close
imports. It contains no runtime, hooks, game memory operations or network code.
No Ploader changes are included. Three focused local control groups pass; native
compile/link and SELF load-segment round trip pass. Console execution untested.

Preserve the existing configuration and the previous module for comparison.
The replacement must wait until Townfall is fully closed. This bundle does not
install itself, start a game or launch Ploader.

#!/bin/sh
# memcheck_strict.sh -- refuse a compile when vm_stat "Pages free" is under 2 GB.
# Strict reading of the task rule ("skip the compile if under 2 GB is free"):
# only truly free pages count (round 1's memcheck.sh also counted inactive and
# speculative pages). Exit 0 = OK, 1 = too little, 2 = vm_stat unreadable.
set -u
MIN_GB="${MIN_GB:-2}"
out=$(vm_stat 2>/dev/null) || { echo "memcheck_strict: vm_stat failed"; exit 2; }
echo "$out" | awk -v min="$MIN_GB" '
/page size of/ {ps=$8} /^Pages free/ {f=$3}
END { gsub("\\.","",f); n=(ps>0)+(f!="");
  if (n<2) { printf "memcheck_strict: read %d of 2 vm_stat fields -> FAIL\n", n; exit 2 }
  g=f*ps/1e9; printf "memcheck_strict: fields=2 free=%.2fGB min=%sGB -> %s\n", g, min, (g>=min?"OK":"TOO-LOW"); exit (g>=min?0:1) }'

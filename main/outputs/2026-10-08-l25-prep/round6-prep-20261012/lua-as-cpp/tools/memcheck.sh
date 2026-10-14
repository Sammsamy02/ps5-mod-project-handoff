#!/bin/sh
# memcheck.sh — refuse a compile when the Mac has under 2 GB available.
# "Available" = vm_stat (free + inactive + speculative) pages, the memory macOS
# can hand out without swapping. The strict "Pages free" figure is printed too.
# Exit 0 = OK to compile, 1 = too little memory, 2 = could not read vm_stat.
# Mac-only helper (lua-as-cpp track, 2026-10-12).
set -u
MIN_GB="${MIN_GB:-2}"
out=$(vm_stat 2>/dev/null) || { echo "memcheck: vm_stat failed"; exit 2; }
echo "$out" | awk -v min="$MIN_GB" '
/page size of/ {ps=$8}
/^Pages free/ {f=$3} /^Pages inactive/ {i=$3} /^Pages speculative/ {s=$3}
END {
  gsub("\\.","",f); gsub("\\.","",i); gsub("\\.","",s);
  n=0; if (ps>0) n++; if (f!="") n++; if (i!="") n++; if (s!="") n++;
  if (n<4) { printf "memcheck: read %d of 4 vm_stat fields -> FAIL\n", n; exit 2 }
  avail=(f+i+s)*ps/1e9; strict=f*ps/1e9;
  printf "memcheck: fields=4 available=%.2fGB (free+inactive+speculative) strict_free=%.2fGB min=%sGB -> %s\n", avail, strict, min, (avail>=min?"OK":"TOO-LOW");
  exit (avail>=min?0:1)
}'

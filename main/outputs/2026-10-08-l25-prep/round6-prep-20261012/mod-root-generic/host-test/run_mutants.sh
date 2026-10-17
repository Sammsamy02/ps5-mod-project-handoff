#!/bin/sh
# run_mutants.sh - proves the host test can FAIL: each mutant breaks one
# fail-closed rule in a COPY of the staged entry (under out/mutants/), and the
# test must report at least one failed check for it. One compile at a time.
# Prints how many mutants it checked; exits 1 if zero ran or any mutant survived.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
LINEAGE=$WHAT/outputs/2026-10-08-l26-package-A/lineage
SRC=$HERE/../staged/lifecycle/runtime_module_entry.c
M=$HERE/out/mutants
rm -rf "$M"; mkdir -p "$M"
checked=0; survived=0
mutant() {   # name, perl substitution [, extra cc args for a variant build]
  name=$1; expr=$2; extra=${3:-}
  mkdir -p "$M/$name/staged/lifecycle" "$M/$name/host-test"
  perl -0pe "$expr" "$SRC" > "$M/$name/staged/lifecycle/runtime_module_entry.c"
  if cmp -s "$SRC" "$M/$name/staged/lifecycle/runtime_module_entry.c"; then
    echo "MUTANT $name: ERROR substitution did not apply"; survived=$((survived+1)); return
  fi
  cp "$HERE/test_title_derive.c" "$M/$name/host-test/"
  # shellcheck disable=SC2086
  cc -std=c99 -O1 -w $extra -I "$LINEAGE/lifecycle" -I "$LINEAGE" \
     "$M/$name/host-test/test_title_derive.c" -o "$M/$name/test" || { echo "MUTANT $name: compile failed"; survived=$((survived+1)); return; }
  out=$("$M/$name/test" "$HERE" 2>&1); rc=$?
  checked=$((checked+1))
  if [ $rc -eq 0 ]; then echo "MUTANT $name: SURVIVED (test passed - BAD)"; survived=$((survived+1))
  else echo "MUTANT $name: killed (exit $rc; $(echo "$out" | grep -c '\] FAIL') case(s) reported FAIL; exit >128 = crashed)"; fi
}
mutant no_nofollow      's/O_RDONLY \| O_NOFOLLOW/O_RDONLY/'
mutant accept_ambiguous 's/if \(confirmed_count > 1\) \{/if (confirmed_count > 99) {/'
mutant ignore_disagree  's/if \(!mbt_streq\(found, set.titles\[t\]\)\) \{/if (0) {/'
mutant allow_dup_title  's/j.title_hits != 1/j.title_hits < 1/'
mutant any_mode_enables 's/if \(mode <= 0\)\n            continue;/if (mode < -9)\n            continue;/'
mutant no_binding_check 's/if \(open_binding == NULL \|\| pread_binding == NULL \|\| close_binding == NULL\)\n        return -4;\n//' \
       "-DHOST_UNBOUND_PREAD -Wl,-U,_mbt_host_unbound_pread"
echo "MUTANTS: checked $checked, survived $survived"
[ $checked -gt 0 ] && [ $survived -eq 0 ]

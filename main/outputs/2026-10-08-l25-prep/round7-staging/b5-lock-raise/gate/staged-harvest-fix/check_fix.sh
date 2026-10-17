#!/bin/sh
# Mac-only. Shows the lex_cxx defect on a 2-line sample (original vs staged fix) and runs no_detour_gate and
# fs_throw_gate with the ORIGINAL harvest tools and with the staged fixed copy on the same trees; the verdicts and
# the violation sets must be identical (the fix only reveals lines the old lexer hid: the PS5-live line count differs and is
# normalised away, file paths are normalised too). Writes nothing outside this dir.
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
H=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest
B=$HERE/../..
n=0; bad=0
t() { n=$((n+1)); if [ "$2" = "$3" ]; then echo "  ok   $1"; else echo "  BAD  $1 (orig=[$2] fixed=[$3])"; bad=$((bad+1)); fi; }
S="['int a; /* x */ int b;','lua.throw_error(\"x\");']"
o=$(cd $H && python3 -B -c "from no_detour_gate import lex_cxx; v=lex_cxx($S); print(repr(v[1].code))")
f=$(cd $HERE/harvest-copy && python3 -B -c "from no_detour_gate import lex_cxx; v=lex_cxx($S); print(repr(v[1].code))")
echo "line 2 code view: original=$o  fixed=$f"
t "original lexer hides line 2, fixed shows it" "''" "$o"   # the 'original' column must be the defect
[ "$f" != "''" ] && echo "  ok   fixed lexer shows line 2" || { echo "  BAD  fixed lexer still hides line 2"; bad=$((bad+1)); }
for tree in "$B/sites/patched/final-b5" "$B/b6-diagnostics/base" "$B/b6-diagnostics/work"; do
  nm=$(basename $tree); [ "$nm" = work ] && nm=b6-work; [ "$nm" = base ] && nm=final-b5-base
  for g in no_detour_gate fs_throw_gate; do
    a=$(python3 -B $H/$g.py --root $tree 2>&1 | grep -E 'PASS|FAIL|violation|no error_code|throwing' | sed -E 's#^ *[^ ]*/(src|include)/#  #; s/\([0-9]+ PS5-live code lines\)/(N PS5-live code lines)/; s/:[0-9]+:/:/' | sort | md5)
    b=$(python3 -B $HERE/harvest-copy/$g.py --root $tree 2>&1 | grep -E 'PASS|FAIL|violation|no error_code|throwing' | sed -E 's#^ *[^ ]*/(src|include)/#  #; s/\([0-9]+ PS5-live code lines\)/(N PS5-live code lines)/; s/:[0-9]+:/:/' | sort | md5)
    t "$g on $nm: same verdict + violation set (orig vs fixed lexer)" "$a" "$b"
  done
done
echo "check_fix: checked=$n bad=$bad"
[ "$n" -gt 0 ] && [ "$bad" -eq 0 ]

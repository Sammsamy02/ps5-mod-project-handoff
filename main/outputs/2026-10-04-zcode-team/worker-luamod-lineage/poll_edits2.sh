#!/bin/bash
WS=/WORKSPACE/.codex/worktrees/translation-core/what
FILES=(
"$WS/outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp"
"$WS/outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp"
"$WS/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp"
"$WS/outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp"
"$WS/outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedValidation.hpp"
"$WS/outputs/2026-10-04-zcode-team/worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp"
"$WS/outputs/2026-10-04-zcode-controller/launch11-combined/launch11_one_pass.py"
"$WS/outputs/2026-10-04-zcode-controller/launch11-combined/build_runtime_composition6_l11.py"
"$WS/outputs/2026-10-04-zcode-controller/launch11-combined/EDITIONS.json"
)
SNAP=""; for f in "${FILES[@]}"; do SNAP="$SNAP$(shasum -a 256 "$f" | cut -d' ' -f1)"; done
BASE=$SNAP
STABLE=0
for i in $(seq 1 40); do
  sleep 60
  SNAP=""; for f in "${FILES[@]}"; do SNAP="$SNAP$(shasum -a 256 "$f" | cut -d' ' -f1)"; done
  if [ "$SNAP" == "$BASE" ]; then
    STABLE=$((STABLE+1))
  else
    STABLE=0
    BASE=$SNAP
    echo "poll $i: CHURN detected"
  fi
  # settled = at least one change vs the ORIGINAL pre-session world AND 3 stable minutes
  ORIG="75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb5743c39a86b84428e0672a658441108dd546dc8d4cd1484fb107f6a1059927c4874b3d81b0f57188d0c18ff4d308890035c253625c9f3dc36d16038d38eb6552a017b5d4636919b2707c85f2c69f50e338079b03d606a7bdf83fbb6df07ff97aa47b3d81b0f57188d0c18ff4d308890035c253625c9f3dc36d16038d38eb6552a01b3d81b0f57188d0c18ff4d308890035c253625c9f3dc36d16038d38eb6552a01c81f89c7624527427978136bcd12ccbe418e7ebed2b656435617d3239be5f57a3ee720e2712e2ca530800c4e69ec22907176a0b14e43aa449beb3334ad629f84"
  if [ "$STABLE" -ge 3 ] && [ "$SNAP" != "$ORIG" ]; then
    echo "SETTLED at poll $i ($(date -u +%H:%M:%SZ)): 3 stable minutes after edits"
    echo "$SNAP"
    exit 0
  fi
done
echo "TIMEOUT: no settled state in 40 polls"
exit 2

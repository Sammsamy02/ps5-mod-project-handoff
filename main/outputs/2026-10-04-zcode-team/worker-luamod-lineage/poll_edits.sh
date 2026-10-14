#!/bin/bash
WS=/WORKSPACE/.codex/worktrees/translation-core/what
OUT=$WS/outputs/2026-10-04-zcode-team/worker-luamod-lineage
F1=$WS/outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp
F2=$WS/outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp
F3=$WS/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp
F4=$WS/outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp
H1o=75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574
H2o=3c39a86b84428e0672a658441108dd546dc8d4cd1484fb107f6a1059927c4874
H3o=b3d81b0f57188d0c18ff4d308890035c253625c9f3dc36d16038d38eb6552a01
H4o=7b5d4636919b2707c85f2c69f50e338079b03d606a7bdf83fbb6df07ff97aa47
for i in $(seq 1 30); do
  H1=$(shasum -a 256 "$F1" | cut -d' ' -f1)
  H2=$(shasum -a 256 "$F2" | cut -d' ' -f1)
  H3=$(shasum -a 256 "$F3" | cut -d' ' -f1)
  H4=$(shasum -a 256 "$F4" | cut -d' ' -f1)
  CHANGED=""
  [ "$H1" != "$H1o" ] && CHANGED="$CHANGED comp6-program"
  [ "$H2" != "$H2o" ] && CHANGED="$CHANGED LuaMod"
  [ "$H3" != "$H3o" ] && CHANGED="$CHANGED SeedValidation-final-units"
  [ "$H4" != "$H4o" ] && CHANGED="$CHANGED program-wpd1"
  if [ -n "$CHANGED" ]; then
    echo "EDITS LANDED at poll $i ($(date -u +%H:%M:%SZ)):$CHANGED"
    echo "H1=$H1"; echo "H2=$H2"; echo "H3=$H3"; echo "H4=$H4"
    exit 0
  fi
  sleep 60
done
echo "TIMEOUT after 30 polls: no edits landed"
exit 2

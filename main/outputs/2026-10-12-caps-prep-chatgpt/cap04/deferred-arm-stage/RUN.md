# Deferred LoadMap delivery — source-only integration recipe

Status: staged, independently reviewed, not applied, compiled or deployed. This recipe adds no dependency to B7 + hooks + 15-fields build/launch. The runtime owner integrates later and runs its normal gates.

The exact input is `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` under the repository root. Required SHA256: `895a13657e69dae60835b3eea3113d6990eb29538c4b6c09e41cb132fad2b307`. Four exact-once replacements produce derived SHA256 `f15261d6032f951fb33cde23e7c2ea9b977265071d4ea7a7f66f7a25be1dea10`:

- Guard native pre/post Lua callbacks before markers or Lua delivery. PROSPERO eager state0 and fully armed state2 remain admitted; pending1, refused3 and unknown states return.
- Correct the refusal message: a successful half-registration can remain present but inert.
- Prepare state1 and the fallback route before publishing arm-pending; startup no longer writes state after publication.

`delivery-guard.patch` is the review diff. Synchronous callback removal is deliberately absent: the retained detour contract forbids removing callbacks directly or indirectly inside a callback, and the deferred armer runs from a carrier callback. The stage retains partial registration storage and the existing WORLD-EV fallback. It does not prove complete ownership/drain behavior or remove native detours.

From the repository root, dry-run without writing derived source:

```sh
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap04/deferred-arm-stage/stage_delivery_guard.py
```

The tool validates the whole input hash and every anchor before transforming. Drift, empty input and second application are refused. There is no runtime-apply option. If the owner needs an inspectable derived copy, `--derived-out` accepts a new `.cpp` only inside this stage folder and refuses overwrite; no full derived copy is retained in this bundle. A changed owner source needs a newly reviewed stage, not a bypassed hash check.

Six source-level checks are recorded in `TDD-GREEN.log`. To rerun when the source/stager changes:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap04/deferred-arm-stage/test_stage.py
```

The tests inspect source predicates, publication order, inverse transformation and refusal behavior. They do not execute C++, threads or the console. `WRITE-CHECK.json` records one temporary derived write, three forbidden writes and unchanged original input; the temporary copy was removed. `REVIEW.json` binds final files under `final_files`: R1 publication ordering is resolved in the same one independent review, with no open critical/important finding in this narrow scope.

Remaining #4 proof: build/target integration, actual partial-arm behavior, lossless process-start capture and first LoadMap/BeginPlay ordering with common run/build/clock identity. A post-only callback can still emit ordinal1; this stage does not make that ordinal process-first proof. Use the combined #4+#5 console test later in the authorized sequence.

# Scanner proof checklist (Mac-only proposal)

1. Record readable input image SHA-256, classifier source identity and marker-table identity. Keep existing game/mod bytes unchanged.
2. Preserve classifier tier, family/version, configuration, stats, present count, margin and absolute misfit. Exit 0 means report produced: REFUSED can legitimately exit 0 and is not admission.
3. Admit pins only when image identity, engine/version and independently supported anchor pack agree. Tentative/ambiguous profiles fail closed with the reason; version classification alone does not validate addresses.
4. A real UE4 fixture must validate FName/GUObjectArray era and required seed behavior using two independent sources before any values ship. Current synthetic UE4 result is only cost-model self-consistency. Use authorized generic/public fixtures; no blind-test-game preparation.
5. Console receipt: first launch automatically generates the profile; log names the identified engine/version and input identity, then required seeds are behaviorally admitted. Capture same-launch build/hash and line references. Demonstrate unsupported/ambiguous refusal through isolated Mac fixtures rather than adding a console launch.

Fresh checks in this folder are limited to synthetic cost-model cases. Do not run run_phasea_tests.py in place: it writes beside source and may create a large modified fixture.

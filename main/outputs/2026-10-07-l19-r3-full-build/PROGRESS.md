# l19 persistent progress

Scope: approved local implementation and offline verification only. No console, account or network contact; no upload, launch, payload or configuration change. Console remains on l18.

## Verified findings

- Shipping and Stats-off have independent evidence, bound to Townfall image SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`. Shipping does not imply Stats-off. Missing, wrong or conflicting facts still refuse.
- The final compiler census covers all 54 required non-total accessor keys. R3 fixes six additional missing width providers with genuine public getter types, preserving native Owner storage 8 separately from its conservative compiled copy footprint 16.
- Independently reviewed native evidence adds UClass.Interfaces `0x1D8`/16 and UEngine.Tick slot `0x310`/8. Original evidence and earlier source freezes remain unchanged.
- Final R3 freeze passed independent review: 75 source pins, 36 evidence pins, 29 initializer cases, 25 binding cases, 54 target widths and 18 schema refusals. The reviewer freshly executed 56 current binaries.
- XOM source audit: no successful game-instruction write is identified as a prerequisite for the first menu/spawn. The required game-thread carrier changes a UFunction data pointer; its writable coverage, lifetime and delivery remain unproved. Full LoadMap reset behavior remains unproved under XOM.
- Saved l18 timeline: approximately 65 s before PRX entry (cross-clock inference), entry to runtime acknowledgement 4.082 s. Cache 2.130 s is included in admission 2.297 s/FName 2.355 s. Popup display times and individual loader waits were not captured. No timing changes have been applied.

## Unresolved

- Exact native FField and FFieldClass totals remain unproved; extent/placement candidates were not promoted.
- Native UEngine.LoadMap and AGameModeBase.InitGameState slots remain unproved.
- Saved staged maps still have only 17 member and one vtable declarations; no expanded configuration has been generated or deployed. This is not menu/spawn-ready.
- R2 full build stopped before compilation because a new output directory was absent. No R2 artifact is claimed. R3 now has explicit output directories; all inherited gates are retained.

## Next steps

1. Complete the exact FField/FFieldClass totals and LoadMap/InitGameState slots from authenticated native evidence; retain refusals for missing/conflicting facts.
2. Prepare and verify complete profile-derived layout declarations locally; saved 17/1 maps remain incomplete and untouched.
3. Present a concrete console step only after known mandatory refusals are cleared, with the required exact approval and one user launch.
4. Game-thread carrier delivery, controller input, the original menu/spawn and full LoadMap behavior still need console evidence afterward.

Current status: final offline artifact audit PASS. Two PRXs are identical: 31,024,199 bytes, SHA256 `4565373be6c8d2b3d0dc592e0c8b8ae36744fc9447be3e8ed37abf86d6e2ccc9`. All 369 selected objects, 386 inputs, 206 actual/typed imports, 20 static checks per package and final gates passed. No console contact or menu/spawn success; upload HOLD.

Timing provenance: an existing klog received 8,902 appended bytes. Its original 621,320-byte prefix still matches the frozen pin and is saved by the reviewer. The final report qualifies that original prefix separately from the current whole-file hash. No capture was truncated or replaced.

Standing approval update (2026-10-07): reported-launch read-only harvests, conditional runtime-PRX-only uploads, and existing Ploader reloads are authorized under the mission’s exact conditions. Other console changes still require explicit OK. Reports use eight fields within ten lines plus the details link. No console action occurred for this rule update.

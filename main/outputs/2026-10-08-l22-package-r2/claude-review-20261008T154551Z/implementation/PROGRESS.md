# Persistent PS5 progress — l22 local build, 2026-10-08

## Verified findings

Prior console evidence remains PID 184 / runtime 885f43c6: pool, FName and profile gates passed; startup stopped on missing FMalloc::QuantizeSize before the Lua mod. No menu or spawn proof. No console contact occurred during l22.

Mac-only: the four l22 changes are implemented. All 17 corroborated allocator offsets are image-bound; existing 76 pins are unchanged. SCO empty callback alignment is 16 with all five target assertions passing. The exact 63-key preflight covers 49 required, 12 conditional and 2 deferred runtime discoveries; required omissions collect then refuse, conditional omissions retain lazy failure. Requested error/input/queue/widget diagnostics preserve behavior. Original downloaded Lua is unchanged. No UTF-8/ANSI registration change.

Focused checks passed: 10 preflight + 8 SCO/cache controls under ASan/UBSan; 44 diagnostics behavior assertions + 9 markers; 22 actual pad controls in plain and ASan/UBSan modes; original header fails alignment and candidate passes; 11 package checker controls. Independent l22 diff/build reviews found no remaining actionable introduced defect.

Two runtime builds match (167 fresh units per pass, 207 pinned retained units, 374 total, 391 link inputs); runtime SHA 00820f3c18bce1d210bd57fc22114986a2d472e192403403b41f24acad4853a7. Both complete PRXs are 31,261,655 bytes, SHA eb32990807c942730c7c404482b0b57a74bd2f9ed218e10f35f0e0f899f80dec, SELF 4f153d1d, SDK 0x11000000. All 20 static checks pass per package; 207 import identities unchanged; 238 init / 2 fini entries preserved. Local controller failures and failed directories remain preserved separately.

## Unresolved

No native l22 execution, allocator/widget call, input callback, menu or spawn has been observed. FText discovery and conditional APIs remain runtime checks. The 635-key generic inventory is parked per the l22 directive. Source/image corroboration is not evidence of all methods being invoked successfully in the game. Current console state was not queried.

## Next step

Review outputs/2026-10-08-l22-package-r2/BUILD-REPORT.md and the associated packet. Upload approval is pending. After explicit approval only: game closed, fresh decrypt-off live-PRX backup, PRX-only replacement, decrypt-off SHA/magic readback, NEW klog, one user launch/L3+R3 test, read-only harvest before any relaunch. No console action is authorized by this local completion record.

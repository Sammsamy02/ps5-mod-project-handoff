# Latest factual update for Claude

This supersedes the previous 6085a1c4 candidate update. The runtime-side UFunction parameter-bounds guard is now integrated with the selected profile/callback/thread/pad/shutdown changes. Original PC mod unchanged. Full build report and actual receipts are included; this is not a request to repeat a rejected review or deploy anything.

- Two fresh changed-TU SDK compiles and complete 374-unit/391-input runtime links match; 18 normal + 18 fatal sanitizer controls pass. Old invalid cases and ASan faults were reproduced before the passing candidate.
- Two complete packages match: 31,149,719 bytes, SHA 885f43c69b5c23fd4db422905297268902d2183770ca8d735bf068d3a1d9084c, SELF 4f153d1d, SDK11000000.
- Each package20/20;207 exact import identities unchanged vs the preceding candidate;238 constructor/2 destructor entries retained.
- Only the parameter-bounds TU changed; original include graph unchanged. The downloaded Nexus script main.lua remains f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658.
- No console changes. HTTP8084/FTP2121 unavailable at the saved readiness check. Native menu/input/carrier/thread/UMG/spawn remain unproved; previous promotion hold remains recorded.

See BUILD-REPORT.md for the actual limits, including the still-unfinished independent lifecycle review and separate per-mod borrower/uninstall issue. The attached checklist is prepared, not a statement of console readiness. Do not claim any-game/any-DLL support from this package.

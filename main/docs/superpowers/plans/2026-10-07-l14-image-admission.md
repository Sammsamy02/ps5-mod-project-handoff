# L14 image-segment admission implementation plan

User approved local execution; no console contact or upload. Implement inline.

Goal: remove the l13 image/non-image table mismatch without loosening exact-range proof.
Architecture: a generic segment-policy helper consumes module-info or per-game PT_LOAD facts corroborated by frozen live mappings. Image reads require one readable data/BSS segment and a VQ cross-check; heap reads retain VQ-first admission. One 60s deadline covers all checks and diagnostics.
Spec: the user's 2026-10-07 approval and six additions; l13 PID117 report.

- [x] Preserve l13 source/artifacts and pad pins in a new l14 candidate directory. Stop old local klog clients to honor no contact.
- [x] Run the exact 0x400000/0x9765EC0/0x22C0000000 integration fixture against unmodified l13 and record the expected coverage refusal.
- [x] Add tested segment coverage: exact PT_LOAD bounds, readable data/BSS, matching current image base, live mapping corroboration; unavailable module info is never invented.
- [x] Route every metadata and indexed pointer read through this proof. Collect independent failures; log dependent checks as blocked rather than read unknown pointers. Record per-check source/region/segment, oracle/shadow status and total elapsed.
- [x] Run integration regressions, existing range/sink tests and target TU compilation under network denial. Verify negative ranges, boundary/overflow cases, budget exhaustion and pad immutability.
- [x] Clone the pinned launch11-derived l13 pipeline into l14; double-build, strict imports, packaging, manifests and offline gates. Preserve all receipts and wait for upload approval.

Review focus: module-info unavailable; profile-only bounds; VQ contradicts segment facts; one unreadable metadata field amid independent valid fields; expired budget midway through diagnostics. Each must refuse safely or produce explicit blocked evidence.

Completion evidence: `outputs/2026-10-07-l14-full-build/receipts/FINAL-GATES.json` (PASS_MAC_ONLY_UPLOAD_HOLD). No console contact. Separate upload approval pending.

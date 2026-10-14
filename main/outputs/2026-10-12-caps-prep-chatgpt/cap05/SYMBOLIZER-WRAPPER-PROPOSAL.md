# One-command symbolizer wrapper proposal (historical design)

Superseded by crash_report.py and README.md on 2026-10-12. Historical design text below is not an implementation verdict.

Existing symbolizer prints JSON even without --json. A thin offline wrapper could provide a plain report without touching runtime/build tools:

`python3 crash_report.py --klog CAPTURE --prx EXACT_BARE_PRX --symbols-sidecar MATCHING_JSON --output REPORT.md`

Requirements:

- Load the existing read-only symbolizer; disable bytecode and put TMPDIR/output under the assigned prep directory. Accept only local inputs; no console/network capability.
- Hash klog, PRX and supplied symbol sources; record build ID and identities. Pass --prx with --symbols-sidecar so mismatch hard-refuses. A real signed/uploaded SHA and the bare-PRX SHA are distinct identities: record packaging relation rather than comparing them as identical bytes.
- Report block count, signal, RIP, fault address, owner module/fingerprint and offset. Zero crash blocks is **NO CRASH BLOCKS**, not successful crash proof. Unknown owner remains unresolved.
- Preserve evidence tier, pairing status and inside_symbol_bounds. Do not promote nearest-symbol matches outside bounds. MAP-RECONSTRUCTED stays approximate; GAME-SIDECAR stays unverified until image pairing is separately checked. ELF/final-map sources need recorded build pairing before an exact operational claim.
- Filter raw paths/messages from the human report as needed; never copy a raw klog or console address into shareable output. Keep local originals unchanged.
- Suggested isolated checks: this retained 2-block fixture; empty capture; unknown-module RIP; wrong PRX-sidecar pair; unverified sidecar without PRX. Tests must use temporary files under cap05 and must not invoke the original suite in place.

This is a proposal, not a shipped or tested wrapper. Today's fresh check runs only the existing parser and module-range resolver on the preserved fixture. New console proof requires an actual capture tied to the exact build, never invented symbols or a forced crash during the clean #4 launch.

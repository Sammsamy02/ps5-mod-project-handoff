# Current PS5 progress — Oct 8 post-launch harvest

Verified: PID184 runtime executed; current declared profile accepted; pool admitted, ByteProperty round-trip passed; final FName binding2588ms /60000ms. Readback still885f43c6 (full SHA in receipt), original Nexus main.lua unchanged. User reports save loaded, one L3+R3 press, no menu and no crash; process184 remains live at harvest.

Terminal result: runtime-status reports `Virtual FMalloc::QuantizeSize is unavailable, possibly unsupported in engine version`; Item Spawner stopped before any Starting Lua mod line. Mac-only selected-source analysis identifies a missing virtual-map entry, before the native allocator call. Pad remains deferred0x80920004; no F7/menu success. No literal build-SHA startup banner or exact plugin popup bodies were captured.

Next: review evidence with Claude/user. No fix proposed/staged in this evidence-only step; no further launch is needed to interpret this capture. Preserve current game/session and all original captures. Harvest changed nothing on the console.

Report: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/claude-review-20261008T143432Z-cb5e7f/REVIEW-TABLE.md
Claude ZIP: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/claude-review-20261008T143432Z-cb5e7f.zip
ZIP SHA-256: 65962594134ff0a7ac59f3251ef7187351aac4f19739c8d47c8f3e3ee7d9c494

Prior progress preserved at /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/claude-review-20261008T143432Z-cb5e7f-progress-before.md. Independent cleanup acceptance remains unclaimed; this launch does not prove universal mod compatibility.

# Stage 39: preserve guest-cached TLS array views

**Status:** isolated Mac host work; not a console-only wait.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-nested-tls-view-20261013-39`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dynamic-directory-load-20261013-38`

`GuestTebView::refresh()` previously replaced its TLS pointer array on every entry. A DllMain that dynamically loads another DLL can trigger a nested refresh while the outer DLL still holds a pointer copied from `GS:[0x58]`. The old array could be freed. Stage 39 keeps the current array stable when the TLS epoch is unchanged and retains older arrays when a newly registered TLS module requires a new view. Existing TLS block addresses remain stable.

Two new host checks first failed on stage 38 and passed after the change: same-epoch pointer stability and retention of a guest-cached array after a late TLS registration. The focused TEB suite passed **17/17**, including an AddressSanitizer run. All **15** offline regression runners exited 0; the original synthetic fixture passed **11/11**. The three-file patch passed exact-stage-38 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](nested-tls-view-increment-20261013.patch)
- [Base/stage hash manifest](nested-tls-view-manifest-20261013.json)
- [Regression, ASan, and replay evidence](nested-tls-view-evidence-20261013.zip)

Retired pointer arrays live for the guest TEB's lifetime, so memory use grows with distinct TLS registration epochs. This fixes array lifetime, not console GS switching, Windows CRT behavior, or gameplay. No console, shared candidate, kernel file, or original mod binary was changed.

# Stage 38: directory-backed dynamic PE loading

**Status:** isolated Mac host work; not proof of arbitrary Windows DLL execution or a console-only wait.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dynamic-directory-load-20261013-38`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-loadlibrary-route-20261013-37`

This stage connects stage 37's guarded `LoadLibraryA` hook to the existing generic PE dependency source and closure. A caller-owned context fetches a requested DLL by basename, verifies the source did not substitute another name, attaches its available static dependency closure, and returns the root HMODULE only after attachment. Successful batches remain owned by the caller for reverse-order detach. A failed rollback that leaves a provider attached also remains retained for explicit cleanup. The context refuses before guest entry when retention storage is full.

The focused batch suite passed **49/49** checks. A sealed Win64 `LoadLibraryA` call loaded an unchanged synthetic fixture from a temporary directory, exposed its image and export while attached, and revoked both after detach. The test also rejected a mismatched source name and exhausted retention capacity, then demonstrated cleanup after a deliberately blocked rollback. The original fixture and five upstream DLL inputs remained hash-pinned and unchanged. No real gameplay DLL or CRT was executed.

All **15** offline regression runners exited 0. The original synthetic end-to-end fixture passed **11/11**. The five-file patch passed `git apply --check`, exact-stage-37 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](dynamic-directory-load-increment-20261013.patch)
- [Base/stage hash manifest](dynamic-directory-load-manifest-20261013.json)
- [Regression and replay evidence](dynamic-directory-load-evidence-20261013.zip)

The source is still a caller-selected single directory. The caller must reserve retention capacity, quiesce guest threads, and detach retained batches in reverse order. Nested `LoadLibraryA` during a real DllMain, broader dependency layouts, `FreeLibrary` reference counting, CRT/SEH behavior, console segment switching, and game target correspondence remain unproved. No console, shared candidate, kernel file, or original mod binary was changed.

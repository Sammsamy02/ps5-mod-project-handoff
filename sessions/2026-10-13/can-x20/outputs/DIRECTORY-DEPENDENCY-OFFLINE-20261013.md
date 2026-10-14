# Stage 33: directory-backed DLL dependency source

**Status:** isolated Mac host work; not a console-only wait or proof of universal DLL compatibility.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-directory-dependency-20261013-33`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dependency-closure-20261013-32`

`GuestPeDirectorySource` supplies stage 32's generic dependency callback from a caller-selected folder. It searches immediate neighboring files by case-insensitive imported basename, refuses ambiguous variants on filesystems that allow them, rejects symlinks and non-regular files, and reads each dependency into an owned byte buffer with a 512 MiB per-file cap. Directory enumeration is bounded to 4096 entries and each lookup starts from a new directory view. A folder path and guest segment backend are supplied by the caller; no game or mod name is embedded.

The focused Mac suite passed **26/26** checks. A real temporary directory supplied a copied provider DLL, whose export was called by the copied consumer through its IAT; cleanup succeeded. The suite also checked repeated lookups, filesystem case behavior, and symlink refusal before guest entry. The host's case-insensitive volume cannot hold two names differing only by case, so the ambiguous-name branch was exercised only by code inspection in this pass. The five unchanged upstream DLLs were still scanned read-only and their hashes stayed unchanged. No real gameplay DLL executed.

All **14** offline regression runners exited 0; the original synthetic end-to-end fixture passed **11/11**. The four-file patch passed `git apply --check`, exact-stage-32 scratch application, and byte-for-byte replay; the evidence ZIP passed CRC verification.

- [Incremental patch](directory-dependency-increment-20261013.patch)
- [Base/stage hash manifest](directory-dependency-manifest-20261013.json)
- [Regression and replay evidence](directory-dependency-evidence-20261013.zip)

This source searches one directory and does not discover nested folders or obtain system/framework DLLs. Dynamic module APIs, CRT/UE4SS imports, a console guest segment backend, guest-thread lifecycle, game target matching, and gameplay proof remain. There was no PS5 compiler or console run in this pass.

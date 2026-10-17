# Stage 35: local PE fallback for unavailable CRT shim namespaces

**Status:** isolated Mac host work; not proof of Windows CRT compatibility or a console-only wait.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-crt-fallback-20261013-35`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dynamic-module-lookup-20261013-34`

The shim no longer treats a DLL name as occupied merely because it catalogs unreviewed, unavailable functions under that name. Stage 32's closure can therefore request a nearby `VCRUNTIME140.dll`, and stage 31's guarded registry can attach it if its own imports and entry pass. A reviewed built-in namespace such as `KERNEL32.dll` remains reserved. Opaque built-in HMODULE tokens are created only for namespaces with a callable reviewed row.

The focused Mac suite passed **31/31** checks. A copied synthetic PE, named `VCRUNTIME140.dll` only in the test source path, was fetched as a dependency, attached ahead of a copied consumer, and called through that consumer's IAT. `GetModuleHandleA` and `GetProcAddress` found its export while attached and stopped finding it after detach. The shim's unreviewed EH body stayed unavailable; a supplied `KERNEL32.dll` file could not replace the reviewed system namespace. The original synthetic fixture and five upstream DLLs remained hash-pinned and unchanged. This test did **not** execute a real CRT or gameplay DLL.

All **15** offline regression runners exited 0; the original synthetic end-to-end fixture passed **11/11**. The two-file patch passed `git apply --check`, exact-stage-34 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](guest-crt-fallback-increment-20261013.patch)
- [Base/stage hash manifest](guest-crt-fallback-manifest-20261013.json)
- [Regression and replay evidence](guest-crt-fallback-evidence-20261013.zip)

Actual MSVC CRT dependencies, SEH/unwind behavior, the UE4SS C++ ABI, production guest-memory reads and segment switching, target correspondence, and PS5 execution remain unproved. A local DLL with unsupported imports still refuses by name; fallback is not silent emulation. No console, shared candidate, kernel file, or original mod binary was changed.

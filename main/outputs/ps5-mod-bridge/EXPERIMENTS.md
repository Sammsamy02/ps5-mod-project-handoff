# Experiment workflow

User direction, September 30, 2026: use a lighter process for experiments and keep strict review for anything that writes to game files or the console system.

Throwaway branches and ignored research harnesses use focused checks that answer the experiment's question. Full review, sanitizer suites, checksum bundles and repeated broad tests are optional here; run them only for a concrete unresolved risk. Keep a short note of what ran, what was observed, and what remains unknown. Preserve existing artifacts and user files. Do not turn compile success into a runtime or compatibility claim.

Before promoting an experiment into a game/system writer or uploading it to the console payload store, review the actual changes and relevant failure paths. Verify target identity, narrowly scoped writes and cleanup or rollback. Run checks appropriate to the changed code. Batch review at that promotion boundary instead of creating a separate full review gate for every small experiment.

Finished application operations remain PS5-only. Development compilers and controlled test servers may run on the computer. Unsupported or unverified external-runtime paths remain non-installable. Never control the user's FTP service; ordinary file access and readback are allowed.

User direction, October 1, 2026: downloaded mods must be processed automatically by reusable format and engine adapters. Do not manually edit individual mods, depend on AI during installation, or add per-mod conversion recipes. Shared deterministic converters may produce prepared bytes while preserving the original download. Target paths and observed build facts may be profile data; profiles must not hide custom mod translation code. Development can inspect representative packages to implement and test a shared rule. Archive recognition, extraction and copying alone do not establish asset conversion or runtime compatibility.

This explicit user direction takes precedence over default Superpowers experimentation workflows. It does not require another permission question for already authorized reversible work.

# Stage 34: validated dynamic lookup for already attached modules

Base: isolated stage 33. User goal: reusable unchanged-DLL runtime that adapts without game-specific addresses or per-mod edits; unsupported cases refuse clearly. This increment does not claim universal compatibility.

Implement guest-callable integer/pointer Win64 GetModuleHandleA and GetProcAddress using existing context bridge. A caller-installed guest byte reader must validate and copy names; absent reader keeps both imports unpublished. GetModuleHandleA returns registered attached module handles, built-in shim module tokens, or the configured main module handle for NULL. GetProcAddress resolves case-sensitive names and PE ordinals only within a known handle, using existing bounded forwarder logic and callable reviewed built-in wrappers. Reject invalid, unterminated, or excessive strings, unknown handles, and invalid ordinals. Keep absent LoadLibrary and guest-thread cases refused.

Tests first: Win64 calls through published wrappers for registered module and reviewed builtin, main-module NULL, ordinal, invalid pointer, unterminated name, unknown handle, and absent-reader refusal; then full 14 offline regressions, original synthetic E2E, exact-stage-33 patch replay and evidence. No PS5 or gameplay claims.

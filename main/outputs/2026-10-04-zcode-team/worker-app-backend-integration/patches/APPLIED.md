# APPLIED.md — worker-8-app-backend patches applied to the vendored tree

Worker-app-backend-integration, 2026-10-04. Source of truth:
`worker-8-app-backend/PATCHES.md` (recorded, NOT yet applied upstream).
Applied to MY vendored copy only
(`vendored/repo/outputs/ps5-mod-bridge/native-host/`); the original tree at
`outputs/ps5-mod-bridge/native-host/` was never written to.

| Patch | Target | How applied | Verified by |
|---|---|---|---|
| 1 | `mod_service/read_bridge.h` (`MB_MOD_SERVICE_REQUEST`, `mb_mod_service_call` decl) | `patch(1)` from `patches/01-read_bridge.h.applied.patch` | compile (c++20 -Werror in test harness) + grep |
| 2 | `mod_service/read_bridge.cpp` (`mb_mod_service_call` impl) | `patch(1)` from `patches/02-read_bridge.cpp.applied.patch` | compile + runtime: `POST /api/mod-service/request` generic passthrough exercised indirectly via mfw8 service tests lineage; envelope validation unchanged |
| 3 hunk A | `http.c` body-framing gate admits `POST /api/mod-service/request` | `patch(1)` (applied clean on first try) | compile + live binary behavior |
| 3 hunk B | `http.c` `mod_service_response` routes `/api/mod-service/request` | applied MANUALLY (see note) — identical insertions; live admission lines kept | compile + generic-passthrough code path present in built binary |

The applied-patch files (`*.applied.patch`) are `diff -u` outputs of the
PRISTINE original tree vs the vendored tree — they are the exact bytes that
changed, so they double as verification that nothing else moved.

## Honest notes

1. The recorded hunks in worker-8's PATCHES.md carry WRONG line-count headers
   (e.g. `@@ -16,7 +16,8 @@` where the hunk is 8/9 lines) — they do not apply
   with patch(1)/git apply as recorded. I corrected the counts from the live
   file; content is otherwise verbatim.
2. Hunk B's context in PATCHES.md was written against a pretty-printed
   admission-check line. The live `mod_service_response` DOES use the pretty
   form (the minified one-liner sits in the neighboring `downloads_response`),
   so the hunk context matches and the change is semantically identical to
   worker-8's recorded intent.
3. Vendored bootstrap fix (my own, documented in the file): the checked-in
   `tools/bootstrap_deps.py` dependency verification fails on `local://`
   manifest URLs EVEN IN THE ORIGINAL TREE (pre-existing upstream condition).
   The vendored copy accepts the local scheme for byte-verification and never
   fetches from it; all pinned sha256 verification is unchanged.
4. Vendored build-inputs: `build-inputs/mod-service-{profile,config}.json`
   bake the mfw8 archive identity and a fixture `state_dir`/`mod_root_base`
   (host-test only; the console layout stays `/data/modbridge` per
   CONSOLE-CONTRACT.md and is the controller's dispatch-time concern).

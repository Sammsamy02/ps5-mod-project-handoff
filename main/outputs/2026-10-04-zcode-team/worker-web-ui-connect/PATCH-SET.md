# PATCH-SET.md — worker-web-ui-connect: verified combined patch set (backend + frontend)

Origin: worker-web-ui-connect, 2026-10-04. Existing files were NOT modified.
All verification ran on copies under `verify/`. This document is the controller's
apply sheet; `COMPLETE-DIFF.md` embeds the same patches verbatim plus the
user-visible behavior walkthrough.

## 0. Inputs actually available

| Input | Status at verification time |
|---|---|
| `worker-8-app-backend/PATCHES.md` (3 diffs) | Present, read, extracted byte-exact from fenced blocks |
| `worker-8-app-backend/AUDIT.md` | Present, read (route/capability facts used below) |
| `worker-e2e-records-ui/PATCH-OUTLINE.md` | **NOT PRESENT** — `outputs/2026-10-04-zcode-team/worker-e2e-records-ui/` did not exist at first read nor at repeated polls during this session. Fallback frontend patches were derived by this worker (see §5). |

## 1. Verification method

- Pristine copies of `outputs/ps5-mod-bridge/native-host/{http.c,mod_service/read_bridge.h,mod_service/read_bridge.cpp}` and `outputs/ps5-mod-bridge/web/{app.js,index.html}` placed under `verify/tree/` and `verify/canon/{a,b}/` (git-init'd scratch trees, no other repo touched).
- Original diffs extracted byte-exact from PATCHES.md fenced blocks into `verify/patches/backend-*.patch`.
- `git apply --check` run per patch; then sequential real `git apply` of the full set on a fresh tree; final tree compared with `diff -r` against the independently constructed intended state (`verify/canon/b`) — result: **byte-identical**.

## 2. Result: the originals as written DO NOT apply

| Original patch | `git apply --check` result |
|---|---|
| Patch 1 (read_bridge.h) | FAIL — `error: patch fragment without header at line 14: @@ -24,5 +25,11 @@` (both hunk headers under-count; body context itself is correct) |
| Patch 2 (read_bridge.cpp) | FAIL — `error: corrupt patch at line 41` (header claims old count 3; body has 4 context lines) |
| Patch 3 hunk A (http.c frame gate) | FAIL — `error: corrupt patch at line 15` as written; still fails `--recount` and fails with corrected header counts on a fresh tree, although the pre-image was verified byte-identical to http.c:287-293. Root cause of the residual failure not isolated; treated as untrusted. |
| Patch 3 hunk B (http.c route) | FAIL as written — `error: corrupt patch at line 47` (header `-771,14 +771,22` vs actual 29 old / 39 new lines); passes with corrected counts. |

Every `+`/`-` content line in the originals matches the current files; only the
headers are defective (plus the unresolved hunk-A anomaly). Rather than ship
hand-recounted headers, this worker regenerated **canonical patches with
`diff -u` from actual file states** whose change content is transcribed verbatim
from worker-8's intended additions/removals. Canonical patches pass strict
`git apply --check` (no `--recount`, no `--ignore-whitespace`).

## 3. THE APPLY ORDER (controller applies exactly these 5 files, one pass)

All paths relative to the workspace root `/WORKSPACE/.codex/worktrees/translation-core/what`.
Patch files live in `outputs/2026-10-04-zcode-team/worker-web-ui-connect/verify/canon/`.

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what
P=outputs/2026-10-04-zcode-team/worker-web-ui-connect/verify/canon
git apply $P/p1-read_bridge.h.patch            # enum + mb_mod_service_call declaration
git apply $P/p2-read_bridge.cpp.patch          # mb_mod_service_call implementation
git apply $P/p3-http-mod-service-request.patch # frame gate + POST /api/mod-service/request route
git apply $P/f1-app-js-display-states.patch    # honest states, ack-based running, drop /api/installed
git apply $P/f2-index-html-subheading.patch    # Installed tab subheading copy
```

Order rationale:
1-2 are independent of each other but both must precede 3's compile (http.c references `MB_MOD_SERVICE_REQUEST` and `mb_mod_service_call`).
3 depends on 1-2 (PATCHES.md states this; also true at link time).
4-5 are frontend display-only, independent of 1-3 textually; they depend on 3 only semantically (the route exists for future UI use; the displayed states still arrive via the existing GET passthroughs).

The set was verified **exactly in this order** on a fresh tree: 5/5 `--check` clean, 5/5 applied, final tree byte-identical to intended state (`verify/fulltest/`).

## 4. Conflict analysis (backend set vs frontend set)

- Textual: disjoint files (`native-host/*` vs `web/*`) — no textual conflicts possible.
- Semantic interactions found and assessed:
  1. **Assets are embedded**: `http.c` serves `/app.js`, `/style.css`, `/app` from compiled-in assets (`http_asset_app_js`, `http_asset_index_html`; see `native-host/tests/web_assets_stub.c`, `tests/test_web_assets.py`). Frontend patches only reach the user after the host's asset blob is regenerated (build step; build.py). Until a rebuild, the served UI is the old one — expected, no breakage.
  2. **CSRF gap on the new POST (flag, not changed)**: existing mutating POSTs (`/api/jobs`, `/api/auth/*`, `/api/acquisitions`) require `Origin` + `X-CSRF-Token`; the new `/api/mod-service/request` branch requires neither (worker-8 kept the GET passthrough's posture and left authorization to the service backend). The UI's `endpoint()` always sends the CSRF header, so UI behavior is unaffected, but a non-browser local client can POST without CSRF. Controller decision point; noted, not patched (out of this worker's scope).
  3. **Capability gating unchanged**: the UI only reads mod-service data when `capabilities.mod_service_records === true`, which requires the service linked at startup (`host.c:442-455` + `--mod-service-profile`, controller-owned build step per PATCHES.md §Linking). Patches alone enable nothing without that link; with the link, describe/records flow exactly as before.
  4. **Route gap closed in the UI, not the host**: `web/app.js`'s dead `GET /api/installed` call (AUDIT §1, §5.4) is removed by F1 — the host still has no `/api/installed` route and none is added; the honest empty state replaces the silent degradation.
  5. No changes touch `jobs`, `downloads`, auth, or title-inventory paths; the 2500 ms/5000 ms polling intervals and their capability gates are untouched.

## 5. Frontend fallback disclosure

`worker-e2e-records-ui/PATCH-OUTLINE.md` was specified as an input but never
appeared (directory absent across polls spanning this whole session). The
controller's task description ("4 display-only changes to app.js + 1 to
index.html", honest mod states, ack-based running) was used to derive fallbacks
F1 (the 4 app.js changes: F1a unknown-state pill fallback, F1b runtime-ack
label helper + use, F1c remove dead `/api/installed` fetch, F1d records-source
note) and F2 (index.html subheading). **If the sibling outline arrives before
application, prefer it and drop `f1`/`f2`** — they are independent files and
can be swapped without touching p1-p3. Fallbacks keep every honesty rule:
states only from `records.list`, running only from runtime acks, no
installation claims.

## 6. Bounds kept

No existing file modified; all work on copies; no network/console; no nested
subagents. Session tool-call count: ~22 of ≤50.

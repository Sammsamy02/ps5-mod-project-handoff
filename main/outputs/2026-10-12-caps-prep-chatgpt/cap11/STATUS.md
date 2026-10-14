Related #9 source audit now pins the global callback/pause and pre-uninstall ownership requirements separately from GUI coverage. Hook-controller IDs discarded on failed removal and fixed500ms teardown sleeps do not establish retirement. See cap09/hook-contract-audit/HOOK-FIELD-CONTRACTS.md. No native GUI host or semantics gap is promoted by this audit.

# #11 UE4SS GUI windows — Mac-only toolkit, compatibility design-only

Actual ImGui API inventory now comes from the selected real C++ mod sources, not a generic imagined menu. The existing Lua OSD toolkit implements window/text/button primitives over retained UMG; it is not an ImGui C++ backend and cannot currently execute these mods' render callbacks. Prior console UMG evidence is context only, not this session's proof.

Missing: native register_tab/shared-context rendering bridge, remaining real ImGui API semantics, controller/text input and verified widget lifetime/thread behavior. See API-GAP-MATRIX.md and ONE-LAUNCH-PROOF.md. #9 is the C++ dependency; older design references to #10 recompile are a numbering mistake.

Next: implement the bounded required native GUI surface generically, then one Townfall launch showing actual selected UE4SS C++ window(s), action feedback, close/reopen and gameplay input restored. Lua-only toolkit success proves UMG primitives, not native ImGui compatibility. No kernel work or three-restart requirement.

No console, build, dependency installation, runtime edits or staged patches here. Confined host test receipt is HOST-TEST.log.

Update: executable declared-coverage gate now refuses all 76 unsupported native ImGui APIs; see COVERAGE-GATE.md. This is honest gap detection, not GUI success.

Native GUI source audit: NATIVE-GUI-SEAM.md identifies absent host/backend ownership and pre-uninstall callback retirement seams. No enabling registry/context patch was staged because those prerequisites are not supplied. Exact source hashes and actual Git-root provenance checks are recorded.

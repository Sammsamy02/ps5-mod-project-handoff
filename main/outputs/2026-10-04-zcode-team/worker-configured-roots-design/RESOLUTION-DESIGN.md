# RESOLUTION-DESIGN.md — configured roots (B) vs real-path discovery (A)

Worker: worker-configured-roots-design · 2026-10-04 · PROPOSALS ONLY — nothing
here edits wiring, frozen scripts, receipts or staged roots. All diffs target
the canonical files named in CONSUMER-USES.md.

Canonical file map (all absolute, verified on disk):

| Unit | Canonical path (relative to worktree root) | Composition role today |
|---|---|---|
| Adapter | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` | extra unit `lmf-loader-adapter` (freshly compiled every composition run) |
| Adapter header | `…/ue4ss-native-tick-file-owner-1/include/lmf_to_native_loader_facts.hpp` | staged header (overlay, pinned `a5bc3b70…`) |
| Facts/context header | `…/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp` | staged header (overlay, pinned `18c76cdc…`) |
| Context factory | `…/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp` | affected unit `native-loadercontext` (freshly compiled; staged source == original path) |
| Program context header | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp` | port include of the `program` unit; ALSO staged flat into the packaging overlay (overlay copy verified byte-identical to the port copy) |
| Program | `…/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp` | retained unit `program` (object hash-pinned from the base receipt `ue4ss-native-notify-cancel-1/build-1`) |
| Bridge | `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r11.cpp` (current packaged candidate; driver default is r9) | packaging-time lifecycle unit via overlay `--stage-unit` |
| Entry | `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry_r3.c` | packaging-time lifecycle unit; NOT changed by this design |

Composition facts that bound the mechanics (verified in
`worker-composition-4-fix/build_runtime_composition4_fixed.py`):
- `:59` strictly re-checks EVERY unit's original `source`/`source_sha256`
  (including `program` and `native-loadercontext`) — no attestation fallback
  on that loop. Editing those source files under the CURRENT fixed script
  refuses with "Changed input".
- `:61` asserts the affected-unit set exactly equals
  `{unreal-unrealinitializer, native-ownedmappedscansource,
  native-psscanbinding, native-ownedmappedcandidateselection,
  native-loadercontext, native-programfactory}`; `:79` restricts
  `--replace-runtime-unit` to that set.
- `--extra-unit` (`lmf-loader-adapter` …) recompiles fresh every run against
  the loader-context argv template; sources are covered by the
  `--extra-receipt` attestation (`worker-query-s2-arm/integration-patch/
  receipts/composition4-extra-receipt.json`).
- Header restage without a pin already exists: `build_overlay` treats a
  `--stage-header REL=SRC` override as unpinned ("explicit override, no pin"),
  hash-recording what it staged (build_runtime_composition5.py:317-321).
- The overlay manifest of the packaged candidate
  (`worker-composition-5-rebuild/staging/overlay-manifest.json`) shows the
  controller already restages `Platform/NativeProgramContext.hpp` +
  `NativeLoaderContext.hpp` + `lmf_to_native_loader_facts.hpp` per run — a
  restage with edited content is the same operation with new files.

---

## 1. Design invariants (the "distinct" requirement, made checkable)

1. **Game-image identity** lives ONLY in `NativeLoaderFacts.modules`
   (MainExe base/size/readable_ranges from the VmScan anchor) and
   `NativeProgramContext.module_source`/`scanner_source`. The configured
   fields NEVER feed the scanner, candidate selection, or identity checks.
2. **Runtime-module path** stays `module_file_path` (our PRX,
   `/data/plugins/modbridge_supplier_composition.prx`) with its UNCHANGED
   hard gates (lexical + `is_regular_file` — launch-8 proved this stat works
   in-process for /data).
3. **Configured directories** are new explicit fields
   (`configured_game_directory`, `configured_working_directory`), absolute
   lexical-validated strings carrying PROVENANCE "configured" in every
   diagnostic. They are never presented as kernel-observed and never
   substituted for the executable file path.
4. **The PRX is never served as `game_executable_path`.** The r9 source=3
   rung (own-module substitution) stays removed (r10/r11 already removed it;
   launch-8 ledger: "user directive enforced").
5. Default-refusal semantics preserved: every relaxation is behind an explicit
   opt-in flag, mirroring `allow_vm_scan_base_as_module_image_base`.

---

## 2. Resolution B — exact diffs per unit

### 2.1 `Platform/NativeProgramContext.hpp` (port tree; restage overlay copy IDENTICAL)

```diff
--- a/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp
+++ b/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp
@@ -64,6 +64,20 @@ namespace RC::Platform
         // Optional retained activation files. Presence selects immutable logical
         // ue4ss/ keys for config and mod loading, with no disk fallback.
         std::shared_ptr<const NativeModSource> mod_source;
+        // Resolution B (configured roots, 2026-10-04): explicit directory roots
+        // supplied by the native bootstrap. These are CONFIGURATION values,
+        // never kernel-observed paths. They are distinct from game-image
+        // identity (module_source / the retained VmScan anchor facts) and from
+        // the runtime-module path (module_file_path, the loader's own PRX).
+        // When game_executable_path is empty (the executable path is genuinely
+        // unobservable on this target: sysctl KERN_PROC_PATHNAME ENOENT, exec
+        // record kve_path region all zeros — launch-7/launch-8),
+        // configured_game_directory is REQUIRED and carries the directory
+        // derivations UE4SS previously took from
+        // game_executable_path.parent_path(). Either field may be empty when
+        // an observed executable path is present.
+        std::filesystem::path configured_game_directory;
+        std::filesystem::path configured_working_directory;
         // Deferred avoids enumerating, installing, or starting either mod type
         // during construction. A bootstrap may call start_native_mods() once
         // before init(), or deliberately opt into constructor autostart.
@@ -81,32 +95,53 @@ namespace RC::Platform
 
     inline auto ValidateNativeProgramContext(const NativeProgramContext& context) -> void
     {
-        for (const auto* path : {&context.module_file_path, &context.game_executable_path})
+        auto lexical_path_valid = [](const std::filesystem::path& path) {
+            const auto& bytes = path.native();
+            return !(path.empty() || !path.is_absolute() || !path.has_filename() ||
+                     path.filename() == "." || path.filename() == ".." ||
+                     path.lexically_normal() != path ||
+                     bytes.find('\0') != std::filesystem::path::string_type::npos);
+        };
+        // Runtime-module path: the hard lexical gate is UNCHANGED.
+        if (!lexical_path_valid(context.module_file_path))
+            throw std::invalid_argument("Native program paths must be absolute normalized file paths without NUL");
+        const bool has_game_executable = !context.game_executable_path.empty();
+        if (has_game_executable)
         {
-            const auto& bytes = path->native();
-            if (path->empty() || !path->is_absolute() || !path->has_filename() ||
-                path->filename() == "." || path->filename() == ".." ||
-                path->lexically_normal() != *path || bytes.find('\0') != std::filesystem::path::string_type::npos)
-                throw std::invalid_argument("Native program paths must be absolute normalized file paths without NUL");
+            // An observed executable path keeps the original hard lexical gate.
+            if (!lexical_path_valid(context.game_executable_path))
+                throw std::invalid_argument("Native program paths must be absolute normalized file paths without NUL");
         }
+        else
+        {
+            // Resolution B: no observed executable path exists on this target.
+            // The configured game directory is then REQUIRED and must itself be
+            // a clean absolute path; the configured working directory is
+            // optional but validated when present. Configured roots are
+            // directories: no regular-file requirement, no fabrication of a
+            // file path from them.
+            if (context.configured_game_directory.empty() ||
+                !lexical_path_valid(context.configured_game_directory))
+                throw std::invalid_argument("Native program requires an observed game executable path or an explicit configured game directory");
+            if (!context.configured_working_directory.empty() &&
+                !lexical_path_valid(context.configured_working_directory))
+                throw std::invalid_argument("Native configured roots must be absolute normalized directory paths without NUL");
+        }
         if (!context.module_source)
             throw std::invalid_argument("Native program requires an owning loader module snapshot");
         // File availability is a narrow startup prerequisite. It establishes
         // neither image identity nor a mapping-to-path relationship.
+        // The runtime-module file gate is UNCHANGED (proven stat-able
+        // in-process, launch-8). The executable-file gate applies only when an
+        // executable path was actually observed; configured directories never
+        // pass through is_regular_file.
         if (!std::filesystem::is_regular_file(context.module_file_path) ||
-            !std::filesystem::is_regular_file(context.game_executable_path))
+            (has_game_executable && !std::filesystem::is_regular_file(context.game_executable_path)))
             throw std::invalid_argument("Native module and executable paths must name existing regular files");
-        (void)std::filesystem::file_size(context.game_executable_path);
+        if (has_game_executable)
+            (void)std::filesystem::file_size(context.game_executable_path);
     }
 }
```

Notes:
- The launch-6 evidence string is kept verbatim for the module/exe file gate so
  historical diagnostics stay interpretable; a new dedicated message appears
  only for the missing-both case.
- **ODR gate (new pre-flight requirement):** this header is compiled into the
  recompiled `program` unit (port include path) AND staged flat into the
  packaging overlay for the bridge/factory TUs. The driver must sha-compare
  the port copy against the restaged overlay copy and refuse drift — today
  they are byte-identical (diff-verified), the design keeps that invariant
  mechanical.

### 2.2 `NativeLoaderContext.hpp` (W/include; staged header)

```diff
--- a/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp
+++ b/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp
@@ -9,9 +9,15 @@ namespace RC::Platform {
 struct NativeLoaderFacts {
     std::filesystem::path module_file_path;
     std::filesystem::path game_executable_path;
+    // Resolution B: explicit configured roots (see NativeProgramContext).
+    // Carried through MakeNativeProgramContext into the context unchanged.
+    // Configuration, never a kernel-observed path; empty by default.
+    std::filesystem::path configured_game_directory;
+    std::filesystem::path configured_working_directory;
     Unreal::Platform::ModuleInput modules;
     Unreal::Platform::OwnedMappedImageFacts mapped;
     std::shared_ptr<Input::PlatformInputSource> input_source;
     Unreal::Platform::CandidateSelectionPolicy scan_policy{Unreal::Platform::CandidateSelectionPolicy::OriginalOnly};
 };
```

Aggregate-safety: `NativeLoaderFacts` is only value-initialized
(`LmfConversionResult{}`) and field-assigned by name (adapter, bridge); no
positional aggregate init exists in any composed unit (grepped). All TUs that
touch the type (adapter extra unit, `native-loadercontext` affected unit,
bridge packaging unit) recompile in the same run against the same staged
header; retained units never name it.

### 2.3 `NativeLoaderContext.cpp` (W/src; affected unit `native-loadercontext`)

```diff
--- a/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp
+++ b/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp
@@ -12,6 +12,8 @@ auto MakeNativeProgramContext(NativeLoaderFacts facts,const std::vector<uint8_t>
     NativeProgramContext context{};
     context.module_file_path=std::move(facts.module_file_path);
     context.game_executable_path=std::move(facts.game_executable_path);
+    context.configured_game_directory=std::move(facts.configured_game_directory);
+    context.configured_working_directory=std::move(facts.configured_working_directory);
     context.module_source=snapshot;
     context.input_source=std::move(facts.input_source);
     ValidateNativeProgramContext(context);
```

Validation order note: the copy happens BEFORE `ValidateNativeProgramContext`,
so the "executable empty ⇒ configured game directory required" rule is
enforced at the single choke point, and the UE4SSProgram constructor's
re-validation (`UE4SSProgram.cpp:215`, same inline function) sees a context
that already satisfies it.

### 2.4 `lmf_to_native_loader_facts.hpp` (W/include; staged header)

```diff
--- a/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/lmf_to_native_loader_facts.hpp
+++ b/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/lmf_to_native_loader_facts.hpp
@@ -26,8 +26,12 @@
 //   * game_executable_path <- snapshot.executable_path (sysctl
 //     KERN_PROC_PATHNAME, authoritative). Empty -> refuse, never fabricate,
-//     never silently fall back to the link-map image name.
+//     never silently fall back to the link-map image name — EXCEPT behind the
+//     explicit Resolution-B opt-in
+//     allow_configured_roots_for_missing_executable_path, which converts with
+//     the field left EMPTY (never fabricated) and hands directory derivation
+//     to the caller-supplied configured roots on NativeLoaderFacts.
@@ -130,6 +134,13 @@ struct LmfConversionOptions {
     // in-memory-ELF-scan chosen base (IdentitySource::VmScan) to resolve the
     // module image base of a ModuleInfoNotObtained snapshot. Without it, a
     // VmScan-identity snapshot converts never — the same refusal as before
     // this update.
     bool allow_vm_scan_base_as_module_image_base = false;
+    // Explicit opt-in, default REFUSE (Resolution B, 2026-10-04): allow an
+    // EMPTY observed executable path to convert, leaving NativeLoaderFacts::
+    // game_executable_path empty, when the caller supplies explicit configured
+    // roots on the facts. The consuming context then requires
+    // configured_game_directory (ValidateNativeProgramContext). The refusal
+    // message and behavior are unchanged when this is false, and a NON-empty
+    // observed path is unaffected either way.
+    bool allow_configured_roots_for_missing_executable_path = false;
 };
```

### 2.5 `lmf_to_native_loader_facts.cpp` (W/src; extra unit `lmf-loader-adapter`)

```diff
--- a/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp
+++ b/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp
@@ -138,17 +138,21 @@ 
     // Authoritative sysctl executable path: empty -> refuse, never fabricate,
-    // never fall back to the link-map image name.
+    // never fall back to the link-map image name. Resolution B opt-in
+    // (allow_configured_roots_for_missing_executable_path): an empty OBSERVED
+    // path converts with the field left EMPTY — the directory derivations move
+    // to the caller-supplied configured roots, enforced by
+    // ValidateNativeProgramContext. The path is still never fabricated here.
     const char* observed_path = snapshot.executable_path;
-    if (observed_path == nullptr || observed_path[0] == '\0') {
-        result.error = LmfConversionError::GameExecutablePathMissing;
-        result.error_message = "observed executable path is empty; refusing to fabricate game_executable_path";
-        return;
-    }
-    const std::filesystem::path executable_path{std::string{observed_path}};
-    if (!path_lexically_valid(executable_path)) {
-        result.error = LmfConversionError::GameExecutablePathInvalid;
-        result.error_message = "observed executable path is not an absolute normalized file path";
-        return;
+    const bool observed_path_present = observed_path != nullptr && observed_path[0] != '\0';
+    std::filesystem::path executable_path{};
+    if (observed_path_present) {
+        executable_path = std::filesystem::path{std::string{observed_path}};
+        if (!path_lexically_valid(executable_path)) {
+            result.error = LmfConversionError::GameExecutablePathInvalid;
+            result.error_message = "observed executable path is not an absolute normalized file path";
+            return;
+        }
+    } else if (!options.allow_configured_roots_for_missing_executable_path) {
+        result.error = LmfConversionError::GameExecutablePathMissing;
+        result.error_message = "observed executable path is empty; refusing to fabricate game_executable_path";
+        return;
     }
```

(`facts.game_executable_path = executable_path;` at `:224` is unchanged — it
now assigns an empty path exactly when nothing was observed.)

### 2.6 `UE4SSProgram.cpp` (port; retained unit `program` — needs the §3.3 mechanics)

Hunk 1 — startup banner guard (the only startup-functional non-directory use):

```diff
--- a/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp
+++ b/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp
@@ -423,8 +423,17 @@ 
             Output::send(STR("Config: {}\n\n"), ensure_str(m_settings_path_and_file));
             Output::send(STR("root directory: {}\n"), ensure_str(m_root_directory));
             Output::send(STR("working directory: {}\n"), ensure_str(m_working_directory));
             Output::send(STR("game executable directory: {}\n"), ensure_str(m_game_executable_directory));
+#ifdef __PROSPERO__
+            if (!m_game_path_and_exe_name.empty())
+                Output::send(STR("game executable: {} ({} bytes)\n\n\n"), ensure_str(m_game_path_and_exe_name), std::filesystem::file_size(m_game_path_and_exe_name));
+            else
+                Output::send(STR("game executable: (unobserved on this platform; configured game directory: {})\n\n\n"), ensure_str(m_native_context.configured_game_directory));
+#else
             Output::send(STR("game executable: {} ({} bytes)\n\n\n"), ensure_str(m_game_path_and_exe_name), std::filesystem::file_size(m_game_path_and_exe_name));
+#endif
             Output::send(STR("mods directories: \n"));
```

Hunk 2 — configured-root derivations (PROSPERO branch of `setup_paths`):

```diff
--- a/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp
+++ b/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp
@@ -757,19 +757,30 @@ 
 
 #ifdef __PROSPERO__
-        const std::filesystem::path& game_exe_path = m_native_context.game_executable_path;
-#else
-        wchar_t exe_path_buffer[1024];
-        GetModuleFileNameW(GetModuleHandle(nullptr), exe_path_buffer, 1023);
-        std::filesystem::path game_exe_path = exe_path_buffer;
-#endif
+        // Resolution B (configured roots): the game executable path is
+        // legitimately unobservable on this target. Directory derivations come
+        // from the explicit configured game directory when supplied; image
+        // identity lives in the retained mappings, never in this string.
+        std::filesystem::path game_directory_path;
+        if (!m_native_context.configured_game_directory.empty())
+            game_directory_path = m_native_context.configured_game_directory;
+        else
+            game_directory_path = m_native_context.game_executable_path.parent_path();
+        m_game_path_and_exe_name = m_native_context.game_executable_path; // may be empty: unobserved
+#else
         std::filesystem::path game_exe_path{};
         {
             wchar_t exe_path_buffer[1024];
             GetModuleFileNameW(GetModuleHandle(nullptr), exe_path_buffer, 1023);
             game_exe_path = exe_path_buffer;
         }
         std::filesystem::path game_directory_path = game_exe_path.parent_path();
+        m_game_path_and_exe_name = game_exe_path;
+#endif
         m_legacy_root_directory = game_directory_path;
 
         m_working_directory = m_root_directory;
         m_game_executable_directory = game_directory_path;
         m_settings_path_and_file = m_root_directory;
-        m_game_path_and_exe_name = game_exe_path;
         m_object_dumper_output_directory = m_working_directory;
```

Behavior notes:
- PROSPERO+mod_source short-circuit (`:776-784`) is untouched: log dir stays
  `m_working_directory` (module dir), settings stay
  `m_root_directory/"UE4SS-settings.ini"`. `configured_working_directory` is
  carried and validated but deliberately NOT consumed in this change-set —
  changing `m_working_directory` would move the log directory that launch
  evidence and tooling already locate at the module dir. It exists so the
  contract carries both roots distinctly from day one (log relocation would be
  a separate, separately-evidenced change).
- `get_game_executable_path()` (`:2573`) and `get_game_executable_directory()`
  (`:2579`) need NO signature changes: the former may now return an empty path
  (consumers: UEHeaderGenerator/JMapGenerator name-only metadata, see
  CONSUMER-USES B7b/B7c), the latter returns the configured directory, which
  is exactly what LuaMod's directory-walking Lua funcs expect semantically.

### 2.7 Bridge r12 (wiring; derived from packaged r11)

Two hunks. (a) after successful conversion, before `MakeNativeProgramContext`;
(b) a labeled constant near the top constants. r10/r11's stat-both diagnostic
block and the removal of the PRX rung are KEPT AS-IS.

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r11.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
@@ (constants, near the other bridge constants)
+// Resolution B (user directive 2026-10-04): the startup contract's
+// game_executable_path is NEVER satisfied by our own PRX. When no real path
+// is observable, the adapter converts with it EMPTY and these explicitly
+// CONFIGURED roots carry the directory derivations. Distinct provenance:
+// kConfiguredGameDirectory is a title-mount configuration value, never a
+// kernel-observed path; the working root is the entry's mod_root parameter.
+static const char *kConfiguredGameDirectory = "/app0";
+
@@ (Stage 1 -> Stage 2, after the `if (!converted)` block)
         // ---- Stage 2: NativeProgramContext build + validation. Throws on
         // refused/invalid facts or missing regular files (both paths must
         // exist NOW, while /data is still reachable in module_start).
+        // Resolution B: with the observed path empty (the honest r10/r11
+        // refusal), convert behind the configured-roots opt-in and carry the
+        // configured roots on the facts. A real observed path (rung 1/2/3)
+        // still fills game_executable_path and the configured roots simply
+        // ride along, unused by the derivations.
+        if (converted.facts.game_executable_path.empty())
+        {
+            options.allow_configured_roots_for_missing_executable_path = true;  // see note
+        }
```

MECHANICS CORRECTION (the opt-in must be set BEFORE the conversion call, not
after it): the flag belongs in the options block that already exists at
r11:595-599 —

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r11.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
@@ -595,6 +595,9 @@
         RC::Platform::LmfConversionOptions options{};
         // In-game primary identity route (checkpoint zcode_ingame_v1_results):
         // VMMAP span + in-memory ELF base scan. Explicit opt-in, default is
         // refusal; without it a VmScan-identity snapshot never converts.
         options.allow_vm_scan_base_as_module_image_base = true;
+        // Resolution B (configured roots): the observed executable path is
+        // empty on this target. Converting with it EMPTY (never fabricated)
+        // behind this explicit opt-in; ValidateNativeProgramContext then
+        // requires configured_game_directory, set on the facts below.
+        options.allow_configured_roots_for_missing_executable_path = true;
 
         const RC::Platform::LmfConversionResult converted =
             RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
                 retained, std::filesystem::path(module_file_path), options);
@@ (after the `if (!converted) { … return MRS_ERR_CONVERSION; }` block, before Stage 2's try)
+        // Resolution B: attach the EXPLICIT CONFIGURED ROOTS. mod_root is the
+        // entry's MODBRIDGE_MOD_ROOT ("/data/modbridge/roots/PPSA33286"); the
+        // game directory is the configured title-mount prefix "/app0".
+        // Diagnostics label them configured (sig prefixes like exepath).
+        converted.facts.configured_game_directory = std::filesystem::path(kConfiguredGameDirectory);
+        converted.facts.configured_working_directory = std::filesystem::path(mod_root);
+        ::diagnostic_event("bridge_cfg_gamedir", 1u);
+        ::diagnostic_event("bridge_cfg_workdir", 1u);
 
         // ---- Stage 2: NativeProgramContext build + validation. Throws on
```

Also update the r11 ladder comment block (`:375-384`) to document source
labels: 1 kernel-observed, 2 configured-existing-file, 0 none → Resolution B
configured-roots conversion (label the conversion path with
`bridge_exepath_source=0` + the two new `bridge_cfg_*` events so a log reader
can always distinguish "real path served" from "configured roots carried").
No ladder rung is added; no entry change.

---

## 3. Integration path (composition mechanics, per unit)

### 3.1 Rides existing mechanics (no script change)

- **`lmf-loader-adapter`** (extra unit): same channel D2 option (c) uses —
  new source bytes + updated hash in the `--extra-receipt` attestation
  (`worker-query-s2-arm/integration-patch/receipts/composition4-extra-receipt.json`).
- **Staged headers** `NativeLoaderContext.hpp`, `lmf_to_native_loader_facts.hpp`,
  `Platform/NativeProgramContext.hpp`: restage via the existing unpinned
  `--stage-header REL=SRC` override (build_runtime_composition5.py:317-321);
  the overlay manifest records measured hashes automatically.
- **`native-loadercontext`** (affected unit): currently recompiled from its
  original path with the ORIGINAL hash strictly checked at
  build_runtime_composition4_fixed.py:59. Editing
  `W/src/NativeLoaderContext.cpp` in place breaks that check → needs §3.3's
  attested-source-override, OR (zero script change) stage the edited copy at a
  NEW path and point the script's `new_sources['native-loadercontext']` at it —
  but `new_sources` is itself inside the frozen script (sha-pinned by the
  composition-5 driver, `36cc8199…`). Either way §3.3 is required; the
  attested-override form is preferred because it is reusable.

### 3.2 Requires the port-tree edits to be attested (script edition REQUIRED)

Editing `UE4SSProgram.cpp` (unit `program`, retained) or the port
`Platform/NativeProgramContext.hpp` breaks, under the current fixed script:
(a) the `:59` strict unit-source check (`program`), (b) the
`native-coherent-runtime-1/build_runtime.py::validate_inputs()` pin set for
the port header (`:51`), (c) the affected-set assertion `:61` if any newly
edited unit were treated as affected. Therefore Resolution B ships with **one
new bounded fixed-script edition** (call it `build_runtime_composition6_fixed.py`,
same class as the composition-4-fix edition — frozen gates unchanged, additive
mechanism only):

1. Pre-seed `pins` and skip-relax `:59` for EXACTLY the files listed in a new
   `--replace-receipt`-attested map (rr built BEFORE the units loop; the
   composition4-fix lineage already demonstrated this class of pre-motion fix).
2. Extend `--replace-runtime-unit` eligibility with an explicit, attested
   retained-unit allow-list, initially `{"program"}`; assert the replaced
   unit recompiles with its OWN pinned argv (same flags/includes, only source
   and `-o`/`-MF` redirected) and that its dependency walk (fresh `.d`) is
   fully covered by pins ∪ rr.
3. Keep the `:61` affected-set assertion for the ORIGINAL affected set and
   track retained replacements in a separate receipt field
   (`retained_units_replaced`), so old receipts stay diffable.
4. Link-order: `program.o` replacement participates in the existing
   `replacements` map exactly like a runtime-unit replacement.

The full port-generation route (a new `native-coherent-runtime-1` generation
via `compose_runtime.py` + new base receipt) remains the consolidation path
later; it is NOT required for the first console-validated iteration and is
strictly heavier.

### 3.3 Order of operations for the controller (one pass, when evidence lands)

1. Land source edits: §2.1–§2.7 (adapter + headers + context factory + port
   program/header + bridge r12). Port copy of `Platform/NativeProgramContext.hpp`
   and the staged copy MUST be byte-identical (add the sha-equality pre-flight
   to the composition-6 driver).
2. Regenerate attestations: extra-receipt (adapter), replace-receipt
   (`NativeLoaderContext.cpp` staging copy or override, `UE4SSProgram.cpp`,
   `Platform/NativeProgramContext.hpp` port copy, and every port file the
   fresh `program.d` dependency walk touches).
3. Restage headers + bridge r12 via `--stage-header` / wiring channel.
4. Run the composition-6 driver; require 17/17 static checks (the accepted
   composition-4/5 set) + the new ODR sha-equality gate + export-surface
   pre-flight (the 10 `DEFAULT_REQUIRE_SYMBOLS` are unaffected — none of the
   edited units defines or drops them; `MakeNativeProgramContext` and
   `TryMakeNativeLoaderFactsFromSupplier` keep their exact mangled names since
   only field/layout-internal changes land, and both the caller (bridge) and
   callee recompile together).
5. Package → new PRX candidate → controller-owned in-game launch.

Expected next-launch shape (honest prediction): either rung 3 fires
(`bridge_exepath_source=2`… note r10/r11 label `/app0/eboot.bin` as source=2)
if the r10 stat diagnostic shows `/app0/eboot.bin` stat-able after all, or
`bridge_exepath_source=0` + `bridge_cfg_gamedir/workdir=1` and startup
continues past `GameExecutablePathMissing` into Stage 2 (where the r11
page-quantized lease fix gets its first real exercise), then D2's gates.

---

## 4. Resolution A — where a REAL eboot path could come from

### 4.1 Evidence-backed sources (this project's own in-game data)

| Source | Status | Evidence |
|---|---|---|
| `sysctl(CTL_KERN, KERN_PROC_PATHNAME, -1)` | **DEAD** | ENOENT in-game (launch evidence; `live_module_os_ops_sce.cpp:110-119` returns −1) |
| Exec-record `kve_path` (VMMAP record covering the anchored base) | **DEAD** | Region is ZEROS (launch-7 sig 0; `runtime_start_bridge_r9.cpp:262-282` honest NUL-in-record capture) |
| r9 kve DISCOVERY scan (all records, 4 distinct 8-byte prefixes + eboot-substring flag) | **DEAD on this kernel** | launch-8: "kve discovery: ALL ZERO — no mapping on this kernel carries ANY kve_path bytes" (`zcode_composition5_launch8_result`, transport-ledger mirror) |
| Configured `/app0/eboot.bin` stat | **FAILED so far, DECIDING PENDING** | launch-7 `is_regular_file == FALSE`; caveat (is_regular_file itself unproven in-process) is being closed by the r10 stat-both diagnostic (`stat_app0`, `stat_ownmod`, `stat_rootdir` + errno). Counter-evidence that the PREDICATE works: launch-8 passed `ValidateNativeProgramContext` with `is_regular_file("/data/plugins/…prx") == true` in the same libc++ |
| `sceKernelGetModuleInfo` (module-info span) | **DEAD in-game** | observed `ModuleInfoNotObtained` / domain mismatch (launch-2 provider_code −4002; in-game updates) |
| DT_DEBUG link-map walk | **DEAD** | observed g2: DT_DEBUG unfilled (launch-2) |
| rtld `observed_base` / dlopen-handle domain | **DEAD / unproven** | `bridge_flag=false (unproven handle-domain equality)` (r9:94); NoImageAnchor launch-1 |

### 4.2 Title-mount layout enumeration (marking evidence status)

| Layout | Status in THIS project | Note |
|---|---|---|
| `/app0` (standard PS5 title mount; `eboot.bin` at its root) | Evidence-adjacent: used as the bridge's configured rung 3; stat FAILED in launch-7; r10 decides | The only mount name the project has ever exercised in-process |
| `/data/modbridge/roots/PPSA33286` (activation/mod root) | Evidence-backed as EXISTING (activation passed 8×; host_test s1 asserts it) — but it is NOT the game mount; it is the mod/config side | Correct role: `configured_working_directory` |
| `/data/plugins` (our PRX + UE4SS.log) | Evidence-backed (module path stat-able, launch-8) — runtime-module side, never the game side | Correct role: `module_file_path` (already) |
| `/mnt/app0`, `/system_ex/app/<title_id>/…`, `/user/app0`, sandboxed `pfs` mirrors | **SPECULATIVE** — zero mentions in the entire worktree (grepped docs/outputs/observations); no in-process evidence; typical PS5 game-process sandboxes would not see `/system_ex` anyway | Do NOT design against these without a diagnostic that observes them in-process first |
| `/proc/self/exe`-style procfs routes | **SPECULATIVE** — never probed on this firmware; PS5 game processes expose a heavily restricted procfs | At most a future one-shot diagnostic event, never a startup dependency |

### 4.3 Proof standard for converting a discovered prefix into the accepted path

A candidate string P may become `game_executable_path` ONLY if ALL hold:
1. **Provenance**: P is produced in-process by a kernel/loader interface
   (sysctl output, NUL-terminated kve_path, or an equivalent observed
   interface) — never a constant guess. (A constant that PASSES stat is rung-3
   "configured-existing", which is honest but is labeled configured, not
   observed.)
2. **Existence through the consumer's own predicate**:
   `std::filesystem::is_regular_file(P)` via the composition's own libc++
   returns true (the exact gate `ValidateNativeProgramContext` runs).
3. **Identity binding to the retained image**: read the on-disk file's ELF
   header + the GNU build-id window and match it against the already-validated
   in-memory window (window A: 36-byte GNU build-id at p_vaddr `0xBC5F250`,
   igf7-validated against retained eboot sha `3b6cfad4…`). Path→image binding
   is then proven, not assumed — satisfying the header's own rule
   ("The loader must accurately bind these paths to the retained module
   snapshot", NativeProgramContext.hpp:56-58).

With kve all-zero and sysctl ENOENT, no current source passes (1); Resolution
A has no live route on this kernel/firmware today. The r10 stat diagnostic is
the only near-term event that can change A's status (by proving the predicate
and, unexpectedly, `/app0/eboot.bin`).

---

## 5. Decision matrix — what to integrate NOW

Inputs: (i) console tests pending; (ii) r10 stat-both diagnostic decides
whether `is_regular_file` works in-process at all; (iii) D2/text_base
composition change-set is being designed in parallel
(worker-d2-textbase-design, D2-DESIGN.md).

| Situation after the next r10/r11-instrumented launch | Resolution to integrate | Why |
|---|---|---|
| `stat_ownmod=1, stat_app0=0` (expected: predicate works, /app0/eboot.bin genuinely absent) | **B, fully** | No observable path exists; B is the only honest startup route; A is dead by evidence |
| `stat_ownmod=0` (predicate broken in-process) | **B still, with one flagged review** | Contradicts launch-8 (the same predicate passed for the PRX in the SAME binary class) → suspect the diagnostic/instrumentation first; B's configured-root gates are deliberately lexical-only and survive a broken-stat world, but the module-file regular-file gate (kept in §2.1) would then also be void — that single gate gets its own review before any launch, not a design change |
| `stat_app0=1` (unexpected: /app0/eboot.bin stat-able) | **B still; A "accidentally available"** | Rung 3 then serves a REAL stat-able path and startup works without the opt-in ever firing; B still lands as the honest contract (optionality + configured roots) because the path is configured-existing, not observed, and A's proof standard (§4.3 identity binding) remains unmet |
| kve discovery ever returns a NUL-terminated path (future firmware) | A via kve, then B stays as fallback | §4.3 proof standard; the adapter opt-in makes the two resolutions composable, not competing |

**Recommendation: prepare Resolution B NOW, integrate on the next launch
window.** Concretely: land §2.1–§2.7 sources + §3.2 script edition + receipts
in this batch so the moment console evidence lands the rebuild is ONE
controller pass (the composition-5-rebuild precedent). Rationale:
- launch-8 already adjudicated the fork ("Resolution A (real path) DEAD on
  this kernel; Resolution B … is the only path" — transport ledger), and the
  binding user directive names B as the design target.
- The r10 diagnostic cannot rescue A: its best case makes rung 3 work, which
  B tolerates unchanged (the opt-in simply never fires).
- Nothing in B depends on unproven in-process filesystem semantics beyond the
  module-file gate that launch-8 already exercised.

### D2/text_base coordination (scope deconfliction)

- **Shared unit**: `lmf_to_native_loader_facts.cpp`. D2 option (c)
  ("absence-honored text_base", its recommended rank 1) edits the
  `facts.mapped.*` assignment block (`:225-236`) and header contract comments;
  this design edits the executable-path gate (`:139-152`) and the options
  struct. **Hunks are disjoint; merge BOTH into one next adapter revision** so
  only one extra-receipt regeneration is needed. Same for the two shared
  header files each touches (`lmf_to_native_loader_facts.hpp` comments/options;
  no overlap with D2's `PsMappedImageCaller.hpp`/`OwnedMappedImageCaller.hpp`).
- **Disjoint otherwise**: D2 needs NO fixed-script change (its ranked option
  was chosen for exactly that); this design OWNS the one script edition
  (§3.2) and the port-tree edits. No collision on staged headers (sets are
  disjoint), on packaging units (D2 touches none), or on the overlay.
- **Sequencing**: land both change-sets in ONE composition-6 pass (single
  17/17 gate, single restage, single receipt batch); if D2 slips, B integrates
  alone without rework — the merged adapter revision simply carries only B's
  hunks that round.
- Read-and-cited: `worker-d2-textbase-design/D2-DESIGN.md` (its §4 integration
  path and affected-inventory analysis `composition4_fixed.py:57-61,79` — the
  same constraint this design's §3.2 resolves for `program`).

### Residual honest limits of this design

- No console contact was made; every runtime claim cites an existing ledger
  entry or staged source. Nothing here claims the next launch succeeds —
  Stage 2's lease gate (r11 fix) and D2's text gates are downstream and
  separately evidenced.
- `configured_working_directory` is carried and validated but not yet consumed
  by UE4SSProgram (log relocation is a separate, separately-evidenced change).
- The two name-only consumers (SDK gen module name, JMap metadata) degrade to
  empty names under B; hardening them (optional follow-up, same §3.2
  mechanism, units `generator-sdkgenerator-ueheadergenerator` and
  `native-jmap`) is deliberately out of the minimal change-set.

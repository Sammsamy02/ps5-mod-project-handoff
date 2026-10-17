# staging-d1 provenance diffs (staged copy vs source), final regenerated 2026-10-05T02:21:46+00:00Z
# 9 files: 4 D2-content copies (rehearsal markers + quoted overrides), 5 frozen intercept copies (quoted conversions only)
# syntax-only pre-flight with the exact run-3 lmf-loader-adapter argv: PASS

## lmf_to_native_loader_facts.cpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/comp6-integration/lmf_to_native_loader_facts.cpp
```
--- <D2>/lmf_to_native_loader_facts.cpp	2026-10-04 21:11:13
+++ <MY>/lmf_to_native_loader_facts.cpp	2026-10-04 21:16:21
@@ -323,3 +323,11 @@
 }
 
 }  // namespace RC::Platform
+
+// comp6-drybuild rehearsal marker (worker-comp6-drybuild): proves THIS staged
+// adapter copy (D2 Diff A + Resolution B content) reached the composed object.
+namespace RC::Platform { namespace comp6_drybuild {
+auto adapter_staging_marker() -> const char* {
+    return "comp6-drybuild lmf_to_native_loader_facts.cpp staging override (D2 Diff A + Resolution B) reached codegen";
+}
+}}
```

## lmf_to_native_loader_facts.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/comp6-integration/lmf_to_native_loader_facts.hpp
```
--- <D2>/lmf_to_native_loader_facts.hpp	2026-10-04 21:11:13
+++ <MY>/lmf_to_native_loader_facts.hpp	2026-10-04 21:16:21
@@ -70,7 +70,16 @@
 #ifndef LMF_TO_NATIVE_LOADER_FACTS_HPP
 #define LMF_TO_NATIVE_LOADER_FACTS_HPP
 
-#include <NativeLoaderContext.hpp>
+// comp6-drybuild staging override: quoted include binds NativeLoaderContext.hpp
+// from THIS staging dir (same-dir rule) so the intercept chain below reaches the
+// D2 patched headers instead of the pinned -I frozen copies.
+#include "NativeLoaderContext.hpp"
+#ifndef WCD_DRYBUILD_STAGING_PS_HEADER
+#error "comp6-drybuild: staging PsMappedImageCaller.hpp (Diff B) did not win include resolution"
+#endif
+#ifndef WCD_DRYBUILD_STAGING_OWNED_HEADER
+#error "comp6-drybuild: staging OwnedMappedImageCaller.hpp (Diff C) did not win include resolution"
+#endif
 
 #include <cstdint>
 #include <filesystem>
```

## PsMappedImageCaller.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/comp6-integration/PsMappedImageCaller.hpp
```
--- <D2>/PsMappedImageCaller.hpp	2026-10-04 20:53:13
+++ <MY>/PsMappedImageCaller.hpp	2026-10-04 21:21:29
@@ -85,3 +85,7 @@
     const std::shared_ptr<const ModuleSnapshot>& source_owner()const noexcept{return owner_;}
 };
 }
+namespace comp6_drybuild {
+#define WCD_DRYBUILD_STAGING_PS_HEADER 1
+extern const char WCD_PS_DIFF_B_STAGING_MARKER[] = "comp6-drybuild: PsMappedImageCaller.hpp staging override (D2 Diff B text_base-absence rule) compiled into this object";
+}  // namespace comp6_drybuild (RC::Unreal::Platform closed above)
```

## OwnedMappedImageCaller.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/comp6-integration/OwnedMappedImageCaller.hpp
```
--- <D2>/OwnedMappedImageCaller.hpp	2026-10-04 20:53:13
+++ <MY>/OwnedMappedImageCaller.hpp	2026-10-04 21:21:29
@@ -1,5 +1,7 @@
 #pragma once
-#include <PsMappedImageCaller.hpp>
+// comp6-drybuild staging override: quoted include binds the STAGING D2
+// PsMappedImageCaller.hpp (Diff B) ahead of the pinned -I frozen copy.
+#include "PsMappedImageCaller.hpp"
 #include <polyhook2/OwnedMemory.hpp>
 #include <limits>
 #include <vector>
@@ -75,3 +77,7 @@
     }
 };
 }
+namespace comp6_drybuild {
+#define WCD_DRYBUILD_STAGING_OWNED_HEADER 1
+extern const char WCD_OWNED_DIFF_C_STAGING_MARKER[] = "comp6-drybuild: OwnedMappedImageCaller.hpp staging override (D2 Diff C text_base permission rule) compiled into this object";
+}  // namespace comp6_drybuild (RC::Unreal::Platform closed above)
```

## NativeLoaderContext.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp
```
--- <SPIKE>/ue4ss-native-tick-file-owner-1/include/NativeLoaderContext.hpp	2026-10-03 22:44:36
+++ <MY>/NativeLoaderContext.hpp	2026-10-04 21:16:34
@@ -1,5 +1,5 @@
 #pragma once
-#include <OwnedMappedScanSource.hpp>
+#include "OwnedMappedScanSource.hpp"  // comp6-drybuild: quoted -> staging intercept copy
 namespace RC::Platform {
 // Same-SDK C++ interface, not a portable C/STL ABI. A legitimate in-process
 // loader supplies actual retained mappings and already-existing owned-service
```

## OwnedMappedScanSource.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/OwnedMappedScanSource.hpp
```
--- <SPIKE>/ue4ss-native-tick-file-owner-1/include/OwnedMappedScanSource.hpp	2026-10-03 22:44:36
+++ <MY>/OwnedMappedScanSource.hpp	2026-10-04 21:16:34
@@ -1,7 +1,7 @@
 #pragma once
-#include <OwnedMappedCandidateSelection.hpp>
+#include "OwnedMappedCandidateSelection.hpp"  // comp6-drybuild: quoted -> staging intercept copy
 #include <memory>
-#include <OwnedSerializedElfSource.hpp>
+#include "OwnedSerializedElfSource.hpp"  // comp6-drybuild: quoted -> staging intercept copy
 namespace RC::Unreal::Platform {
 struct OwnedMappedImageFacts {
     uint32_t format{};
```

## OwnedMappedCandidateSelection.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/OwnedMappedCandidateSelection.hpp
```
--- <SPIKE>/ue4ss-native-tick-file-owner-1/include/OwnedMappedCandidateSelection.hpp	2026-10-03 22:44:36
+++ <MY>/OwnedMappedCandidateSelection.hpp	2026-10-04 21:16:34
@@ -1,5 +1,5 @@
 #pragma once
-#include <OwnedMappedResolverCollector.hpp>
+#include "OwnedMappedResolverCollector.hpp"  // comp6-drybuild: quoted -> staging intercept copy
 #include <patternsleuth_mapped_candidates.h>
 namespace RC::Unreal::Platform {
 enum class CandidateSelectionPolicy : uint8_t { OriginalOnly,CollectEvidenceOnly,SelectUniqueAfterNoMatch };
```

## OwnedMappedResolverCollector.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-mapped-candidate-selection-1/include/OwnedMappedResolverCollector.hpp
```
--- <SPIKE>/ue4ss-native-mapped-candidate-selection-1/include/OwnedMappedResolverCollector.hpp	2026-10-02 17:34:56
+++ <MY>/OwnedMappedResolverCollector.hpp	2026-10-04 21:16:34
@@ -1,5 +1,5 @@
 #pragma once
-#include <OwnedMappedImageCaller.hpp>
+#include "OwnedMappedImageCaller.hpp"  // comp6-drybuild: quoted -> STAGING D2 Diff C copy
 #include <array>
 #include <cstdint>
 
```

## OwnedSerializedElfSource.hpp  <=  /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/OwnedSerializedElfSource.hpp
```
--- <SPIKE>/ue4ss-native-tick-file-owner-1/include/OwnedSerializedElfSource.hpp	2026-10-03 22:47:45
+++ <MY>/OwnedSerializedElfSource.hpp	2026-10-04 21:19:56
@@ -1,5 +1,5 @@
 #pragma once
-#include <OwnedMappedImageCaller.hpp>
+#include "OwnedMappedImageCaller.hpp"  // comp6-drybuild: quoted -> STAGING D2 Diff C copy
 #include <memory>
 #include <vector>
 namespace RC::Unreal::Platform {
```

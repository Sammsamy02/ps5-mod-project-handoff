# Proposed composition diff (proposal only; originals immutable)

worker-real-input, 2026-10-04. When rung-5 composition lands, the hardened edge
source replaces the plain retained provider at the EXISTING seam — one call site,
`outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp:21-22`:

```diff
-        if (enable_buffered_mods && context.mod_source)
-            ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source);
+        if (enable_buffered_mods && context.mod_source)
+            ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source,
+                                         modbridge::input::PadEdgePolicy{});
```

and inside the retained `ConfigureRetainedPS5PadInput` (spike ue4ss-native-pad-config-1,
NativePadInput.cpp) the provider construction site would construct
`modbridge::input::PadEdgeSource` with the SAME parsed `PS5PadConfiguration` bindings
instead of `PS5PadSource` — same `PlatformInputSource` interface, same serialized
activate/process/deactivate thread ownership, so the Handler sees no difference.
Default policy min_rearm_us=40000 (one press = one spawn); 0 = exact retained parity.
No per-mod recipes are introduced: the shared ue4ss/PS5Pad.ini remains the only
binding surface. Requires linking PadEdgeSource.cpp into the runtime build
(append-objects pattern, integration-manifest "append_objects").

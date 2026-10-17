# Untouched original mod: full menu-graph requirements

**Mac-only model evidence.** The byte-identical Nexus V1.0 script executes in retained
Lua5.4.7. The original script SHA isf586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658.
The model checks explicit class/method/property/payload requirements; it does not
implement or exercise the runtime's native UMG/reflection/parameter marshalling.
No native widget, game-thread, controller or spawn success is claimed.

## Verified outcomes

56 cases pass with their expected outcomes:

- Full initial graph: rootUserWidget→WidgetTree→CanvasPanel→Border→Border→VerticalBox,
 18 TextBlocks including title/divider/control text, section headers and12 rows.
- Explicit root/tree/child outer relationships and ownership model; viewport request
 AddToViewport(9999), initial hidden enum1, then visible enum0.
- Close and reopen reuse the existing graph. The original map-reset callback clears
 its Lua references and the next press builds a new model graph.
- WidgetBlueprintLibrary.Create failure falls back to StaticConstructObject. Failure
 of both paths refuses the menu. Missing controller/library/class also refuses.
- Each of46 mandatory construction calls is fault-injected individually and prevents
 the original “Menu created.” result. This includes object creation, child attachment,
 content assignment, viewport insertion and the initial visibility call.
- Optional presentation/input-lock failures do not prevent creation, as the mod uses
 pcall. An existing menu therefore does not prove its text/styles/input lock worked.
- Two deliberate F7 closures queued before a drain execute two toggles: the menu opens
 then closes. A third press reopens. This is original-script behavior, not proof of
 any prior PS5 launch's failure cause.
- Two negative model controls are rejected: missing viewport record and wrong tree
 outer. The harness does not simply accept any returned object as a correct graph.

## Scope and limits

Class names, payload fields and graph expectations come from the unchanged release
script. They are requirements on the runtime, not validated native reflected schemas.
FName/FText are tagged host-model values, not native allocations. Lua tables stand in
 for native objects. The queue is a controlled host drain; use the separate55-check
 current-source chain evidence for actual Handler/LuaMod queue logic.

The original mandatory-failure handler clears Lua UI references. It does not explicitly
 destroy all already-created native objects. This model deliberately does not claim
 leak freedom or native GC/ownership correctness. Vendor Lua objects are retained
 uninstrumented; no sanitizer coverage of those objects is claimed here.

## Next native acceptance boundary

Preserve the mod unchanged. Validate current carrier/thread dispatch, real reflected
 object/class identities, StaticConstructObject/Create parameter ABI, FText/struct
 pushers and required method payloads. Then the single PS5 test must show the original
 “Menu created.” line plus viewport/visibility result and an observed visible menu.
 All56 host cases cannot replace that console result.

Files: graph_model.lua, runner.cpp, BUILD.json, RESULTS.v2.json. Initial harness
 diagnostics are retained; original mod/source/console files were never modified.

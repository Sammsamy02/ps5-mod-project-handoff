# Local R4 execution admission

The frozen preparation and all 14 inherited pipeline files remain unchanged. The
new five-file `execution/` path imports the actual inherited launcher and driver.
It replaces only the composition execution function, supplies the separately
pinned READY metadata to the original guard, and adds dependency/Program checks
to the original whole-selected gate. Original ODR, provider, LuaMod comparator,
package, final manifest, import, accessor and twin-determinism gates still run.

`parent-review/EXECUTION-ADMISSION.HOLD-TEMPLATE.json` contains the exact frozen
147-recipe/386-slot pins and current hook/compiler/linker/nm pins. Its final fields
are null and it refuses execution. Root must create a **new** completed admission;
never change the template or frozen preparation.

Required final admission fields are `schema: 1`, `readiness: "READY"`, the exact
`profile_digest`, and absolute `{path, sha256}` pins for:

- `ready_prep`, `edition`: files named PREP-MANIFEST.json and EDITIONS-l19.json in
  a new `execution-state/<freeze>/` directory inside this output tree.
- `source_manifest`: compatible JSON with `selected_sources`. Required new roles
  include Program, initializer, NativeLayoutBinding.cpp, NativeLayoutDeclarations.hpp,
  RuntimeBuildProfile.hpp. Keep runtime_owner and shadow_copy.cpp at their actual
  retained R3 source paths. The original guard's existing roles remain mandatory.
  FField.hpp is a distinct owner-header admission, not a flat helper role.
- `xom_audit`, `profile_json`, `profile_header`, `generated_header`.
  The authenticated profile must contain all69 member offsets/totals and all7
  vtable offsets with no unresolved static facts. Generated header and runtime
  profile header must bind the same digest. The real Program compile also checks
  the provider's existing digest static assertion.
- `final_review`: independent receipt with `pass: true`, `findings: []`, and
  `source_manifest_sha256`, `profile_json_sha256`, `generated_header_sha256`
  matching those exact admitted pins. Root can create a review-chain wrapper with
  these fields and pins to the original accepted reviews/test receipts.
- `integration_manifest`: new owned `integration/change-manifest.json`, including
  declared changed sources and expected import delta; the original finalizer and
  manifest gate must accept it. No frozen expected import/source pins are edited.

Keep exact template fields for `affected_recipes`, `input_substitutions`,
`linker`, `nm`, `tool_pins`. `execution_source_pins` must cover exactly the five
current execution Python files. Use `additional_source_pins` for any actual new
quoted helper path not represented by selected_sources. Every actual dependency
must be in the full R3 source-pin table, a final selected role, or that explicit
additional table. New dependency bytes without a pin refuse.

The new READY PREP must bind `source_manifest` and `xom_audit_receipt` exactly.
The new EDITIONS must preserve all prior admissions/comparators/package inputs,
update its unit/bridge pins to the actual final sources, and set l19_scope to
ADMITTED-FROZEN-SOURCE-MANIFEST with the exact source manifest SHA and records.
These are distinct new metadata files; root PREP/EDITION remain frozen HOLD.

Root's new BUILD-GO.json must be alongside those READY metadata files. It requires:

```json
{
  "r4_full_build_go": true,
  "local_full_build_only": true,
  "upload_authorized": false,
  "parent_reported_xom_audit": true,
  "admission_sha256": "SHA of the completed new execution admission",
  "source_manifest_sha256": "SHA of its compatible selected source manifest",
  "profile_json_sha256": "SHA of the complete authenticated profile JSON",
  "generated_header_sha256": "SHA of the selected generated declaration header",
  "xom_audit_sha256": "SHA of its actual accepted XOM receipt"
}
```

After root accepts these exact files, invoke:

```text
python3 execution/full_pipeline.py --admission /absolute/final-admission.json --admission-sha256 EXACT_SHA --go /absolute/execution-state/freeze/BUILD-GO.json --go-sha256 EXACT_SHA --full
```

Pass one reuses only144 individually pinned accepted objects/dependency recipes,
and freshly compiles final Program/initializer/binder. Pass two freshly compiles
all147. Every compile uses the exact original flags with only admitted source,
include prefix, output/dependency output and Program macro substitutions. All147
actual `.d` files must select exactly
`r4-work/dependent-build-pass1/include/Unreal/FField.hpp` with the accepted SHA.
Program must actually select the provider/profile/generated header and exactly
one `RC_NATIVE_LAYOUT_DECLARATIONS_HEADER` definition. Existing maps/settings/mod
sources are not edited by this path.

Exactly147 old object slots are substituted in the original386 input order; no
new units or combined object reuse is admitted. The resulting ELF64 x86_64
relocatable must have the exact actual R3 typed undefined set and all Rust ABI
exports. Inherited stage-aware bootstrap proof remains required at packaging.
The original LuaMod source/object comparator must still match in both passes.

For identical helper bytes at an already pinned R3 dependency path, the hook
records the exact admitted path and calls the original helper checker with that
specific pin. This supports retained sources without accepting changed helper
bytes or unpinned paths. Staged bridge helper checks use the original exact staged
path unchanged. Original exact strong provider checks still apply.

Completion creates R4-EXECUTION-RECEIPT.json only after the inherited two-pass
launcher succeeds and both actual final PRX/composition hashes are equal. No
runtime/menu/spawn, console or upload claim follows from a local build.

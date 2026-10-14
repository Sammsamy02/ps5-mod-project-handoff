# Object construction and the unchanged DLL flow

Mac-only analysis, 2026-10-13. No console contact or new console evidence. Source IDs resolve to absolute paths and complete SHA256s in SOURCE-PINS.json. Original artifacts are unchanged. The handoff supplied context; it did not authorize console actions, kernel changes, external dispatch, or publication.

Appending `construct_object` would expose an existing engine service; it would **not** establish unchanged UE4SS C++ DLL compatibility. Matrix follows the S7 reconstruction, which is a COFF census object with stand-in STL types, not an upstream DLL.

| Flow step | Existing support and effect of the new row | Remaining obligation |
|---|---|---|
| DllMain, start_mod, CppUserModBase construction | Some fixture PE entry/export execution recorded in S12; construct row has no effect | Real CRT bootstrap, allocator pairing, MSVC base/vtable/wstring layout; S10 remains a sketch with no export bodies |
| Register ClientRestart hook | S8 has register_ufunction_hook; S16:394 onward resolves the path and registers pre/post callbacks through the existing Func-pointer route | Exact mangled export, UTF16 string demarshal, both by-value MSVC std::function objects, callback ownership/copy/destruction/invocation and return descriptor; no unchanged DLL hook proof |
| Receive callback and read controller | Host context exists in S16’s mbm_hook_call construction | Convert into the upstream context layout; preserve return/parameter semantics and guest lifetime; native structures cannot be blindly reinterpreted |
| Read CheatManager and CheatClass property slots | property_address/get services exist in S8; S7 dereferences the returned pointer | Resolve and type-check reflected object properties; verify game-thread access and object liveness. No live proof for this reconstruction |
| Fallback class lookup | find_first_of exists (S16:276 onward) | A UObject instance named by class is not automatically a UClass suitable for construction. Validate class identity; prefer the existing CheatClass when valid. Match intended fallback semantics |
| Construct engine object | NEW row would remove the C-table exposure gap; S14:146-197 already dispatches by engine version | Implement/admit the row, bind the engine target, validate UClass/outer, error/null behavior and game thread; pin guest FStaticConstructObjectParameters layout and translate into native parameters |
| Write created object to property | S7 writes directly through the returned property pointer; property_set exists for a C-ABI port | Correct slot type, object validity, engine assignment semantics and later observable behavior. Success requires object created with the intended class/outer and visible in the intended game flow |
| Remove hooks and unload | Host unregister/remove support exists | Guest callback destruction, matching allocators and no surviving invocation after unload; restore all changed state required by the operation |

**ABI correction:** S18 B5 asserts SysV returns an eight-byte pair of ints in RAX:RDX and prescribes EDX→high-RAX repacking. Fresh Apple clang assembly for a two-int aggregate on both x86_64 SysV and Win64 packs it in **RAX**; real host libc++ std::pair<int,int> also returns in RAX. This contradicts the general SysV assertion. It does not pin the actual MSVC STL type: nontrivial type rules and toolchain version still need guest evidence. S9 explicitly discusses 9–16-byte aggregates; it does not prove the reported aggregate-8 refusal. Remove the unverified repack requirement from the implementation assumptions. The by-value std::function blocker remains independently real.

The proposed two-object construction row covers only defaults. If a DLL directly writes Name/flags/template/etc. into the guest parameter object, Class/Outer-only translation silently loses requested behavior. Either extend generic typed parameter support with versioned layout pins, or refuse unsupported values. S19 suppresses compiler failures with `|| true`; its final “Pinned” message alone cannot certify layouts.

Append at table end, check struct_size, preserve old readers. S8 defines integer ABI version 1; “1.1” is prose, not an existing numeric encoding. Review version-check compatibility before a bump. Object creation is a state change. MBM_ROUTE_DATA_ONLY_PARTIAL describes a data-only route with timing differences, not a general state-change flag; do not repurpose it to admit construction. Review a separate service permission/capability contract.

## Loading

Fixtures provide a partial Mac mechanism proof; the new row changes no loader or CRT blocker.

## Target resolution

Host lookup/binding implementations exist. Cross-build semantic correspondence remains unproved.

## Working patch behavior

No reconstruction or unchanged upstream gameplay DLL has demonstrated this full flow. The earlier claim “everything before construct works” is source-level possibility, not execution evidence.

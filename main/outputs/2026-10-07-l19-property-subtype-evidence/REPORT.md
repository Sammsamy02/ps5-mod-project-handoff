Saved-image property subtype evidence, LOCAL ONLY.

All addresses are ELF image virtual addresses, mapped through the actual PT_LOAD segments. The source image SHA is 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc. No console contact, runtime source mutation, live layout proof, or mod change occurred.

| Required fact | Value | Width | Basis |
|---|---:|---:|---|
| FEnumProperty.UEP_TotalSize | 0x80 | total | named-registered-constructor-allocation |
| FArrayProperty.UEP_TotalSize | 0x80 | total | named-registered-constructor-allocation |
| FBoolProperty.UEP_TotalSize | 0x78 | total | named-registered-constructor-allocation |
| FStructProperty.UEP_TotalSize | 0x78 | total | named-registered-constructor-allocation |
| FArrayProperty.Inner | 0x78 | 8 | native-nested-property-array-flow |
| FStructProperty.Struct | 0x70 | 8 | native-struct-value-flow |
| FBoolProperty.ByteOffset | 0x71 | 1 | native-bit-offset-and-mask-flow |
| FBoolProperty.ByteMask | 0x72 | 1 | native-bit-offset-and-mask-flow |
| FBoolProperty.FieldMask | 0x73 | 1 | native-bit-offset-and-mask-flow |
| FEnumProperty.UnderlyingProp | 0x70 | 8 | native-numeric-underlying-property-flow |
| FEnumProperty.Enum | 0x78 | 8 | native-enum-provider-and-object-reference-flow |

The exact allocation requests come from the UTF16-named property descriptor's registered ConstructFn, not from a largest-member estimate. The complete constructed pointer is retained and returned unadjusted. The seven virtual bindings use unique actual DT_RELA entries; the common FProperty copy/destroy/alignment slot identities were independently established in the class-size receipt.

Array.Inner is **0x78**, not the nearby array flag at 0x70. Bool ByteMask and FieldMask are separated by the native-bool path: ByteMask 1 versus FieldMask FF, as well as value-write and copy behavior. Enum.UnderlyingProp dispatches numeric FProperty virtuals; Enum.Enum follows generated enum-provider and UObject-reference serialization paths.

Validation: 873 byte-checked instruction windows, four exact named callbacks, seven unique relative vtable entries, eleven facts. This proves static layout only. FField/FFieldClass exact sizes, UClass.Interfaces, and three engine/game-mode slots remain unresolved.

SUBTYPE-FACTS.json SHA256 5429bed5e3e6a05c5d44f2baec856fdc221e30abced63c7a11ec6460147be033.

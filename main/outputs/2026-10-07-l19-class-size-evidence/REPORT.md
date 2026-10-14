# Townfall named native class sizes and FProperty slots

This is a local static evidence supplement for the saved decrypted ELF, SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, 194241424 bytes. It proves six target native totals and three required FProperty slots. FField and FFieldClass totals and three remaining game/engine slots are unresolved. No console, network, contact, upload, game execution, source edit, frozen-input edit, or prior-report edit was performed.

`CLASS-SIZE-FACTS.json` records the exact instruction bytes, ELF image addresses, PT_LOAD-derived file offsets, UTF16 name bytes, normalized rel32 targets, and the relevant RELATIVE relocation entry. All 152 instruction windows, seven name windows, allocator call targets, and the alignment relocation entry were checked against the authenticated image. These are saved-image facts; they do not assert a fresh installed-image hash or live object validation.

## Common size argument and allocation proof

The common named class helper at image `0x30d860` receives a UTF16 class name in RSI, native size in R8D, and alignment in R9D. It saves the name at `0x30d88b`, size at `0x30d8a0` (`4589c5`, R13D = R8D), and alignment at `0x30d89d`. At `0x30d904` (`45896c2458`) it writes that size unchanged to the new UClass descriptor's PropertiesSize at +0x58; at `0x30d909` it writes alignment as a word at +0x5C. It publishes the descriptor at `0x30da63` and passes the preserved name to registration at `0x30dac2`/`0x30dac5`.

Native allocation at `0x490fb0` takes that class descriptor. `0x49112a` reads +0x58, `0x491141` sign-extends it into the allocation byte count RDI, and `0x49116c` calls image `0xb7a20`. The allocation wrapper preserves the byte count and passes it to the allocator virtual at +0x30. The alternative permanent allocator path uses the same byte count. This establishes native class allocation size semantics for PropertiesSize; an argument is not identified as sizeof merely because it resembles a conventional compiler layout.

## Positive facts

| Profile key | Value | Target identity and proof | Confidence |
|---|---:|---|---|
| UObjectBase.UEP_TotalSize | 0x28 | **Two-sided inference with target containment**, described below; named Object size 0x28 is direct, base identity is not claimed from the name alone. | High, inferred |
| UField.UEP_TotalSize | 0x30 | UTF16 `Field` at 0x77A9EF0; private registration 0x31A4F0 sets PropertiesSize to 0x30 at 0x31A590. Independently, named Struct's parent Field call passes R8D=0x30 at 0x320D12 to the common helper. | High |
| UStruct.UEP_TotalSize | 0xB0 | UTF16 `Struct` at 0x78CCCA0; private registration 0x320AF0 sets PropertiesSize to 0xB0 at 0x320B98. Named Function's parent Struct call independently passes R8D=0xB0 at 0x3286DF. | High |
| UFunction.UEP_TotalSize | 0xE0 | UTF16 `Function` at 0x7755232; private registration 0x3284B0 sets PropertiesSize to 0xE0 at 0x328553 and word alignment 8 at 0x32855A; the same name reaches final registration 0x328763/0x32876D. | High |
| UClass.UEP_TotalSize | 0x200 | UTF16 `Class` at 0x76AE74E; private registration 0x3264C0 sets PropertiesSize to 0x200 at 0x32656A. The descriptor itself independently requests 0x200 bytes at 0x3264F6/0x326505 through 0x480D70, whose byte count reaches 0xB7A20 unchanged. | High |
| FProperty.UEP_TotalSize | 0x70 | Named FProperty StaticClass 0x3E1CD0 supplies UTF16 `FProperty` at 0x758E17E and ConstructFn 0x3E1B50 at 0x3E1D0C. Descriptor constructor 0x363990 stores R9 at +0x30. That ConstructFn directly allocates 0x70 bytes at 0x3E1B6E/0x3E1B7F, installs final FProperty vptr 0x7EEA438, and returns the unadjusted allocation pointer. | High |
| FProperty.CopyValuesInternal.vtable_byte_offset | 0xF8 | Wrappers 0x3E2470 and 0x3E24B0 preserve Dest/Source and dispatch +0xF8 with Count=1 or ArrayDim. Their alternative POD paths memcpy ElementSize*Count. | High |
| FProperty.DestroyValueInternal.vtable_byte_offset | 0x140 | Wrapper 0x3E2E60 calls +0x140 on the property value when the NoDestructor flag is clear, then frees that same value through image 0xB7A90. It independently corroborates the earlier destructor-chain dispatch. | High |
| FProperty.GetMinAlignment.vtable_byte_offset | 0x170 | Wrapper 0x3E2E10 computes ArrayDim*ElementSize, calls +0x170, and supplies its return value as allocation alignment to image 0xBF050. Named FProperty vptr 0x7EEA438 has a RELATIVE relocation at 0x7EEA5A8 (+0x170) to image 0x5B0, which returns 1, the base property's minimum alignment. | High |

### UObjectBase containment inference

Named Object registration at `0x3c5bb0` selects UTF16 `Object` at `0x79ec802`, passes R8D=0x28 at `0x3c5c13` and R9D=8 at `0x3c5c19`, and calls the common helper at `0x3c5c3b`. This independently proves the complete named native UObject allocation extent and alignment, 0x28/8.

The native allocator's malloc result RAX jumps from `0x491171` to `0x49122e` (`4889c3`, RBX = RAX), then at `0x49125e` jumps to `0x4916c5`. `0x4916d0` (`4989dc`) sets R12 = RBX. `0x49174f` (`4c89e7`) passes R12 unchanged as the constructor this pointer, and `0x491760` calls the fundamental-base constructor at image `0x485430`. There is no subobject pointer adjustment in this path.

That constructor at `0x485445` loads the same fundamental-base vptr 0x7EFF620 installed by the previously measured constructor `0x485400`; at `0x485458` it installs it at this+0. It writes the same Flags+8, InternalIndex+0xC, Class+0x10, Name+0x18, and Outer+0x20 fields. The qword Outer store at `0x485492` (`4c896320`) requires occupied base storage through 0x28. The fundamental base is therefore explicitly contained at the complete native UObject allocation start: occupied lower bound 0x28, independently registered complete-object upper bound 0x28, alignment 8. Together they establish physical base total 0x28. This qualification is separate from the direct named Object allocation fact; there is no registration name `UObjectBase` in this proof.

## Unresolved facts

| Key | Candidate, if any | Why not promoted |
|---|---:|---|
| FField.UEP_TotalSize | 0x30 | The named FField descriptor selects ConstructFn image 0xE0, an empty abstract stub. Its constructor writes through +0x28/4; alignment suggests 0x30, but neither a native sizeof argument nor a named direct allocation was recovered. |
| FFieldClass.UEP_TotalSize | 0x40 | Descriptor construction writes through +0x38/4, and named static descriptor/guard spacing is 0x40. Extent and guard spacing do not independently prove exact sizeof. Descriptor +8 stores a cast id (FProperty 0x8000), not a size. |
| UEngine.LoadMap.vtable_byte_offset | — | Numerous Engine references; no unique native LoadMap behavior plus receiver slot binding was recovered in this bounded pass. |
| UEngine.Tick.vtable_byte_offset | — | No unique UEngine tick function and receiver slot binding was recovered. |
| AGameModeBase.InitGameState.vtable_byte_offset | — | GameModeBase name references were preserved; InitGameState behavior and exact receiver slot were not uniquely identified. |

Property subtype evidence is being investigated in a separate parent-owned slice. UClass cast flags, default object, CreateDefaultObject slot, and FFieldClass members are independently frozen under `../2026-10-07-l19-unresolved-evidence/`; this supplement does not rewrite them.

## Interpretation limits and preserved diagnostics

UFunction size 0xE0 fits the active GetFunc419 access at +0xD8 with width 8. The unchanged binding's conservative width 16 does not fit that extent. Neither total nor target slot width is changed to accommodate the validator; that separate binding issue remains explicit.

All authoritative addresses here are ELF image virtual addresses. The existing rdis display prefix and displayed call operands add 0x400000. Actual rel32 operands were decoded against the image; image targets are 0xB7A20 for calls at 0x3E1B7F, 0x480D89, and 0x49116C, 0xBF050 at 0x3E2E34, and 0x485430 at 0x491760. The rdis call footer double-adds the base and was ignored.

`FMemoryMalloc.asm` was an earlier capture at mistakenly base-added address 0x4B7A20; its misaligned bytes are preserved as diagnostic material and excluded from proof. `FMemoryMalloc-confirmed.asm` begins at the correct image 0xB7A20. Candidate captures and name-reference scans are preserved without promoting their labels to fact. `MallocZeroed.asm` is a retained capture label; its evidence is allocator byte-count/alignment forwarding, not a claim that its body zeroes memory.

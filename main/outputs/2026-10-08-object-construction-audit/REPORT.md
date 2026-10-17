# Object construction boundary — local evidence

Status: **Mac-only static correlation. No native object construction was executed.**

The census compiled the actual R4-selected `Above55::FStaticConstructObjectParameters` with the actual PS5 recipe, replacing only the census translation unit and output paths. It reports a 128-byte, 8-byte-aligned declaration. Thirteen offsets correlate with reads in Townfall's saved `StaticConstructObject` and object-initializer instruction bodies. A read-only replay verifies all 200 instruction windows against the frozen image through its PT_LOAD mappings. Fifteen negative layout controls refuse, including the older serial-number/subobject offsets. This establishes member positions, not the native aggregate's exact total size or successful widget construction.

| Parameter | Offset | Selected width | Native evidence |
| --- | --- | --- | --- |
| Class | 0x00 | 8 | constructor reads through RDI |
| Outer | 0x08 | 8 | constructor reads through RDI |
| Name | 0x10 | 8 | constructor reads through saved params RBX |
| SetFlags | 0x18 | 4 | constructor ECX load |
| InternalSetFlags | 0x1c | 4 | constructor R8D load |
| CopyTransients | 0x20 | 1 | initializer reads through RDX |
| AssumeTemplateIsArchetype | 0x21 | 1 | constructor byte check |
| Template | 0x28 | 8 | constructor R12 load |
| InstanceGraph | 0x30 | 8 | initializer pointer load |
| ExternalPackage | 0x38 | 8 | constructor pointer load |
| PropertyInitCallback | 0x40 | 48 | initializer checks first storage word; inner ABI unproved |
| SerialNumber | 0x70 | 4 | constructor R13D load |
| SubobjectOverrides | 0x78 | 8 | initializer pointer load |

The native constructor receives a parameter pointer in RDI and returns an object pointer in RAX. The initializer receives this parameter pointer in RDX. These are saved-image observations; this audit does not claim a successful call. Only an empty property-initializer callback is supported: the inner native TFunction ownership/callable layout is not established. Nonempty callbacks must remain unavailable.

The historical assembly captions labeled `f` represent image virtual addresses, not raw file offsets. The new validator independently translates them using the ELF program headers and verifies the actual bytes. No original capture was changed.

Remaining boundaries: reflected `Create` parameters and return marshalling; valid native outer/class/lifetime; native UMG methods; current game-thread execution. The original Lua widget model proves the script's control flow and required call sequence only. Neither artifact proves the PS5 menu.

Receipts: `COMMAND.json`, `CENSUS.v2.json`, `VALIDATION.json`; replay with `python3 outputs/2026-10-08-object-construction-audit/validate_census.py`. The census is not recompiled during replay.

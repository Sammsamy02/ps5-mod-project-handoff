Local static evidence supplement for the authenticated retained executable.

The frozen l19-v1 sources, profile, and receipts were not changed. No console, network, account, upload, or game/system write occurred. These facts establish native binary layout only; they do not prove a live menu or spawn run.

| Required field | Native offset | Width | Evidence |
|---|---:|---:|---|
| FFieldClass.FName | 0 | 8 | Named FField descriptor constructor builds an FName and copies its qword to descriptor+0. |
| FFieldClass.CastFlags | 0x10 | 8 | Constructor stores RCX qword; independent IsA helper tests the full qword after loading FField.ClassPrivate. |
| FFieldClass.SuperClass | 0x20 | 8 | Constructor stores R8 qword; independent IsA helper walks the same superclass chain. |
| UClass.ClassCastFlags | 0xD8 | 8 | Named Class descriptor writes CASTCLASS_UClass (0x20) as a qword; superclass propagation independently reads and ORs qwords. |
| UClass.ClassDefaultObject | 0x110 | 8 | Native CreateDefaultObject allocates with flags RF_Public, RF_ClassDefaultObject and RF_ArchetypeObject, stores the returned pointer, and returns this field. |

Supplemental UClass.CreateDefaultObject vtable byte offset is 0x400. The selected port has no active call site for that wrapper, so this adds no mandatory startup check. Its proof combines the named UClass constructor's installed vptr, the exact ELF DT_RELA record for vptr+0x400, the native caller, and the callee's default-object construction flow.

The two frozen fact JSON reports contain 49 exact instruction windows mapped through actual PT_LOAD segments, plus the unique DT_RELA record and native UTF16 name bytes. The relocation-aware evidence script reconstructs the actual standard DT_RELA bounds from PT_DYNAMIC. The original disassembler prints leading runtime-base addresses; authoritative addresses in the JSON are ELF image virtual addresses and separately mapped file offsets.

No exact class size is promoted from the constructor's highest touched member. FFieldClass's last dword at 0x38 establishes a lower bound of 0x3C. A neighboring static guard at distance 0x40 does not uniquely prove sizeof. Exact-size work is separately recorded in the class-size evidence directory.

The selected compiled accessor providers already obtain both cast widths through sizeof of the declared accessor value types. EClassCastFlags explicitly uses uint64_t, and the alternative base accessors return uint64 references. Both providers therefore yield 8 without any game-specific fallback or runtime policy change.

PROFILE-ADDITIONS-RED.json records a deliberate acceptance failure against the immutable v1 profile: all five newly measured required fields are still unknown and both cast-width unknown entries are still present. A later profile revision can turn this check green while preserving all v1 artifacts.

Fact receipts:

- FIELDC-FACTS.json: SHA256 76aa9df32429b26a378649719ce2bafffae21e100ac2dfa7e20b0475eb1e693c.
- UCLASS-FACTS.json: SHA256 10495e5740008dc389729934e6e1a36f00108f771fff52d58fb9c3754d3372f1.

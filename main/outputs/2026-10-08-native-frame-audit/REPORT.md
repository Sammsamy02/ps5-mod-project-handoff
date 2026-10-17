# Native frame / GNatives audit — saved image only

The selected callback aliases and 13 physical frame members match independent native instructions. This does **not** establish that every engine entry into a modified UFunction.Func passes through UFunction::Invoke, nor make the synthetic frame branch ready. The frozen R4 69-member/7-slot profile has no frame/GNatives contract, and was not edited.

The actual target compilation yields 52 census values. The native receipt checks 721 exact instruction windows and 28 exact relocation records against decrypted-eboot SHA `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (194241424 bytes). All addresses below are ELF image virtual addresses, translated with PT_LOAD; they are not live addresses. The historical disassembly's extra `0x400000` display bias is excluded here. The earlier gap-l2a `Code+18` and `Func+C8` statements are stale and rejected by direct bytes.

`TARGET-CENSUS.json` is a compiler observation of the **actual selected port declarations**, built with the stored FFrame recipe, R4 owner-header include prefix, original target flags and SDK wrapper. `FRAME-PROFILE-CANDIDATE.json` is separate candidate data with qualified evidence and 11 unresolved obligations. Native exact total sizes are not promoted from member extent or local stack reservation.

| Datum | Target compiler | Native evidence / limit |
|---|---:|---|
| FNativeFuncPtr / UnrealScriptFunction | ordinary function pointer, size8/align8; same type | Invoke326B25 performs one qword indirect code call at UFunction+D8; GNatives Step4577C3 performs qword indexed call. Neither uses member-pointer adjustment/virtual-bit decoding. |
| UFunction GetFunc active footprint | 8 | GetFunc delegates to GetFunc419; GetFuncPtr/SetFuncPtr bit-cast its pointer representation. The misleading std::function-pointer spelling is not a native std::function object. |
| Hypothetical UObject member function pointer | 16 | Compiler control emits this-adjust and virtual-bit decoding absent from both native dispatch sites. Do not substitute this representation. |
| Callback ABI | ContextRDI, FrameRSI, resultRDX | Invoke326B15/18/1B and Step4577BA/BD pass these exact registers. Frame reference is passed as an address; no by-value frame object is exchanged. |
| Thin FFrame accessor facade | sizeof1 | It contains methods only; native frame allocation cannot use sizeof(FFrame). Actual port allocation uses concrete version structs. |
| FFrame_51_AndAbove | sizeofA8, align8 | Native occupied extent reaches A2. Exact native sizeof/alignment not independently established; the observed prefix matches. |
| FFrame_50_AndBelow | sizeof98, align8 | Comparator only; current declared target axes choose5.6. Do not use its CurrentNativeFunction88 offset on this image. |
| FOutputDevice | sizeof10/align8, vptr0, bool8/9 | ProcessEvent calls1D4680 at frame base; ctor installs base vptr7EC8C80 and writes word8=100. This matches false/true selected bool defaults. Exact native total remains unproved. |
| FOutParmRec | sizeof18/align8; Property0/PropAddr8/Next10 | ProcessEvent459B0F/13/1E/33 writes these qwords; StepExplicit457810/13/17/1C walks them. Native occupied prefix18 proved; exact native sizeof not independently proved. |
| GNatives table | port variable is an ordinary pointer; initially null | Native Step RIP-relative LEA4577AB identifies image93B8900, in writable BSS. Byte opcode4577C0 and scale8 call4577C3 permit256 indices/minimum reachable span800. Exact declared count, entries, live initialization/base and readiness not proved. |

| FFrame_51 member | Compiler offset / access width | Native anchors |
|---|---:|---|
| Node | 10 / 8 | ProcessEvent459A24 stores function at frame+10; Serialize4579FE reads it. |
| Object | 18 / 8 | ProcessEvent459A28 stores context at frame+18; Serialize4579CD reads it. |
| Code | 20 / 8 | Function.Script60 becomes frame+20 at459A2C/30; native Step4577A4/B6 reads/increments it. |
| Locals | 28 / 8 | ProcessEvent459A38; StepExplicit4577DA. |
| MostRecentProperty | 30 / 8 | Native property operand reader459DD5 stores its just-read qword into frame30. |
| MostRecentPropertyAddress | 38 / 8 | StepExplicit4577E5/457820. |
| MostRecentPropertyContainer | 40 / 8 | StepExplicit4577E9/457824. |
| FlowStack | 48 / footprint30 | Partial geometry only: native heap pointer68, Num70, Max74; ctor initializes Num0/Max8. Exact native container/element/allocator contract not closed. |
| PreviousFrame | 78 / 8 | Native stack walker457C71 follows frame+78 while collecting Node/Object names. |
| OutParms | 80 / 8 | ProcessEvent459AC5 and StepExplicit4577FE. |
| PropertyChainForCompiledIn | 88 / 8 | Function.ChildProperties50 read4599F7 becomes frame88 at459A6C. |
| CurrentNativeFunction | 90 / 8 | Invoke326B0E saves previous,326B1E writes function self,326B2C restores previous. |
| PreviousTrackingFrame | 98 / 8 | ProcessEvent459A87/95 saves previous TLS tracker; dtor459D54 reads it. |
| bArrayContextFailed | A0 / 1 | Native ctor zeroes wordA0 (A0/A1); separate semantic byte identity not pinned. |
| bAbortingExecution | A1 / 1 | ProcessEvent459AA1/A8 inherits it; dtor459D73/7A propagates it to prior tracker. Physical byte evidence is direct; name correlates selected declaration. |

Names in this table correlate selected declarations with native dataflow; physical operands are independently decoded. ProcessEvent's frame base is stack-local RBX+58; its field stores subtract58. The validator independently recomputes these displacements rather than trusting table captions.

The proven installation and entry path is precise:

1. Selected UFunction.cpp:GetFunctionHookData captures `GetFuncPtr()`, inserts a hook-map entry keyed by that UFunction object, and calls `SetFuncPtr(&Internal::UnrealScriptFunctionHook)`. It does not change GNatives.
2. Native ProcessEvent459B6F/459C5A calls Invoke326AC0 with function self/context/frame/result in RDI/RSI/RDX/RCX.
3. Invoke saves self inR15, context inR12, frame inRBX, result inR14. It sets frame90=self, calls the ordinary qword FuncD8 with callback arguments RDI/RSI/RDX, then restores frame90.
4. Therefore a hook entered through this path has the exact registered function identity during its callback. The selected wrapper can look it up and call its captured original while preserving this frame. Completeness across other native Func callsites is a separate unresolved question.

Native Step4577A0 calls GNatives directly with the same three callback arguments and does not set frame90. This does not demonstrate that the common wrapper is a GNatives entry. Hook installation modifies only Func. Table readiness/entry semantics cannot be inferred from the ordinary callback ABI.

In the current staged R4 setup, Script.cpp initializes GNatives_Internal=null and armed-seed UnrealInitializer.cpp1227 disables its sole binding assignment at1635. Thus the selected wrapper's `Code && GNatives_Internal` clone branch is inactive under that stated configuration. This is a local configuration fact; no live state is claimed. Native original functions still use the engine's own dispatch table; the null port variable does not disable the engine table. Dynamic callback parameter/result types, geometry, alignment, lifetime and thread admission still need their own checks.

The two `Code-8` reads at UFunctionStructs.cpp168/176 have no safe identity/storage proof. The first happens before a Code null check. A nonnull cursor also does not establish an accessible8-byte prefix containing a UFunction pointer. Code20 is a cursor inside function bytecode; Serialize457A32/41 computes it relative to Node.Script60. CurrentNativeFunction is justified for the proved Invoke path; Code-prefix identity is not.

Synthetic frame construction has independent obligations:

- Native ProcessEvent replaces the base vptr with frame vptr7EFEF70. Exact RELA records map slot0 to459D40, slot8 to46E270, slot10 to457980 (frame Serialize); several inherited slots match the base table. The selected malloc/memset path leaves vptr0 zero, so native virtual use is unsupported.
- Native base ctor sets bool8/9 defaults false/true. memset supplies false/false.
- Native flow init sets heap68=null, Num70=0, Max74=8. memset supplies Max0. Full flow push/pop/allocator semantics have not been established.
- Native frame construction saves TLS[-6C8] into98, installs the current frame in that TLS slot, and inherits abortA1. Destructor459D40 restores previous98, propagates abortA1 and frees flow heap68. A native destructor cannot safely be bolted onto an untracked memset frame.
- The selected clone fills Node/Object/PreviousFrame/Locals/OutParms/chain but leaves CurrentNativeFunction90 zero. Native Invoke's identity contract is consequently lost when it directly calls the captured original with this clone.
- Allocation is checked after memset. Cleanup is conditioned on mutable original Code/GNatives state, not whether a clone was actually created. Exceptions can leave midexecution set and owned frame/property state unreleased. Native original behavior remains untested here.

Bounded generic recommendations (no implementation or approval implied): refuse unsupported frame/entry profiles **before** replacing Func and preserve the original; remove Code-8 recovery; distinguish supported mapped Invoke entries from unsupported/unknown entries. A common wrapper cannot safely forward an unknown identity through an arbitrary original; a per-registration identity/original thunk would need a separate ABI/reentrancy/lifetime proof. Keep unsupported script/GNatives cloning disabled until its constructor and lifetime contract is closed. Check allocation before memset and use ownership-aware exception cleanup.

Useful fail-first extracted-method tests: known-current/nullCode; known-current/non-nullCode/GNativesnull; unknown/null-current with inaccessible Code prefix; absent/null/self-original refusal; unsupported-profile refusal before Func/map mutation; allocation failure before memset; callback throw/reentrancy with midexecution/owned-state cleanup. They establish local wrapper behavior, not native engine success.

Reproduction: `python3 outputs/2026-10-08-native-frame-audit/build_census.py` compiles only the new census object, `python3 .../native_evidence.py` reads the saved image, and `python3 .../validate_audit.py` is read-only. Fresh validation checks1186 actual compiler dependencies, decodes all52 ELF constant values, matches13 native field operands, verifies721 instruction windows/28 relocations, and rejects four contradictions: stale Code18, old CurrentNative88, member-pointer width16, GNatives stride16. No target object was executed, no console/network action occurred, and no frozen input was changed.

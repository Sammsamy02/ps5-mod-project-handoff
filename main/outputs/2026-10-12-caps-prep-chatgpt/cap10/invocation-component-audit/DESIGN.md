# Genuine invocation component: conditional feasibility

Schema12 VERIFICATION.json and the accepted proof-observation-audit were read as current context. This audit changes neither. **The retained Mono APIs support a small actual invocation component, conditional on a real owner-provided GC/embedding/lifetime contract. They do not establish a safe self-contained arbitrary-thread host.** No implementation, GC shim, host callbacks, synthetic success, build or execution is supplied here. All original files are unchanged.

## Exact existing APIs and source conditions

Paths below are under cap10/. Official retained Mono commit0cbf0e290c31adb476f9de0fa44b1d8829affa40 remains pinned; SOURCE-RECEIPT.json hashes inspected files.

| Genuine signature / anchor | Component use and limit |
| --- | --- |
| `MonoThread* mono_thread_attach(MonoDomain*)`, `threads.h:43`; current at30, detach44 | Owner attaches actual init-callback thread before entry and retains its attachment. Component must not initialize Mono or detach a thread it does not own. Declaration alone does not prove target TLS/GC mode/stack registration. |
| `MonoClass* mono_class_from_name(MonoImage*,const char*,const char*)`, `class.h:44`; `MonoMethod* mono_class_get_method_from_name(MonoClass*,const char*,int)`, :294 | Resolve actual ModBridge.Cap10.Probe and Run/1, ThrowForHostBoundaryTest/0 from the retained validated assembly image. Require real metadata/signature/hash association; name/arity lookup alone is not source-body proof. |
| `MonoImage* mono_get_corlib(void)`, `appdomain.h:155`; `MonoClass* mono_get_int32_class(void)`, :179 | Resolve expected InvalidOperationException via mono_class_from_name(mono_get_corlib(),"System","InvalidOperationException"). Compare actual class pointers in the same retained runtime/domain/corlib, not arbitrary exception-name strings. |
| `MonoObject* mono_runtime_invoke(MonoMethod*,void*,void**,MonoObject**)`, `object.h:266–267`; `transition-audit/upstream/mono/metadata/object.c:3063–3112` | Real static invocation: obj=NULL, Run params[0]=address of native int32 cookie, nonnull exc output initialized NULL. Check exc first; never use result when exc!=NULL. Return value type is boxed. |
| `MonoClass* mono_object_get_class(MonoObject*)`, `object.h:209`, object.c:7197–7200; `void* mono_object_unbox(MonoObject*)`, object.h:212, object.c:7242–7247 | Verify actual result class equals expected Int32 before unboxing. Unbox returns borrowed pointer; non-valuetype can assert. Copy exactly int32 while boxed storage is safely rooted/stable. |
| `uint32_t mono_gchandle_new(MonoObject*,mono_bool pinned)`, `MonoObject* mono_gchandle_get_target(uint32_t)`, `void mono_gchandle_free(uint32_t)`, `object.h:358–373` | Strong handle retains; pinned handle prevents movement. Track ownership separately from numeric handle value: retained declarations do not define a zero-handle failure sentinel. Free every successfully acquired handle exactly once. Avoid invented handle success/error constants. |

## GC boundary: what is missing

Actual runtime_invoke enters GC-unsafe at object.c:3098 and exits at3111 **before returning raw MonoObject***. Class/unbox implementations also use MONO_EXTERNAL_ONLY_GC_UNSAFE wrappers. These internal transitions do not by themselves demonstrate that an external caller's raw result/exception remains rooted and unmoved between calls or during mono_gchandle_new.

The retained set lacks the definitions/implementations of MONO_ENTER_GC_UNSAFE / MONO_EXIT_GC_UNSAFE / MONO_EXTERNAL_ONLY_GC_UNSAFE, the actual cooperative transition rules and GC-handle allocation implementation. object.c includes `mono/utils/mono-threads-coop.h`, `mono/metadata/gc-internals.h` and `mono/metadata/external-only.h` (local include), but those files are absent from the retained trees. threads.h exposes attachment, not a caller-spanning GC-state RAII API. No cooperative mode may be guessed from an annotation or replaced with empty enter/exit functions.

A **conditional same-image internal component** can require the actual owner/port to establish all of these before calling it:

1. Real initialized Mono runtime/domain/corlib/image/metadata ownership; no unload/cleanup through return. Actual current callback thread is attached, has real TLS/context/native-stack registration, and is within the real native bridge callback window.
2. A genuine GC/embedding scope spanning invocation, raw return/exception inspection, strong/pinned handle acquisition and native scalar copy. It must explicitly guarantee raw-return rooting until the first handle owns it, including allocation/GC during handle creation, and permit the called public external APIs/nested transitions with correct restoration. Merely saying "GC-unsafe" without stack-root/entry semantics is insufficient.
3. Correct exception/unwind/AOT transition support and imported native ABI; provider module remains loaded. Ownership contract must survive managed throw and native early return.

No new owner callback type is invented to pretend these conditions hold. The caller may satisfy them through the real pinned Mono internal facilities after their actual source/port is admitted; their absence is a concrete source prerequisite. Without this contract, staging a public-only routine that assumes attach + raw pointer + immediate gchandle is sufficient would overclaim safety. A guessed C++ RAII guard calling unverified GC transition symbols would not fix it.

## Concrete component flow once that contract is real

Use a same-toolchain internal function taking actual retained domain/image/method ownership plus int32 cookie; no Mono pointers become a public PRX ABI. It does not create a host or return success without performing calls.

- Resolve/check expected Probe class/methods and expected Int32/InvalidOperationException classes under the admitted GC scope. Refuse missing/incorrect metadata. Owner supplies real compiled-assembly identity and actual load association; the existing metadata preflight remains only preflight.
- Invoke actual Run with native cookie storage/params and exc=NULL. If exc returned, safely acquire/retain exception under the owner GC contract, record failure, ignore result and clean up. If no exception, require nonnull boxed result, safely acquire a short-lived pinned strong handle, reacquire target, check exact Int32 class, unbox and copy int32, then free handle. Require copied value0, but do not label this import-call proof.
- Invoke actual ThrowForHostBoundaryTest with static NULL receiver and no argument array, separate initialized exc output. Require nonnull exception, root it under the same contract, compare exact class with resolved System.InvalidOperationException, ignore the result unconditionally on exception, then release handle. Never stringify exception or invoke a managed message accessor just to validate this minimal proof.
- Return an honest native outcome covering actual call/result/exception facts, restoring real owner GC state and releasing every acquired handle on all exits. Do not destroy the domain, detach someone else's thread or unload provider. Existing user's proof contract additionally calls Run again with fresh cookie(s) around the exception; later implementation can repeat this same genuine helper, not replace calls with canned outcomes.

Pinning here is deliberately short, only through class/unbox/copy. Initial handle acquisition still requires the actual raw-return rooting guarantee above. No claim that copied int32 or exact-class comparison proves correct managed IL or native imports.

## Same-invocation import observation seam

The component's true boundary is immediately before and after the **actual** mono_runtime_invoke(Run...) call inside the existing ready callback. A future real observer must open/reset a per-invocation thread/cookie/provider-bound window there; the actual AbiVersion and RuntimeProbe C entry/exit paths record their real executions and outcomes into that same window. Both calls must be observed in expected order, with the runtime probe cookie/reply/status, before the Run call's actual boxed result is accepted. Resolved addresses/import metadata, aggregate historic counters and direct host diagnostics do not count. Throw should produce the deliberate managed exception and no native import calls.

The reviewed bridge-stage currently has no shared observer-window interface or counters. Its TU-local callback scope is necessary admission, not an invocation observation service. Do not invent an already-existing observer API. Root can design that exact internal seam together with the future genuine host component once owner GC contract/source is supplied. Even a correct invocation helper alone cannot upgrade capability10 from native prerequisite/metadata preflight to managed+bridge proof.

## Recommended next action

Admit/review the pinned real cooperative-GC/external-entry and handle-allocation source, or require a concrete port-owner contract with the exact same guarantees and evidence. Then an isolated source-only actual mono_runtime_invoke helper is implementable without a fake runtime, still conditional/uncompiled. Today the typed call flow is concrete, but raw-object handoff safety is not established by the retained public declarations. Do not add a new B7 gate or alter current bridge/runtime code for this audit.

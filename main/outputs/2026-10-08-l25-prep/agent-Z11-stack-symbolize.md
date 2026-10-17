# Agent Z11 — l24 crash stack symbolization (klog fatal blocks)

Date: 2026-10-08. READ-ONLY on all inputs; working files under /tmp/l25-symbolize/;
this report is the only file written outside it.

## 0. Sources

| Source | Role |
|---|---|
| `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/klog-20261008-173728.log` | 8 fatal blocks (6 abort + 2 SIGSEGV cascade), capture window 12:37–14:27 local (UTC-5) |
| `outputs/2026-10-08-l24-package/pass-1/modbridge_supplier_composition.prx` | l24 candidate FSELF, sha256 `2e712a8b…` (pass-1/pass-2 twins identical) |
| `outputs/2026-10-08-l24-package/pass-1/build/combined-supplier.o` | pre-package partial-link object (`ld.lld -r`), full symtab + per-function sections |
| `outputs/2026-10-08-l24-package/pass-1/harness/tools/SharpProspero.Link/DynamicWriter.cs` | the actual section→segment layout algorithm (reimplemented to map vaddrs) |
| `/tmp/eboot-backup.bin` | game eboot decrypted view (truncated; phdrs only, no symtab) |

## 1. Which build actually ran (fp `ffad2cd5…`)

- Every one of the 8 blocks prints the same module set: eboot fp `04bb4330…`,
  `/data/plugins/modbridge_supplier_composition.prx` xotext `0x80a8c000:0x81cb4000`,
  fp `ffad2cd5674725e2ce744b8089bf33d9` — byte-identical in the 13:38 blocks and in
  the 13:47 (pid 208) and 14:07 (pid 266) blocks. There is exactly ONE loaded plugin
  image in the whole window.
- The console's pre-l24 backup `/tmp/l24-backup-prx.self` (13:34 local) is sha
  `0da199eb…` = the l23 upload (12:17 local). The l24 PRX (`2e712a8b…`, packaged
  13:28) was uploaded ~13:35; the first post-upload launch (pid 144, crash 13:38)
  already shows the l24-only behavior (type scan `missing-keys=3 core-ready=true`
  from the l24 spanpub fix, full diag tail byte-identical to the l24 trace — agent F).
  All later launches (pids 178/193/208/222/251/266) are l24-era relaunches or A/B
  arms of the same image.
- Whole-file/segment hashes of both candidates do not reproduce the kernel fp
  (it is kernel-computed over the decrypted loaded image), but the fp's constancy
  plus the UE4SS/diag evidence pins the loaded binary to the l24 build in every
  abort block. **Symbolization below is against l24 (`2e712a8b…`) and applies to
  all 6 abort blocks — they are one identical crash signature, including pid 208
  (13:47:16–46) and pid 266 (14:07:16–46, the A/B v2 VALID arm).**

## 2. Method

1. Frame addresses → module-relative offsets via each block's own xotext map
   (modbridge base `0x80a8c000`, eboot base `0x400000`, libc.prx base `0x80000000`).
2. The PRX is stripped; symbols recovered from `combined-supplier.o` (207,709
   sections, 295,090 syms, per-function `.text.<mangled>` sections). Final-image
   vaddrs reconstructed by reimplementing `SharpProspero.Link.DynamicWriter`'s text
   layout (16-byte image-head reserve; exec sections grouped `.init/.text/.fini`
   then first-seen; per-section alignment). **Calibrated exact** against the final
   PRX's own dynsym: `module_start` = `0x15a0` (size `0x472`) and `module_stop` =
   `0x1e00` (size `0x17b`) both reproduced to the byte.
3. Every one of the 15 modbridge frames was then verified by decoding the actual
   call instruction that ends at that return address in the shipped PRX bytes
   (FSELF data block at file `0x9bb0`): each frame's call target lands exactly on
   the next-inner frame's function entry. This is a call-graph-verified chain, not
   just nearest-symbol matching.

## 3. Symbolized chain (pid 208 and pid 266 — identical, innermost first)

```
rip  0x80004920                     libc.prx+0x4920  abort()
#  0x40f918   eboot+0xf918      game fatal-error routine: formats message
                                (lea rdi,fmt; 7-arg format incl. a double),
                                then `cmp byte[bss-flag],0; jne skip;
                                 call abort@PLT(0x7472200); ud2`
#  0x5e3ad8   eboot+0x1e3ad8    fn@0x1e3900: ... `call [rax+0x10]`  ← VIRTUAL
                                dispatch, vtable slot +0x10, entered the
                                fatal-error routine above
#  0x575197   eboot+0x175197    fn@0x1750b0: `call rel32` → 0x1e3900
#  0x4b2379   eboot+0xb2379     fn@0xb2160:  `call rel32` → 0x1750b0
#  0x4b4b75   eboot+0xb4b75     game allocator entry (target of the modbridge
                                GMalloc dispatch): `call rel32` → 0xb2160
#  0x80b2d36b mb+0xa1240+0x12b  RC::Unreal::FMalloc::Realloc(void*, ulong, uint)
                                `mov edx,NewSize; mov ecx,Alignment; call rax`
                                → game GMalloc Realloc (first live game call)
#  0x80b81f80 mb+0xf5ed0+0xb0   RC::Unreal::FMemory::Realloc(void*, ulong, uint)
#  0x818b6840 mb+0xe2a7b0+0x90  RC::Unreal::TSizedHeapAllocator<32, RC::Unreal::FMemory>::
                                ForAnyElementType::ResizeAllocation(int,int,ulong)
#  0x80ad6392 mb+0x4a2d0+0xc2   RC::Unreal::TArray<RC::Unreal::FUObjectDeleteListener*,
                                TSizedDefaultAllocator<32>>::ResizeTo(int)   [via PLT 0x1227db0]
#  0x80ad4a14 mb+0x48930+0xe4   RC::Unreal::TArray<FUObjectDeleteListener*,…>::Append(…)
#  0x80ad486f mb+0x487a0+0xcf   RC::Unreal::FUObjectArray::AddUObjectDeleteListener(
                                RC::Unreal::FUObjectDeleteListener*)
#  0x80b5068a mb+0xc4290+0x3fa  RC::Unreal::UnrealInitializer::PostInitialize(Config const&)
                                — lea rdi,<listener name string>; lea rax,AddUObjectDeleteListener; call rax
#  0x80b4bdd3 mb+0xbc180+0x3c53 RC::Unreal::UnrealInitializer::Initialize(Config const&)
#  0x80db89fe mb+0x32c2f0+0x70e RC::UE4SSProgram::setup_unreal()   [call rax]
#  0x80db7ded mb+0x32bbe0+0x20d RC::UE4SSProgram::init()
#  0x81c0b4b7 mb+0x117f470+0x47 modbridge::native::(anon)::UE4SSBackend::initialize(std::function<void()>)
#  0x81c0e094 mb+0x1181f20+0x174 modbridge::native::RuntimeOwner::initialize()
#  0x81c0fc52 mb+0x1183bf0+0x62 std::__1::__invoke<&RuntimeOwner::initialize>
#  0x81c0fbde mb+0x1183bb0+0x2e std::__1::__thread_execute<…RuntimeOwner…>
#  0x81c0f9b0 mb+0x1183950+0x60 std::__1::__thread_proxy<…>   ← thread entry
#  0x8000763d libc.prx+0x763d   scePthreadCreate trampoline (thread root)
```

Offsets are frame-minus-base; the "+entry" values are the true function entries
confirmed by the call edges (a few sections carry a small cold fragment before
the hot entry; e.g. FMalloc::Realloc's section starts 0x20 before its entry —
the entry used above is the one callers actually target).

## 4. Thread identity

- klog `thread name` is **empty** in all abort blocks; tid 101547 (pid 208),
  102053 (pid 266). Outermost frames are `__thread_proxy` ← libc scePthread
  trampoline: an **unnamed `std::thread`**.
- Source confirms which one: `lineage/runtime_owner.hpp` — `RuntimeOwner` owns
  `std::thread thread_`; `RuntimeOwner::initialize()` runs the backend
  (`UE4SSBackend::initialize` → `RC::UE4SSProgram::init`). `module_start` →
  `StartActivatedRuntimeWithAck` → `owner->start()` spawns this serialized
  **runtime init thread** (runtime_start_bridge.cpp ~L1864).
- So the crashing thread is the **modbridge UE4SS init thread** — NOT the game
  thread, NOT the pad-poll thread, and NOT the one-shot copyin-initializer
  worker (that is separate machinery around `module_start`, already retired per
  agent Q).

## 5. eboot frames (game side)

No eboot symbol table exists locally: `/tmp/eboot-backup.bin` (194 MB decrypted
view, Oct 6) has its section-header table beyond EOF (shoff `0x6f48a0a8`), so
only program headers are usable; the dynsym is import-only. The five eboot
frames were therefore resolved by decoding call edges in the decrypted bytes
(all five are genuine return addresses; every call target matches the next
frame's function start):

- `eboot+0xb4b75` fn ← entered from modbridge `FMalloc::Realloc`'s `call rax`
  (the resolved game GMalloc Realloc path);
- chain of three direct calls (`0xb2160` → `0x1750b0` → `0x1e3900`);
- `fn@0x1e3900+0x4d8`: `call [rax+0x10]` — a **virtual dispatch at vtable slot
  +0x10** — landed in the deepest function;
- deepest fn (≈`0xf850`): a fatal-error formatter — loads a format string
  (`lea rdi,…`), calls a 7-arg formatter (incl. an xmm double), conditionally
  frees, checks a global "quiet" byte flag, and on flag==0 executes
  `call abort@PLT (0x7472200); ud2`. This is the game's appErrorf-style fatal
  path, i.e. the game itself decided the condition was fatal and called abort.

## 6. Mechanism inference (SaveGame mount/unmount window)

All six abort sessions show the same sequence: the runtime init thread completes
UE4SS bootstrap (last log line "Adding shutdown listener" = `UnrealInitializer.cpp:527`,
printed at the top of `PostInitialize`), and then crashes INSIDE the very next
call — `AddUObjectDeleteListener` (`:528`) growing the delete-listener
`TArray` (`Append → ResizeTo → ResizeAllocation → FMemory::Realloc →
RC::Unreal::FMalloc::Realloc`), whose `call rax` is the **first live call into
the game's GMalloc from the listener-registration path**. Inside the game
allocator, three frames deep, a virtual call at slot +0x10 dispatched into the
game's fatal-error formatter which called `abort()` → CE-108255-1. The klog
correlates these crashes with the game thread's SaveData mount/unmount
completing seconds earlier (Z1 §2): the init thread's first live heap mutation
races the game's save-teardown, and the game side treats the outcome as a fatal
allocator condition (heap-state race or an FMalloc-vtable/slot mismatch of the
kind already flagged — l21's "Virtual FMalloc::QuantizeSize unavailable" and
§4b's missing vtable slot 4). The `copyin: has nonsleeping lock` line at the
backtrace boundary is a kernel dump artifact (it could not copy the user stack
while the thread held a nonsleeping lock), not a modbridge copyin call — no
modbridge copyin/probe frame appears anywhere in the stack. This confirms
agent G's §3 prediction exactly: the init stall/crash is in
`AddUObjectDeleteListener`, the first write into live game state, in every arm
(hooks on or off).

Practical implication for l25: the fault is not in hook settings, Lua mods, or
the retired copyin probe; it is the UE4SS init thread's first GMalloc-mediated
write racing/faulting during save-data teardown. A code-level bisect should
start at deferring `AddUObjectDeleteListener` (and any init-time TArray growth
through the game allocator) out of the save-load window, or routing listener
registration through a safer allocation path.

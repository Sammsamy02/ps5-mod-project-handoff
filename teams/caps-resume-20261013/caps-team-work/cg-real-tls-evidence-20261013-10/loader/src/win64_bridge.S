// win64_bridge.S -- Win64 <-> SysV x86_64 calling-convention bridge (asm).
//
// STATUS: staged Mac tests verify integer argument placement for host->guest
// call5/6 and guest->host arities 5..8. Other ABI shapes and target behavior
// remain unverified.
// Read DESIGN/native-dll-loader.md section 4a first.
//
// Conventions:
//   * Win64: int args RCX,RDX,R8,R9 then stack at [rsp_entry+0x28+8k]
//     (0x08 return addr + 0x20 shadow space); RSI,RDI,RBX,RBP,R12-R15 and
//     XMM6-XMM15 non-volatile; rsp 16-aligned BEFORE a call instruction.
//   * SysV:  int args RDI,RSI,RDX,RCX,R8,R9 then stack at [rsp_entry+8+8k];
//     RBX,RBP,R12-R15 non-volatile (RSI,RDI and all XMM volatile).
//   * Both:  return in RAX; rsp must be 16-byte aligned at each `call`.
//
// Syntax: Intel (noprefix). Symbol naming: macOS prefixes C symbols with
// '_'; ELF (PS5) does not. The prefix is applied by a CPP-level macro only
// at top level; the gas .macro below takes only numeric arguments so the
// two macro systems never mix.

#ifdef __APPLE__
#define GLOBALFN(name) .globl _##name ; _##name:
#else
#define GLOBALFN(name) .globl name ; name:
#endif

.intel_syntax noprefix
.text

// ==========================================================================
// HOST -> GUEST: SysV caller invokes cap13_win64_callN(fn, ...).
// The guest preserves a superset of SysV non-volatiles, so only the register
// remap, the 32 B shadow space and stack-arg relocation are needed.
// ==========================================================================

// call0: rdi = fn
GLOBALFN(cap13_win64_call0)
    sub  rsp, 0x28              // shadow space + alignment (entry rsp%16==8)
    call rdi
    add  rsp, 0x28
    ret

// call1: rdi = fn, rsi = a1
GLOBALFN(cap13_win64_call1)
    sub  rsp, 0x28
    mov  rax, rdi               // target (rcx is needed for the remap)
    mov  rcx, rsi               // win a1 = sysv a1
    call rax
    add  rsp, 0x28
    ret

// call2: rdi = fn, rsi = a1, rdx = a2 (rdx is win a2 already)
GLOBALFN(cap13_win64_call2)
    sub  rsp, 0x28
    mov  rax, rdi
    mov  rcx, rsi
    call rax
    add  rsp, 0x28
    ret

// call3: rdi = fn, rsi = a1, rdx = a2, rcx = a3 (DllMain shape)
GLOBALFN(cap13_win64_call3)
    sub  rsp, 0x28
    mov  rax, rdi
    mov  r8,  rcx               // win a3 = sysv a3
    mov  rcx, rsi               // win a1 = sysv a1 (after the r8 move!)
    call rax
    add  rsp, 0x28
    ret

// call4: rdi = fn, rsi = a1, rdx = a2, rcx = a3, r8 = a4
GLOBALFN(cap13_win64_call4)
    sub  rsp, 0x28
    mov  rax, rdi
    mov  r9,  r8                // win a4 = sysv a4
    mov  r8,  rcx               // win a3 = sysv a3
    mov  rcx, rsi               // win a1 = sysv a1 (rdx already win a2)
    call rax
    add  rsp, 0x28
    ret

// call5: fn consumes SysV RDI, so a5 arrives in R9; Win64 a5 is on stack.
GLOBALFN(cap13_win64_call5)
    sub  rsp, 0x28
    mov  [rsp + 0x20], r9       // win a5 slot (just above the shadow space)
    mov  rax, rdi
    mov  r9,  r8
    mov  r8,  rcx
    mov  rcx, rsi
    call rax
    add  rsp, 0x28
    ret

// call6: SysV a5 is R9, a6 is [rsp_entry+8]; Win64 a5/a6 use stack slots.
GLOBALFN(cap13_win64_call6)
    mov  r10, [rsp + 8]         // read a6 before allocating the Win64 frame
    sub  rsp, 0x38              // shadow + two slots + alignment; preserve our return address
    mov  [rsp + 0x20], r9       // a5
    mov  [rsp + 0x28], r10      // a6
    mov  rax, rdi
    mov  r9,  r8
    mov  r8,  rcx
    mov  rcx, rsi
    call rax
    add  rsp, 0x38
    ret

// ==========================================================================
// GUEST -> HOST: the Win64 caller enters the stamped trampoline, which
// loads r11 = SysV target and jumps into one of these shared arity thunks.
//
// Frame (all variants), rsp-relative after `sub rsp, 0xE8`:
//   0x000..0x01F  staged SysV stack args (guest args 7 and 8)
//   0x020..0x02F  pad
//   0x030..0x0CF  XMM6..XMM15 saves (10 x 16 B)
//   0x0D0         saved RSI      0x0D8         saved RDI
//   frame 0xE8 = 232: entry rsp%16==8 -> rsp%16==0 before the call.
// Guest stack arg k (k=0 is win arg5) sits at [rsp_entry + 0x28 + 8k],
// i.e. [rsp + 0xE8 + 0x28 + 8k] = [rsp + 0x110 + 8k] after the sub.
// R10 is the only scratch (volatile in both conventions); R11 keeps the
// SysV target until `call r11`.
// ==========================================================================

.macro G2H_THUNK n_stack
    sub  rsp, 0xE8
    movups [rsp + 0x030], xmm6
    movups [rsp + 0x040], xmm7
    movups [rsp + 0x050], xmm8
    movups [rsp + 0x060], xmm9
    movups [rsp + 0x070], xmm10
    movups [rsp + 0x080], xmm11
    movups [rsp + 0x090], xmm12
    movups [rsp + 0x0A0], xmm13
    movups [rsp + 0x0B0], xmm14
    movups [rsp + 0x0C0], xmm15
    mov  [rsp + 0x0D0], rsi
    mov  [rsp + 0x0D8], rdi

.if \n_stack > 2
    mov  r10, [rsp + 0x120]
    mov  [rsp + 0x000], r10
.endif
.if \n_stack > 3
    mov  r10, [rsp + 0x128]
    mov  [rsp + 0x008], r10
.endif

    mov  rdi, rcx               // sysv a1 = win a1
    mov  rsi, rdx               // sysv a2 = win a2
    mov  rdx, r8                // sysv a3 = win a3
    mov  rcx, r9                // sysv a4 = win a4
.if \n_stack > 0
    mov  r8,  [rsp + 0x110]     // sysv a5 = win stack a5
.endif
.if \n_stack > 1
    mov  r9,  [rsp + 0x118]     // sysv a6 = win stack a6
.endif
    call r11                    // SysV target (r11 from the trampoline)

    movups xmm6,  [rsp + 0x030]
    movups xmm7,  [rsp + 0x040]
    movups xmm8,  [rsp + 0x050]
    movups xmm9,  [rsp + 0x060]
    movups xmm10, [rsp + 0x070]
    movups xmm11, [rsp + 0x080]
    movups xmm12, [rsp + 0x090]
    movups xmm13, [rsp + 0x0A0]
    movups xmm14, [rsp + 0x0B0]
    movups xmm15, [rsp + 0x0C0]
    mov  rsi, [rsp + 0x0D0]
    mov  rdi, [rsp + 0x0D8]
    add  rsp, 0xE8
    ret
.endm

GLOBALFN(cap13_g2h_thunk4s0)
    G2H_THUNK 0

GLOBALFN(cap13_g2h_thunk4s1)
    G2H_THUNK 1

GLOBALFN(cap13_g2h_thunk4s2)
    G2H_THUNK 2

GLOBALFN(cap13_g2h_thunk4s3)
    G2H_THUNK 3

GLOBALFN(cap13_g2h_thunk4s4)
    G2H_THUNK 4

// NOTE (honest scope): mixed int/float signatures are NOT covered here --
// Win64 and SysV assign XMM registers differently for mixed argument lists.
// The M1 generator emits per-signature thunks from the descriptor table;
// until then, mixed-signature crossings are refused at shim registration.

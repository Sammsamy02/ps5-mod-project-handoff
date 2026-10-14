#ifdef __APPLE__
#define GLOBALFN(name) .globl _##name ; _##name:
#else
#define GLOBALFN(name) .globl name ; name:
#endif

.intel_syntax noprefix
.text

// Legal SysV target: RSI, RDI and all XMM registers are volatile there.
GLOBALFN(cap13_hostile_sysv_target)
    xor rsi, rsi
    xor rdi, rdi
    pxor xmm6, xmm6
    pxor xmm15, xmm15
    mov eax, 0x5A
    ret

GLOBALFN(cap13_sysv_alignment_target)
    mov rax, rsp
    and eax, 0xF
    ret

// SysV test caller sets Windows nonvolatiles, then invokes a Win64 entry.
// Return bits: 0 RSI, 1 RDI, 2 XMM6, 3 XMM15, 4 target return value.
GLOBALFN(cap13_test_context_nonvolatiles)
    mov r11, rdi
    movabs rsi, 0x1122334455667788
    movabs rdi, 0x8877665544332211
    pcmpeqd xmm6, xmm6
    pcmpeqd xmm15, xmm15
    sub rsp, 0x28
    call r11
    add rsp, 0x28

    xor r8d, r8d
    cmp eax, 0x5A
    jne 1f
    or r8d, 0x10
1:
    movabs rdx, 0x1122334455667788
    cmp rsi, rdx
    jne 2f
    or r8d, 0x01
2:
    movabs rdx, 0x8877665544332211
    cmp rdi, rdx
    jne 3f
    or r8d, 0x02
3:
    pmovmskb edx, xmm6
    cmp edx, 0xFFFF
    jne 4f
    or r8d, 0x04
4:
    pmovmskb edx, xmm15
    cmp edx, 0xFFFF
    jne 5f
    or r8d, 0x08
5:
    mov eax, r8d
    ret

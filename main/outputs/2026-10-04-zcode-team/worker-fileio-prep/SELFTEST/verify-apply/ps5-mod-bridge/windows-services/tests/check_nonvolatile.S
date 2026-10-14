/* SysV test caller -> Microsoft x64 service. Returns 0 if every Microsoft
 * nonvolatile register except RSP keeps its sentinel; normal return and our
 * saved-register restores also depend on the original RSP. Not production. */
#if defined(__APPLE__)
#define TEST_SYMBOL _mb_test_nonvolatile
#else
#define TEST_SYMBOL mb_test_nonvolatile
#endif
    .text
    .globl TEST_SYMBOL
TEST_SYMBOL:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    /* Six pushes leave RSP == 8 (mod 16). 40 bytes align the call and
     * reserve the Microsoft ABI's 32 bytes of shadow space. */
    subq $40, %rsp
    movq %rdi, %rax       /* callee address */
    movq %rcx, %r8        /* third service argument */
    movq %rsi, %rcx       /* first; second already in RDX */
    movabsq $0x1234567887654321, %r10
    movq %r10, %rbp
    movq %r10, %rbx
    movq %r10, %rdi
    movq %r10, %rsi
    movq %r10, %r12
    movq %r10, %r13
    movq %r10, %r14
    movq %r10, %r15
    pcmpeqd %xmm6, %xmm6
    pcmpeqd %xmm7, %xmm7
    pcmpeqd %xmm8, %xmm8
    pcmpeqd %xmm9, %xmm9
    pcmpeqd %xmm10, %xmm10
    pcmpeqd %xmm11, %xmm11
    pcmpeqd %xmm12, %xmm12
    pcmpeqd %xmm13, %xmm13
    pcmpeqd %xmm14, %xmm14
    pcmpeqd %xmm15, %xmm15
    callq *%rax
    movabsq $0x1234567887654321, %r10
    cmpq %r10, %rbp
    jne .Lbad
    cmpq %r10, %rbx
    jne .Lbad
    cmpq %r10, %rdi
    jne .Lbad
    cmpq %r10, %rsi
    jne .Lbad
    cmpq %r10, %r12
    jne .Lbad
    cmpq %r10, %r13
    jne .Lbad
    cmpq %r10, %r14
    jne .Lbad
    cmpq %r10, %r15
    jne .Lbad
    .macro CHECK_XMM register
    pcmpeqd %xmm0, %xmm0
    pcmpeqb %\register, %xmm0
    pmovmskb %xmm0, %r11d
    cmpl $65535, %r11d
    jne .Lbad
    .endm
    CHECK_XMM xmm6
    CHECK_XMM xmm7
    CHECK_XMM xmm8
    CHECK_XMM xmm9
    CHECK_XMM xmm10
    CHECK_XMM xmm11
    CHECK_XMM xmm12
    CHECK_XMM xmm13
    CHECK_XMM xmm14
    CHECK_XMM xmm15
    xorl %eax, %eax
    jmp .Ldone
.Lbad:
    movl $1, %eax
.Ldone:
    addq $40, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    retq
#if defined(__ELF__)
    .section .note.GNU-stack,"",@progbits
#endif

	.file	"target_call.cpp"
	.section	.text.l15_target_call,"ax",@progbits
	.globl	l15_target_call                 # -- Begin function l15_target_call
	.p2align	4
	.type	l15_target_call,@function
l15_target_call:                        # @l15_target_call
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%r15
	pushq	%r14
	pushq	%rbx
	subq	$2088, %rsp                     # imm = 0x828
	.cfi_offset %rbx, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	leaq	.L.str.4(%rip), %rax
	movb	$0, -48(%rbp)
	movq	$0, -40(%rbp)
	movq	%rax, -32(%rbp)
	testq	%rdi, %rdi
	je	.LBB0_14
# %bb.1:
	movq	%rdi, %rbx
	leaq	-2112(%rbp), %rdi
	movl	$2050, %edx                     # imm = 0x802
	xorl	%esi, %esi
	leaq	-40(%rbp), %r14
	xorl	%r15d, %r15d
	callq	*memset@GOTPCREL(%rip)
	leaq	.L.str(%rip), %rax
	xorl	%ecx, %ecx
	jmp	.LBB0_2
	.p2align	4
.LBB0_11:                               #   in Loop: Header=BB0_2 Depth=1
	movw	%dx, -2112(%rbp,%r15,2)
	incq	%r15
.LBB0_12:                               #   in Loop: Header=BB0_2 Depth=1
	cmpq	$1023, %rcx                     # imm = 0x3FF
	leaq	1(%rcx), %rcx
	jae	.LBB0_14
.LBB0_2:                                # =>This Inner Loop Header: Depth=1
	movzwl	(%rax,%rcx,2), %edx
	testl	%edx, %edx
	je	.LBB0_13
# %bb.3:                                #   in Loop: Header=BB0_2 Depth=1
	movl	%edx, %esi
	andl	$64512, %esi                    # imm = 0xFC00
	cmpw	$-10240, %si                    # imm = 0xD800
	je	.LBB0_5
# %bb.4:                                #   in Loop: Header=BB0_2 Depth=1
	cmpl	$56320, %esi                    # imm = 0xDC00
	jne	.LBB0_8
	jmp	.LBB0_14
	.p2align	4
.LBB0_5:                                #   in Loop: Header=BB0_2 Depth=1
	cmpq	$1023, %rcx                     # imm = 0x3FF
	je	.LBB0_14
# %bb.6:                                #   in Loop: Header=BB0_2 Depth=1
	movzwl	2(%rax,%rcx,2), %esi
	movl	%esi, %edi
	andl	$64512, %edi                    # imm = 0xFC00
	cmpw	$-9216, %di                     # imm = 0xDC00
	jne	.LBB0_14
# %bb.7:                                #   in Loop: Header=BB0_2 Depth=1
	shll	$10, %edx
	incq	%rcx
	leal	-56613888(%rdx,%rsi), %edx
.LBB0_8:                                #   in Loop: Header=BB0_2 Depth=1
	cmpl	$65536, %edx                    # imm = 0x10000
	movq	%r15, %rsi
	sbbq	$-1, %rsi
	incq	%rsi
	cmpq	$1024, %rsi                     # imm = 0x400
	ja	.LBB0_14
# %bb.9:                                #   in Loop: Header=BB0_2 Depth=1
	cmpl	$65536, %edx                    # imm = 0x10000
	jb	.LBB0_11
# %bb.10:                               #   in Loop: Header=BB0_2 Depth=1
	leal	67043328(%rdx), %esi
	andl	$1023, %edx                     # imm = 0x3FF
	shrl	$10, %esi
	orl	$56320, %edx                    # imm = 0xDC00
	addl	$-10240, %esi                   # imm = 0xD800
	movw	%si, -2112(%rbp,%r15,2)
	movw	%dx, -2110(%rbp,%r15,2)
	addq	$2, %r15
	jmp	.LBB0_12
.LBB0_13:
	leaq	-2112(%rbp), %rsi
	movq	%r14, %rdi
	xorl	%edx, %edx
	movw	$0, -2112(%rbp,%r15,2)
	callq	*%rbx
.LBB0_14:
	movq	-40(%rbp), %rax
	addq	$2088, %rsp                     # imm = 0x828
	popq	%rbx
	popq	%r14
	popq	%r15
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	l15_target_call, .Lfunc_end0-l15_target_call
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str2.2,"aMS",@progbits,2
	.p2align	1, 0x0
.L.str:
	.short	66                              # 0x42
	.short	121                             # 0x79
	.short	116                             # 0x74
	.short	101                             # 0x65
	.short	80                              # 0x50
	.short	114                             # 0x72
	.short	111                             # 0x6f
	.short	112                             # 0x70
	.short	101                             # 0x65
	.short	114                             # 0x72
	.short	116                             # 0x74
	.short	121                             # 0x79
	.short	0                               # 0x0
	.size	.L.str, 26

	.type	.L.str.4,@object                # @.str.4
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str.4:
	.asciz	"unavailable constructor"
	.size	.L.str.4, 24

	.ident	"clang version 21.1.8 (https://github.com/llvm/llvm-project 2078da43e25a4623cab2d0d60decddf709aaea28)"
	.section	".note.GNU-stack","",@progbits
	.addrsig

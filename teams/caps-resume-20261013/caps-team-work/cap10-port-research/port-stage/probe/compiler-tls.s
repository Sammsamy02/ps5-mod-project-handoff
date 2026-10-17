	.file	"compiler-tls.c"
	.section	.text.get_tls_address,"ax",@progbits
	.globl	get_tls_address                 # -- Begin function get_tls_address
	.p2align	4
	.type	get_tls_address,@function
get_tls_address:                        # @get_tls_address
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	__emutls_v.tls_value@GOTPCREL(%rip), %rdi
	movq	__emutls_get_address@GOTPCREL(%rip), %rax
	callq	*%rax
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	get_tls_address, .Lfunc_end0-get_tls_address
	.cfi_endproc
                                        # -- End function
	.type	__emutls_v.tls_value,@object    # @__emutls_v.tls_value
	.section	.data.__emutls_v.tls_value,"aw",@progbits
	.globl	__emutls_v.tls_value
	.p2align	3, 0x0
__emutls_v.tls_value:
	.quad	8                               # 0x8
	.quad	8                               # 0x8
	.quad	0
	.quad	__emutls_t.tls_value
	.size	__emutls_v.tls_value, 32

	.type	__emutls_t.tls_value,@object    # @__emutls_t.tls_value
	.section	.rodata.__emutls_t.tls_value,"a",@progbits
	.globl	__emutls_t.tls_value
	.p2align	3, 0x0
__emutls_t.tls_value:
	.quad	0
	.size	__emutls_t.tls_value, 8

	.ident	"clang version 21.1.8 (https://github.com/llvm/llvm-project 2078da43e25a4623cab2d0d60decddf709aaea28)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __emutls_t.tls_value

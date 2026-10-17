	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 15, 0	sdk_version 26, 2
	.globl	_make_real_pair                 ## -- Begin function make_real_pair
	.p2align	4, 0x90
_make_real_pair:                        ## @make_real_pair
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
                                        ## kill: def $esi killed $esi def $rsi
	shlq	$32, %rsi
	movl	%edi, %eax
	orq	%rsi, %rax
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
.subsections_via_symbols

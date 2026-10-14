	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 15, 0	sdk_version 26, 2
	.globl	_make_pair8                     ## -- Begin function make_pair8
	.p2align	4, 0x90
_make_pair8:                            ## @make_pair8
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
	.globl	_consume_pair8                  ## -- Begin function consume_pair8
	.p2align	4, 0x90
_consume_pair8:                         ## @consume_pair8
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, %rax
	shrq	$32, %rax
	addl	%edi, %eax
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
.subsections_via_symbols

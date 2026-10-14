	.file	"mono-tls.c"
	.section	.text.mono_tls_init_gc_keys,"ax",@progbits
	.globl	mono_tls_init_gc_keys           # -- Begin function mono_tls_init_gc_keys
	.p2align	4
	.type	mono_tls_init_gc_keys,@function
mono_tls_init_gc_keys:                  # @mono_tls_init_gc_keys
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_sgen_thread_info@GOTPCREL(%rip), %rdi
	xorl	%eax, %eax
	movl	%eax, %esi
	callq	mono_native_tls_alloc
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	$-1, 12(%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	mono_tls_init_gc_keys, .Lfunc_end0-mono_tls_init_gc_keys
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_native_tls_alloc,"ax",@progbits
	.p2align	4                               # -- Begin function mono_native_tls_alloc
	.type	mono_native_tls_alloc,@function
mono_native_tls_alloc:                  # @mono_native_tls_alloc
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rdi
	movq	-16(%rbp), %rsi
	callq	*pthread_key_create@GOTPCREL(%rip)
	cmpl	$0, %eax
	sete	%al
	andb	$1, %al
	movzbl	%al, %eax
	addq	$16, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	mono_native_tls_alloc, .Lfunc_end1-mono_native_tls_alloc
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_init_runtime_keys,"ax",@progbits
	.globl	mono_tls_init_runtime_keys      # -- Begin function mono_tls_init_runtime_keys
	.p2align	4
	.type	mono_tls_init_runtime_keys,@function
mono_tls_init_runtime_keys:             # @mono_tls_init_runtime_keys
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_thread@GOTPCREL(%rip), %rdi
	xorl	%eax, %eax
	movl	%eax, %esi
	callq	mono_native_tls_alloc
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	$-1, 16(%rax)
	movq	mono_tls_key_jit_tls@GOTPCREL(%rip), %rdi
	xorl	%eax, %eax
	movl	%eax, %esi
	callq	mono_native_tls_alloc
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	$-1, 4(%rax)
	movq	mono_tls_key_domain@GOTPCREL(%rip), %rdi
	xorl	%eax, %eax
	movl	%eax, %esi
	callq	mono_native_tls_alloc
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	$-1, (%rax)
	movq	mono_tls_key_lmf_addr@GOTPCREL(%rip), %rdi
	xorl	%eax, %eax
	movl	%eax, %esi
	callq	mono_native_tls_alloc
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	$-1, 8(%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end2:
	.size	mono_tls_init_runtime_keys, .Lfunc_end2-mono_tls_init_runtime_keys
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_free_keys,"ax",@progbits
	.globl	mono_tls_free_keys              # -- Begin function mono_tls_free_keys
	.p2align	4
	.type	mono_tls_free_keys,@function
mono_tls_free_keys:                     # @mono_tls_free_keys
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_thread@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	mono_native_tls_free
	movq	mono_tls_key_jit_tls@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	mono_native_tls_free
	movq	mono_tls_key_domain@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	mono_native_tls_free
	movq	mono_tls_key_sgen_thread_info@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	mono_native_tls_free
	movq	mono_tls_key_lmf_addr@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	mono_native_tls_free
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end3:
	.size	mono_tls_free_keys, .Lfunc_end3-mono_tls_free_keys
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_native_tls_free,"ax",@progbits
	.p2align	4                               # -- Begin function mono_native_tls_free
	.type	mono_native_tls_free,@function
mono_native_tls_free:                   # @mono_native_tls_free
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	%edi, -4(%rbp)
	movl	-4(%rbp), %edi
	callq	*pthread_key_delete@GOTPCREL(%rip)
	addq	$16, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end4:
	.size	mono_native_tls_free, .Lfunc_end4-mono_native_tls_free
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_thread_extern,"ax",@progbits
	.globl	mono_tls_get_thread_extern      # -- Begin function mono_tls_get_thread_extern
	.p2align	4
	.type	mono_tls_get_thread_extern,@function
mono_tls_get_thread_extern:             # @mono_tls_get_thread_extern
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	callq	mono_tls_get_thread
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end5:
	.size	mono_tls_get_thread_extern, .Lfunc_end5-mono_tls_get_thread_extern
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_thread,"ax",@progbits
	.p2align	4                               # -- Begin function mono_tls_get_thread
	.type	mono_tls_get_thread,@function
mono_tls_get_thread:                    # @mono_tls_get_thread
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_thread@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	*pthread_getspecific@GOTPCREL(%rip)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end6:
	.size	mono_tls_get_thread, .Lfunc_end6-mono_tls_get_thread
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_jit_tls_extern,"ax",@progbits
	.globl	mono_tls_get_jit_tls_extern     # -- Begin function mono_tls_get_jit_tls_extern
	.p2align	4
	.type	mono_tls_get_jit_tls_extern,@function
mono_tls_get_jit_tls_extern:            # @mono_tls_get_jit_tls_extern
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	callq	mono_tls_get_jit_tls
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end7:
	.size	mono_tls_get_jit_tls_extern, .Lfunc_end7-mono_tls_get_jit_tls_extern
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_jit_tls,"ax",@progbits
	.p2align	4                               # -- Begin function mono_tls_get_jit_tls
	.type	mono_tls_get_jit_tls,@function
mono_tls_get_jit_tls:                   # @mono_tls_get_jit_tls
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_jit_tls@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	*pthread_getspecific@GOTPCREL(%rip)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end8:
	.size	mono_tls_get_jit_tls, .Lfunc_end8-mono_tls_get_jit_tls
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_domain_extern,"ax",@progbits
	.globl	mono_tls_get_domain_extern      # -- Begin function mono_tls_get_domain_extern
	.p2align	4
	.type	mono_tls_get_domain_extern,@function
mono_tls_get_domain_extern:             # @mono_tls_get_domain_extern
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	callq	mono_tls_get_domain
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end9:
	.size	mono_tls_get_domain_extern, .Lfunc_end9-mono_tls_get_domain_extern
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_domain,"ax",@progbits
	.p2align	4                               # -- Begin function mono_tls_get_domain
	.type	mono_tls_get_domain,@function
mono_tls_get_domain:                    # @mono_tls_get_domain
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_domain@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	*pthread_getspecific@GOTPCREL(%rip)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end10:
	.size	mono_tls_get_domain, .Lfunc_end10-mono_tls_get_domain
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_sgen_thread_info_extern,"ax",@progbits
	.globl	mono_tls_get_sgen_thread_info_extern # -- Begin function mono_tls_get_sgen_thread_info_extern
	.p2align	4
	.type	mono_tls_get_sgen_thread_info_extern,@function
mono_tls_get_sgen_thread_info_extern:   # @mono_tls_get_sgen_thread_info_extern
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	callq	mono_tls_get_sgen_thread_info
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end11:
	.size	mono_tls_get_sgen_thread_info_extern, .Lfunc_end11-mono_tls_get_sgen_thread_info_extern
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_sgen_thread_info,"ax",@progbits
	.p2align	4                               # -- Begin function mono_tls_get_sgen_thread_info
	.type	mono_tls_get_sgen_thread_info,@function
mono_tls_get_sgen_thread_info:          # @mono_tls_get_sgen_thread_info
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_sgen_thread_info@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	*pthread_getspecific@GOTPCREL(%rip)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end12:
	.size	mono_tls_get_sgen_thread_info, .Lfunc_end12-mono_tls_get_sgen_thread_info
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_lmf_addr_extern,"ax",@progbits
	.globl	mono_tls_get_lmf_addr_extern    # -- Begin function mono_tls_get_lmf_addr_extern
	.p2align	4
	.type	mono_tls_get_lmf_addr_extern,@function
mono_tls_get_lmf_addr_extern:           # @mono_tls_get_lmf_addr_extern
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	callq	mono_tls_get_lmf_addr
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end13:
	.size	mono_tls_get_lmf_addr_extern, .Lfunc_end13-mono_tls_get_lmf_addr_extern
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_get_lmf_addr,"ax",@progbits
	.p2align	4                               # -- Begin function mono_tls_get_lmf_addr
	.type	mono_tls_get_lmf_addr,@function
mono_tls_get_lmf_addr:                  # @mono_tls_get_lmf_addr
	.cfi_startproc
# %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	mono_tls_key_lmf_addr@GOTPCREL(%rip), %rax
	movl	(%rax), %edi
	callq	*pthread_getspecific@GOTPCREL(%rip)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end14:
	.size	mono_tls_get_lmf_addr, .Lfunc_end14-mono_tls_get_lmf_addr
	.cfi_endproc
                                        # -- End function
	.type	mono_tls_key_sgen_thread_info,@object # @mono_tls_key_sgen_thread_info
	.section	.bss.mono_tls_key_sgen_thread_info,"aw",@nobits
	.globl	mono_tls_key_sgen_thread_info
	.p2align	2, 0x0
mono_tls_key_sgen_thread_info:
	.long	0                               # 0x0
	.size	mono_tls_key_sgen_thread_info, 4

	.type	mono_tls_offsets,@object        # @mono_tls_offsets
	.section	.bss.mono_tls_offsets,"aw",@nobits
	.globl	mono_tls_offsets
	.p2align	4, 0x0
mono_tls_offsets:
	.zero	20
	.size	mono_tls_offsets, 20

	.type	mono_tls_key_thread,@object     # @mono_tls_key_thread
	.section	.bss.mono_tls_key_thread,"aw",@nobits
	.globl	mono_tls_key_thread
	.p2align	2, 0x0
mono_tls_key_thread:
	.long	0                               # 0x0
	.size	mono_tls_key_thread, 4

	.type	mono_tls_key_jit_tls,@object    # @mono_tls_key_jit_tls
	.section	.bss.mono_tls_key_jit_tls,"aw",@nobits
	.globl	mono_tls_key_jit_tls
	.p2align	2, 0x0
mono_tls_key_jit_tls:
	.long	0                               # 0x0
	.size	mono_tls_key_jit_tls, 4

	.type	mono_tls_key_domain,@object     # @mono_tls_key_domain
	.section	.bss.mono_tls_key_domain,"aw",@nobits
	.globl	mono_tls_key_domain
	.p2align	2, 0x0
mono_tls_key_domain:
	.long	0                               # 0x0
	.size	mono_tls_key_domain, 4

	.type	mono_tls_key_lmf_addr,@object   # @mono_tls_key_lmf_addr
	.section	.bss.mono_tls_key_lmf_addr,"aw",@nobits
	.globl	mono_tls_key_lmf_addr
	.p2align	2, 0x0
mono_tls_key_lmf_addr:
	.long	0                               # 0x0
	.size	mono_tls_key_lmf_addr, 4

	.ident	"clang version 21.1.8 (https://github.com/llvm/llvm-project 2078da43e25a4623cab2d0d60decddf709aaea28)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym mono_native_tls_alloc
	.addrsig_sym mono_native_tls_free
	.addrsig_sym mono_tls_get_thread
	.addrsig_sym mono_tls_get_jit_tls
	.addrsig_sym mono_tls_get_domain
	.addrsig_sym mono_tls_get_sgen_thread_info
	.addrsig_sym mono_tls_get_lmf_addr
	.addrsig_sym pthread_key_create
	.addrsig_sym pthread_key_delete
	.addrsig_sym pthread_getspecific
	.addrsig_sym mono_tls_key_sgen_thread_info
	.addrsig_sym mono_tls_offsets
	.addrsig_sym mono_tls_key_thread
	.addrsig_sym mono_tls_key_jit_tls
	.addrsig_sym mono_tls_key_domain
	.addrsig_sym mono_tls_key_lmf_addr

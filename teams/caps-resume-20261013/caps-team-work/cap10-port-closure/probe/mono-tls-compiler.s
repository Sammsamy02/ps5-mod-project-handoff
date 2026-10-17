	.file	"mono-tls.c"
	.section	.text.mono_tls_init_gc_keys,"ax",@progbits
	.globl	mono_tls_init_gc_keys           # -- Begin function mono_tls_init_gc_keys
	.p2align	4
	.type	mono_tls_init_gc_keys,@function
mono_tls_init_gc_keys:                  # @mono_tls_init_gc_keys
	.cfi_startproc
# %bb.0:
# %bb.1:
	#APP
	movq	mono_tls_sgen_thread_info@GOTTPOFF(%rip), %rax
	#NO_APP
	movq	%rax, -8(%rsp)
	movq	-8(%rsp), %rax
	movl	%eax, %ecx
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	%ecx, 12(%rax)
# %bb.2:
	retq
.Lfunc_end0:
	.size	mono_tls_init_gc_keys, .Lfunc_end0-mono_tls_init_gc_keys
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_init_runtime_keys,"ax",@progbits
	.globl	mono_tls_init_runtime_keys      # -- Begin function mono_tls_init_runtime_keys
	.p2align	4
	.type	mono_tls_init_runtime_keys,@function
mono_tls_init_runtime_keys:             # @mono_tls_init_runtime_keys
	.cfi_startproc
# %bb.0:
# %bb.1:
	#APP
	movq	mono_tls_thread@GOTTPOFF(%rip), %rax
	#NO_APP
	movq	%rax, -8(%rsp)
	movq	-8(%rsp), %rax
	movl	%eax, %ecx
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	%ecx, 16(%rax)
# %bb.2:
	jmp	.LBB1_3
.LBB1_3:
	#APP
	movq	mono_tls_jit_tls@GOTTPOFF(%rip), %rax
	#NO_APP
	movq	%rax, -16(%rsp)
	movq	-16(%rsp), %rax
	movl	%eax, %ecx
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	%ecx, 4(%rax)
# %bb.4:
	jmp	.LBB1_5
.LBB1_5:
	#APP
	movq	mono_tls_domain@GOTTPOFF(%rip), %rax
	#NO_APP
	movq	%rax, -24(%rsp)
	movq	-24(%rsp), %rax
	movl	%eax, %ecx
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	%ecx, (%rax)
# %bb.6:
	jmp	.LBB1_7
.LBB1_7:
	#APP
	movq	mono_tls_lmf_addr@GOTTPOFF(%rip), %rax
	#NO_APP
	movq	%rax, -32(%rsp)
	movq	-32(%rsp), %rax
	movl	%eax, %ecx
	movq	mono_tls_offsets@GOTPCREL(%rip), %rax
	movl	%ecx, 8(%rax)
# %bb.8:
	retq
.Lfunc_end1:
	.size	mono_tls_init_runtime_keys, .Lfunc_end1-mono_tls_init_runtime_keys
	.cfi_endproc
                                        # -- End function
	.section	.text.mono_tls_free_keys,"ax",@progbits
	.globl	mono_tls_free_keys              # -- Begin function mono_tls_free_keys
	.p2align	4
	.type	mono_tls_free_keys,@function
mono_tls_free_keys:                     # @mono_tls_free_keys
	.cfi_startproc
# %bb.0:
	retq
.Lfunc_end2:
	.size	mono_tls_free_keys, .Lfunc_end2-mono_tls_free_keys
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
.Lfunc_end3:
	.size	mono_tls_get_thread_extern, .Lfunc_end3-mono_tls_get_thread_extern
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
	movq	__emutls_v.mono_tls_thread@GOTPCREL(%rip), %rdi
	movq	__emutls_get_address@GOTPCREL(%rip), %rax
	callq	*%rax
	movq	(%rax), %rax
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end4:
	.size	mono_tls_get_thread, .Lfunc_end4-mono_tls_get_thread
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
.Lfunc_end5:
	.size	mono_tls_get_jit_tls_extern, .Lfunc_end5-mono_tls_get_jit_tls_extern
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
	movq	__emutls_v.mono_tls_jit_tls@GOTPCREL(%rip), %rdi
	movq	__emutls_get_address@GOTPCREL(%rip), %rax
	callq	*%rax
	movq	(%rax), %rax
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end6:
	.size	mono_tls_get_jit_tls, .Lfunc_end6-mono_tls_get_jit_tls
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
.Lfunc_end7:
	.size	mono_tls_get_domain_extern, .Lfunc_end7-mono_tls_get_domain_extern
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
	movq	__emutls_v.mono_tls_domain@GOTPCREL(%rip), %rdi
	movq	__emutls_get_address@GOTPCREL(%rip), %rax
	callq	*%rax
	movq	(%rax), %rax
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end8:
	.size	mono_tls_get_domain, .Lfunc_end8-mono_tls_get_domain
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
.Lfunc_end9:
	.size	mono_tls_get_sgen_thread_info_extern, .Lfunc_end9-mono_tls_get_sgen_thread_info_extern
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
	movq	__emutls_v.mono_tls_sgen_thread_info@GOTPCREL(%rip), %rdi
	movq	__emutls_get_address@GOTPCREL(%rip), %rax
	callq	*%rax
	movq	(%rax), %rax
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end10:
	.size	mono_tls_get_sgen_thread_info, .Lfunc_end10-mono_tls_get_sgen_thread_info
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
.Lfunc_end11:
	.size	mono_tls_get_lmf_addr_extern, .Lfunc_end11-mono_tls_get_lmf_addr_extern
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
	movq	__emutls_v.mono_tls_lmf_addr@GOTPCREL(%rip), %rdi
	movq	__emutls_get_address@GOTPCREL(%rip), %rax
	callq	*%rax
	movq	(%rax), %rax
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end12:
	.size	mono_tls_get_lmf_addr, .Lfunc_end12-mono_tls_get_lmf_addr
	.cfi_endproc
                                        # -- End function
	.type	mono_tls_offsets,@object        # @mono_tls_offsets
	.section	.bss.mono_tls_offsets,"aw",@nobits
	.globl	mono_tls_offsets
	.p2align	4, 0x0
mono_tls_offsets:
	.zero	20
	.size	mono_tls_offsets, 20

	.type	__emutls_v.mono_tls_thread,@object # @__emutls_v.mono_tls_thread
	.section	.data.__emutls_v.mono_tls_thread,"aw",@progbits
	.globl	__emutls_v.mono_tls_thread
	.p2align	3, 0x0
__emutls_v.mono_tls_thread:
	.quad	8                               # 0x8
	.quad	8                               # 0x8
	.quad	0
	.quad	__emutls_t.mono_tls_thread
	.size	__emutls_v.mono_tls_thread, 32

	.type	__emutls_t.mono_tls_thread,@object # @__emutls_t.mono_tls_thread
	.section	.rodata.__emutls_t.mono_tls_thread,"a",@progbits
	.globl	__emutls_t.mono_tls_thread
	.p2align	3, 0x0
__emutls_t.mono_tls_thread:
	.quad	0
	.size	__emutls_t.mono_tls_thread, 8

	.type	__emutls_v.mono_tls_jit_tls,@object # @__emutls_v.mono_tls_jit_tls
	.section	.data.__emutls_v.mono_tls_jit_tls,"aw",@progbits
	.globl	__emutls_v.mono_tls_jit_tls
	.p2align	3, 0x0
__emutls_v.mono_tls_jit_tls:
	.quad	8                               # 0x8
	.quad	8                               # 0x8
	.quad	0
	.quad	__emutls_t.mono_tls_jit_tls
	.size	__emutls_v.mono_tls_jit_tls, 32

	.type	__emutls_t.mono_tls_jit_tls,@object # @__emutls_t.mono_tls_jit_tls
	.section	.rodata.__emutls_t.mono_tls_jit_tls,"a",@progbits
	.globl	__emutls_t.mono_tls_jit_tls
	.p2align	3, 0x0
__emutls_t.mono_tls_jit_tls:
	.quad	0
	.size	__emutls_t.mono_tls_jit_tls, 8

	.type	__emutls_v.mono_tls_domain,@object # @__emutls_v.mono_tls_domain
	.section	.data.__emutls_v.mono_tls_domain,"aw",@progbits
	.globl	__emutls_v.mono_tls_domain
	.p2align	3, 0x0
__emutls_v.mono_tls_domain:
	.quad	8                               # 0x8
	.quad	8                               # 0x8
	.quad	0
	.quad	__emutls_t.mono_tls_domain
	.size	__emutls_v.mono_tls_domain, 32

	.type	__emutls_t.mono_tls_domain,@object # @__emutls_t.mono_tls_domain
	.section	.rodata.__emutls_t.mono_tls_domain,"a",@progbits
	.globl	__emutls_t.mono_tls_domain
	.p2align	3, 0x0
__emutls_t.mono_tls_domain:
	.quad	0
	.size	__emutls_t.mono_tls_domain, 8

	.type	__emutls_v.mono_tls_sgen_thread_info,@object # @__emutls_v.mono_tls_sgen_thread_info
	.section	.data.__emutls_v.mono_tls_sgen_thread_info,"aw",@progbits
	.globl	__emutls_v.mono_tls_sgen_thread_info
	.p2align	3, 0x0
__emutls_v.mono_tls_sgen_thread_info:
	.quad	8                               # 0x8
	.quad	8                               # 0x8
	.quad	0
	.quad	__emutls_t.mono_tls_sgen_thread_info
	.size	__emutls_v.mono_tls_sgen_thread_info, 32

	.type	__emutls_t.mono_tls_sgen_thread_info,@object # @__emutls_t.mono_tls_sgen_thread_info
	.section	.rodata.__emutls_t.mono_tls_sgen_thread_info,"a",@progbits
	.globl	__emutls_t.mono_tls_sgen_thread_info
	.p2align	3, 0x0
__emutls_t.mono_tls_sgen_thread_info:
	.quad	0
	.size	__emutls_t.mono_tls_sgen_thread_info, 8

	.type	__emutls_v.mono_tls_lmf_addr,@object # @__emutls_v.mono_tls_lmf_addr
	.section	.data.__emutls_v.mono_tls_lmf_addr,"aw",@progbits
	.globl	__emutls_v.mono_tls_lmf_addr
	.p2align	3, 0x0
__emutls_v.mono_tls_lmf_addr:
	.quad	8                               # 0x8
	.quad	8                               # 0x8
	.quad	0
	.quad	__emutls_t.mono_tls_lmf_addr
	.size	__emutls_v.mono_tls_lmf_addr, 32

	.type	__emutls_t.mono_tls_lmf_addr,@object # @__emutls_t.mono_tls_lmf_addr
	.section	.rodata.__emutls_t.mono_tls_lmf_addr,"a",@progbits
	.globl	__emutls_t.mono_tls_lmf_addr
	.p2align	3, 0x0
__emutls_t.mono_tls_lmf_addr:
	.quad	0
	.size	__emutls_t.mono_tls_lmf_addr, 8

	.ident	"clang version 21.1.8 (https://github.com/llvm/llvm-project 2078da43e25a4623cab2d0d60decddf709aaea28)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym mono_tls_get_thread
	.addrsig_sym mono_tls_get_jit_tls
	.addrsig_sym mono_tls_get_domain
	.addrsig_sym mono_tls_get_sgen_thread_info
	.addrsig_sym mono_tls_get_lmf_addr
	.addrsig_sym mono_tls_offsets
	.addrsig_sym __emutls_t.mono_tls_thread
	.addrsig_sym __emutls_t.mono_tls_jit_tls
	.addrsig_sym __emutls_t.mono_tls_domain
	.addrsig_sym __emutls_t.mono_tls_sgen_thread_info
	.addrsig_sym __emutls_t.mono_tls_lmf_addr

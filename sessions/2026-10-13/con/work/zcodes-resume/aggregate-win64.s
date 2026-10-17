	.text
	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"aggregate_probe.cpp"
	.def	make_pair8;
	.scl	2;
	.type	32;
	.endef
	.globl	make_pair8                      # -- Begin function make_pair8
	.p2align	4, 0x90
make_pair8:                             # @make_pair8
# %bb.0:
                                        # kill: def $edx killed $edx def $rdx
	shlq	$32, %rdx
	movl	%ecx, %eax
	orq	%rdx, %rax
	retq
                                        # -- End function
	.def	consume_pair8;
	.scl	2;
	.type	32;
	.endef
	.globl	consume_pair8                   # -- Begin function consume_pair8
	.p2align	4, 0x90
consume_pair8:                          # @consume_pair8
# %bb.0:
	movq	%rcx, %rax
	shrq	$32, %rax
	addl	%ecx, %eax
	retq
                                        # -- End function
	.addrsig

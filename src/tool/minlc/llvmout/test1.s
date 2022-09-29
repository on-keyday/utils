	.text
	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"test1.min"
	.def	main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
.seh_proc main
# %bb.0:                                # %entry
	subq	$40, %rsp
	.seh_stackalloc 40
	.seh_endprologue
	movl	$1, %ecx
	movl	$2, %edx
	movl	$3, %r8d
	callq	mul_add
	nop
	addq	$40, %rsp
	retq
	.seh_endproc
                                        # -- End function
	.def	mul_add;
	.scl	2;
	.type	32;
	.endef
	.globl	mul_add                         # -- Begin function mul_add
	.p2align	4, 0x90
mul_add:                                # @mul_add
# %bb.0:                                # %entry
	movl	%ecx, %eax
	imull	%edx, %eax
	addl	%r8d, %eax
	retq
                                        # -- End function
	.def	test;
	.scl	2;
	.type	32;
	.endef
	.globl	test                            # -- Begin function test
	.p2align	4, 0x90
test:                                   # @test
# %bb.0:                                # %entry
	retq
                                        # -- End function
	.addrsig
	.addrsig_sym mul_add

# Compiler-owned PIC candidate fixture: bytes decoded only, never executed.
.text
.hidden candidate_gu, candidate_malloc, candidate_ctor, gu_alloc, gu_free, malloc_free
.globl candidate_ctor, gu_alloc, gu_free, malloc_free, gu_alloc_call, gu_free_call, malloc_virtual_call
# Incoming caller evidence for independent diagnostic routines.
caller_alloc:
lea candidate_gu(%rip),%rdi
#ifdef BAD_ARGUMENT
xor %edi,%edi
#endif
gu_alloc_call: call gu_alloc
ret
caller_free:
lea candidate_gu(%rip),%rdi
gu_free_call: call gu_free
ret
gu_alloc:
mov 12(%rsi),%eax
shr $16,%eax
lea (%rax,%rax,2),%rax
shl $3,%eax
mov %eax,12(%rsi)
lea alloc_literal(%rip),%rdx
ret
gu_free:
mov 12(%rsi),%eax
shr $16,%eax
lea (%rax,%rax,2),%rax
shl $3,%eax
movl $-1,12(%rsi)
#ifndef MISSING_FREE
lea free_literal(%rip),%rdx
#endif
#ifdef INCOMPLETE_GU
jmp *%rcx
#else
ret
#endif
# All later callees are beyond diagnostic anchors, so GU's unchanged bounded
# near-anchor search cannot invent an unrelated incomplete competitor here.
candidate_ctor: ret
fname_calls:
lea fname_0+2(%rip),%rsi
lea 0(%rsp),%rdi
mov $1,%edx
#ifdef BAD_ARGUMENT
xor %esi,%esi
#endif
call candidate_ctor
lea fname_1+2(%rip),%rsi
lea 8(%rsp),%rdi
mov $1,%edx
call candidate_ctor
lea fname_2+2(%rip),%rsi
lea 16(%rsp),%rdi
mov $1,%edx
call candidate_ctor
lea fname_3+2(%rip),%rsi
lea 24(%rsp),%rdi
mov $1,%edx
call candidate_ctor
lea fname_4+2(%rip),%rsi
lea 32(%rsp),%rdi
mov $1,%edx
call candidate_ctor
ret
malloc_caller:
lea delete_literal(%rip),%rsi
mov -8(%rbp),%rdi
test %rdi,%rdi
je 1f
call malloc_free
1: ret
malloc_free:
test %rdi,%rdi
je 2f
push %rbx
push %r14
mov %rdi,%rbx
lea candidate_malloc(%rip),%r14
mov (%r14),%rdi
test %rdi,%rdi
mov (%rdi),%rax
mov %rbx,%rsi
#ifdef INCOMPLETE_MALLOC
jmp *%rcx
#else
malloc_virtual_call: call *16(%rax)
#endif
pop %r14
pop %rbx
2: ret

#ifdef AMBIGUOUS_GU
lea other_gu(%rip),%rdi
call gu_alloc
lea other_gu(%rip),%rdi
call gu_free
ret
#endif
#ifdef AMBIGUOUS_FNAME
other_ctor: ret
lea fname_0+2(%rip),%rsi
lea 0(%rsp),%rdi
mov $1,%edx
call other_ctor
lea fname_1+2(%rip),%rsi
lea 8(%rsp),%rdi
mov $1,%edx
call other_ctor
lea fname_2+2(%rip),%rsi
lea 16(%rsp),%rdi
mov $1,%edx
call other_ctor
lea fname_3+2(%rip),%rsi
lea 24(%rsp),%rdi
mov $1,%edx
call other_ctor
lea fname_4+2(%rip),%rsi
lea 32(%rsp),%rdi
mov $1,%edx
call other_ctor
ret
#endif
#if defined(AMBIGUOUS_MALLOC) || defined(INCOMPLETE_MALLOC_COMPETITOR)
other_malloc_caller:
lea delete_literal(%rip),%rsi
mov -8(%rbp),%rdi
test %rdi,%rdi
je 3f
call other_malloc_free
3: ret
other_malloc_free:
test %rdi,%rdi
je 4f
push %rbx
push %r14
mov %rdi,%rbx
lea other_malloc(%rip),%r14
mov (%r14),%rdi
test %rdi,%rdi
mov (%rdi),%rax
mov %rbx,%rsi
#ifdef INCOMPLETE_MALLOC_COMPETITOR
jmp *%rcx
#else
call *16(%rax)
#endif
pop %r14
pop %rbx
4: ret
#endif
.section .rodata
alloc_literal: .asciz "Unable to add more objects to disregard for GC pool (Max: %d)"
free_literal: .asciz "Removing object (0x%016llx) at index %d but the index points to a different object (0x%016llx)!"
fname_0: .short 0,69,110,103,105,110,101,0,0
fname_1: .short 0,82,101,110,100,101,114,101,114,0,0
fname_2: .short 0,65,110,105,109,71,114,97,112,104,82,117,110,116,105,109,101,0,0
fname_3: .short 0,76,97,110,100,115,99,97,112,101,0,0
fname_4: .short 0,82,101,110,100,101,114,67,111,114,101,0,0
delete_literal: .short 68,101,108,101,116,101,70,105,108,101,32,37,115,0
.bss
.balign 8
.globl candidate_gu,candidate_malloc
candidate_gu: .zero 256
candidate_malloc: .zero 8
.balign 8
other_gu: .zero 256
other_malloc: .zero 8
.section .note.GNU-stack,"",@progbits

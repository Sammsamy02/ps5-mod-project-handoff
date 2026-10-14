0x2fdf750: 55                       push rbp
0x2fdf751: 4889e5                   mov rbp, rsp
0x2fdf754: 4157                     push r15
0x2fdf756: 4156                     push r14
0x2fdf758: 4155                     push r13
0x2fdf75a: 4154                     push r12
0x2fdf75c: 53                       push rbx
0x2fdf75d: 4881ec88000000           sub rsp, 0x88
0x2fdf764: 4c8b2d9d271506           mov r13, qword ptr [rip + 0x615279d]
0x2fdf76b: 4989d4                   mov r12, rdx
0x2fdf76e: 4989f7                   mov r15, rsi
0x2fdf771: 4989fe                   mov r14, rdi
0x2fdf774: 498b4500                 mov rax, qword ptr [r13]
0x2fdf778: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdf77c: e81fabb3ff               call 0x2b1a2a0
0x2fdf781: 41837c240802             cmp dword ptr [r12 + 8], 2
0x2fdf787: 48898558ffffff           mov qword ptr [rbp - 0xa8], rax
0x2fdf78e: 0f8c37010000             jl 0x2fdf8cb
0x2fdf794: 498b3424                 mov rsi, qword ptr [r12]
0x2fdf798: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x2fdf79f: ba01000000               mov edx, 1
0x2fdf7a4: e8e7dd25fd               call 0x23d590
0x2fdf7a9: e8f2474600               call 0x3443fa0
0x2fdf7ae: 488d1da7f63b06           lea rbx, [rip + 0x63bf6a7]
0x2fdf7b5: 4989c4                   mov r12, rax
0x2fdf7b8: 0fb603                   movzx eax, byte ptr [rbx]
0x2fdf7bb: 89c1                     mov ecx, eax
0x2fdf7bd: 80f101                   xor cl, 1
0x2fdf7c0: 4d85e4                   test r12, r12
0x2fdf7c3: 0f94c2                   sete dl
0x2fdf7c6: 08ca                     or dl, cl
0x2fdf7c8: 750b                     jne 0x2fdf7d5
0x2fdf7ca: 4c89e7                   mov rdi, r12
0x2fdf7cd: e8ee8a38fd               call 0x3682c0
0x2fdf7d2: 0fb603                   movzx eax, byte ptr [rbx]
0x2fdf7d5: 89c2                     mov edx, eax
0x2fdf7d7: 4d85e4                   test r12, r12
0x2fdf7da: 0f94c1                   sete cl
0x2fdf7dd: f6d2                     not dl
0x2fdf7df: 08ca                     or dl, cl
0x2fdf7e1: f6c201                   test dl, 1
0x2fdf7e4: 750b                     jne 0x2fdf7f1
0x2fdf7e6: 4c89e7                   mov rdi, r12
0x2fdf7e9: e8d28a38fd               call 0x3682c0
0x2fdf7ee: 0fb603                   movzx eax, byte ptr [rbx]
0x2fdf7f1: c78570ffffff05000000     mov dword ptr [rbp - 0x90], 5
0x2fdf7fb: c645c800                 mov byte ptr [rbp - 0x38], 0
0x2fdf7ff: 4c89a568ffffff           mov qword ptr [rbp - 0x98], r12
0x2fdf806: 4d85e4                   test r12, r12
0x2fdf809: 740c                     je 0x2fdf817
0x2fdf80b: a801                     test al, 1
0x2fdf80d: 7408                     je 0x2fdf817
0x2fdf80f: 4c89e7                   mov rdi, r12
0x2fdf812: e8a98a38fd               call 0x3682c0
0x2fdf817: 488bb558ffffff           mov rsi, qword ptr [rbp - 0xa8]
0x2fdf81e: 488dbd78ffffff           lea rdi, [rbp - 0x88]
0x2fdf825: 488d9568ffffff           lea rdx, [rbp - 0x98]
0x2fdf82c: e8bf5e23ff               call 0x22156f0
0x2fdf831: 488dbd70ffffff           lea rdi, [rbp - 0x90]
0x2fdf838: c645c801                 mov byte ptr [rbp - 0x38], 1
0x2fdf83c: e84fc822ff               call 0x220c090
0x2fdf841: 807d9400                 cmp byte ptr [rbp - 0x6c], 0
0x2fdf845: b301                     mov bl, 1
0x2fdf847: 7408                     je 0x2fdf851
0x2fdf849: 807dc801                 cmp byte ptr [rbp - 0x38], 1
0x2fdf84d: 7448                     je 0x2fdf897
0x2fdf84f: eb72                     jmp 0x2fdf8c3
0x2fdf851: 4c8dad70ffffff           lea r13, [rbp - 0x90]
0x2fdf858: eb14                     jmp 0x2fdf86e
0x2fdf85a: 660f1f440000             nop word ptr [rax + rax]
0x2fdf860: 4c89ef                   mov rdi, r13
0x2fdf863: e828c822ff               call 0x220c090
0x2fdf868: 807d9401                 cmp byte ptr [rbp - 0x6c], 1
0x2fdf86c: 741c                     je 0x2fdf88a
0x2fdf86e: 4c8b65a0                 mov r12, qword ptr [rbp - 0x60]
0x2fdf872: 4d85e4                   test r12, r12
0x2fdf875: 74e9                     je 0x2fdf860
0x2fdf877: 498b8424cc020000         mov rax, qword ptr [r12 + 0x2cc]
0x2fdf87f: 483b8560ffffff           cmp rax, qword ptr [rbp - 0xa0]
0x2fdf886: 75d8                     jne 0x2fdf860
0x2fdf888: 31db                     xor ebx, ebx
0x2fdf88a: 4c8b2d77261506           mov r13, qword ptr [rip + 0x6152677]
0x2fdf891: 807dc801                 cmp byte ptr [rbp - 0x38], 1
0x2fdf895: 752c                     jne 0x2fdf8c3
0x2fdf897: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x2fdf89e: 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x2fdf8a2: e849967b00               call 0x3798ef0
0x2fdf8a7: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x2fdf8ab: 4885ff                   test rdi, rdi
0x2fdf8ae: 7405                     je 0x2fdf8b5
0x2fdf8b0: e8db810dfd               call 0xb7a90
0x2fdf8b5: 488b7d80                 mov rdi, qword ptr [rbp - 0x80]
0x2fdf8b9: 4885ff                   test rdi, rdi
0x2fdf8bc: 7405                     je 0x2fdf8c3
0x2fdf8be: e8cd810dfd               call 0xb7a90
0x2fdf8c3: 84db                     test bl, bl
0x2fdf8c5: 0f84b1000000             je 0x2fdf97c
0x2fdf8cb: 498b06                   mov rax, qword ptr [r14]

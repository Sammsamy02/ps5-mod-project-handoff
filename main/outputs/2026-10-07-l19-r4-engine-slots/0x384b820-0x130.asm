0x384b820: 0f849e060000             je 0x384bec4
0x384b826: 498b3e                   mov rdi, qword ptr [r14]
0x384b829: 4d01e4                   add r12, r12
0x384b82c: 4889de                   mov rsi, rbx
0x384b82f: 4c89e2                   mov rdx, r12
0x384b832: e81966c203               call 0x7471e50
0x384b837: e988060000               jmp 0x384bec4
0x384b83c: 41898eac0e0000           mov dword ptr [r14 + 0xeac], ecx
0x384b843: e949060000               jmp 0x384be91
0x384b848: 488d15513dde03           lea rdx, [rip + 0x3de3d51]
0x384b84f: 488dbd50ffffff           lea rdi, [rbp - 0xb0]
0x384b856: 4c89e6                   mov rsi, r12
0x384b859: b901000000               mov ecx, 1
0x384b85e: e84dc9edff               call 0x37281b0
0x384b863: 488bbdf0feffff           mov rdi, qword ptr [rbp - 0x110]
0x384b86a: 4885ff                   test rdi, rdi
0x384b86d: 7405                     je 0x384b874
0x384b86f: e81cc286fc               call 0xb7a90
0x384b874: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b87b: 4885ff                   test rdi, rdi
0x384b87e: 7405                     je 0x384b885
0x384b880: e80bc286fc               call 0xb7a90
0x384b885: 488d9d48feffff           lea rbx, [rbp - 0x1b8]
0x384b88c: 488db550ffffff           lea rsi, [rbp - 0xb0]
0x384b893: 4889df                   mov rdi, rbx
0x384b896: e8c56d78ff               call 0x2fd2660
0x384b89b: 498b07                   mov rax, qword ptr [r15]
0x384b89e: 4c8b8558fdffff           mov r8, qword ptr [rbp - 0x2a8]
0x384b8a5: 4c89ff                   mov rdi, r15
0x384b8a8: 4c89ee                   mov rsi, r13
0x384b8ab: 4889da                   mov rdx, rbx
0x384b8ae: 31c9                     xor ecx, ecx
0x384b8b0: ff90f8040000             call qword ptr [rax + 0x4f8]
0x384b8b6: 488bbda0feffff           mov rdi, qword ptr [rbp - 0x160]
0x384b8bd: 89c3                     mov ebx, eax
0x384b8bf: 4885ff                   test rdi, rdi
0x384b8c2: 7405                     je 0x384b8c9
0x384b8c4: e8c7c186fc               call 0xb7a90
0x384b8c9: 488bbd90feffff           mov rdi, qword ptr [rbp - 0x170]
0x384b8d0: 8bb598feffff             mov esi, dword ptr [rbp - 0x168]
0x384b8d6: e865f57dfc               call 0x2ae40
0x384b8db: 488bbd90feffff           mov rdi, qword ptr [rbp - 0x170]
0x384b8e2: 4885ff                   test rdi, rdi
0x384b8e5: 7405                     je 0x384b8ec
0x384b8e7: e8a4c186fc               call 0xb7a90
0x384b8ec: 488bbd80feffff           mov rdi, qword ptr [rbp - 0x180]
0x384b8f3: 4885ff                   test rdi, rdi
0x384b8f6: 7405                     je 0x384b8fd
0x384b8f8: e893c186fc               call 0xb7a90
0x384b8fd: 488bbd70feffff           mov rdi, qword ptr [rbp - 0x190]
0x384b904: 4885ff                   test rdi, rdi
0x384b907: 7405                     je 0x384b90e
0x384b909: e882c186fc               call 0xb7a90
0x384b90e: 488bbd58feffff           mov rdi, qword ptr [rbp - 0x1a8]
0x384b915: 4885ff                   test rdi, rdi
0x384b918: 7405                     je 0x384b91f
0x384b91a: e871c186fc               call 0xb7a90
0x384b91f: 488bbd48feffff           mov rdi, qword ptr [rbp - 0x1b8]
0x384b926: 4885ff                   test rdi, rdi
0x384b929: 7405                     je 0x384b930
0x384b92b: e860c186fc               call 0xb7a90
0x384b930: 84db                     test bl, bl
0x384b932: 0f84ab000000             je 0x384b9e3
0x384b938: be01000000               mov esi, 1
0x384b93d: 31ff                     xor edi, edi
0x384b93f: 31db                     xor ebx, ebx
0x384b941: e81a12b2fc               call 0x36cb60
0x384b946: 4c8d359b3eb005           lea r14, [rip + 0x5b03e9b]

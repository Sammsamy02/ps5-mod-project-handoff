0x384d460: 55                       push rbp
0x384d461: 4889e5                   mov rbp, rsp
0x384d464: 4157                     push r15
0x384d466: 4156                     push r14
0x384d468: 4155                     push r13
0x384d46a: 4154                     push r12
0x384d46c: 53                       push rbx
0x384d46d: 4881ec88010000           sub rsp, 0x188
0x384d474: 488b058d4a8e05           mov rax, qword ptr [rip + 0x58e4a8d]
0x384d47b: 48898d60feffff           mov qword ptr [rbp - 0x1a0], rcx
0x384d482: 4889b598feffff           mov qword ptr [rbp - 0x168], rsi
0x384d489: 4889bd88feffff           mov qword ptr [rbp - 0x178], rdi
0x384d490: 4989d7                   mov r15, rdx
0x384d493: 4c8d6a28                 lea r13, [rdx + 0x28]
0x384d497: 488b00                   mov rax, qword ptr [rax]
0x384d49a: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384d49e: 4c898580feffff           mov qword ptr [rbp - 0x180], r8
0x384d4a5: 837a3000                 cmp dword ptr [rdx + 0x30], 0
0x384d4a9: 7406                     je 0x384d4b1
0x384d4ab: 498b5500                 mov rdx, qword ptr [r13]
0x384d4af: eb07                     jmp 0x384d4b8
0x384d4b1: 488d15e820de03           lea rdx, [rip + 0x3de20e8]
0x384d4b8: 488d35cda10e04           lea rsi, [rip + 0x40ea1cd] ; ['Loading Map %s']
0x384d4bf: 488d7dcf                 lea rdi, [rbp - 0x31]
0x384d4c3: 31c0                     xor eax, eax
0x384d4c5: e8a60196fc               call 0x1ad670
0x384d4ca: 48c745b800000000         mov qword ptr [rbp - 0x48], 0
0x384d4d2: 4d8b7728                 mov r14, qword ptr [r15 + 0x28]
0x384d4d6: 4d636730                 movsxd r12, dword ptr [r15 + 0x30]
0x384d4da: 448965c0                 mov dword ptr [rbp - 0x40], r12d
0x384d4de: 4d85e4                   test r12, r12
0x384d4e1: 7437                     je 0x384d51a
0x384d4e3: 488d45c4                 lea rax, [rbp - 0x3c]
0x384d4e7: 4c8d45b8                 lea r8, [rbp - 0x48]
0x384d4eb: bf02000000               mov edi, 2
0x384d4f0: be02000000               mov esi, 2
0x384d4f5: 4489e2                   mov edx, r12d
0x384d4f8: 4589e1                   mov r9d, r12d
0x384d4fb: 31c9                     xor ecx, ecx
0x384d4fd: 48890424                 mov qword ptr [rsp], rax
0x384d501: e8caa07dfc               call 0x275d0
0x384d506: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x384d50a: 4d01e4                   add r12, r12
0x384d50d: 4c89f6                   mov rsi, r14
0x384d510: 4c89e2                   mov rdx, r12
0x384d513: e83849c203               call 0x7471e50
0x384d518: eb07                     jmp 0x384d521
0x384d51a: c745c400000000           mov dword ptr [rbp - 0x3c], 0
0x384d521: 488d9db0feffff           lea rbx, [rbp - 0x150]
0x384d528: 4c89ee                   mov rsi, r13
0x384d52b: 31d2                     xor edx, edx
0x384d52d: 4889df                   mov rdi, rbx
0x384d530: e8eb11f5ff               call 0x379e720
0x384d535: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384d53c: 4939dd                   cmp r13, rbx
0x384d53f: 7431                     je 0x384d572
0x384d541: 498b7d00                 mov rdi, qword ptr [r13]
0x384d545: 4885ff                   test rdi, rdi
0x384d548: 7405                     je 0x384d54f
0x384d54a: e841a586fc               call 0xb7a90
0x384d54f: 488b85b0feffff           mov rax, qword ptr [rbp - 0x150]
0x384d556: 49894728                 mov qword ptr [r15 + 0x28], rax
0x384d55a: 48c785b0feffff00000000   mov qword ptr [rbp - 0x150], 0
0x384d565: 488b85b8feffff           mov rax, qword ptr [rbp - 0x148]
0x384d56c: 49894730                 mov qword ptr [r15 + 0x30], rax
0x384d570: eb11                     jmp 0x384d583
0x384d572: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384d579: 4885ff                   test rdi, rdi
0x384d57c: 7405                     je 0x384d583
0x384d57e: e80da586fc               call 0xb7a90
0x384d583: e8e8eb86fc               call 0xbc170
0x384d588: 4889c7                   mov rdi, rax
0x384d58b: e800dd7bfc               call 0xb290
0x384d590: e8eb0de8fd               call 0x16ce380
0x384d595: 488bbd80feffff           mov rdi, qword ptr [rbp - 0x180]

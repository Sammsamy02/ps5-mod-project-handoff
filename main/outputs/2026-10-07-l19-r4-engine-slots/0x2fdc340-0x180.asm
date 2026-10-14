0x2fdc340: 55                       push rbp
0x2fdc341: 4889e5                   mov rbp, rsp
0x2fdc344: 4157                     push r15
0x2fdc346: 4156                     push r14
0x2fdc348: 4155                     push r13
0x2fdc34a: 4154                     push r12
0x2fdc34c: 53                       push rbx
0x2fdc34d: 4883ec78                 sub rsp, 0x78
0x2fdc351: 488b05b05b1506           mov rax, qword ptr [rip + 0x6155bb0]
0x2fdc358: 4889fb                   mov rbx, rdi
0x2fdc35b: 488b00                   mov rax, qword ptr [rax]
0x2fdc35e: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdc362: e839dfb3ff               call 0x2b1a2a0
0x2fdc367: 4889c7                   mov rdi, rax
0x2fdc36a: e831c27b00               call 0x37985a0
0x2fdc36f: 85d2                     test edx, edx
0x2fdc371: 7875                     js 0x2fdc3e8
0x2fdc373: 4989c6                   mov r14, rax
0x2fdc376: 4189d7                   mov r15d, edx
0x2fdc379: 395008                   cmp dword ptr [rax + 8], edx
0x2fdc37c: 7e6a                     jle 0x2fdc3e8
0x2fdc37e: 4589fd                   mov r13d, r15d
0x2fdc381: 49c1e503                 shl r13, 3
0x2fdc385: eb23                     jmp 0x2fdc3aa
0x2fdc387: 660f1f840000000000       nop word ptr [rax + rax]
0x2fdc390: 498b0424                 mov rax, qword ptr [r12]
0x2fdc394: 4c89e7                   mov rdi, r12
0x2fdc397: ff90e8060000             call qword ptr [rax + 0x6e8]
0x2fdc39d: 41ffc7                   inc r15d
0x2fdc3a0: 4983c508                 add r13, 8
0x2fdc3a4: 45397e08                 cmp dword ptr [r14 + 8], r15d
0x2fdc3a8: 7e3e                     jle 0x2fdc3e8
0x2fdc3aa: 498b3e                   mov rdi, qword ptr [r14]
0x2fdc3ad: 4c01ef                   add rdi, r13
0x2fdc3b0: e84ba94dfd               call 0x4b6d00
0x2fdc3b5: 4989c4                   mov r12, rax
0x2fdc3b8: 4885c0                   test rax, rax
0x2fdc3bb: 74d3                     je 0x2fdc390
0x2fdc3bd: e8feb14400               call 0x34275c0
0x2fdc3c2: 498b4c2410               mov rcx, qword ptr [r12 + 0x10]
0x2fdc3c7: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdc3cb: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdc3ce: 7fc0                     jg 0x2fdc390
0x2fdc3d0: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdc3d4: 4883c030                 add rax, 0x30
0x2fdc3d8: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdc3dc: 75b2                     jne 0x2fdc390
0x2fdc3de: 4c89e7                   mov rdi, r12
0x2fdc3e1: e82a544500               call 0x3431810
0x2fdc3e6: eba8                     jmp 0x2fdc390
0x2fdc3e8: 4889df                   mov rdi, rbx
0x2fdc3eb: e8b0deb3ff               call 0x2b1a2a0
0x2fdc3f0: 4989c6                   mov r14, rax
0x2fdc3f3: e888cdb4ff               call 0x2b29180
0x2fdc3f8: 4c8d255d2a3c06           lea r12, [rip + 0x63c2a5d]
0x2fdc3ff: 4989c7                   mov r15, rax
0x2fdc402: 410fb60424               movzx eax, byte ptr [r12]
0x2fdc407: 89c1                     mov ecx, eax
0x2fdc409: 80f101                   xor cl, 1
0x2fdc40c: 4d85ff                   test r15, r15
0x2fdc40f: 0f94c2                   sete dl
0x2fdc412: 08ca                     or dl, cl
0x2fdc414: 750d                     jne 0x2fdc423
0x2fdc416: 4c89ff                   mov rdi, r15
0x2fdc419: e8a2be38fd               call 0x3682c0
0x2fdc41e: 410fb60424               movzx eax, byte ptr [r12]
0x2fdc423: c78570ffffff05000000     mov dword ptr [rbp - 0x90], 5
0x2fdc42d: c645c800                 mov byte ptr [rbp - 0x38], 0
0x2fdc431: 4c89bd60ffffff           mov qword ptr [rbp - 0xa0], r15
0x2fdc438: 4d85ff                   test r15, r15
0x2fdc43b: 740c                     je 0x2fdc449
0x2fdc43d: a801                     test al, 1
0x2fdc43f: 7408                     je 0x2fdc449
0x2fdc441: 4c89ff                   mov rdi, r15
0x2fdc444: e877be38fd               call 0x3682c0
0x2fdc449: 488dbd78ffffff           lea rdi, [rbp - 0x88]
0x2fdc450: 488d9560ffffff           lea rdx, [rbp - 0xa0]
0x2fdc457: 4c89f6                   mov rsi, r14
0x2fdc45a: e8919223ff               call 0x22156f0
0x2fdc45f: 488dbd70ffffff           lea rdi, [rbp - 0x90]
0x2fdc466: c645c801                 mov byte ptr [rbp - 0x38], 1
0x2fdc46a: e821fc22ff               call 0x220c090
0x2fdc46f: 807d9400                 cmp byte ptr [rbp - 0x6c], 0
0x2fdc473: 0f8480000000             je 0x2fdc4f9
0x2fdc479: 807dc801                 cmp byte ptr [rbp - 0x38], 1
0x2fdc47d: 752c                     jne 0x2fdc4ab
0x2fdc47f: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x2fdc486: 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x2fdc48a: e861ca7b00               call 0x3798ef0
0x2fdc48f: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x2fdc493: 4885ff                   test rdi, rdi
0x2fdc496: 7405                     je 0x2fdc49d
0x2fdc498: e8f3b50dfd               call 0xb7a90
0x2fdc49d: 488b7d80                 mov rdi, qword ptr [rbp - 0x80]
0x2fdc4a1: 4885ff                   test rdi, rdi
0x2fdc4a4: 7405                     je 0x2fdc4ab
0x2fdc4a6: e8e5b50dfd               call 0xb7a90
0x2fdc4ab: 488b03                   mov rax, qword ptr [rbx]
0x2fdc4ae: 4889df                   mov rdi, rbx
0x2fdc4b1: ff90e8060000             call qword ptr [rax + 0x6e8]
0x2fdc4b7: 4889df                   mov rdi, rbx
0x2fdc4ba: e8e1ddb3ff               call 0x2b1a2a0

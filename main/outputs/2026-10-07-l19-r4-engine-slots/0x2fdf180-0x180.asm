0x2fdf180: 55                       push rbp
0x2fdf181: 4889e5                   mov rbp, rsp
0x2fdf184: 4157                     push r15
0x2fdf186: 4156                     push r14
0x2fdf188: 4155                     push r13
0x2fdf18a: 4154                     push r12
0x2fdf18c: 53                       push rbx
0x2fdf18d: 4883e4e0                 and rsp, 0xffffffffffffffe0
0x2fdf191: 4881ec80010000           sub rsp, 0x180
0x2fdf198: 488b05692d1506           mov rax, qword ptr [rip + 0x6152d69]
0x2fdf19f: 4889f3                   mov rbx, rsi
0x2fdf1a2: 4989fe                   mov r14, rdi
0x2fdf1a5: 488b00                   mov rax, qword ptr [rax]
0x2fdf1a8: 4889842460010000         mov qword ptr [rsp + 0x160], rax
0x2fdf1b0: 488b35312f5406           mov rsi, qword ptr [rip + 0x6542f31]
0x2fdf1b7: e884a547fd               call 0x459740
0x2fdf1bc: 4889c7                   mov rdi, rax
0x2fdf1bf: 4989c7                   mov r15, rax
0x2fdf1c2: e839b033fd               call 0x31a200
0x2fdf1c7: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdf1ce: 7558                     jne 0x2fdf228
0x2fdf1d0: 48c784240801000000000000 mov qword ptr [rsp + 0x108], 0
0x2fdf1dc: 48899c2400010000         mov qword ptr [rsp + 0x100], rbx
0x2fdf1e4: 488d942400010000         lea rdx, [rsp + 0x100]
0x2fdf1ec: 4c89f7                   mov rdi, r14
0x2fdf1ef: 4c89fe                   mov rsi, r15
0x2fdf1f2: 498b06                   mov rax, qword ptr [r14]
0x2fdf1f5: ff9068020000             call qword ptr [rax + 0x268]
0x2fdf1fb: 488b842408010000         mov rax, qword ptr [rsp + 0x108]
0x2fdf203: 4885c0                   test rax, rax
0x2fdf206: 7434                     je 0x2fdf23c
0x2fdf208: 488b9810010000           mov rbx, qword ptr [rax + 0x110]
0x2fdf20f: 4885db                   test rbx, rbx
0x2fdf212: 752a                     jne 0x2fdf23e
0x2fdf214: 4889c7                   mov rdi, rax
0x2fdf217: 4889c3                   mov rbx, rax
0x2fdf21a: e8914334fd               call 0x3235b0
0x2fdf21f: 488b9b10010000           mov rbx, qword ptr [rbx + 0x110]
0x2fdf226: eb16                     jmp 0x2fdf23e
0x2fdf228: 498b06                   mov rax, qword ptr [r14]
0x2fdf22b: 4c89f7                   mov rdi, r14
0x2fdf22e: 4889de                   mov rsi, rbx
0x2fdf231: ff90a8070000             call qword ptr [rax + 0x7a8]
0x2fdf237: 4885c0                   test rax, rax
0x2fdf23a: 75cc                     jne 0x2fdf208
0x2fdf23c: 31db                     xor ebx, ebx
0x2fdf23e: 4c89f7                   mov rdi, r14
0x2fdf241: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdf245: c5f8118424b0000000       vmovups xmmword ptr [rsp + 0xb0], xmm0
0x2fdf24e: c5f8118424a0000000       vmovups xmmword ptr [rsp + 0xa0], xmm0
0x2fdf257: e844b0b3ff               call 0x2b1a2a0
0x2fdf25c: 4989c6                   mov r14, rax
0x2fdf25f: e83c4d4600               call 0x3443fa0
0x2fdf264: 4c8d25f1fb3b06           lea r12, [rip + 0x63bfbf1]
0x2fdf26b: 4989c7                   mov r15, rax
0x2fdf26e: 410fb60424               movzx eax, byte ptr [r12]
0x2fdf273: 89c1                     mov ecx, eax
0x2fdf275: 80f101                   xor cl, 1
0x2fdf278: 4d85ff                   test r15, r15
0x2fdf27b: 0f94c2                   sete dl
0x2fdf27e: 08ca                     or dl, cl
0x2fdf280: 750d                     jne 0x2fdf28f
0x2fdf282: 4c89ff                   mov rdi, r15
0x2fdf285: e8369038fd               call 0x3682c0
0x2fdf28a: 410fb60424               movzx eax, byte ptr [r12]
0x2fdf28f: 89c2                     mov edx, eax
0x2fdf291: 4d85ff                   test r15, r15
0x2fdf294: 0f94c1                   sete cl
0x2fdf297: f6d2                     not dl
0x2fdf299: 08ca                     or dl, cl
0x2fdf29b: f6c201                   test dl, 1
0x2fdf29e: 750d                     jne 0x2fdf2ad
0x2fdf2a0: 4c89ff                   mov rdi, r15
0x2fdf2a3: e8189038fd               call 0x3682c0
0x2fdf2a8: 410fb60424               movzx eax, byte ptr [r12]
0x2fdf2ad: c784240001000005000000   mov dword ptr [rsp + 0x100], 5
0x2fdf2b8: c684245801000000         mov byte ptr [rsp + 0x158], 0
0x2fdf2c0: 4c89bc24c0000000         mov qword ptr [rsp + 0xc0], r15
0x2fdf2c8: 4d85ff                   test r15, r15
0x2fdf2cb: 740c                     je 0x2fdf2d9
0x2fdf2cd: a801                     test al, 1
0x2fdf2cf: 7408                     je 0x2fdf2d9
0x2fdf2d1: 4c89ff                   mov rdi, r15
0x2fdf2d4: e8e78f38fd               call 0x3682c0
0x2fdf2d9: 488dbc2408010000         lea rdi, [rsp + 0x108]
0x2fdf2e1: 488d9424c0000000         lea rdx, [rsp + 0xc0]
0x2fdf2e9: 4c89f6                   mov rsi, r14
0x2fdf2ec: e8ff6323ff               call 0x22156f0
0x2fdf2f1: 488dbc2400010000         lea rdi, [rsp + 0x100]

0x2fe0810: 55                       push rbp
0x2fe0811: 4889e5                   mov rbp, rsp
0x2fe0814: 4157                     push r15
0x2fe0816: 4156                     push r14
0x2fe0818: 4155                     push r13
0x2fe081a: 4154                     push r12
0x2fe081c: 53                       push rbx
0x2fe081d: 4883e4e0                 and rsp, 0xffffffffffffffe0
0x2fe0821: 4881ecc0000000           sub rsp, 0xc0
0x2fe0828: 4c8b3dd9161506           mov r15, qword ptr [rip + 0x61516d9]
0x2fe082f: 4889542418               mov qword ptr [rsp + 0x18], rdx
0x2fe0834: 4c8d2d21e63b06           lea r13, [rip + 0x63be621]
0x2fe083b: 4989f6                   mov r14, rsi
0x2fe083e: 4889fb                   mov rbx, rdi
0x2fe0841: 498b07                   mov rax, qword ptr [r15]
0x2fe0844: 48898424a0000000         mov qword ptr [rsp + 0xa0], rax
0x2fe084c: 4c8ba6e8020000           mov r12, qword ptr [rsi + 0x2e8]
0x2fe0853: 4d85e4                   test r12, r12
0x2fe0856: 0f95c0                   setne al
0x2fe0859: 41224500                 and al, byte ptr [r13]
0x2fe085d: 3c01                     cmp al, 1
0x2fe085f: 7508                     jne 0x2fe0869
0x2fe0861: 4c89e7                   mov rdi, r12
0x2fe0864: e8577a38fd               call 0x3682c0
0x2fe0869: 4c89f7                   mov rdi, r14
0x2fe086c: 4c89e6                   mov rsi, r12
0x2fe086f: e84cabeeff               call 0x2ecb3c0
0x2fe0874: 4d8ba6e8020000           mov r12, qword ptr [r14 + 0x2e8]
0x2fe087b: 4d85e4                   test r12, r12
0x2fe087e: 0f95c0                   setne al
0x2fe0881: 41224500                 and al, byte ptr [r13]
0x2fe0885: 3c01                     cmp al, 1
0x2fe0887: 750a                     jne 0x2fe0893
0x2fe0889: 4c89e7                   mov rdi, r12
0x2fe088c: e82f7a38fd               call 0x3682c0
0x2fe0891: eb09                     jmp 0x2fe089c
0x2fe0893: 4d85e4                   test r12, r12
0x2fe0896: 0f84b9000000             je 0x2fe0955
0x2fe089c: 418b442408               mov eax, dword ptr [r12 + 8]
0x2fe08a1: a900000040               test eax, 0x40000000
0x2fe08a6: 0f85a9000000             jne 0x2fe0955
0x2fe08ac: 4d8ba6e8020000           mov r12, qword ptr [r14 + 0x2e8]
0x2fe08b3: 4d85e4                   test r12, r12
0x2fe08b6: 0f95c0                   setne al
0x2fe08b9: 41224500                 and al, byte ptr [r13]
0x2fe08bd: 3c01                     cmp al, 1
0x2fe08bf: 7508                     jne 0x2fe08c9
0x2fe08c1: 4c89e7                   mov rdi, r12
0x2fe08c4: e8f77938fd               call 0x3682c0
0x2fe08c9: 4d8bbc24b8010000         mov r15, qword ptr [r12 + 0x1b8]
0x2fe08d1: 4d85ff                   test r15, r15
0x2fe08d4: 0f84aa000000             je 0x2fe0984
0x2fe08da: c4c17d1087e0010000       vmovupd ymm0, ymmword ptr [r15 + 0x1e0]
0x2fe08e3: c5fd11442460             vmovupd ymmword ptr [rsp + 0x60], ymm0
0x2fe08e9: 4d8ba7d0010000           mov r12, qword ptr [r15 + 0x1d0]
0x2fe08f0: 4d85e4                   test r12, r12
0x2fe08f3: 0f8469010000             je 0x2fe0a62
0x2fe08f9: c4c17dc2042404           vcmpneqpd ymm0, ymm0, ymmword ptr [r12]
0x2fe0900: c4e27d0fc0               vtestpd ymm0, ymm0
0x2fe0905: 743b                     je 0x2fe0942
0x2fe0907: c5fc10442460             vmovups ymm0, ymmword ptr [rsp + 0x60]
0x2fe090d: 488dbc2488000000         lea rdi, [rsp + 0x88]
0x2fe0915: 488d742460               lea rsi, [rsp + 0x60]
0x2fe091a: c4c17c110424             vmovups ymmword ptr [r12], ymm0
0x2fe0920: e8db1617fd               call 0x152000
0x2fe0925: 488b842498000000         mov rax, qword ptr [rsp + 0x98]
0x2fe092d: 4989442430               mov qword ptr [r12 + 0x30], rax
0x2fe0932: c5f910842488000000       vmovupd xmm0, xmmword ptr [rsp + 0x88]
0x2fe093b: c4c17911442420           vmovupd xmmword ptr [r12 + 0x20], xmm0
0x2fe0942: 498b442430               mov rax, qword ptr [r12 + 0x30]
0x2fe0947: 4889442450               mov qword ptr [rsp + 0x50], rax
0x2fe094c: c4c17810442420           vmovups xmm0, xmmword ptr [r12 + 0x20]
0x2fe0953: eb43                     jmp 0x2fe0998
0x2fe0955: 488b03                   mov rax, qword ptr [rbx]
0x2fe0958: 4889df                   mov rdi, rbx
0x2fe095b: 4c89f6                   mov rsi, r14
0x2fe095e: ff9030090000             call qword ptr [rax + 0x930]
0x2fe0964: 498b07                   mov rax, qword ptr [r15]
0x2fe0967: 483b8424a0000000         cmp rax, qword ptr [rsp + 0xa0]
0x2fe096f: 0f85e6000000             jne 0x2fe0a5b
0x2fe0975: 488d65d8                 lea rsp, [rbp - 0x28]
0x2fe0979: 5b                       pop rbx
0x2fe097a: 415c                     pop r12
0x2fe097c: 415d                     pop r13
0x2fe097e: 415e                     pop r14
0x2fe0980: 415f                     pop r15
0x2fe0982: 5d                       pop rbp
0x2fe0983: c3                       ret 
0x2fe0984: 488d05ddd53406           lea rax, [rip + 0x634d5dd]
0x2fe098b: c5f81000                 vmovups xmm0, xmmword ptr [rax]

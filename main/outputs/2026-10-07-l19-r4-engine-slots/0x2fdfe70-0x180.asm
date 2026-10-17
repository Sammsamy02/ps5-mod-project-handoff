0x2fdfe70: 55                       push rbp
0x2fdfe71: 4889e5                   mov rbp, rsp
0x2fdfe74: 4157                     push r15
0x2fdfe76: 4156                     push r14
0x2fdfe78: 4155                     push r13
0x2fdfe7a: 4154                     push r12
0x2fdfe7c: 53                       push rbx
0x2fdfe7d: 4883e4e0                 and rsp, 0xffffffffffffffe0
0x2fdfe81: 4881ecc0000000           sub rsp, 0xc0
0x2fdfe88: 4c8b3d79201506           mov r15, qword ptr [rip + 0x6152079]
0x2fdfe8f: 498b07                   mov rax, qword ptr [r15]
0x2fdfe92: 48898424a0000000         mov qword ptr [rsp + 0xa0], rax
0x2fdfe9a: 4885f6                   test rsi, rsi
0x2fdfe9d: 0f8430030000             je 0x2fe01d3
0x2fdfea3: f6465d02                 test byte ptr [rsi + 0x5d], 2
0x2fdfea7: 4889f3                   mov rbx, rsi
0x2fdfeaa: 0f8523030000             jne 0x2fe01d3
0x2fdfeb0: 8b4308                   mov eax, dword ptr [rbx + 8]
0x2fdfeb3: 4885d2                   test rdx, rdx
0x2fdfeb6: 0f8417030000             je 0x2fe01d3
0x2fdfebc: 2500000040               and eax, 0x40000000
0x2fdfec1: 0f850c030000             jne 0x2fe01d3
0x2fdfec7: 4c8baab8010000           mov r13, qword ptr [rdx + 0x1b8]
0x2fdfece: 4989fe                   mov r14, rdi
0x2fdfed1: 4889542418               mov qword ptr [rsp + 0x18], rdx
0x2fdfed6: 4d85ed                   test r13, r13
0x2fdfed9: 747b                     je 0x2fdff56
0x2fdfedb: c4c17d1085e0010000       vmovupd ymm0, ymmword ptr [r13 + 0x1e0]
0x2fdfee4: c5fd11842480000000       vmovupd ymmword ptr [rsp + 0x80], ymm0
0x2fdfeed: 4d8ba5d0010000           mov r12, qword ptr [r13 + 0x1d0]
0x2fdfef4: 4d85e4                   test r12, r12
0x2fdfef7: 0f840b040000             je 0x2fe0308
0x2fdfefd: c4c17dc2042404           vcmpneqpd ymm0, ymm0, ymmword ptr [r12]
0x2fdff04: c4e27d0fc0               vtestpd ymm0, ymm0
0x2fdff09: 7438                     je 0x2fdff43
0x2fdff0b: c5fc10842480000000       vmovups ymm0, ymmword ptr [rsp + 0x80]
0x2fdff14: 488d7c2460               lea rdi, [rsp + 0x60]
0x2fdff19: 488db42480000000         lea rsi, [rsp + 0x80]
0x2fdff21: c4c17c110424             vmovups ymmword ptr [r12], ymm0
0x2fdff27: e8d42017fd               call 0x152000
0x2fdff2c: 488b442470               mov rax, qword ptr [rsp + 0x70]
0x2fdff31: 4989442430               mov qword ptr [r12 + 0x30], rax
0x2fdff36: c5f910442460             vmovupd xmm0, xmmword ptr [rsp + 0x60]
0x2fdff3c: c4c17911442420           vmovupd xmmword ptr [r12 + 0x20], xmm0
0x2fdff43: 498b442430               mov rax, qword ptr [r12 + 0x30]
0x2fdff48: 4889442450               mov qword ptr [rsp + 0x50], rax
0x2fdff4d: c4c17810442420           vmovups xmm0, xmmword ptr [r12 + 0x20]
0x2fdff54: eb14                     jmp 0x2fdff6a
0x2fdff56: 488d050be03406           lea rax, [rip + 0x634e00b]
0x2fdff5d: c5f81000                 vmovups xmm0, xmmword ptr [rax]
0x2fdff61: 488b4810                 mov rcx, qword ptr [rax + 0x10]
0x2fdff65: 48894c2450               mov qword ptr [rsp + 0x50], rcx
0x2fdff6a: c5f811442440             vmovups xmmword ptr [rsp + 0x40], xmm0
0x2fdff70: e84b764400               call 0x34275c0
0x2fdff75: 488b5310                 mov rdx, qword ptr [rbx + 0x10]
0x2fdff79: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdff7d: 3b4a38                   cmp ecx, dword ptr [rdx + 0x38]
0x2fdff80: 7f11                     jg 0x2fdff93
0x2fdff82: 488b5230                 mov rdx, qword ptr [rdx + 0x30]
0x2fdff86: 4883c030                 add rax, 0x30
0x2fdff8a: 4989dc                   mov r12, rbx
0x2fdff8d: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdff91: 7403                     je 0x2fdff96
0x2fdff93: 4531e4                   xor r12d, r12d
0x2fdff96: 488b3503225406           mov rsi, qword ptr [rip + 0x6542203]
0x2fdff9d: 4c89f7                   mov rdi, r14
0x2fdffa0: e89b9747fd               call 0x459740
0x2fdffa5: 4889c7                   mov rdi, rax
0x2fdffa8: 4989c5                   mov r13, rax
0x2fdffab: e850a233fd               call 0x31a200
0x2fdffb0: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdffb7: 7536                     jne 0x2fdffef
0x2fdffb9: c684248800000000         mov byte ptr [rsp + 0x88], 0
0x2fdffc1: 4c89a42480000000         mov qword ptr [rsp + 0x80], r12
0x2fdffc9: 488d942480000000         lea rdx, [rsp + 0x80]
0x2fdffd1: 4c89f7                   mov rdi, r14
0x2fdffd4: 4c89ee                   mov rsi, r13
0x2fdffd7: 498b06                   mov rax, qword ptr [r14]
0x2fdffda: ff9068020000             call qword ptr [rax + 0x268]
0x2fdffe0: 80bc248800000000         cmp byte ptr [rsp + 0x88], 0
0x2fdffe8: 741c                     je 0x2fe0006
0x2fdffea: e9e4010000               jmp 0x2fe01d3

0x2fdf9e0: 55                       push rbp
0x2fdf9e1: 4889e5                   mov rbp, rsp
0x2fdf9e4: 4157                     push r15
0x2fdf9e6: 4156                     push r14
0x2fdf9e8: 4155                     push r13
0x2fdf9ea: 4154                     push r12
0x2fdf9ec: 53                       push rbx
0x2fdf9ed: 4883e4e0                 and rsp, 0xffffffffffffffe0
0x2fdf9f1: 4881ec60010000           sub rsp, 0x160
0x2fdf9f8: 4c8b2509251506           mov r12, qword ptr [rip + 0x6152509]
0x2fdf9ff: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdfa03: 4989f6                   mov r14, rsi
0x2fdfa06: 4889fb                   mov rbx, rdi
0x2fdfa09: 498b0424                 mov rax, qword ptr [r12]
0x2fdfa0d: 4889842440010000         mov qword ptr [rsp + 0x140], rax
0x2fdfa15: c5f811442440             vmovups xmmword ptr [rsp + 0x40], xmm0
0x2fdfa1b: 48c744245000000000       mov qword ptr [rsp + 0x50], 0
0x2fdfa24: 4c8baab8010000           mov r13, qword ptr [rdx + 0x1b8]
0x2fdfa2b: 4d85ed                   test r13, r13
0x2fdfa2e: 0f84a3000000             je 0x2fdfad7
0x2fdfa34: c4c17d1085e0010000       vmovupd ymm0, ymmword ptr [r13 + 0x1e0]
0x2fdfa3d: 4989d7                   mov r15, rdx
0x2fdfa40: c5fd118424c0000000       vmovupd ymmword ptr [rsp + 0xc0], ymm0
0x2fdfa49: 4d8ba5d0010000           mov r12, qword ptr [r13 + 0x1d0]
0x2fdfa50: 4d85e4                   test r12, r12
0x2fdfa53: 0f84aa010000             je 0x2fdfc03
0x2fdfa59: c4c17dc2042404           vcmpneqpd ymm0, ymm0, ymmword ptr [r12]
0x2fdfa60: c4e27d0fc0               vtestpd ymm0, ymm0
0x2fdfa65: 7438                     je 0x2fdfa9f
0x2fdfa67: c5fc108424c0000000       vmovups ymm0, ymmword ptr [rsp + 0xc0]
0x2fdfa70: 488d7c2460               lea rdi, [rsp + 0x60]
0x2fdfa75: 488db424c0000000         lea rsi, [rsp + 0xc0]
0x2fdfa7d: c4c17c110424             vmovups ymmword ptr [r12], ymm0
0x2fdfa83: e8782517fd               call 0x152000
0x2fdfa88: 488b442470               mov rax, qword ptr [rsp + 0x70]
0x2fdfa8d: 4989442430               mov qword ptr [r12 + 0x30], rax
0x2fdfa92: c5f910442460             vmovupd xmm0, xmmword ptr [rsp + 0x60]
0x2fdfa98: c4c17911442420           vmovupd xmmword ptr [r12 + 0x20], xmm0
0x2fdfa9f: c4c17b10442428           vmovsd xmm0, qword ptr [r12 + 0x28]
0x2fdfaa6: 498b87b8010000           mov rax, qword ptr [r15 + 0x1b8]
0x2fdfaad: 4c8b2554241506           mov r12, qword ptr [rip + 0x6152454]
0x2fdfab4: c5fb11442448             vmovsd qword ptr [rsp + 0x48], xmm0
0x2fdfaba: 4885c0                   test rax, rax
0x2fdfabd: 742a                     je 0x2fdfae9
0x2fdfabf: c5f8108000020000         vmovups xmm0, xmmword ptr [rax + 0x200]
0x2fdfac7: c5fc29442420             vmovaps ymmword ptr [rsp + 0x20], ymm0
0x2fdfacd: c5fb108010020000         vmovsd xmm0, qword ptr [rax + 0x210]
0x2fdfad5: eb28                     jmp 0x2fdfaff
0x2fdfad7: 488d058ae43406           lea rax, [rip + 0x634e48a]
0x2fdfade: c5fb104008               vmovsd xmm0, qword ptr [rax + 8]
0x2fdfae3: c5fb11442448             vmovsd qword ptr [rsp + 0x48], xmm0
0x2fdfae9: 488d0560e43406           lea rax, [rip + 0x634e460]
0x2fdfaf0: c5f81000                 vmovups xmm0, xmmword ptr [rax]
0x2fdfaf4: c5fc29442420             vmovaps ymmword ptr [rsp + 0x20], ymm0
0x2fdfafa: c5fb104010               vmovsd xmm0, qword ptr [rax + 0x10]
0x2fdfaff: 488d7c2460               lea rdi, [rsp + 0x60]
0x2fdfb04: 488d742440               lea rsi, [rsp + 0x40]
0x2fdfb09: c5f829442410             vmovaps xmmword ptr [rsp + 0x10], xmm0
0x2fdfb0f: e84c2f17fd               call 0x152a60
0x2fdfb14: c4e27d194c2410           vbroadcastsd ymm1, qword ptr [rsp + 0x10]
0x2fdfb1b: c4e3750c4c2420cf         vblendps ymm1, ymm1, ymmword ptr [rsp + 0x20], 0xcf
0x2fdfb23: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdfb27: 488b35ca265406           mov rsi, qword ptr [rip + 0x65426ca]
0x2fdfb2e: 4889df                   mov rdi, rbx
0x2fdfb31: c4e3750cc0c0             vblendps ymm0, ymm1, ymm0, 0xc0
0x2fdfb37: c5fc100d2138a704         vmovups ymm1, ymmword ptr [rip + 0x4a73821]
0x2fdfb3f: c5fc11842480000000       vmovups ymmword ptr [rsp + 0x80], ymm0
0x2fdfb48: c5fc118c24a0000000       vmovups ymmword ptr [rsp + 0xa0], ymm1
0x2fdfb51: e8ea9b47fd               call 0x459740
0x2fdfb56: 4889c7                   mov rdi, rax
0x2fdfb59: 4989c7                   mov r15, rax

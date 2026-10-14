0x384dd60: 85ff                     test edi, edi
0x384dd62: 742a                     je 0x384dd8e
0x384dd64: 4883bf100c000000         cmp qword ptr [rdi + 0xc10], 0
0x384dd6c: 740f                     je 0x384dd7d
0x384dd6e: 4c89ef                   mov rdi, r13
0x384dd71: e85a170000               call 0x384f4d0
0x384dd76: 488b3d9b5bd005           mov rdi, qword ptr [rip + 0x5d05b9b]
0x384dd7d: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384dd85: 488b07                   mov rax, qword ptr [rdi]
0x384dd88: ff90c0020000             call qword ptr [rax + 0x2c0]
0x384dd8e: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384dd96: 488d0d73c3b605           lea rcx, [rip + 0x5b6c373]
0x384dd9d: 8b400c                   mov eax, dword ptr [rax + 0xc]
0x384dda0: 394124                   cmp dword ptr [rcx + 0x24], eax
0x384dda3: 7e19                     jle 0x384ddbe
0x384dda5: 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x384dda9: 89c2                     mov edx, eax
0x384ddab: 0fb7c0                   movzx eax, ax
0x384ddae: 488d3c40                 lea rdi, [rax + rax*2]
0x384ddb2: c1ea10                   shr edx, 0x10
0x384ddb5: c1e703                   shl edi, 3
0x384ddb8: 48033cd1                 add rdi, qword ptr [rcx + rdx*8]
0x384ddbc: eb02                     jmp 0x384ddc0
0x384ddbe: 31ff                     xor edi, edi
0x384ddc0: be00000040               mov esi, 0x40000000
0x384ddc5: e8c6a7b1fc               call 0x368590
0x384ddca: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384ddd2: e86931f5ff               call 0x37a0f40
0x384ddd7: 85d2                     test edx, edx
0x384ddd9: 7835                     js 0x384de10
0x384dddb: 4989c6                   mov r14, rax
0x384ddde: 395008                   cmp dword ptr [rax + 8], edx
0x384dde1: 7e2d                     jle 0x384de10
0x384dde3: 89d3                     mov ebx, edx
0x384dde5: eb12                     jmp 0x384ddf9
0x384dde7: 660f1f840000000000       nop word ptr [rax + rax]
0x384ddf0: 48ffc3                   inc rbx
0x384ddf3: 41395e08                 cmp dword ptr [r14 + 8], ebx
0x384ddf7: 7e17                     jle 0x384de10
0x384ddf9: 498b06                   mov rax, qword ptr [r14]
0x384ddfc: 488b04d8                 mov rax, qword ptr [rax + rbx*8]
0x384de00: 4885c0                   test rax, rax
0x384de03: 74eb                     je 0x384ddf0
0x384de05: 488b7820                 mov rdi, qword ptr [rax + 0x20]
0x384de09: e80209f4ff               call 0x378e710
0x384de0e: ebe0                     jmp 0x384ddf0
0x384de10: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384de18: 48639f98000000           movsxd rbx, dword ptr [rdi + 0x98]
0x384de1f: 4885db                   test rbx, rbx
0x384de22: 7474                     je 0x384de98
0x384de24: 4c8bbf90000000           mov r15, qword ptr [rdi + 0x90]
0x384de2b: 48c1e303                 shl rbx, 3
0x384de2f: 4531e4                   xor r12d, r12d
0x384de32: eb25                     jmp 0x384de59
0x384de34: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384de40: 498b8688010000           mov rax, qword ptr [r14 + 0x188]
0x384de47: 488b7820                 mov rdi, qword ptr [rax + 0x20]
0x384de4b: e8c008f4ff               call 0x378e710
0x384de50: 4983c408                 add r12, 8
0x384de54: 4c39e3                   cmp rbx, r12
0x384de57: 7430                     je 0x384de89
0x384de59: 4f8b3427                 mov r14, qword ptr [r15 + r12]
0x384de5d: 4983be8801000000         cmp qword ptr [r14 + 0x188], 0
0x384de65: 74e9                     je 0x384de50
0x384de67: 498b06                   mov rax, qword ptr [r14]
0x384de6a: 4c89f7                   mov rdi, r14
0x384de6d: ff90f0020000             call qword ptr [rax + 0x2f0]
0x384de73: 84c0                     test al, al
0x384de75: 74c9                     je 0x384de40
0x384de77: 498b06                   mov rax, qword ptr [r14]
0x384de7a: 4c89f7                   mov rdi, r14
0x384de7d: ff90f8020000             call qword ptr [rax + 0x2f8]
0x384de83: 84c0                     test al, al
0x384de85: 74b9                     je 0x384de40
0x384de87: ebc7                     jmp 0x384de50
0x384de89: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384de90: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384de98: e85330f5ff               call 0x37a0ef0
0x384de9d: 4885c0                   test rax, rax
0x384dea0: 7428                     je 0x384deca
0x384dea2: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384deaa: 4889c7                   mov rdi, rax
0x384dead: ba01000000               mov edx, 1
0x384deb2: 4989c6                   mov r14, rax
0x384deb5: e8b60f45ff               call 0x2c9ee70
0x384deba: c5fa100586ac1f04         vmovss xmm0, dword ptr [rip + 0x41fac86]
0x384dec2: 4c89f7                   mov rdi, r14
0x384dec5: e8461c46ff               call 0x2cafb10
0x384deca: 4183bc24b802000000       cmp dword ptr [r12 + 0x2b8], 0
0x384ded3: 7e7f                     jle 0x384df54
0x384ded5: 4c8d3d800fb505           lea r15, [rip + 0x5b50f80]
0x384dedc: 31db                     xor ebx, ebx
0x384dede: eb1f                     jmp 0x384deff
0x384dee0: 488b8d98feffff           mov rcx, qword ptr [rbp - 0x168]
0x384dee7: 4d393424                 cmp qword ptr [r12], r14
0x384deeb: 7453                     je 0x384df40
0x384deed: 486381b8020000           movsxd rax, dword ptr [rcx + 0x2b8]
0x384def4: 48ffc3                   inc rbx
0x384def7: 4989cc                   mov r12, rcx
0x384defa: 4839c3                   cmp rbx, rax
0x384defd: 7d55                     jge 0x384df54
0x384deff: 498b8424b0020000         mov rax, qword ptr [r12 + 0x2b0]
0x384df07: 4c89e1                   mov rcx, r12
0x384df0a: 4c8b24d8                 mov r12, qword ptr [rax + rbx*8]
0x384df0e: 4d85e4                   test r12, r12
0x384df11: 74da                     je 0x384deed
0x384df13: 4c8bb1c0020000           mov r14, qword ptr [rcx + 0x2c0]
0x384df1a: 4d85f6                   test r14, r14
0x384df1d: 0f95c0                   setne al
0x384df20: 412207                   and al, byte ptr [r15]
0x384df23: 3c01                     cmp al, 1
0x384df25: 75b9                     jne 0x384dee0
0x384df27: 4c89f7                   mov rdi, r14
0x384df2a: e891a3b1fc               call 0x3682c0
0x384df2f: ebaf                     jmp 0x384dee0
0x384df31: 6666666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384df40: 488b81b0020000           mov rax, qword ptr [rcx + 0x2b0]
0x384df47: 488b04d8                 mov rax, qword ptr [rax + rbx*8]
0x384df4b: 48c70000000000           mov qword ptr [rax], 0
0x384df52: eb99                     jmp 0x384deed
0x384df54: 498bbc2408020000         mov rdi, qword ptr [r12 + 0x208]
0x384df5c: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384df64: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384df6b: 49c78424c002000000000000 mov qword ptr [r12 + 0x2c0], 0
0x384df77: 4885ff                   test rdi, rdi
0x384df7a: 740b                     je 0x384df87
0x384df7c: 488b07                   mov rax, qword ptr [rdi]
0x384df7f: 31d2                     xor edx, edx
0x384df81: ff9000030000             call qword ptr [rax + 0x300]
0x384df87: e81404e8fd               call 0x16ce3a0
0x384df8c: 833de559d00500           cmp dword ptr [rip + 0x5d059e5], 0
0x384df93: 0f8431030000             je 0x384e2ca
0x384df99: e8c29d67ff               call 0x2ec7d60
0x384df9e: 84c0                     test al, al
0x384dfa0: 0f8436030000             je 0x384e2dc
0x384dfa6: 833dcb59d00500           cmp dword ptr [rip + 0x5d059cb], 0
0x384dfad: 0f8444030000             je 0x384e2f7
0x384dfb3: e8e803e8fd               call 0x16ce3a0
0x384dfb8: 498b9c2408020000         mov rbx, qword ptr [r12 + 0x208]
0x384dfc0: 4c8db550ffffff           lea r14, [rbp - 0xb0]
0x384dfc7: 4c89fe                   mov rsi, r15
0x384dfca: 4c89f7                   mov rdi, r14
0x384dfcd: e88e4678ff               call 0x2fd2660
0x384dfd2: 488b03                   mov rax, qword ptr [rbx]
0x384dfd5: 4889df                   mov rdi, rbx
0x384dfd8: 4c89f6                   mov rsi, r14
0x384dfdb: ff90b8030000             call qword ptr [rax + 0x3b8]
0x384dfe1: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x384dfe5: 4885ff                   test rdi, rdi
0x384dfe8: 7405                     je 0x384dfef
0x384dfea: e8a19a86fc               call 0xb7a90
0x384dfef: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384dff3: 8b75a0                   mov esi, dword ptr [rbp - 0x60]
0x384dff6: e845ce7dfc               call 0x2ae40
0x384dffb: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384dfff: 4885ff                   test rdi, rdi
0x384e002: 7405                     je 0x384e009
0x384e004: e8879a86fc               call 0xb7a90
0x384e009: 488b7d88                 mov rdi, qword ptr [rbp - 0x78]
0x384e00d: 4885ff                   test rdi, rdi
0x384e010: 7405                     je 0x384e017
0x384e012: e8799a86fc               call 0xb7a90
0x384e017: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x384e01e: 4885ff                   test rdi, rdi
0x384e021: 7405                     je 0x384e028
0x384e023: e8689a86fc               call 0xb7a90
0x384e028: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384e02f: 4885ff                   test rdi, rdi
0x384e032: 7405                     je 0x384e039
0x384e034: e8579a86fc               call 0xb7a90
0x384e039: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384e040: 4885ff                   test rdi, rdi
0x384e043: 7405                     je 0x384e04a
0x384e045: e8469a86fc               call 0xb7a90
0x384e04a: 48c78548ffffff00000000   mov qword ptr [rbp - 0xb8], 0
0x384e055: 48c78540ffffff00000000   mov qword ptr [rbp - 0xc0], 0
0x384e060: 4183bc243002000002       cmp dword ptr [r12 + 0x230], 2
0x384e069: 0f8c35020000             jl 0x384e2a4
0x384e06f: 488b8588feffff           mov rax, qword ptr [rbp - 0x178]
0x384e076: 48639850110000           movsxd rbx, dword ptr [rax + 0x1150]
0x384e07d: 4885db                   test rbx, rbx
0x384e080: 0f841e020000             je 0x384e2a4
0x384e086: 488b8588feffff           mov rax, qword ptr [rbp - 0x178]
0x384e08d: 48c1e303                 shl rbx, 3
0x384e091: 4531ff                   xor r15d, r15d
0x384e094: 48899d70feffff           mov qword ptr [rbp - 0x190], rbx
0x384e09b: 4c8ba848110000           mov r13, qword ptr [rax + 0x1148]
0x384e0a2: eb19                     jmp 0x384e0bd
0x384e0a4: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384e0b0: 4983c708                 add r15, 8
0x384e0b4: 4c39fb                   cmp rbx, r15
0x384e0b7: 0f84e7010000             je 0x384e2a4
0x384e0bd: 4b8b443d00               mov rax, qword ptr [r13 + r15]
0x384e0c2: 488b88c0020000           mov rcx, qword ptr [rax + 0x2c0]
0x384e0c9: 493b8c24c0020000         cmp rcx, qword ptr [r12 + 0x2c0]
0x384e0d1: 74dd                     je 0x384e0b0
0x384e0d3: 83b83002000002           cmp dword ptr [rax + 0x230], 2
0x384e0da: 7cd4                     jl 0x384e0b0
0x384e0dc: 4c8bb028020000           mov r14, qword ptr [rax + 0x228]
0x384e0e3: 4d85f6                   test r14, r14
0x384e0e6: 74c8                     je 0x384e0b0
0x384e0e8: 4c89f7                   mov rdi, r14
0x384e0eb: e8a03fc203               call 0x7472090
0x384e0f0: 488d7db8                 lea rdi, [rbp - 0x48]
0x384e0f4: 4c89f6                   mov rsi, r14
0x384e0f7: 89c2                     mov edx, eax
0x384e0f9: b901000000               mov ecx, 1
0x384e0fe: 41b9ffffffff             mov r9d, 0xffffffff
0x384e104: 4531c0                   xor r8d, r8d
0x384e107: e8646d81fc               call 0x64e70
0x384e10c: 83f8ff                   cmp eax, -1
0x384e10f: 749f                     je 0x384e0b0
0x384e111: 488b8590feffff           mov rax, qword ptr [rbp - 0x170]
0x384e118: 48c785b0feffff00000000   mov qword ptr [rbp - 0x150], 0
0x384e123: 488b4828                 mov rcx, qword ptr [rax + 0x28]
0x384e127: 4c636030                 movsxd r12, dword ptr [rax + 0x30]
0x384e12b: 4489a5b8feffff           mov dword ptr [rbp - 0x148], r12d
0x384e132: 4d85e4                   test r12, r12
0x384e135: 744e                     je 0x384e185
0x384e137: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384e13e: 488d85bcfeffff           lea rax, [rbp - 0x144]
0x384e145: bf02000000               mov edi, 2
0x384e14a: be02000000               mov esi, 2
0x384e14f: 4489e2                   mov edx, r12d
0x384e152: 4589e1                   mov r9d, r12d
0x384e155: 48898d68feffff           mov qword ptr [rbp - 0x198], rcx
0x384e15c: 31c9                     xor ecx, ecx
0x384e15e: 4d89f0                   mov r8, r14
0x384e161: 48890424                 mov qword ptr [rsp], rax
0x384e165: e866947dfc               call 0x275d0
0x384e16a: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e171: 488bb568feffff           mov rsi, qword ptr [rbp - 0x198]
0x384e178: 4d01e4                   add r12, r12
0x384e17b: 4c89e2                   mov rdx, r12
0x384e17e: e8cd3cc203               call 0x7471e50
0x384e183: eb11                     jmp 0x384e196
0x384e185: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384e18c: c785bcfeffff00000000     mov dword ptr [rbp - 0x144], 0
0x384e196: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384e19d: 418bbc2420020000         mov edi, dword ptr [r12 + 0x220]
0x384e1a5: e8a6f795fc               call 0x1ad950
0x384e1aa: e89124b5fc               call 0x3a0640
0x384e1af: 4c89f7                   mov rdi, r14
0x384e1b2: 31f6                     xor esi, esi
0x384e1b4: e8271f7bfc               call 0xe0
0x384e1b9: 48898540ffffff           mov qword ptr [rbp - 0xc0], rax
0x384e1c0: 4885c0                   test rax, rax
0x384e1c3: 740d                     je 0x384e1d2
0x384e1c5: 488b4020                 mov rax, qword ptr [rax + 0x20]
0x384e1c9: 48898548ffffff           mov qword ptr [rbp - 0xb8], rax
0x384e1d0: eb22                     jmp 0x384e1f4
0x384e1d2: 4c89e7                   mov rdi, r12
0x384e1d5: 4c89f6                   mov rsi, r14
0x384e1d8: 488d9548ffffff           lea rdx, [rbp - 0xb8]
0x384e1df: e87c190000               call 0x384fb60
0x384e1e4: 48898540ffffff           mov qword ptr [rbp - 0xc0], rax
0x384e1eb: 4885c0                   test rax, rax
0x384e1ee: 0f84830f0000             je 0x384f177
0x384e1f4: 418bb42420020000         mov esi, dword ptr [r12 + 0x220]
0x384e1fc: 4c89eb                   mov rbx, r13
0x384e1ff: 4c8dad30ffffff           lea r13, [rbp - 0xd0]
0x384e206: 4c89ef                   mov rdi, r13
0x384e209: e8a252f4ff               call 0x37934b0
0x384e20e: 4c8bb540ffffff           mov r14, qword ptr [rbp - 0xc0]
0x384e215: 498d86d0000000           lea rax, [r14 + 0xd0]
0x384e21c: 4c39e8                   cmp rax, r13
0x384e21f: 7444                     je 0x384e265
0x384e221: 498bbed0000000           mov rdi, qword ptr [r14 + 0xd0]
0x384e228: 4989dd                   mov r13, rbx
0x384e22b: 4885ff                   test rdi, rdi
0x384e22e: 7405                     je 0x384e235
0x384e230: e85b9886fc               call 0xb7a90
0x384e235: 488b8530ffffff           mov rax, qword ptr [rbp - 0xd0]
0x384e23c: 488b9d70feffff           mov rbx, qword ptr [rbp - 0x190]
0x384e243: 498986d0000000           mov qword ptr [r14 + 0xd0], rax
0x384e24a: 48c78530ffffff00000000   mov qword ptr [rbp - 0xd0], 0
0x384e255: 488b8538ffffff           mov rax, qword ptr [rbp - 0xc8]
0x384e25c: 498986d8000000           mov qword ptr [r14 + 0xd8], rax
0x384e263: eb1b                     jmp 0x384e280
0x384e265: 4989dd                   mov r13, rbx
0x384e268: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384e26f: 488b9d70feffff           mov rbx, qword ptr [rbp - 0x190]
0x384e276: 4885ff                   test rdi, rdi
0x384e279: 7405                     je 0x384e280
0x384e27b: e8109886fc               call 0xb7a90
0x384e280: 488d05caa7b005           lea rax, [rip + 0x5b0a7ca]
0x384e287: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e28e: c60001                   mov byte ptr [rax], 1
0x384e291: 4885ff                   test rdi, rdi
0x384e294: 0f8416feffff             je 0x384e0b0
0x384e29a: e8f19786fc               call 0xb7a90
0x384e29f: e90cfeffff               jmp 0x384e0b0
0x384e2a4: 41803c2406               cmp byte ptr [r12], 6
0x384e2a9: 4c8bad78feffff           mov r13, qword ptr [rbp - 0x188]
0x384e2b0: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384e2b7: 0f8581000000             jne 0x384e33e
0x384e2bd: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384e2c2: 7448                     je 0x384e30c
0x384e2c4: 498b7500                 mov rsi, qword ptr [r13]
0x384e2c8: eb49                     jmp 0x384e313
0x384e2ca: e861130000               call 0x384f630
0x384e2cf: e88c9a67ff               call 0x2ec7d60
0x384e2d4: 84c0                     test al, al
0x384e2d6: 0f85cafcffff             jne 0x384dfa6
0x384e2dc: e8dfb066ff               call 0x2eb93c0
0x384e2e1: 488b08                   mov rcx, qword ptr [rax]
0x384e2e4: 4889c7                   mov rdi, rax
0x384e2e7: ff5130                   call qword ptr [rcx + 0x30]
0x384e2ea: 833d8756d00500           cmp dword ptr [rip + 0x5d05687], 0
0x384e2f1: 0f85bcfcffff             jne 0x384dfb3
0x384e2f7: 498b4500                 mov rax, qword ptr [r13]
0x384e2fb: 4c89ef                   mov rdi, r13
0x384e2fe: 4c89e6                   mov rsi, r12
0x384e301: ff9030050000             call qword ptr [rax + 0x530]
0x384e307: e9a7fcffff               jmp 0x384dfb3
0x384e30c: 488d358d12de03           lea rsi, [rip + 0x3de128d]
0x384e313: 488dbd28ffffff           lea rdi, [rbp - 0xd8]
0x384e31a: ba01000000               mov edx, 1
0x384e31f: e86cf29efc               call 0x23d590
0x384e324: 488bbd28ffffff           mov rdi, qword ptr [rbp - 0xd8]
0x384e32b: 488db548ffffff           lea rsi, [rbp - 0xb8]
0x384e332: 488d9540ffffff           lea rdx, [rbp - 0xc0]
0x384e339: e8627977ff               call 0x2fc5ca0
0x384e33e: 48c78530ffffff00000000   mov qword ptr [rbp - 0xd0], 0
0x384e349: 4d8b7728                 mov r14, qword ptr [r15 + 0x28]
0x384e34d: 4d636730                 movsxd r12, dword ptr [r15 + 0x30]
0x384e351: 4489a538ffffff           mov dword ptr [rbp - 0xc8], r12d
0x384e358: 4d85e4                   test r12, r12
0x384e35b: 0f8415010000             je 0x384e476
0x384e361: 488d853cffffff           lea rax, [rbp - 0xc4]
0x384e368: 4c8d8530ffffff           lea r8, [rbp - 0xd0]
0x384e36f: bf02000000               mov edi, 2
0x384e374: be02000000               mov esi, 2
0x384e379: 4489e2                   mov edx, r12d
0x384e37c: 4589e1                   mov r9d, r12d
0x384e37f: 31c9                     xor ecx, ecx
0x384e381: 48890424                 mov qword ptr [rsp], rax
0x384e385: e846927dfc               call 0x275d0
0x384e38a: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384e391: 4d01e4                   add r12, r12
0x384e394: 4c89f6                   mov rsi, r14
0x384e397: 4c89e2                   mov rdx, r12
0x384e39a: e8b13ac203               call 0x7471e50
0x384e39f: 488b9d40ffffff           mov rbx, qword ptr [rbp - 0xc0]
0x384e3a6: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384e3ad: 4885db                   test rbx, rbx
0x384e3b0: 0f84e1000000             je 0x384e497
0x384e3b6: 4d8bb42408020000         mov r14, qword ptr [r12 + 0x208]
0x384e3be: 4889de                   mov rsi, rbx
0x384e3c1: 4d85f6                   test r14, r14
0x384e3c4: 741e                     je 0x384e3e4
0x384e3c6: 488d058f0ab505           lea rax, [rip + 0x5b50a8f]
0x384e3cd: 4889de                   mov rsi, rbx
0x384e3d0: 803800                   cmp byte ptr [rax], 0
0x384e3d3: 740f                     je 0x384e3e4
0x384e3d5: 4c89f7                   mov rdi, r14
0x384e3d8: e8e39eb1fc               call 0x3682c0
0x384e3dd: 488bb540ffffff           mov rsi, qword ptr [rbp - 0xc0]
0x384e3e4: 488d05452ed005           lea rax, [rip + 0x5d02e45]
0x384e3eb: 4c89e7                   mov rdi, r12
0x384e3ee: 4c89b328020000           mov qword ptr [rbx + 0x228], r14
0x384e3f5: 488930                   mov qword ptr [rax], rsi
0x384e3f8: e8c377feff               call 0x3835bc0
0x384e3fd: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384e405: 410fb60c24               movzx ecx, byte ptr [r12]
0x384e40a: 488b9d88feffff           mov rbx, qword ptr [rbp - 0x178]
0x384e411: 4c8bbd60feffff           mov r15, qword ptr [rbp - 0x1a0]
0x384e418: 88888a010000             mov byte ptr [rax + 0x18a], cl
0x384e41e: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e426: 80f903                   cmp cl, 3
0x384e429: 7523                     jne 0x384e44e
0x384e42b: be02000000               mov esi, 2
0x384e430: e8fb7aa7fc               call 0x2c5f30
0x384e435: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384e43d: 488b03                   mov rax, qword ptr [rbx]
0x384e440: 4889df                   mov rdi, rbx
0x384e443: ff9040040000             call qword ptr [rax + 0x440]
0x384e449: e9a8000000               jmp 0x384e4f6
0x384e44e: 8b470c                   mov eax, dword ptr [rdi + 0xc]
0x384e451: 488d0db8bcb605           lea rcx, [rip + 0x5b6bcb8]
0x384e458: 394124                   cmp dword ptr [rcx + 0x24], eax
0x384e45b: 7e4e                     jle 0x384e4ab
0x384e45d: 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x384e461: 89c2                     mov edx, eax
0x384e463: 0fb7c0                   movzx eax, ax
0x384e466: 488d3c40                 lea rdi, [rax + rax*2]
0x384e46a: c1ea10                   shr edx, 0x10
0x384e46d: c1e703                   shl edi, 3
0x384e470: 48033cd1                 add rdi, qword ptr [rcx + rdx*8]
0x384e474: eb37                     jmp 0x384e4ad
0x384e476: c7853cffffff00000000     mov dword ptr [rbp - 0xc4], 0
0x384e480: 488b9d40ffffff           mov rbx, qword ptr [rbp - 0xc0]
0x384e487: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384e48e: 4885db                   test rbx, rbx
0x384e491: 0f851fffffff             jne 0x384e3b6
0x384e497: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384e49c: 0f8439090000             je 0x384eddb
0x384e4a2: 498b7500                 mov rsi, qword ptr [r13]
0x384e4a6: e937090000               jmp 0x384ede2
0x384e4ab: 31ff                     xor edi, edi
0x384e4ad: be00000040               mov esi, 0x40000000
0x384e4b2: e8b99fb1fc               call 0x368470
0x384e4b7: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e4bf: f6878b01000040           test byte ptr [rdi + 0x18b], 0x40
0x384e4c6: 752e                     jne 0x384e4f6
0x384e4c8: b800e0ffff               mov eax, 0xffffe000
0x384e4cd: 238518ffffff             and eax, dword ptr [rbp - 0xe8]
0x384e4d3: 488db518ffffff           lea rsi, [rbp - 0xe8]
0x384e4da: 0d7f160000               or eax, 0x167f
0x384e4df: 66898518ffffff           mov word ptr [rbp - 0xe8], ax
0x384e4e6: 48c78520ffffff00000000   mov qword ptr [rbp - 0xe0], 0
0x384e4f1: e8fadff3ff               call 0x378c4f0
0x384e4f6: 4d85ff                   test r15, r15
0x384e4f9: 740f                     je 0x384e50a
0x384e4fb: 488b03                   mov rax, qword ptr [rbx]
0x384e4fe: 4889df                   mov rdi, rbx
0x384e501: 4c89e6                   mov rsi, r12
0x384e504: ff9070050000             call qword ptr [rax + 0x570]
0x384e50a: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e512: 488bb590feffff           mov rsi, qword ptr [rbp - 0x170]
0x384e519: e8c289f4ff               call 0x3796ee0
0x384e51e: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e526: e8c529f5ff               call 0x37a0ef0
0x384e52b: 4885c0                   test rax, rax
0x384e52e: 742b                     je 0x384e55b
0x384e530: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e538: ba01000000               mov edx, 1
0x384e53d: 31f6                     xor esi, esi
0x384e53f: 4989c6                   mov r14, rax
0x384e542: e809eef3ff               call 0x378d350
0x384e547: 4885c0                   test rax, rax
0x384e54a: 740f                     je 0x384e55b
0x384e54c: 488bb0d8030000           mov rsi, qword ptr [rax + 0x3d8]
0x384e553: 4c89f7                   mov rdi, r14
0x384e556: e8752645ff               call 0x2ca0bd0
0x384e55b: 4d85ff                   test r15, r15
0x384e55e: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384e565: 752f                     jne 0x384e596
0x384e567: 488d059ca4b005           lea rax, [rip + 0x5b0a49c]
0x384e56e: 803801                   cmp byte ptr [rax], 1
0x384e571: 7513                     jne 0x384e586
0x384e573: 488d35a6d41004           lea rsi, [rip + 0x410d4a6] ; ['Listen']
0x384e57a: 4c89ff                   mov rdi, r15
0x384e57d: e8aebeedff               call 0x372a430
0x384e582: 84c0                     test al, al
0x384e584: 7410                     je 0x384e596
0x384e586: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e58e: 4c89fe                   mov rsi, r15
0x384e591: e8faecf4ff               call 0x379d290
0x384e596: 4c8d2d0310de03           lea r13, [rip + 0x3de1003]
0x384e59d: 488d35ecd5d203           lea rsi, [rip + 0x3d2d5ec] ; ['Mutator=']
0x384e5a4: 4c89ff                   mov rdi, r15
0x384e5a7: 4c89ea                   mov rdx, r13
0x384e5aa: e831bfedff               call 0x372a4e0
0x384e5af: 4885c0                   test rax, rax
0x384e5b2: 0f84be000000             je 0x384e676
0x384e5b8: 488d9d08ffffff           lea rbx, [rbp - 0xf8]
0x384e5bf: 4889c6                   mov rsi, rax
0x384e5c2: c5f957c0                 vxorpd xmm0, xmm0, xmm0
0x384e5c6: c5f91185b0feffff         vmovupd xmmword ptr [rbp - 0x150], xmm0
0x384e5ce: 4889df                   mov rdi, rbx
0x384e5d1: e81a5981fc               call 0x63ef0
0x384e5d6: 488d15ff231704           lea rdx, [rip + 0x41723ff] ; [',']
0x384e5dd: 488db5b0feffff           lea rsi, [rbp - 0x150]
0x384e5e4: 4889df                   mov rdi, rbx
0x384e5e7: b901000000               mov ecx, 1
0x384e5ec: e87fb781fc               call 0x69d70
0x384e5f1: 488bbd08ffffff           mov rdi, qword ptr [rbp - 0xf8]
0x384e5f8: 4885ff                   test rdi, rdi
0x384e5fb: 7405                     je 0x384e602
0x384e5fd: e88e9486fc               call 0xb7a90
0x384e602: 8bb5b8feffff             mov esi, dword ptr [rbp - 0x148]
0x384e608: 85f6                     test esi, esi
0x384e60a: 7e4d                     jle 0x384e659
0x384e60c: 4c8bbd88feffff           mov r15, qword ptr [rbp - 0x178]
0x384e613: 31db                     xor ebx, ebx
0x384e615: 4531f6                   xor r14d, r14d
0x384e618: 0f1f840000000000         nop dword ptr [rax + rax]
0x384e620: 488b8db0feffff           mov rcx, qword ptr [rbp - 0x150]
0x384e627: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384e62f: 4c89ff                   mov rdi, r15
0x384e632: ba04000000               mov edx, 4
0x384e637: 4801d9                   add rcx, rbx
0x384e63a: e871170000               call 0x384fdb0
0x384e63f: 4863b5b8feffff           movsxd rsi, dword ptr [rbp - 0x148]
0x384e646: 49ffc6                   inc r14
0x384e649: 4883c310                 add rbx, 0x10
0x384e64d: 4939f6                   cmp r14, rsi
0x384e650: 7cce                     jl 0x384e620
0x384e652: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384e659: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e660: e8dbc77dfc               call 0x2ae40
0x384e665: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e66c: 4885ff                   test rdi, rdi
0x384e66f: 7405                     je 0x384e676
0x384e671: e81a9486fc               call 0xb7a90
0x384e676: 488d058b77cf05           lea rax, [rip + 0x5cf778b]
0x384e67d: 488b38                   mov rdi, qword ptr [rax]
0x384e680: 4885ff                   test rdi, rdi
0x384e683: 740c                     je 0x384e691
0x384e685: ba01000000               mov edx, 1
0x384e68a: 31f6                     xor esi, esi
0x384e68c: e8cf7fcfff               call 0x3546660
0x384e691: 498b9c24c0020000         mov rbx, qword ptr [r12 + 0x2c0]
0x384e699: 488b7b30                 mov rdi, qword ptr [rbx + 0x30]
0x384e69d: e8ae7ac3fc               call 0x486150
0x384e6a2: 488b4018                 mov rax, qword ptr [rax + 0x18]
0x384e6a6: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384e6ad: 488db508ffffff           lea rsi, [rbp - 0xf8]
0x384e6b4: 4c89f7                   mov rdi, r14
0x384e6b7: 48898508ffffff           mov qword ptr [rbp - 0xf8], rax
0x384e6be: e8fd099ffc               call 0x23f0c0
0x384e6c3: 488bbd88feffff           mov rdi, qword ptr [rbp - 0x178]
0x384e6ca: 4889de                   mov rsi, rbx
0x384e6cd: 4c89f1                   mov rcx, r14
0x384e6d0: 31d2                     xor edx, edx
0x384e6d2: e8d9160000               call 0x384fdb0
0x384e6d7: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e6de: 4885ff                   test rdi, rdi
0x384e6e1: 7405                     je 0x384e6e8
0x384e6e3: e8a89386fc               call 0xb7a90
0x384e6e8: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e6f0: be02000000               mov esi, 2
0x384e6f5: e8d6fdf3ff               call 0x378e4d0
0x384e6fa: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384e6ff: 4d8bb424c0020000         mov r14, qword ptr [r12 + 0x2c0]
0x384e707: 740a                     je 0x384e713
0x384e709: 488b8578feffff           mov rax, qword ptr [rbp - 0x188]
0x384e710: 4c8b28                   mov r13, qword ptr [rax]
0x384e713: 488dbd00ffffff           lea rdi, [rbp - 0x100]
0x384e71a: 4c89ee                   mov rsi, r13
0x384e71d: ba01000000               mov edx, 1
0x384e722: e869ee9efc               call 0x23d590
0x384e727: 488bb500ffffff           mov rsi, qword ptr [rbp - 0x100]
0x384e72e: 4c89f7                   mov rdi, r14
0x384e731: e80a23f5ff               call 0x37a0a40
0x384e736: e865fce7fd               call 0x16ce3a0
0x384e73b: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e743: e858dcf3ff               call 0x378c3a0
0x384e748: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384e750: 488d9db0feffff           lea rbx, [rbp - 0x150]
0x384e757: 4889df                   mov rdi, rbx
0x384e75a: e8a17a50ff               call 0x2d56200
0x384e75f: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e767: 4c89fe                   mov rsi, r15
0x384e76a: ba01000000               mov edx, 1
0x384e76f: 4889d9                   mov rcx, rbx
0x384e772: e8e988f4ff               call 0x3797060
0x384e777: 4889df                   mov rdi, rbx
0x384e77a: e8b17a50ff               call 0x2d56230
0x384e77f: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e787: be01000000               mov esi, 1
0x384e78c: b901000000               mov ecx, 1
0x384e791: 31d2                     xor edx, edx
0x384e793: 4531c0                   xor r8d, r8d
0x384e796: e825b32fff               call 0x2b49ac0
0x384e79b: 498dbc24c0000000         lea rdi, [r12 + 0xc0]
0x384e7a3: 4c89fe                   mov rsi, r15
0x384e7a6: e8551922ff               call 0x2a70100
0x384e7ab: 4d8db424e8000000         lea r14, [r12 + 0xe8]
0x384e7b3: 488d8530ffffff           lea rax, [rbp - 0xd0]
0x384e7ba: 4c89e3                   mov rbx, r12
0x384e7bd: 4939c6                   cmp r14, rax
0x384e7c0: 7466                     je 0x384e828
0x384e7c2: 8b8bf4000000             mov ecx, dword ptr [rbx + 0xf4]
0x384e7c8: 4c63ad38ffffff           movsxd r13, dword ptr [rbp - 0xc8]
0x384e7cf: 4c8ba530ffffff           mov r12, qword ptr [rbp - 0xd0]
0x384e7d6: 488d83f4000000           lea rax, [rbx + 0xf4]
0x384e7dd: 89ca                     mov edx, ecx
0x384e7df: 4489abf0000000           mov dword ptr [rbx + 0xf0], r13d
0x384e7e6: 4409ea                   or edx, r13d
0x384e7e9: 0f84b5050000             je 0x384eda4
0x384e7ef: bf02000000               mov edi, 2
0x384e7f4: be02000000               mov esi, 2
0x384e7f9: 4489ea                   mov edx, r13d
0x384e7fc: 4d89f0                   mov r8, r14
0x384e7ff: 4589e9                   mov r9d, r13d
0x384e802: 48890424                 mov qword ptr [rsp], rax
0x384e806: e8c58d7dfc               call 0x275d0
0x384e80b: 488b9d98feffff           mov rbx, qword ptr [rbp - 0x168]
0x384e812: 4585ed                   test r13d, r13d
0x384e815: 7411                     je 0x384e828
0x384e817: 498b3e                   mov rdi, qword ptr [r14]
0x384e81a: 4d01ed                   add r13, r13
0x384e81d: 4c89e6                   mov rsi, r12
0x384e820: 4c89ea                   mov rdx, r13
0x384e823: e82836c203               call 0x7471e50
0x384e828: 488bbbc0020000           mov rdi, qword ptr [rbx + 0x2c0]
0x384e82f: e8dc31f5ff               call 0x37a1a10
0x384e834: 83f803                   cmp eax, 3
0x384e837: 750f                     jne 0x384e848
0x384e839: 488dbb28010000           lea rdi, [rbx + 0x128]
0x384e840: 4c89fe                   mov rsi, r15
0x384e843: e8b81822ff               call 0x2a70100
0x384e848: e853fbe7fd               call 0x16ce3a0
0x384e84d: 488bbb08020000           mov rdi, qword ptr [rbx + 0x208]
0x384e854: 4885ff                   test rdi, rdi
0x384e857: 0f84f2000000             je 0x384e94f
0x384e85d: e8ce9277ff               call 0x2fc7b30
0x384e862: 85d2                     test edx, edx
0x384e864: 0f88e5000000             js 0x384e94f
0x384e86a: 4989c5                   mov r13, rax
0x384e86d: 395008                   cmp dword ptr [rax + 8], edx
0x384e870: 0f8ed9000000             jle 0x384e94f
0x384e876: 4189d7                   mov r15d, edx
0x384e879: 4c8da508ffffff           lea r12, [rbp - 0xf8]
0x384e880: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384e887: eb39                     jmp 0x384e8c2
0x384e889: 488d3dfd7e2e04           lea rdi, [rip + 0x42e7efd]
0x384e890: be223f0000               mov esi, 0x3f22
0x384e895: 488d15fab0ec03           lea rdx, [rip + 0x3ecb0fa] ; ["Couldn't spawn player: %s"]
0x384e89c: 31c0                     xor eax, eax
0x384e89e: e80d6892fc               call 0x1750b0
0x384e8a3: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e8aa: 4885ff                   test rdi, rdi
0x384e8ad: 7406                     je 0x384e8b5
0x384e8af: 90                       nop 
0x384e8b0: e8db9186fc               call 0xb7a90
0x384e8b5: 49ffc7                   inc r15
0x384e8b8: 45397d08                 cmp dword ptr [r13 + 8], r15d
0x384e8bc: 0f8e8d000000             jle 0x384e94f
0x384e8c2: c5f957c0                 vxorpd xmm0, xmm0, xmm0
0x384e8c6: c5f91185b0feffff         vmovupd xmmword ptr [rbp - 0x150], xmm0
0x384e8ce: 488bb590feffff           mov rsi, qword ptr [rbp - 0x170]
0x384e8d5: 4c89e7                   mov rdi, r12
0x384e8d8: ba01000000               mov edx, 1
0x384e8dd: 498b4500                 mov rax, qword ptr [r13]
0x384e8e1: 4a8b1cf8                 mov rbx, qword ptr [rax + r15*8]
0x384e8e5: e8f6b5edff               call 0x3729ee0
0x384e8ea: 488b8598feffff           mov rax, qword ptr [rbp - 0x168]
0x384e8f1: 4889df                   mov rdi, rbx
0x384e8f4: 4c89e6                   mov rsi, r12
0x384e8f7: 4c89f2                   mov rdx, r14
0x384e8fa: 488b88c0020000           mov rcx, qword ptr [rax + 0x2c0]
0x384e901: 488b03                   mov rax, qword ptr [rbx]
0x384e904: ff9020030000             call qword ptr [rax + 0x320]
0x384e90a: 488bbd08ffffff           mov rdi, qword ptr [rbp - 0xf8]
0x384e911: 89c3                     mov ebx, eax
0x384e913: 4885ff                   test rdi, rdi
0x384e916: 7405                     je 0x384e91d
0x384e918: e8739186fc               call 0xb7a90
0x384e91d: 84db                     test bl, bl
0x384e91f: 740e                     je 0x384e92f
0x384e921: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384e928: 4885ff                   test rdi, rdi
0x384e92b: 7583                     jne 0x384e8b0
0x384e92d: eb86                     jmp 0x384e8b5
0x384e92f: 83bdb8feffff00           cmp dword ptr [rbp - 0x148], 0
0x384e936: 488d0d630cde03           lea rcx, [rip + 0x3de0c63]
0x384e93d: 0f8446ffffff             je 0x384e889
0x384e943: 488b8db0feffff           mov rcx, qword ptr [rbp - 0x150]
0x384e94a: e93affffff               jmp 0x384e889
0x384e94f: e86caa66ff               call 0x2eb93c0
0x384e954: 488b08                   mov rcx, qword ptr [rax]
0x384e957: 4889c7                   mov rdi, rax
0x384e95a: ff5138                   call qword ptr [rcx + 0x38]
0x384e95d: 488b05b44fd005           mov rax, qword ptr [rip + 0x5d04fb4]
0x384e964: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384e96b: 4885c0                   test rax, rax
0x384e96e: 7418                     je 0x384e988
0x384e970: 488bb8c80f0000           mov rdi, qword ptr [rax + 0xfc8]
0x384e977: 4885ff                   test rdi, rdi
0x384e97a: 740c                     je 0x384e988
0x384e97c: 488b07                   mov rax, qword ptr [rdi]
0x384e97f: 4c89e6                   mov rsi, r12
0x384e982: ff9068010000             call qword ptr [rax + 0x168]
0x384e988: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384e990: e87b8ef4ff               call 0x3797810
0x384e995: e806fae7fd               call 0x16ce3a0
0x384e99a: 488d1d5fb5b605           lea rbx, [rip + 0x5b6b55f]
0x384e9a1: 4d8bb424c0020000         mov r14, qword ptr [r12 + 0x2c0]
0x384e9a9: 8b4b14                   mov ecx, dword ptr [rbx + 0x14]
0x384e9ac: 8d4101                   lea eax, [rcx + 1]
0x384e9af: 894314                   mov dword ptr [rbx + 0x14], eax
0x384e9b2: 48634308                 movsxd rax, dword ptr [rbx + 8]
0x384e9b6: 4885c0                   test rax, rax
0x384e9b9: 0f8e83000000             jle 0x384ea42
0x384e9bf: 4531ff                   xor r15d, r15d
0x384e9c2: eb1d                     jmp 0x384e9e1
0x384e9c4: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384e9d0: 498d45ff                 lea rax, [r13 - 1]
0x384e9d4: 41b701                   mov r15b, 1
0x384e9d7: 4983fd01                 cmp r13, 1
0x384e9db: 0f861e070000             jbe 0x384f0ff
0x384e9e1: 4989c4                   mov r12, rax
0x384e9e4: 4989c5                   mov r13, rax
0x384e9e7: 49c1e404                 shl r12, 4
0x384e9eb: 4983c4f8                 add r12, -8
0x384e9ef: 90                       nop 
0x384e9f0: 488b03                   mov rax, qword ptr [rbx]
0x384e9f3: 42833c2000               cmp dword ptr [rax + r12], 0
0x384e9f8: 74d6                     je 0x384e9d0
0x384e9fa: 4a8b7c20f8               mov rdi, qword ptr [rax + r12 - 8]
0x384e9ff: 4885ff                   test rdi, rdi
0x384ea02: 74cc                     je 0x384e9d0
0x384ea04: 488b07                   mov rax, qword ptr [rdi]
0x384ea07: 4c89f6                   mov rsi, r14
0x384ea0a: ff5068                   call qword ptr [rax + 0x68]
0x384ea0d: 84c0                     test al, al
0x384ea0f: 74bf                     je 0x384e9d0
0x384ea11: 49ffcd                   dec r13
0x384ea14: 4983c4f0                 add r12, -0x10
0x384ea18: 498d4501                 lea rax, [r13 + 1]
0x384ea1c: 4883f801                 cmp rax, 1
0x384ea20: 77ce                     ja 0x384e9f0
0x384ea22: ff4b14                   dec dword ptr [rbx + 0x14]
0x384ea25: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384ea2c: 41f6c701                 test r15b, 1
0x384ea30: 7413                     je 0x384ea45
0x384ea32: 488d3dc7b4b605           lea rdi, [rip + 0x5b6b4c7]
0x384ea39: 31f6                     xor esi, esi
0x384ea3b: e8b0237efc               call 0x30df0
0x384ea40: eb03                     jmp 0x384ea45
0x384ea42: 894b14                   mov dword ptr [rbx + 0x14], ecx
0x384ea45: e846c87bfc               call 0xb290
0x384ea4a: e871f9e7fd               call 0x16ce3c0
0x384ea4f: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384ea57: 488bbd88feffff           mov rdi, qword ptr [rbp - 0x178]
0x384ea5e: 31f6                     xor esi, esi
0x384ea60: 83888b01000001           or dword ptr [rax + 0x18b], 1
0x384ea67: 488b07                   mov rax, qword ptr [rdi]
0x384ea6a: ff9000050000             call qword ptr [rax + 0x500]
0x384ea70: e84ba966ff               call 0x2eb93c0
0x384ea75: 4889c7                   mov rdi, rax
0x384ea78: be01000000               mov esi, 1
0x384ea7d: e82e9c67ff               call 0x2ec86b0
0x384ea82: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384ea8a: 4c8bbd78feffff           mov r15, qword ptr [rbp - 0x188]
0x384ea91: 488bb8a8010000           mov rdi, qword ptr [rax + 0x1a8]
0x384ea98: 4885ff                   test rdi, rdi
0x384ea9b: 7411                     je 0x384eaae
0x384ea9d: 488b07                   mov rax, qword ptr [rdi]
0x384eaa0: ff90e0080000             call qword ptr [rax + 0x8e0]
0x384eaa6: 84c0                     test al, al
0x384eaa8: 0f85f0000000             jne 0x384eb9e
0x384eaae: 488bbd90feffff           mov rdi, qword ptr [rbp - 0x170]
0x384eab5: 488d3548d91804           lea rsi, [rip + 0x418d948] ; ['DemoRec']
0x384eabc: e86fb9edff               call 0x372a430
0x384eac1: 84c0                     test al, al
0x384eac3: 0f84d5000000             je 0x384eb9e
0x384eac9: 4983bc240802000000       cmp qword ptr [r12 + 0x208], 0
0x384ead2: 0f84c6000000             je 0x384eb9e
0x384ead8: 4c8bad90feffff           mov r13, qword ptr [rbp - 0x170]
0x384eadf: 488d35cc2e1604           lea rsi, [rip + 0x4162ecc] ; ['DemoRec=']
0x384eae6: 31d2                     xor edx, edx
0x384eae8: 4c89ef                   mov rdi, r13
0x384eaeb: e8f0b9edff               call 0x372a4e0
0x384eaf0: 498b9c2408020000         mov rbx, qword ptr [r12 + 0x208]
0x384eaf8: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384eaff: 4889c6                   mov rsi, rax
0x384eb02: 4c89f7                   mov rdi, r14
0x384eb05: e8e65381fc               call 0x63ef0
0x384eb0a: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384eb12: 4c8da508ffffff           lea r12, [rbp - 0xf8]
0x384eb19: 4c89e7                   mov rdi, r12
0x384eb1c: e82fe9f3ff               call 0x378d450
0x384eb21: c5f957c0                 vxorpd xmm0, xmm0, xmm0
0x384eb25: c5f91185a0feffff         vmovupd xmmword ptr [rbp - 0x160], xmm0
0x384eb2d: 498d4d48                 lea rcx, [r13 + 0x48]
0x384eb31: 4c8d85a0feffff           lea r8, [rbp - 0x160]
0x384eb38: 4889df                   mov rdi, rbx
0x384eb3b: 4c89f6                   mov rsi, r14
0x384eb3e: 4c89e2                   mov rdx, r12
0x384eb41: 488b03                   mov rax, qword ptr [rbx]
0x384eb44: ff9070030000             call qword ptr [rax + 0x370]
0x384eb4a: 4c8bb5a8feffff           mov r14, qword ptr [rbp - 0x158]
0x384eb51: 4d85f6                   test r14, r14
0x384eb54: 741f                     je 0x384eb75
0x384eb56: f041ff4e08               lock dec dword ptr [r14 + 8]
0x384eb5b: 7518                     jne 0x384eb75
0x384eb5d: 498b06                   mov rax, qword ptr [r14]
0x384eb60: 4c89f7                   mov rdi, r14
0x384eb63: ff10                     call qword ptr [rax]
0x384eb65: f041ff4e0c               lock dec dword ptr [r14 + 0xc]
0x384eb6a: 7509                     jne 0x384eb75
0x384eb6c: 498b06                   mov rax, qword ptr [r14]
0x384eb6f: 4c89f7                   mov rdi, r14
0x384eb72: ff5010                   call qword ptr [rax + 0x10]
0x384eb75: 488bbd08ffffff           mov rdi, qword ptr [rbp - 0xf8]
0x384eb7c: 4885ff                   test rdi, rdi
0x384eb7f: 7405                     je 0x384eb86
0x384eb81: e80a8f86fc               call 0xb7a90
0x384eb86: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384eb8d: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384eb94: 4885ff                   test rdi, rdi
0x384eb97: 7405                     je 0x384eb9e
0x384eb99: e8f28e86fc               call 0xb7a90
0x384eb9e: e8bd37c203               call 0x7472360
0x384eba3: c4e1f96ec0               vmovq xmm0, rax
0x384eba8: c5f9620510e31e04         vpunpckldq xmm0, xmm0, xmmword ptr [rip + 0x41ee310]
0x384ebb0: 488d05b9d4a805           lea rax, [rip + 0x5a8d4b9] ; ['鸰锒輭㸅Ā']
0x384ebb7: c5f95c0571c81e04         vsubpd xmm0, xmm0, xmmword ptr [rip + 0x41ec871]
0x384ebbf: c5f9c6c801               vshufpd xmm1, xmm0, xmm0, 1
0x384ebc4: c5f358c0                 vaddsd xmm0, xmm1, xmm0
0x384ebc8: c5fb5900                 vmulsd xmm0, xmm0, qword ptr [rax]
0x384ebcc: c5fb5c8558feffff         vsubsd xmm0, xmm0, qword ptr [rbp - 0x1a8]
0x384ebd4: c5fb118598feffff         vmovsd qword ptr [rbp - 0x168], xmm0
0x384ebdc: e86faf9cfc               call 0x219b50
0x384ebe1: 4889c7                   mov rdi, rax
0x384ebe4: e8a7c67bfc               call 0xb290
0x384ebe9: c5fb108598feffff         vmovsd xmm0, qword ptr [rbp - 0x168]
0x384ebf1: 488b8590feffff           mov rax, qword ptr [rbp - 0x170]
0x384ebf8: 4d8bb42408020000         mov r14, qword ptr [r12 + 0x208]
0x384ec00: 83783000                 cmp dword ptr [rax + 0x30], 0
0x384ec04: c5fb5ac0                 vcvtsd2ss xmm0, xmm0, xmm0
0x384ec08: c5fa118598feffff         vmovss dword ptr [rbp - 0x168], xmm0
0x384ec10: 7405                     je 0x384ec17
0x384ec12: 498b37                   mov rsi, qword ptr [r15]
0x384ec15: eb07                     jmp 0x384ec1e
0x384ec17: 488d358209de03           lea rsi, [rip + 0x3de0982]
0x384ec1e: 488d9db0feffff           lea rbx, [rbp - 0x150]
0x384ec25: 4889df                   mov rdi, rbx
0x384ec28: e8c35281fc               call 0x63ef0
0x384ec2d: c5fa108598feffff         vmovss xmm0, dword ptr [rbp - 0x168]
0x384ec35: 498b06                   mov rax, qword ptr [r14]
0x384ec38: 4c89f7                   mov rdi, r14
0x384ec3b: 4889de                   mov rsi, rbx
0x384ec3e: ff9020030000             call qword ptr [rax + 0x320]
0x384ec44: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384ec4b: 4885ff                   test rdi, rdi
0x384ec4e: 7405                     je 0x384ec55
0x384ec50: e83b8e86fc               call 0xb7a90
0x384ec55: 833d1c4dd00500           cmp dword ptr [rip + 0x5d04d1c], 0
0x384ec5c: 741d                     je 0x384ec7b
0x384ec5e: e8cd090000               call 0x384f630
0x384ec63: 833d0e4dd00501           cmp dword ptr [rip + 0x5d04d0e], 1
0x384ec6a: 41b601                   mov r14b, 1
0x384ec6d: b301                     mov bl, 1
0x384ec6f: 750f                     jne 0x384ec80
0x384ec71: c705fd4cd00500000000     mov dword ptr [rip + 0x5d04cfd], 0
0x384ec7b: 41b601                   mov r14b, 1
0x384ec7e: b301                     mov bl, 1
0x384ec80: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384ec87: 4885ff                   test rdi, rdi
0x384ec8a: 7405                     je 0x384ec91
0x384ec8c: e8ff8d86fc               call 0xb7a90
0x384ec91: 4584f6                   test r14b, r14b
0x384ec94: 0f85b4000000             jne 0x384ed4e
0x384ec9a: 4c8d3d5fb2b605           lea r15, [rip + 0x5b6b25f]
0x384eca1: 418b4f14                 mov ecx, dword ptr [r15 + 0x14]
0x384eca5: 8d4101                   lea eax, [rcx + 1]
0x384eca8: 41894714                 mov dword ptr [r15 + 0x14], eax
0x384ecac: 49634708                 movsxd rax, dword ptr [r15 + 8]
0x384ecb0: 4885c0                   test rax, rax
0x384ecb3: 0f8e91000000             jle 0x384ed4a
0x384ecb9: 4531e4                   xor r12d, r12d
0x384ecbc: 899d98feffff             mov dword ptr [rbp - 0x168], ebx
0x384ecc2: eb1d                     jmp 0x384ece1
0x384ecc4: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384ecd0: 488d43ff                 lea rax, [rbx - 1]
0x384ecd4: 41b401                   mov r12b, 1
0x384ecd7: 4883fb01                 cmp rbx, 1
0x384ecdb: 0f863c040000             jbe 0x384f11d
0x384ece1: 4989c5                   mov r13, rax
0x384ece4: 4889c3                   mov rbx, rax
0x384ece7: 49c1e504                 shl r13, 4
0x384eceb: 4983c5f8                 add r13, -8
0x384ecef: 90                       nop 
0x384ecf0: 498b07                   mov rax, qword ptr [r15]
0x384ecf3: 42833c2800               cmp dword ptr [rax + r13], 0
0x384ecf8: 74d6                     je 0x384ecd0
0x384ecfa: 4a8b7c28f8               mov rdi, qword ptr [rax + r13 - 8]
0x384ecff: 4885ff                   test rdi, rdi
0x384ed02: 74cc                     je 0x384ecd0
0x384ed04: 488b07                   mov rax, qword ptr [rdi]
0x384ed07: 31f6                     xor esi, esi
0x384ed09: ff5068                   call qword ptr [rax + 0x68]
0x384ed0c: 84c0                     test al, al
0x384ed0e: 74c0                     je 0x384ecd0
0x384ed10: 48ffcb                   dec rbx
0x384ed13: 4983c5f0                 add r13, -0x10
0x384ed17: 488d4301                 lea rax, [rbx + 1]
0x384ed1b: 4883f801                 cmp rax, 1
0x384ed1f: 77cf                     ja 0x384ecf0
0x384ed21: 41ff4f14                 dec dword ptr [r15 + 0x14]
0x384ed25: 41f6c401                 test r12b, 1
0x384ed29: 0f849c000000             je 0x384edcb
0x384ed2f: 488d3dcab1b605           lea rdi, [rip + 0x5b6b1ca]
0x384ed36: 31f6                     xor esi, esi
0x384ed38: e8b3207efc               call 0x30df0
0x384ed3d: 8b9d98feffff             mov ebx, dword ptr [rbp - 0x168]
0x384ed43: 4584f6                   test r14b, r14b
0x384ed46: 7510                     jne 0x384ed58
0x384ed48: eb09                     jmp 0x384ed53
0x384ed4a: 41894f14                 mov dword ptr [r15 + 0x14], ecx
0x384ed4e: 4584f6                   test r14b, r14b
0x384ed51: 7505                     jne 0x384ed58
0x384ed53: e838c57bfc               call 0xb290
0x384ed58: e813d486fc               call 0xbc170
0x384ed5d: 4889c7                   mov rdi, rax
0x384ed60: e82bc57bfc               call 0xb290
0x384ed65: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x384ed69: 4885ff                   test rdi, rdi
0x384ed6c: 7405                     je 0x384ed73
0x384ed6e: e81d8d86fc               call 0xb7a90
0x384ed73: 488d7dcf                 lea rdi, [rbp - 0x31]
0x384ed77: e834ea95fc               call 0x1ad7b0
0x384ed7c: 488b0585318e05           mov rax, qword ptr [rip + 0x58e3185]
0x384ed83: 488b00                   mov rax, qword ptr [rax]
0x384ed86: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x384ed8a: 0f859e040000             jne 0x384f22e
0x384ed90: 89d8                     mov eax, ebx
0x384ed92: 4881c488010000           add rsp, 0x188
0x384ed99: 5b                       pop rbx
0x384ed9a: 415c                     pop r12
0x384ed9c: 415d                     pop r13
0x384ed9e: 415e                     pop r14
0x384eda0: 415f                     pop r15
0x384eda2: 5d                       pop rbp
0x384eda3: c3                       ret 
0x384eda4: 488b9d98feffff           mov rbx, qword ptr [rbp - 0x168]
0x384edab: c70000000000             mov dword ptr [rax], 0
0x384edb1: 488bbbc0020000           mov rdi, qword ptr [rbx + 0x2c0]
0x384edb8: e8532cf5ff               call 0x37a1a10
0x384edbd: 83f803                   cmp eax, 3
0x384edc0: 0f8473faffff             je 0x384e839
0x384edc6: e97dfaffff               jmp 0x384e848
0x384edcb: 8b9d98feffff             mov ebx, dword ptr [rbp - 0x168]
0x384edd1: 4584f6                   test r14b, r14b
0x384edd4: 7582                     jne 0x384ed58
0x384edd6: e978ffffff               jmp 0x384ed53
0x384eddb: 488d35be07de03           lea rsi, [rip + 0x3de07be]
0x384ede2: 4c8db508ffffff           lea r14, [rbp - 0xf8]
0x384ede9: ba01000000               mov edx, 1
0x384edee: 4c89f7                   mov rdi, r14
0x384edf1: e89ae79efc               call 0x23d590
0x384edf6: 488bbd08ffffff           mov rdi, qword ptr [rbp - 0xf8]
0x384edfd: 410fb61c24               movzx ebx, byte ptr [r12]
0x384ee02: 4989ff                   mov r15, rdi
0x384ee05: 49c1ef20                 shr r15, 0x20
0x384ee09: e8c2ac9efc               call 0x239ad0
0x384ee0e: 4c8d252324d005           lea r12, [rip + 0x5d02423]
0x384ee15: 4401f8                   add eax, r15d
0x384ee18: 418b4c2408               mov ecx, dword ptr [r12 + 8]
0x384ee1d: 413b4c2434               cmp ecx, dword ptr [r12 + 0x34]
0x384ee22: 7462                     je 0x384ee86
0x384ee24: 498b4c2440               mov rcx, qword ptr [r12 + 0x40]
0x384ee29: 498d542438               lea rdx, [r12 + 0x38]
0x384ee2e: 4885c9                   test rcx, rcx
0x384ee31: 480f45d1                 cmovne rdx, rcx
0x384ee35: 418b4c2448               mov ecx, dword ptr [r12 + 0x48]
0x384ee3a: ffc9                     dec ecx
0x384ee3c: 21c1                     and ecx, eax
0x384ee3e: 8b148a                   mov edx, dword ptr [rdx + rcx*4]
0x384ee41: 83faff                   cmp edx, -1
0x384ee44: 7440                     je 0x384ee86
0x384ee46: 498b0c24                 mov rcx, qword ptr [r12]
0x384ee4a: 488bb508ffffff           mov rsi, qword ptr [rbp - 0xf8]
0x384ee51: 6666666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384ee60: 4863d2                   movsxd rdx, edx
0x384ee63: 488d3c92                 lea rdi, [rdx + rdx*4]
0x384ee67: 488d14b9                 lea rdx, [rcx + rdi*4]
0x384ee6b: 483934b9                 cmp qword ptr [rcx + rdi*4], rsi
0x384ee6f: 740a                     je 0x384ee7b
0x384ee71: 8b520c                   mov edx, dword ptr [rdx + 0xc]
0x384ee74: 83faff                   cmp edx, -1
0x384ee77: 75e7                     jne 0x384ee60
0x384ee79: eb0b                     jmp 0x384ee86
0x384ee7b: 4885d2                   test rdx, rdx
0x384ee7e: 7406                     je 0x384ee86
0x384ee80: 4883c208                 add rdx, 8
0x384ee84: eb29                     jmp 0x384eeaf
0x384ee86: 488d95b0feffff           lea rdx, [rbp - 0x150]
0x384ee8d: 4c89e7                   mov rdi, r12
0x384ee90: 89c6                     mov esi, eax
0x384ee92: 31c9                     xor ecx, ecx
0x384ee94: 4c89b5b0feffff           mov qword ptr [rbp - 0x150], r14
0x384ee9b: e8d0d5d2fd               call 0x157c470
0x384eea0: 498b0c24                 mov rcx, qword ptr [r12]
0x384eea4: 4898                     cdqe 
0x384eea6: 488d0480                 lea rax, [rax + rax*4]
0x384eeaa: 488d548108               lea rdx, [rcx + rax*4 + 8]
0x384eeaf: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384eeb6: 891a                     mov dword ptr [rdx], ebx
0x384eeb8: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384eebf: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384eec4: 7406                     je 0x384eecc
0x384eec6: 498b7500                 mov rsi, qword ptr [r13]
0x384eeca: eb07                     jmp 0x384eed3
0x384eecc: 488d35cd06de03           lea rsi, [rip + 0x3de06cd]
0x384eed3: 31ff                     xor edi, edi
0x384eed5: e8f6c9c3fc               call 0x48b8d0
0x384eeda: 48898548ffffff           mov qword ptr [rbp - 0xb8], rax
0x384eee1: 4885c0                   test rax, rax
0x384eee4: 0f95c3                   setne bl
0x384eee7: 7534                     jne 0x384ef1d
0x384eee9: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384eeee: 7406                     je 0x384eef6
0x384eef0: 498b7500                 mov rsi, qword ptr [r13]
0x384eef4: eb07                     jmp 0x384eefd
0x384eef6: 488d35a306de03           lea rsi, [rip + 0x3de06a3]
0x384eefd: 31d2                     xor edx, edx
0x384eeff: 41803c2403               cmp byte ptr [r12], 3
0x384ef04: 0f94c2                   sete dl
0x384ef07: 31ff                     xor edi, edi
0x384ef09: 31c9                     xor ecx, ecx
0x384ef0b: 4531c0                   xor r8d, r8d
0x384ef0e: c1e213                   shl edx, 0x13
0x384ef11: e82ad6c3fc               call 0x48c540
0x384ef16: 48898548ffffff           mov qword ptr [rbp - 0xb8], rax
0x384ef1d: 488bb508ffffff           mov rsi, qword ptr [rbp - 0xf8]
0x384ef24: 488d3d0d23d005           lea rdi, [rip + 0x5d0230d]
0x384ef2b: e880a57ffd               call 0x10494b0
0x384ef30: 488bbd48ffffff           mov rdi, qword ptr [rbp - 0xb8]
0x384ef37: 4885ff                   test rdi, rdi
0x384ef3a: 0f84d8000000             je 0x384f018
0x384ef40: e89bf0f4ff               call 0x379dfe0
0x384ef45: 48898540ffffff           mov qword ptr [rbp - 0xc0], rax
0x384ef4c: 4885c0                   test rax, rax
0x384ef4f: 0f8488000000             je 0x384efdd
0x384ef55: 488b7830                 mov rdi, qword ptr [rax + 0x30]
0x384ef59: e882a996ff               call 0x31b98e0
0x384ef5e: 410fb60424               movzx eax, byte ptr [r12]
0x384ef63: 83f801                   cmp eax, 1
0x384ef66: 0f843e010000             je 0x384f0aa
0x384ef6c: 83f803                   cmp eax, 3
0x384ef6f: 0f85e2010000             jne 0x384f157
0x384ef75: 84db                     test bl, bl
0x384ef77: 7514                     jne 0x384ef8d
0x384ef79: 488bb540ffffff           mov rsi, qword ptr [rbp - 0xc0]
0x384ef80: 80be8a01000002           cmp byte ptr [rsi + 0x18a], 2
0x384ef87: 0f8599010000             jne 0x384f126
0x384ef8d: 4183bc2420020000ff       cmp dword ptr [r12 + 0x220], -1
0x384ef96: 750c                     jne 0x384efa4
0x384ef98: 41c784242002000000000000 mov dword ptr [r12 + 0x220], 0
0x384efa4: 488b9d88feffff           mov rbx, qword ptr [rbp - 0x178]
0x384efab: 488b9540ffffff           mov rdx, qword ptr [rbp - 0xc0]
0x384efb2: 4c89e6                   mov rsi, r12
0x384efb5: 4c89e9                   mov rcx, r13
0x384efb8: 488b03                   mov rax, qword ptr [rbx]
0x384efbb: 4889df                   mov rdi, rbx
0x384efbe: ff9038040000             call qword ptr [rax + 0x438]
0x384efc4: 488d0d869ab005           lea rcx, [rip + 0x5b09a86]
0x384efcb: 4889c6                   mov rsi, rax
0x384efce: 48898540ffffff           mov qword ptr [rbp - 0xc0], rax
0x384efd5: c60101                   mov byte ptr [rcx], 1
0x384efd8: e96e010000               jmp 0x384f14b
0x384efdd: 488bbd48ffffff           mov rdi, qword ptr [rbp - 0xb8]
0x384efe4: 31f6                     xor esi, esi
0x384efe6: e865f0f4ff               call 0x379e050
0x384efeb: 48898540ffffff           mov qword ptr [rbp - 0xc0], rax
0x384eff2: 4885c0                   test rax, rax
0x384eff5: 7421                     je 0x384f018
0x384eff7: 4889c7                   mov rdi, rax
0x384effa: e85171c3fc               call 0x486150
0x384efff: 48898548ffffff           mov qword ptr [rbp - 0xb8], rax
0x384f006: 488b8540ffffff           mov rax, qword ptr [rbp - 0xc0]
0x384f00d: b301                     mov bl, 1
0x384f00f: 4885c0                   test rax, rax
0x384f012: 0f853dffffff             jne 0x384ef55
0x384f018: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384f01d: 7406                     je 0x384f025
0x384f01f: 498b5500                 mov rdx, qword ptr [r13]
0x384f023: eb07                     jmp 0x384f02c
0x384f025: 488d157405de03           lea rdx, [rip + 0x3de0574]
0x384f02c: 488b9d80feffff           mov rbx, qword ptr [rbp - 0x180]
0x384f033: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384f03a: 488d35f5111104           lea rsi, [rip + 0x41111f5] ; ["Failed to load package '%s'"]
0x384f041: 4c89f7                   mov rdi, r14
0x384f044: 31c0                     xor eax, eax
0x384f046: e835ca81fc               call 0x6ba80
0x384f04b: 4939de                   cmp r14, rbx
0x384f04e: 742f                     je 0x384f07f
0x384f050: 488b3b                   mov rdi, qword ptr [rbx]
0x384f053: 4885ff                   test rdi, rdi
0x384f056: 7405                     je 0x384f05d
0x384f058: e8338a86fc               call 0xb7a90
0x384f05d: 488b85b0feffff           mov rax, qword ptr [rbp - 0x150]
0x384f064: 488903                   mov qword ptr [rbx], rax
0x384f067: 48c785b0feffff00000000   mov qword ptr [rbp - 0x150], 0
0x384f072: 488b85b8feffff           mov rax, qword ptr [rbp - 0x148]
0x384f079: 48894308                 mov qword ptr [rbx + 8], rax
0x384f07d: eb11                     jmp 0x384f090
0x384f07f: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384f086: 4885ff                   test rdi, rdi
0x384f089: 7405                     je 0x384f090
0x384f08b: e8008a86fc               call 0xb7a90
0x384f090: 4531f6                   xor r14d, r14d
0x384f093: 31db                     xor ebx, ebx
0x384f095: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384f09c: 4885ff                   test rdi, rdi
0x384f09f: 0f85e7fbffff             jne 0x384ec8c
0x384f0a5: e9e7fbffff               jmp 0x384ec91
0x384f0aa: 488b8540ffffff           mov rax, qword ptr [rbp - 0xc0]
0x384f0b1: f6808b01000040           test byte ptr [rax + 0x18b], 0x40
0x384f0b8: 0f8499000000             je 0x384f157
0x384f0be: 4183bc2420020000ff       cmp dword ptr [r12 + 0x220], -1
0x384f0c7: 0f848a000000             je 0x384f157
0x384f0cd: 488d9548ffffff           lea rdx, [rbp - 0xb8]
0x384f0d4: 4c89e7                   mov rdi, r12
0x384f0d7: 4c89ee                   mov rsi, r13
0x384f0da: e8810a0000               call 0x384fb60
0x384f0df: 48898540ffffff           mov qword ptr [rbp - 0xc0], rax
0x384f0e6: 4885c0                   test rax, rax
0x384f0e9: 756c                     jne 0x384f157
0x384f0eb: 41837f3000               cmp dword ptr [r15 + 0x30], 0
0x384f0f0: 0f8417010000             je 0x384f20d
0x384f0f6: 498b5500                 mov rdx, qword ptr [r13]
0x384f0fa: e915010000               jmp 0x384f214
0x384f0ff: ff4b14                   dec dword ptr [rbx + 0x14]
0x384f102: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384f109: e924f9ffff               jmp 0x384ea32
0x384f10e: ff4b14                   dec dword ptr [rbx + 0x14]
0x384f111: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384f118: e9a7eaffff               jmp 0x384dbc4
0x384f11d: 41ff4f14                 dec dword ptr [r15 + 0x14]
0x384f121: e909fcffff               jmp 0x384ed2f
0x384f126: 418b842420020000         mov eax, dword ptr [r12 + 0x220]
0x384f12e: 83f8ff                   cmp eax, -1
0x384f131: 7411                     je 0x384f144
0x384f133: 4889f7                   mov rdi, rsi
0x384f136: 89c6                     mov esi, eax
0x384f138: e853c17bfc               call 0xb290
0x384f13d: 488bb540ffffff           mov rsi, qword ptr [rbp - 0xc0]
0x384f144: 488b9d88feffff           mov rbx, qword ptr [rbp - 0x178]
0x384f14b: 488b03                   mov rax, qword ptr [rbx]
0x384f14e: 4889df                   mov rdi, rbx
0x384f151: ff9010040000             call qword ptr [rax + 0x410]
0x384f157: 488b9d40ffffff           mov rbx, qword ptr [rbp - 0xc0]
0x384f15e: 4d8bb42408020000         mov r14, qword ptr [r12 + 0x208]
0x384f166: 4889de                   mov rsi, rbx
0x384f169: 4d85f6                   test r14, r14
0x384f16c: 0f8554f2ffff             jne 0x384e3c6
0x384f172: e96df2ffff               jmp 0x384e3e4
0x384f177: 83bdb8feffff00           cmp dword ptr [rbp - 0x148], 0
0x384f17e: 488d151b04de03           lea rdx, [rip + 0x3de041b]
0x384f185: 7407                     je 0x384f18e
0x384f187: 488b95b0feffff           mov rdx, qword ptr [rbp - 0x150]
0x384f18e: 4c8db530ffffff           lea r14, [rbp - 0xd0]
0x384f195: 488d35107bfc03           lea rsi, [rip + 0x3fc7b10] ; ["Failed to load package '%s' while in PIE"]
0x384f19c: 31c0                     xor eax, eax
0x384f19e: 4c89f7                   mov rdi, r14
0x384f1a1: e8dac881fc               call 0x6ba80
0x384f1a6: 488b9d80feffff           mov rbx, qword ptr [rbp - 0x180]
0x384f1ad: 4939de                   cmp r14, rbx
0x384f1b0: 742f                     je 0x384f1e1
0x384f1b2: 488b3b                   mov rdi, qword ptr [rbx]
0x384f1b5: 4885ff                   test rdi, rdi
0x384f1b8: 7405                     je 0x384f1bf
0x384f1ba: e8d18886fc               call 0xb7a90
0x384f1bf: 488b8530ffffff           mov rax, qword ptr [rbp - 0xd0]
0x384f1c6: 488903                   mov qword ptr [rbx], rax
0x384f1c9: 48c78530ffffff00000000   mov qword ptr [rbp - 0xd0], 0
0x384f1d4: 488b8538ffffff           mov rax, qword ptr [rbp - 0xc8]
0x384f1db: 48894308                 mov qword ptr [rbx + 8], rax
0x384f1df: eb11                     jmp 0x384f1f2
0x384f1e1: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384f1e8: 4885ff                   test rdi, rdi
0x384f1eb: 7405                     je 0x384f1f2
0x384f1ed: e89e8886fc               call 0xb7a90
0x384f1f2: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384f1f9: 4885ff                   test rdi, rdi
0x384f1fc: 7405                     je 0x384f203
0x384f1fe: e88d8886fc               call 0xb7a90
0x384f203: 31db                     xor ebx, ebx
0x384f205: 4531f6                   xor r14d, r14d
0x384f208: e98dfaffff               jmp 0x384ec9a
0x384f20d: 488d158c03de03           lea rdx, [rip + 0x3de038c]
0x384f214: 488b9d80feffff           mov rbx, qword ptr [rbp - 0x180]
0x384f21b: 4c8db5b0feffff           lea r14, [rbp - 0x150]
0x384f222: 488d357b52ef03           lea rsi, [rip + 0x3ef527b] ; ["Failed to load package '%s' into a new game world."]
0x384f229: e913feffff               jmp 0x384f041
0x384f22e: e82d2cc203               call 0x7471e60
0x384f233: 0f0b                     ud2 
0x384f235: cc                       int3 
0x384f236: cc                       int3 
0x384f237: cc                       int3 
0x384f238: cc                       int3 
0x384f239: cc                       int3 
0x384f23a: cc                       int3 
0x384f23b: cc                       int3 
0x384f23c: cc                       int3 
0x384f23d: cc                       int3 
0x384f23e: cc                       int3 
0x384f23f: cc                       int3 
0x384f240: 55                       push rbp
0x384f241: 4889e5                   mov rbp, rsp
0x384f244: 4157                     push r15
0x384f246: 4156                     push r14
0x384f248: 4155                     push r13
0x384f24a: 4154                     push r12
0x384f24c: 53                       push rbx
0x384f24d: 4883ec18                 sub rsp, 0x18
0x384f251: 83bea001000000           cmp dword ptr [rsi + 0x1a0], 0
0x384f258: 8955cc                   mov dword ptr [rbp - 0x34], edx
0x384f25b: 0f8e76010000             jle 0x384f3d7
0x384f261: 4c8d35a8aeb605           lea r14, [rip + 0x5b6aea8]
0x384f268: 4989cf                   mov r15, rcx
0x384f26b: 31db                     xor ebx, ebx
0x384f26d: 488975d0                 mov qword ptr [rbp - 0x30], rsi
0x384f271: 48894dc0                 mov qword ptr [rbp - 0x40], rcx
0x384f275: eb3c                     jmp 0x384f2b3
0x384f277: 660f1f840000000000       nop word ptr [rax + rax]
0x384f280: 488d351903de03           lea rsi, [rip + 0x3de0319]
0x384f287: e814da83fc               call 0x8cca0
0x384f28c: 488b75d0                 mov rsi, qword ptr [rbp - 0x30]
0x384f290: 85c0                     test eax, eax
0x384f292: 7477                     je 0x384f30b
0x384f294: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384f2a0: 486386a0010000           movsxd rax, dword ptr [rsi + 0x1a0]
0x384f2a7: 48ffc3                   inc rbx
0x384f2aa: 4839c3                   cmp rbx, rax
0x384f2ad: 0f8d24010000             jge 0x384f3d7
0x384f2b3: 4c8bae98010000           mov r13, qword ptr [rsi + 0x198]
0x384f2ba: 486bc338                 imul rax, rbx, 0x38
0x384f2be: 410fb64c0500             movzx ecx, byte ptr [r13 + rax]
0x384f2c4: 3b4dcc                   cmp ecx, dword ptr [rbp - 0x34]
0x384f2c7: 75d7                     jne 0x384f2a0
0x384f2c9: 4901c5                   add r13, rax
0x384f2cc: 418b4708                 mov eax, dword ptr [r15 + 8]
0x384f2d0: 418b4d10                 mov ecx, dword ptr [r13 + 0x10]
0x384f2d4: 39c1                     cmp ecx, eax
0x384f2d6: 7528                     jne 0x384f300
0x384f2d8: 83f902                   cmp ecx, 2
0x384f2db: 7c2e                     jl 0x384f30b
0x384f2dd: 498b7d08                 mov rdi, qword ptr [r13 + 8]
0x384f2e1: 498b37                   mov rsi, qword ptr [r15]
0x384f2e4: e8b7d983fc               call 0x8cca0
0x384f2e9: 85c0                     test eax, eax
0x384f2eb: 741e                     je 0x384f30b
0x384f2ed: 418b4708                 mov eax, dword ptr [r15 + 8]
0x384f2f1: e9ca000000               jmp 0x384f3c0
0x384f2f6: 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x384f300: 01c1                     add ecx, eax
0x384f302: 83f901                   cmp ecx, 1
0x384f305: 0f85b5000000             jne 0x384f3c0
0x384f30b: 41837d3000               cmp dword ptr [r13 + 0x30], 0
0x384f310: 4d8d6528                 lea r12, [r13 + 0x28]
0x384f314: 7e5a                     jle 0x384f370
0x384f316: 4531ff                   xor r15d, r15d
0x384f319: eb32                     jmp 0x384f34d
0x384f31b: 0f1f440000               nop dword ptr [rax + rax]
0x384f320: 498b5610                 mov rdx, qword ptr [r14 + 0x10]
0x384f324: 89c1                     mov ecx, eax
0x384f326: 0fb7c0                   movzx eax, ax
0x384f329: 488d3c40                 lea rdi, [rax + rax*2]
0x384f32d: c1e910                   shr ecx, 0x10
0x384f330: c1e703                   shl edi, 3
0x384f333: 48033cca                 add rdi, qword ptr [rdx + rcx*8]
0x384f337: be00000040               mov esi, 0x40000000
0x384f33c: e84f92b1fc               call 0x368590
0x384f341: 49634530                 movsxd rax, dword ptr [r13 + 0x30]
0x384f345: 49ffc7                   inc r15
0x384f348: 4939c7                   cmp r15, rax
0x384f34b: 7d23                     jge 0x384f370
0x384f34d: 498b0424                 mov rax, qword ptr [r12]
0x384f351: 4a8b04f8                 mov rax, qword ptr [rax + r15*8]
0x384f355: 8b400c                   mov eax, dword ptr [rax + 0xc]
0x384f358: 41394624                 cmp dword ptr [r14 + 0x24], eax
0x384f35c: 7fc2                     jg 0x384f320
0x384f35e: 31ff                     xor edi, edi
0x384f360: ebd5                     jmp 0x384f337
0x384f362: 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384f370: 41c7453000000000         mov dword ptr [r13 + 0x30], 0
0x384f378: 4c8b7dc0                 mov r15, qword ptr [rbp - 0x40]
0x384f37c: 488b75d0                 mov rsi, qword ptr [rbp - 0x30]
0x384f380: 41837d3400               cmp dword ptr [r13 + 0x34], 0
0x384f385: 0f8415ffffff             je 0x384f2a0
0x384f38b: 4983c534                 add r13, 0x34
0x384f38f: bf08000000               mov edi, 8
0x384f394: be08000000               mov esi, 8
0x384f399: 4c89e1                   mov rcx, r12
0x384f39c: 31d2                     xor edx, edx
0x384f39e: 4531c0                   xor r8d, r8d
0x384f3a1: 4d89e9                   mov r9, r13
0x384f3a4: e8776a7dfc               call 0x25e20
0x384f3a9: 488b75d0                 mov rsi, qword ptr [rbp - 0x30]
0x384f3ad: e9eefeffff               jmp 0x384f2a0
0x384f3b2: 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384f3c0: 488d3dd901de03           lea rdi, [rip + 0x3de01d9]
0x384f3c7: 85c0                     test eax, eax
0x384f3c9: 0f84b1feffff             je 0x384f280
0x384f3cf: 498b3f                   mov rdi, qword ptr [r15]
0x384f3d2: e9a9feffff               jmp 0x384f280
0x384f3d7: 4883c418                 add rsp, 0x18
0x384f3db: 5b                       pop rbx
0x384f3dc: 415c                     pop r12
0x384f3de: 415d                     pop r13
0x384f3e0: 415e                     pop r14
0x384f3e2: 415f                     pop r15
0x384f3e4: 5d                       pop rbp
0x384f3e5: c3                       ret 
0x384f3e6: cc                       int3 
0x384f3e7: cc                       int3 
0x384f3e8: cc                       int3 
0x384f3e9: cc                       int3 
0x384f3ea: cc                       int3 
0x384f3eb: cc                       int3 
0x384f3ec: cc                       int3 
0x384f3ed: cc                       int3 
0x384f3ee: cc                       int3 
0x384f3ef: cc                       int3 
0x384f3f0: 55                       push rbp
0x384f3f1: 4889e5                   mov rbp, rsp
0x384f3f4: 53                       push rbx
0x384f3f5: 50                       push rax
0x384f3f6: c786b001000000000000     mov dword ptr [rsi + 0x1b0], 0
0x384f400: 4889f3                   mov rbx, rsi
0x384f403: 83beb401000000           cmp dword ptr [rsi + 0x1b4], 0
0x384f40a: 7422                     je 0x384f42e
0x384f40c: 4c8d8bb4010000           lea r9, [rbx + 0x1b4]
0x384f413: 488d8ba8010000           lea rcx, [rbx + 0x1a8]
0x384f41a: bf08000000               mov edi, 8
0x384f41f: be04000000               mov esi, 4
0x384f424: 31d2                     xor edx, edx
0x384f426: 4531c0                   xor r8d, r8d
0x384f429: e8f2697dfc               call 0x25e20
0x384f42e: c783c001000000000000     mov dword ptr [rbx + 0x1c0], 0
0x384f438: 83bbc401000000           cmp dword ptr [rbx + 0x1c4], 0
0x384f43f: 7422                     je 0x384f463
0x384f441: 4c8d8bc4010000           lea r9, [rbx + 0x1c4]
0x384f448: 488d8bb8010000           lea rcx, [rbx + 0x1b8]
0x384f44f: bf08000000               mov edi, 8
0x384f454: be08000000               mov esi, 8
0x384f459: 31d2                     xor edx, edx
0x384f45b: 4531c0                   xor r8d, r8d
0x384f45e: e8bd697dfc               call 0x25e20
0x384f463: 488dbbc8010000           lea rdi, [rbx + 0x1c8]
0x384f46a: 488d352f01de03           lea rsi, [rip + 0x3de012f]
0x384f471: e8da5081fc               call 0x64550
0x384f476: 80a3d8010000fe           and byte ptr [rbx + 0x1d8], 0xfe
0x384f47d: 488b8bc0020000           mov rcx, qword ptr [rbx + 0x2c0]
0x384f484: 4885c9                   test rcx, rcx
0x384f487: 7435                     je 0x384f4be
0x384f489: c7813008000000000000     mov dword ptr [rcx + 0x830], 0
0x384f493: 83b93408000000           cmp dword ptr [rcx + 0x834], 0
0x384f49a: 7422                     je 0x384f4be
0x384f49c: 4c8d8934080000           lea r9, [rcx + 0x834]
0x384f4a3: 4881c128080000           add rcx, 0x828
0x384f4aa: bf08000000               mov edi, 8
0x384f4af: be04000000               mov esi, 4
0x384f4b4: 31d2                     xor edx, edx
0x384f4b6: 4531c0                   xor r8d, r8d
0x384f4b9: e862697dfc               call 0x25e20
0x384f4be: 4883c408                 add rsp, 8
0x384f4c2: 5b                       pop rbx
0x384f4c3: 5d                       pop rbp
0x384f4c4: c3                       ret 
0x384f4c5: cc                       int3 
0x384f4c6: cc                       int3 
0x384f4c7: cc                       int3 
0x384f4c8: cc                       int3 
0x384f4c9: cc                       int3 
0x384f4ca: cc                       int3 
0x384f4cb: cc                       int3 
0x384f4cc: cc                       int3 
0x384f4cd: cc                       int3 
0x384f4ce: cc                       int3 
0x384f4cf: cc                       int3 
0x384f4d0: 55                       push rbp
0x384f4d1: 4889e5                   mov rbp, rsp
0x384f4d4: 4157                     push r15
0x384f4d6: 4156                     push r14
0x384f4d8: 4155                     push r13
0x384f4da: 4154                     push r12
0x384f4dc: 53                       push rbx
0x384f4dd: 50                       push rax
0x384f4de: 4c8bb7100c0000           mov r14, qword ptr [rdi + 0xc10]
0x384f4e5: 48897dd0                 mov qword ptr [rbp - 0x30], rdi
0x384f4e9: 418b4e50                 mov ecx, dword ptr [r14 + 0x50]
0x384f4ed: 85c9                     test ecx, ecx
0x384f4ef: 0f8e25010000             jle 0x384f61a
0x384f4f5: 4d8d6e50                 lea r13, [r14 + 0x50]
0x384f4f9: 4531e4                   xor r12d, r12d
0x384f4fc: eb57                     jmp 0x384f555
0x384f4fe: 6690                     nop 
0x384f500: 4863d0                   movsxd rdx, eax
0x384f503: 488d7720                 lea rsi, [rdi + 0x20]
0x384f507: 48c1e205                 shl rdx, 5
0x384f50b: e88029c203               call 0x7471e90
0x384f510: 418b4d00                 mov ecx, dword ptr [r13]
0x384f514: 4983c654                 add r14, 0x54
0x384f518: ffc9                     dec ecx
0x384f51a: bf20000000               mov edi, 0x20
0x384f51f: be08000000               mov esi, 8
0x384f524: 4c89fa                   mov rdx, r15
0x384f527: 4d89f0                   mov r8, r14
0x384f52a: 41894d00                 mov dword ptr [r13], ecx
0x384f52e: e80d7c7dfc               call 0x27140
0x384f533: 41ffcc                   dec r12d
0x384f536: 488b45d0                 mov rax, qword ptr [rbp - 0x30]
0x384f53a: 41ffc4                   inc r12d
0x384f53d: 4c8bb0100c0000           mov r14, qword ptr [rax + 0xc10]
0x384f544: 418b4e50                 mov ecx, dword ptr [r14 + 0x50]
0x384f548: 4d8d6e50                 lea r13, [r14 + 0x50]
0x384f54c: 4139cc                   cmp r12d, ecx
0x384f54f: 0f8dc5000000             jge 0x384f61a
0x384f555: 498b7e48                 mov rdi, qword ptr [r14 + 0x48]
0x384f559: 4963dc                   movsxd rbx, r12d
0x384f55c: 48c1e305                 shl rbx, 5
0x384f560: 4c8b3c1f                 mov r15, qword ptr [rdi + rbx]
0x384f564: 4d85ff                   test r15, r15
0x384f567: 7520                     jne 0x384f589
0x384f569: 418d542401               lea edx, [r12 + 1]
0x384f56e: 89c8                     mov eax, ecx
0x384f570: 4d8d7e48                 lea r15, [r14 + 0x48]
0x384f574: 29d0                     sub eax, edx
0x384f576: 749c                     je 0x384f514
0x384f578: 4801df                   add rdi, rbx
0x384f57b: eb83                     jmp 0x384f500
0x384f57d: 0f1f00                   nop dword ptr [rax]
0x384f580: 4d8b7f20                 mov r15, qword ptr [r15 + 0x20]
0x384f584: 4d85ff                   test r15, r15
0x384f587: 74ad                     je 0x384f536
0x384f589: e8627092ff               call 0x31765f0
0x384f58e: 498b4f10                 mov rcx, qword ptr [r15 + 0x10]
0x384f592: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x384f596: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x384f599: 7f0e                     jg 0x384f5a9
0x384f59b: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x384f59f: 4883c030                 add rax, 0x30
0x384f5a3: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x384f5a7: 7440                     je 0x384f5e9
0x384f5a9: e83296f3ff               call 0x3788be0
0x384f5ae: 498b4f10                 mov rcx, qword ptr [r15 + 0x10]
0x384f5b2: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x384f5b6: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x384f5b9: 7f0e                     jg 0x384f5c9
0x384f5bb: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x384f5bf: 4883c030                 add rax, 0x30
0x384f5c3: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x384f5c7: 7420                     je 0x384f5e9
0x384f5c9: e8b29b2dff               call 0x2b29180
0x384f5ce: 498b4f10                 mov rcx, qword ptr [r15 + 0x10]
0x384f5d2: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x384f5d6: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x384f5d9: 7fa5                     jg 0x384f580
0x384f5db: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x384f5df: 4883c030                 add rax, 0x30
0x384f5e3: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x384f5e7: 7597                     jne 0x384f580
0x384f5e9: 488b45d0                 mov rax, qword ptr [rbp - 0x30]
0x384f5ed: 418d542401               lea edx, [r12 + 1]
0x384f5f2: 4c8bb0100c0000           mov r14, qword ptr [rax + 0xc10]
0x384f5f9: 418b4e50                 mov ecx, dword ptr [r14 + 0x50]
0x384f5fd: 4d8d7e48                 lea r15, [r14 + 0x48]
0x384f601: 4d8d6e50                 lea r13, [r14 + 0x50]
0x384f605: 89c8                     mov eax, ecx
0x384f607: 29d0                     sub eax, edx
0x384f609: 0f8405ffffff             je 0x384f514
0x384f60f: 49031f                   add rbx, qword ptr [r15]
0x384f612: 4889df                   mov rdi, rbx
0x384f615: e9e6feffff               jmp 0x384f500
0x384f61a: 4883c408                 add rsp, 8
0x384f61e: 5b                       pop rbx
0x384f61f: 415c                     pop r12
0x384f621: 415d                     pop r13
0x384f623: 415e                     pop r14
0x384f625: 415f                     pop r15
0x384f627: 5d                       pop rbp
0x384f628: c3                       ret 
0x384f629: cc                       int3 
0x384f62a: cc                       int3 
0x384f62b: cc                       int3 
0x384f62c: cc                       int3 
0x384f62d: cc                       int3 
0x384f62e: cc                       int3 
0x384f62f: cc                       int3 
0x384f630: 55                       push rbp
0x384f631: 4889e5                   mov rbp, rsp
0x384f634: 4157                     push r15
0x384f636: 4156                     push r14
0x384f638: 4155                     push r13
0x384f63a: 4154                     push r12
0x384f63c: 53                       push rbx
0x384f63d: 4883ec48                 sub rsp, 0x48
0x384f641: 4c8b25c0288e05           mov r12, qword ptr [rip + 0x58e28c0]
0x384f648: 498b0424                 mov rax, qword ptr [r12]
0x384f64c: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384f650: 488b05c142d005           mov rax, qword ptr [rip + 0x5d042c1]
0x384f657: 4885c0                   test rax, rax
0x384f65a: 7407                     je 0x384f663
0x384f65c: c6800d0f000001           mov byte ptr [rax + 0xf0d], 1
0x384f663: be01000000               mov esi, 1
0x384f668: 31ff                     xor edi, edi
0x384f66a: e8f1d4b1fc               call 0x36cb60
0x384f66f: 488b05a242d005           mov rax, qword ptr [rip + 0x5d042a2]
0x384f676: 4885c0                   test rax, rax
0x384f679: 7407                     je 0x384f682
0x384f67b: c6800d0f000000           mov byte ptr [rax + 0xf0d], 0
0x384f682: e899ce86fc               call 0xbc520
0x384f687: 84c0                     test al, al
0x384f689: 7455                     je 0x384f6e0
0x384f68b: e880b81bfd               call 0xa0af10
0x384f690: 488d1d61e2b905           lea rbx, [rip + 0x5b9e261]
0x384f697: be02000000               mov esi, 2
0x384f69c: 31d2                     xor edx, edx
0x384f69e: 4883c338                 add rbx, 0x38
0x384f6a2: 4889df                   mov rdi, rbx
0x384f6a5: e8d67511fd               call 0x966c80
0x384f6aa: 4889df                   mov rdi, rbx
0x384f6ad: be02000000               mov esi, 2
0x384f6b2: 31d2                     xor edx, edx
0x384f6b4: e8c77511fd               call 0x966c80
0x384f6b9: 488d0588c8b905           lea rax, [rip + 0x5b9c888]
0x384f6c0: 488b38                   mov rdi, qword ptr [rax]
0x384f6c3: 488b07                   mov rax, qword ptr [rdi]
0x384f6c6: ff9000030000             call qword ptr [rax + 0x300]
0x384f6cc: 4889df                   mov rdi, rbx
0x384f6cf: be03000000               mov esi, 3
0x384f6d4: 31d2                     xor edx, edx
0x384f6d6: e8a57511fd               call 0x966c80
0x384f6db: e989030000               jmp 0x384fa69
0x384f6e0: 488d05f10ebb05           lea rax, [rip + 0x5bb0ef1]
0x384f6e7: 803800                   cmp byte ptr [rax], 0
0x384f6ea: 750d                     jne 0x384f6f9
0x384f6ec: e89f9986fc               call 0xb9090
0x384f6f1: 84c0                     test al, al
0x384f6f3: 0f859c000000             jne 0x384f795
0x384f6f9: 488d05100fbb05           lea rax, [rip + 0x5bb0f10]
0x384f700: 833800                   cmp dword ptr [rax], 0
0x384f703: 746c                     je 0x384f771
0x384f705: 488d3d8410bb05           lea rdi, [rip + 0x5bb1084]
0x384f70c: 488d0585337004           lea rax, [rip + 0x4703385]
0x384f713: 488d1526bf0000           lea rdx, [rip + 0xbf26]
0x384f71a: 488d0d2f8b1afd           lea rcx, [rip - 0x2e574d1]
0x384f721: 48c745c800000000         mov qword ptr [rbp - 0x38], 0
0x384f729: 488d35f6160404           lea rsi, [rip + 0x40416f6] ; ['FlushCommand']
0x384f730: 488945b0                 mov qword ptr [rbp - 0x50], rax
0x384f734: 488955b8                 mov qword ptr [rbp - 0x48], rdx
0x384f738: 48894da0                 mov qword ptr [rbp - 0x60], rcx
0x384f73c: 488d156149d005           lea rdx, [rip + 0x5d04961]
0x384f743: 488d4da0                 lea rcx, [rbp - 0x60]
0x384f747: e814ba1bfd               call 0xa0b160
0x384f74c: 48837da000               cmp qword ptr [rbp - 0x60], 0
0x384f751: 0f8412030000             je 0x384fa69
0x384f757: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x384f75b: 488d7db0                 lea rdi, [rbp - 0x50]
0x384f75f: 4885c0                   test rax, rax
0x384f762: 480f45f8                 cmovne rdi, rax
0x384f766: 488b07                   mov rax, qword ptr [rdi]
0x384f769: ff5010                   call qword ptr [rax + 0x10]
0x384f76c: e9f8020000               jmp 0x384fa69
0x384f771: 66666664488b042500000000 mov rax, qword ptr fs:[0]
0x384f77d: 4989c6                   mov r14, rax
0x384f780: 4c8db878eeffff           lea r15, [rax - 0x1188]
0x384f787: 488b8078eeffff           mov rax, qword ptr [rax - 0x1188]
0x384f78e: 4885c0                   test rax, rax
0x384f791: 756e                     jne 0x384f801
0x384f793: eb0f                     jmp 0x384f7a4
0x384f795: e876b71bfd               call 0xa0af10
0x384f79a: e8a1be0000               call 0x385b640
0x384f79f: e9c5020000               jmp 0x384fa69
0x384f7a4: 4180be88eeffff00         cmp byte ptr [r14 - 0x1178], 0
0x384f7ac: 0f846a030000             je 0x384fb1c
0x384f7b2: 4c89f0                   mov rax, r14
0x384f7b5: 4889c1                   mov rcx, rax
0x384f7b8: 488b8080eeffff           mov rax, qword ptr [rax - 0x1180]
0x384f7bf: 48c78180eeffff00000000   mov qword ptr [rcx - 0x1180], 0
0x384f7ca: 4885c0                   test rax, rax
0x384f7cd: 750f                     jne 0x384f7de
0x384f7cf: bf00000100               mov edi, 0x10000
0x384f7d4: be00000100               mov esi, 0x10000
0x384f7d9: e8428286fc               call 0xb7a20
0x384f7de: 4c89ff                   mov rdi, r15
0x384f7e1: 488d4858                 lea rcx, [rax + 0x58]
0x384f7e5: c700ffffffff             mov dword ptr [rax], 0xffffffff
0x384f7eb: 48894840                 mov qword ptr [rax + 0x40], rcx
0x384f7ef: c7404800000000           mov dword ptr [rax + 0x48], 0
0x384f7f6: 498907                   mov qword ptr [r15], rax
0x384f7f9: e882d77dfc               call 0x2cf80
0x384f7fe: 498b07                   mov rax, qword ptr [r15]
0x384f801: 488b5840                 mov rbx, qword ptr [rax + 0x40]
0x384f805: 8b4848                   mov ecx, dword ptr [rax + 0x48]
0x384f808: 488db000000100           lea rsi, [rax + 0x10000]
0x384f80f: 4883c307                 add rbx, 7
0x384f813: 4883e3f8                 and rbx, 0xfffffffffffffff8
0x384f817: 488d93b0000000           lea rdx, [rbx + 0xb0]
0x384f81e: 4839f2                   cmp rdx, rsi
0x384f821: 0f86ee020000             jbe 0x384fb15
0x384f827: 48897040                 mov qword ptr [rax + 0x40], rsi
0x384f82b: f7d1                     not ecx
0x384f82d: f02908                   lock sub dword ptr [rax], ecx
0x384f830: 0f856effffff             jne 0x384f7a4
0x384f836: 498b07                   mov rax, qword ptr [r15]
0x384f839: 488d585f                 lea rbx, [rax + 0x5f]
0x384f83d: 488d4858                 lea rcx, [rax + 0x58]
0x384f841: c700ffffffff             mov dword ptr [rax], 0xffffffff
0x384f847: 488db000000100           lea rsi, [rax + 0x10000]
0x384f84e: 4883e3f8                 and rbx, 0xfffffffffffffff8
0x384f852: 48894840                 mov qword ptr [rax + 0x40], rcx
0x384f856: b901000000               mov ecx, 1
0x384f85b: c7404800000000           mov dword ptr [rax + 0x48], 0
0x384f862: 488d93b0000000           lea rdx, [rbx + 0xb0]
0x384f869: 4839f2                   cmp rdx, rsi
0x384f86c: 7642                     jbe 0x384f8b0
0x384f86e: 6690                     nop 
0x384f870: 48897040                 mov qword ptr [rax + 0x40], rsi
0x384f874: f0ff00                   lock inc dword ptr [rax]
0x384f877: 0f8527ffffff             jne 0x384f7a4
0x384f87d: 498b07                   mov rax, qword ptr [r15]
0x384f880: 488d585f                 lea rbx, [rax + 0x5f]
0x384f884: 488d5058                 lea rdx, [rax + 0x58]
0x384f888: c700ffffffff             mov dword ptr [rax], 0xffffffff
0x384f88e: 488db000000100           lea rsi, [rax + 0x10000]
0x384f895: 4883e3f8                 and rbx, 0xfffffffffffffff8
0x384f899: 48895040                 mov qword ptr [rax + 0x40], rdx
0x384f89d: c7404800000000           mov dword ptr [rax + 0x48], 0
0x384f8a4: 488d93b0000000           lea rdx, [rbx + 0xb0]
0x384f8ab: 4839f2                   cmp rdx, rsi
0x384f8ae: 77c0                     ja 0x384f870
0x384f8b0: 48895040                 mov qword ptr [rax + 0x40], rdx
0x384f8b4: 894848                   mov dword ptr [rax + 0x48], ecx
0x384f8b7: 48b90100000001000000     movabs rcx, 0x100000001
0x384f8c1: 48be0000000001000000     movabs rsi, 0x100000000
0x384f8cb: 488d15a6e16604           lea rdx, [rip + 0x466e1a6]
0x384f8d2: 4c8d7370                 lea r14, [rbx + 0x70]
0x384f8d6: 48894b08                 mov qword ptr [rbx + 8], rcx
0x384f8da: 488d0d87d86604           lea rcx, [rip + 0x466d887]
0x384f8e1: 48c7431000000000         mov qword ptr [rbx + 0x10], 0
0x384f8e9: c7431800000000           mov dword ptr [rbx + 0x18], 0
0x384f8f0: c74320ffffffff           mov dword ptr [rbx + 0x20], 0xffffffff
0x384f8f7: 48c7433000000000         mov qword ptr [rbx + 0x30], 0
0x384f8ff: 48897338                 mov qword ptr [rbx + 0x38], rsi
0x384f903: c6434001                 mov byte ptr [rbx + 0x40], 1
0x384f907: 48894b70                 mov qword ptr [rbx + 0x70], rcx
0x384f90b: 48b920000000000000e8     movabs rcx, 0xe800000000000020
0x384f915: 48c7437800000000         mov qword ptr [rbx + 0x78], 0
0x384f91d: 48898b80000000           mov qword ptr [rbx + 0x80], rcx
0x384f924: 48c7839000000000000000   mov qword ptr [rbx + 0x90], 0
0x384f92f: 4889b398000000           mov qword ptr [rbx + 0x98], rsi
0x384f936: 66c783a00000000000       mov word ptr [rbx + 0xa0], 0
0x384f93f: 488913                   mov qword ptr [rbx], rdx
0x384f942: b1ff                     mov cl, 0xff
0x384f944: f00fc04b40               lock xadd byte ptr [rbx + 0x40], cl
0x384f949: f6c102                   test cl, 2
0x384f94c: 0f85f1010000             jne 0x384fb43
0x384f952: 488d0d8bc7a805           lea rcx, [rip + 0x5a8c78b]
0x384f959: 488d05706bdc04           lea rax, [rip + 0x4dc6b70]
0x384f960: 488d75a0                 lea rsi, [rbp - 0x60]
0x384f964: 488d559f                 lea rdx, [rbp - 0x61]
0x384f968: 488903                   mov qword ptr [rbx], rax
0x384f96b: 8b39                     mov edi, dword ptr [rcx]
0x384f96d: e8ee709efc               call 0x236a60
0x384f972: 488b4370                 mov rax, qword ptr [rbx + 0x70]
0x384f976: 440fb66da0               movzx r13d, byte ptr [rbp - 0x60]
0x384f97b: 440fb67d9f               movzx r15d, byte ptr [rbp - 0x61]
0x384f980: 488d7348                 lea rsi, [rbx + 0x48]
0x384f984: 4c89f7                   mov rdi, r14
0x384f987: ff5018                   call qword ptr [rax + 0x18]
0x384f98a: 48833d96258e0500         cmp qword ptr [rip + 0x58e2596], 0
0x384f992: 488d052fe16604           lea rax, [rip + 0x466e12f]
0x384f999: 48894370                 mov qword ptr [rbx + 0x70], rax
0x384f99d: 48895b48                 mov qword ptr [rbx + 0x48], rbx
0x384f9a1: 48895b50                 mov qword ptr [rbx + 0x50], rbx
0x384f9a5: 7405                     je 0x384f9ac
0x384f9a7: e854067bfc               call 0
0x384f9ac: 64488b042500000000       mov rax, qword ptr fs:[0]
0x384f9b5: 488d8068eeffff           lea rax, [rax - 0x1198]
0x384f9bc: 488b00                   mov rax, qword ptr [rax]
0x384f9bf: 4885c0                   test rax, rax
0x384f9c2: 7414                     je 0x384f9d8
0x384f9c4: 4180fd06                 cmp r13b, 6
0x384f9c8: 7508                     jne 0x384f9d2
0x384f9ca: 4c8b6838                 mov r13, qword ptr [rax + 0x38]
0x384f9ce: 49c1ed3b                 shr r13, 0x3b
0x384f9d2: 488b4030                 mov rax, qword ptr [rax + 0x30]
0x384f9d6: eb14                     jmp 0x384f9ec
0x384f9d8: 4180fd06                 cmp r13b, 6
0x384f9dc: 410fb6c5                 movzx eax, r13b
0x384f9e0: 41bd01000000             mov r13d, 1
0x384f9e6: 440f45e8                 cmovne r13d, eax
0x384f9ea: 31c0                     xor eax, eax
0x384f9ec: 488d15bf861404           lea rdx, [rip + 0x41486bf] ; ['GraphTask']
0x384f9f3: 48894378                 mov qword ptr [rbx + 0x78], rax
0x384f9f7: 410fb6c5                 movzx eax, r13b
0x384f9fb: 48b980ffffffffffff07     movabs rcx, 0x7ffffffffffff80
0x384fa05: 48c1e206                 shl rdx, 6
0x384fa09: 48c1e03b                 shl rax, 0x3b
0x384fa0d: 4821d1                   and rcx, rdx
0x384fa10: 48ba00000000000000c0     movabs rdx, 0xc000000000000000
0x384fa1a: 4809c8                   or rax, rcx
0x384fa1d: 4809c2                   or rdx, rax
0x384fa20: 48899380000000           mov qword ptr [rbx + 0x80], rdx
0x384fa27: 44887b1c                 mov byte ptr [rbx + 0x1c], r15b
0x384fa2b: 4885db                   test rbx, rbx
0x384fa2e: 7429                     je 0x384fa59
0x384fa30: f0ff4308                 lock inc dword ptr [rbx + 8]
0x384fa34: 488d75a0                 lea rsi, [rbp - 0x60]
0x384fa38: 4889df                   mov rdi, rbx
0x384fa3b: 4c8d7308                 lea r14, [rbx + 8]
0x384fa3f: c645a001                 mov byte ptr [rbp - 0x60], 1
0x384fa43: e8e8977efc               call 0x39230
0x384fa48: f041ff0e                 lock dec dword ptr [r14]
0x384fa4c: 751b                     jne 0x384fa69
0x384fa4e: 488b03                   mov rax, qword ptr [rbx]
0x384fa51: 4889df                   mov rdi, rbx
0x384fa54: ff5008                   call qword ptr [rax + 8]
0x384fa57: eb10                     jmp 0x384fa69
0x384fa59: 488d75a0                 lea rsi, [rbp - 0x60]
0x384fa5d: 4889df                   mov rdi, rbx
0x384fa60: c645a001                 mov byte ptr [rbp - 0x60], 1
0x384fa64: e8c7977efc               call 0x39230
0x384fa69: e8f2721bfd               call 0xa06d60
0x384fa6e: e82dd995fc               call 0x1ad3a0
0x384fa73: 8b4814                   mov ecx, dword ptr [rax + 0x14]
0x384fa76: 4889c3                   mov rbx, rax
0x384fa79: 8d4101                   lea eax, [rcx + 1]
0x384fa7c: 894314                   mov dword ptr [rbx + 0x14], eax
0x384fa7f: 48634308                 movsxd rax, dword ptr [rbx + 8]
0x384fa83: 4885c0                   test rax, rax
0x384fa86: 7e6c                     jle 0x384faf4
0x384fa88: 4531f6                   xor r14d, r14d
0x384fa8b: eb10                     jmp 0x384fa9d
0x384fa8d: 0f1f00                   nop dword ptr [rax]
0x384fa90: 498d45ff                 lea rax, [r13 - 1]
0x384fa94: 41b601                   mov r14b, 1
0x384fa97: 4983fd01                 cmp r13, 1
0x384fa9b: 7673                     jbe 0x384fb10
0x384fa9d: 4989c7                   mov r15, rax
0x384faa0: 4989c5                   mov r13, rax
0x384faa3: 49c1e704                 shl r15, 4
0x384faa7: 4983c7f8                 add r15, -8
0x384faab: 0f1f440000               nop dword ptr [rax + rax]
0x384fab0: 488b03                   mov rax, qword ptr [rbx]
0x384fab3: 42833c3800               cmp dword ptr [rax + r15], 0
0x384fab8: 74d6                     je 0x384fa90
0x384faba: 4a8b7c38f8               mov rdi, qword ptr [rax + r15 - 8]
0x384fabf: 4885ff                   test rdi, rdi
0x384fac2: 74cc                     je 0x384fa90
0x384fac4: 488b07                   mov rax, qword ptr [rdi]
0x384fac7: ff5068                   call qword ptr [rax + 0x68]
0x384faca: 84c0                     test al, al
0x384facc: 74c2                     je 0x384fa90
0x384face: 49ffcd                   dec r13
0x384fad1: 4983c7f0                 add r15, -0x10
0x384fad5: 498d4501                 lea rax, [r13 + 1]
0x384fad9: 4883f801                 cmp rax, 1
0x384fadd: 77d1                     ja 0x384fab0
0x384fadf: ff4b14                   dec dword ptr [rbx + 0x14]
0x384fae2: 41f6c601                 test r14b, 1
0x384fae6: 740f                     je 0x384faf7
0x384fae8: 4889df                   mov rdi, rbx
0x384faeb: 31f6                     xor esi, esi
0x384faed: e8fe127efc               call 0x30df0
0x384faf2: eb03                     jmp 0x384faf7
0x384faf4: 894b14                   mov dword ptr [rbx + 0x14], ecx
0x384faf7: 498b0424                 mov rax, qword ptr [r12]
0x384fafb: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x384faff: 7550                     jne 0x384fb51
0x384fb01: 4883c448                 add rsp, 0x48
0x384fb05: 5b                       pop rbx
0x384fb06: 415c                     pop r12
0x384fb08: 415d                     pop r13
0x384fb0a: 415e                     pop r14
0x384fb0c: 415f                     pop r15
0x384fb0e: 5d                       pop rbp
0x384fb0f: c3                       ret 
0x384fb10: ff4b14                   dec dword ptr [rbx + 0x14]
0x384fb13: ebd3                     jmp 0x384fae8
0x384fb15: ffc1                     inc ecx
0x384fb17: e994fdffff               jmp 0x384f8b0
0x384fb1c: 498db680eeffff           lea rsi, [r14 - 0x1180]
0x384fb23: 488d3d56487bfc           lea rdi, [rip - 0x384b7aa]
0x384fb2a: 488d15cfc4a805           lea rdx, [rip + 0x5a8c4cf]
0x384fb31: e8ea29c203               call 0x7472520
0x384fb36: 41c68688eeffff01         mov byte ptr [r14 - 0x1178], 1
0x384fb3e: e96ffcffff               jmp 0x384f7b2
0x384fb43: 488d7b40                 lea rdi, [rbx + 0x40]
0x384fb47: e894fc7efc               call 0x3f7e0
0x384fb4c: e901feffff               jmp 0x384f952
0x384fb51: e80a23c203               call 0x7471e60
0x384fb56: 0f0b                     ud2 
0x384fb58: cc                       int3 
0x384fb59: cc                       int3 
0x384fb5a: cc                       int3 
0x384fb5b: cc                       int3 
0x384fb5c: cc                       int3 
0x384fb5d: cc                       int3 
0x384fb5e: cc                       int3 
0x384fb5f: cc                       int3 
0x384fb60: 55                       push rbp
0x384fb61: 4889e5                   mov rbp, rsp
0x384fb64: 4157                     push r15
0x384fb66: 4156                     push r14
0x384fb68: 4155                     push r13
0x384fb6a: 4154                     push r12
0x384fb6c: 53                       push rbx
0x384fb6d: 4883ec38                 sub rsp, 0x38
0x384fb71: 488b0590238e05           mov rax, qword ptr [rip + 0x58e2390]
0x384fb78: 4989d6                   mov r14, rdx
0x384fb7b: 4889fb                   mov rbx, rdi
0x384fb7e: 4989f7                   mov r15, rsi
0x384fb81: 488b00                   mov rax, qword ptr [rax]
0x384fb84: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384fb88: 8b9720020000             mov edx, dword ptr [rdi + 0x220]
0x384fb8e: 488d7db8                 lea rdi, [rbp - 0x48]
0x384fb92: e88937f4ff               call 0x3793320
0x384fb97: 837dc000                 cmp dword ptr [rbp - 0x40], 0
0x384fb9b: 4c8975a8                 mov qword ptr [rbp - 0x58], r14
0x384fb9f: 7406                     je 0x384fba7
0x384fba1: 488b75b8                 mov rsi, qword ptr [rbp - 0x48]
0x384fba5: eb07                     jmp 0x384fbae
0x384fba7: 488d35f2f9dd03           lea rsi, [rip + 0x3ddf9f2]
0x384fbae: 4c8d65b0                 lea r12, [rbp - 0x50]
0x384fbb2: ba01000000               mov edx, 1
0x384fbb7: 4c89e7                   mov rdi, r12
0x384fbba: e8d1d99efc               call 0x23d590
0x384fbbf: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x384fbc3: 440fb633                 movzx r14d, byte ptr [rbx]
0x384fbc7: 4989fd                   mov r13, rdi
0x384fbca: 49c1ed20                 shr r13, 0x20
0x384fbce: e8fd9e9efc               call 0x239ad0
0x384fbd3: 4401e8                   add eax, r13d
0x384fbd6: 4c8d2d5b16d005           lea r13, [rip + 0x5d0165b]
0x384fbdd: 418b4d08                 mov ecx, dword ptr [r13 + 8]
0x384fbe1: 413b4d34                 cmp ecx, dword ptr [r13 + 0x34]
0x384fbe5: 744f                     je 0x384fc36
0x384fbe7: 498b4d40                 mov rcx, qword ptr [r13 + 0x40]
0x384fbeb: 498d5538                 lea rdx, [r13 + 0x38]
0x384fbef: 4885c9                   test rcx, rcx
0x384fbf2: 480f45d1                 cmovne rdx, rcx
0x384fbf6: 418b4d48                 mov ecx, dword ptr [r13 + 0x48]
0x384fbfa: ffc9                     dec ecx
0x384fbfc: 21c1                     and ecx, eax
0x384fbfe: 8b148a                   mov edx, dword ptr [rdx + rcx*4]
0x384fc01: 83faff                   cmp edx, -1
0x384fc04: 7430                     je 0x384fc36
0x384fc06: 498b4d00                 mov rcx, qword ptr [r13]
0x384fc0a: 488b75b0                 mov rsi, qword ptr [rbp - 0x50]
0x384fc0e: 6690                     nop 
0x384fc10: 4863d2                   movsxd rdx, edx
0x384fc13: 488d3c92                 lea rdi, [rdx + rdx*4]
0x384fc17: 488d14b9                 lea rdx, [rcx + rdi*4]
0x384fc1b: 483934b9                 cmp qword ptr [rcx + rdi*4], rsi
0x384fc1f: 740a                     je 0x384fc2b
0x384fc21: 8b520c                   mov edx, dword ptr [rdx + 0xc]
0x384fc24: 83faff                   cmp edx, -1
0x384fc27: 75e7                     jne 0x384fc10
0x384fc29: eb0b                     jmp 0x384fc36
0x384fc2b: 4885d2                   test rdx, rdx
0x384fc2e: 7406                     je 0x384fc36
0x384fc30: 4883c208                 add rdx, 8
0x384fc34: eb23                     jmp 0x384fc59
0x384fc36: 488d55c8                 lea rdx, [rbp - 0x38]
0x384fc3a: 4c89ef                   mov rdi, r13
0x384fc3d: 89c6                     mov esi, eax
0x384fc3f: 31c9                     xor ecx, ecx
0x384fc41: 4c8965c8                 mov qword ptr [rbp - 0x38], r12
0x384fc45: e826c8d2fd               call 0x157c470
0x384fc4a: 498b4d00                 mov rcx, qword ptr [r13]
0x384fc4e: 4898                     cdqe 
0x384fc50: 488d0480                 lea rax, [rax + rax*4]
0x384fc54: 488d548108               lea rdx, [rcx + rax*4 + 8]
0x384fc59: 448932                   mov dword ptr [rdx], r14d
0x384fc5c: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x384fc60: e8fb4cc1fc               call 0x464960
0x384fc65: 837dc000                 cmp dword ptr [rbp - 0x40], 0
0x384fc69: 7406                     je 0x384fc71
0x384fc6b: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x384fc6f: eb07                     jmp 0x384fc78
0x384fc71: 488d3d28f9dd03           lea rdi, [rip + 0x3ddf928]
0x384fc78: 4c8b75a8                 mov r14, qword ptr [rbp - 0x58]
0x384fc7c: e8ff59c3fc               call 0x485680
0x384fc81: 31d2                     xor edx, edx
0x384fc83: 4885c0                   test rax, rax
0x384fc86: 740e                     je 0x384fc96
0x384fc88: 803b03                   cmp byte ptr [rbx], 3
0x384fc8b: 7509                     jne 0x384fc96
0x384fc8d: 80482e10                 or byte ptr [rax + 0x2e], 0x10
0x384fc91: ba00000800               mov edx, 0x80000
0x384fc96: 41837f0800               cmp dword ptr [r15 + 8], 0
0x384fc9b: 7405                     je 0x384fca2
0x384fc9d: 498b37                   mov rsi, qword ptr [r15]
0x384fca0: eb07                     jmp 0x384fca9
0x384fca2: 488d35f7f8dd03           lea rsi, [rip + 0x3ddf8f7]
0x384fca9: 4889c7                   mov rdi, rax
0x384fcac: 4531ff                   xor r15d, r15d
0x384fcaf: 31c9                     xor ecx, ecx
0x384fcb1: 4531c0                   xor r8d, r8d
0x384fcb4: e887c8c3fc               call 0x48c540
0x384fcb9: 498906                   mov qword ptr [r14], rax
0x384fcbc: 488d3d7515d005           lea rdi, [rip + 0x5d01575]
0x384fcc3: 488b75b0                 mov rsi, qword ptr [rbp - 0x50]
0x384fcc7: e8e4977ffd               call 0x10494b0
0x384fccc: 498b3e                   mov rdi, qword ptr [r14]
0x384fccf: 4885ff                   test rdi, rdi
0x384fcd2: 0f8492000000             je 0x384fd6a
0x384fcd8: e803e3f4ff               call 0x379dfe0
0x384fcdd: 498b3e                   mov rdi, qword ptr [r14]
0x384fce0: 4885c0                   test rax, rax
0x384fce3: 7415                     je 0x384fcfa
0x384fce5: 4989c7                   mov r15, rax
0x384fce8: 804f2e10                 or byte ptr [rdi + 0x2e], 0x10
0x384fcec: 4d63b798000000           movsxd r14, dword ptr [r15 + 0x98]
0x384fcf3: 4d85f6                   test r14, r14
0x384fcf6: 7547                     jne 0x384fd3f
0x384fcf8: eb70                     jmp 0x384fd6a
0x384fcfa: 31f6                     xor esi, esi
0x384fcfc: e84fe3f4ff               call 0x379e050
0x384fd01: 4885c0                   test rax, rax
0x384fd04: 7423                     je 0x384fd29
0x384fd06: 4889c7                   mov rdi, rax
0x384fd09: 4989c7                   mov r15, rax
0x384fd0c: e83f64c3fc               call 0x486150
0x384fd11: 4889c7                   mov rdi, rax
0x384fd14: 498906                   mov qword ptr [r14], rax
0x384fd17: 804f2e10                 or byte ptr [rdi + 0x2e], 0x10
0x384fd1b: 4d63b798000000           movsxd r14, dword ptr [r15 + 0x98]
0x384fd22: 4d85f6                   test r14, r14
0x384fd25: 7518                     jne 0x384fd3f
0x384fd27: eb41                     jmp 0x384fd6a
0x384fd29: 498b3e                   mov rdi, qword ptr [r14]
0x384fd2c: 4531ff                   xor r15d, r15d
0x384fd2f: 804f2e10                 or byte ptr [rdi + 0x2e], 0x10
0x384fd33: 4d63b798000000           movsxd r14, dword ptr [r15 + 0x98]
0x384fd3a: 4d85f6                   test r14, r14
0x384fd3d: 742b                     je 0x384fd6a
0x384fd3f: 4d8ba790000000           mov r12, qword ptr [r15 + 0x90]
0x384fd46: 49c1e603                 shl r14, 3
0x384fd4a: 4531ed                   xor r13d, r13d
0x384fd4d: 0f1f00                   nop dword ptr [rax]
0x384fd50: 4b8b3c2c                 mov rdi, qword ptr [r12 + r13]
0x384fd54: 8bb320020000             mov esi, dword ptr [rbx + 0x220]
0x384fd5a: 31d2                     xor edx, edx
0x384fd5c: e8afb793ff               call 0x318b510
0x384fd61: 4983c508                 add r13, 8
0x384fd65: 4d39ee                   cmp r14, r13
0x384fd68: 75e6                     jne 0x384fd50
0x384fd6a: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x384fd6e: 4885ff                   test rdi, rdi
0x384fd71: 7405                     je 0x384fd78
0x384fd73: e8187d86fc               call 0xb7a90
0x384fd78: 488b0589218e05           mov rax, qword ptr [rip + 0x58e2189]
0x384fd7f: 488b00                   mov rax, qword ptr [rax]
0x384fd82: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x384fd86: 7512                     jne 0x384fd9a
0x384fd88: 4c89f8                   mov rax, r15
0x384fd8b: 4883c438                 add rsp, 0x38
0x384fd8f: 5b                       pop rbx
0x384fd90: 415c                     pop r12
0x384fd92: 415d                     pop r13
0x384fd94: 415e                     pop r14
0x384fd96: 415f                     pop r15
0x384fd98: 5d                       pop rbp
0x384fd99: c3                       ret 
0x384fd9a: e8c120c203               call 0x7471e60
0x384fd9f: 0f0b                     ud2 
0x384fda1: cc                       int3 
0x384fda2: cc                       int3 
0x384fda3: cc                       int3 
0x384fda4: cc                       int3 
0x384fda5: cc                       int3 
0x384fda6: cc                       int3 
0x384fda7: cc                       int3 
0x384fda8: cc                       int3 
0x384fda9: cc                       int3 
0x384fdaa: cc                       int3 
0x384fdab: cc                       int3 
0x384fdac: cc                       int3 
0x384fdad: cc                       int3 
0x384fdae: cc                       int3 
0x384fdaf: cc                       int3 
0x384fdb0: 55                       push rbp
0x384fdb1: 4889e5                   mov rbp, rsp
0x384fdb4: 4157                     push r15
0x384fdb6: 4156                     push r14
0x384fdb8: 4155                     push r13
0x384fdba: 4154                     push r12
0x384fdbc: 53                       push rbx
0x384fdbd: 4881ec98000000           sub rsp, 0x98
0x384fdc4: 488b053d218e05           mov rax, qword ptr [rip + 0x58e213d]
0x384fdcb: 89957cffffff             mov dword ptr [rbp - 0x84], edx
0x384fdd1: 4889cb                   mov rbx, rcx
0x384fdd4: 488b00                   mov rax, qword ptr [rax]
0x384fdd7: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384fddb: 48638750110000           movsxd rax, dword ptr [rdi + 0x1150]
0x384fde2: 4885c0                   test rax, rax
0x384fde5: 7441                     je 0x384fe28
0x384fde7: 488b8f48110000           mov rcx, qword ptr [rdi + 0x1148]
0x384fdee: 48c1e003                 shl rax, 3
0x384fdf2: 31d2                     xor edx, edx
0x384fdf4: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384fe00: 4c8b3411                 mov r14, qword ptr [rcx + rdx]
0x384fe04: 4939b6c0020000           cmp qword ptr [r14 + 0x2c0], rsi
0x384fe0b: 740b                     je 0x384fe18
0x384fe0d: 4883c208                 add rdx, 8
0x384fe11: 4839d0                   cmp rax, rdx
0x384fe14: 75ea                     jne 0x384fe00
0x384fe16: eb10                     jmp 0x384fe28
0x384fe18: 4d85f6                   test r14, r14
0x384fe1b: 740b                     je 0x384fe28
0x384fe1d: 837b0800                 cmp dword ptr [rbx + 8], 0
0x384fe21: 741c                     je 0x384fe3f
0x384fe23: 488b3b                   mov rdi, qword ptr [rbx]
0x384fe26: eb1e                     jmp 0x384fe46
0x384fe28: 488b3de93ad005           mov rdi, qword ptr [rip + 0x5d03ae9]
0x384fe2f: 31f6                     xor esi, esi
0x384fe31: e86a080000               call 0x38506a0
0x384fe36: 4989c6                   mov r14, rax
0x384fe39: 837b0800                 cmp dword ptr [rbx + 8], 0
0x384fe3d: 75e4                     jne 0x384fe23
0x384fe3f: 488d3d5af7dd03           lea rdi, [rip + 0x3ddf75a]
0x384fe46: 488d352710dd03           lea rsi, [rip + 0x3dd1027] ; ['___TAILONLY___']
0x384fe4d: 48895d98                 mov qword ptr [rbp - 0x68], rbx
0x384fe51: e84ace83fc               call 0x8cca0
0x384fe56: 49638ea0010000           movsxd rcx, dword ptr [r14 + 0x1a0]
0x384fe5d: 85c0                     test eax, eax
0x384fe5f: 740c                     je 0x384fe6d
0x384fe61: 85c9                     test ecx, ecx
0x384fe63: 0f8e2d060000             jle 0x3850496
0x384fe69: 31c9                     xor ecx, ecx
0x384fe6b: eb03                     jmp 0x384fe70
0x384fe6d: 48ffc9                   dec rcx
0x384fe70: 4c8d65b0                 lea r12, [rbp - 0x50]
0x384fe74: 4c8d6dc0                 lea r13, [rbp - 0x40]
0x384fe78: 4c89b560ffffff           mov qword ptr [rbp - 0xa0], r14
0x384fe7f: eb40                     jmp 0x384fec1
0x384fe81: 488d35ec0fdd03           lea rsi, [rip + 0x3dd0fec] ; ['___TAILONLY___']
0x384fe88: e813ce83fc               call 0x8cca0
0x384fe8d: 85c0                     test eax, eax
0x384fe8f: 0f84db000000             je 0x384ff70
0x384fe95: 66662e0f1f840000000000   nop word ptr cs:[rax + rax]
0x384fea0: 4c8bb560ffffff           mov r14, qword ptr [rbp - 0xa0]
0x384fea7: 488b8d58ffffff           mov rcx, qword ptr [rbp - 0xa8]
0x384feae: 496386a0010000           movsxd rax, dword ptr [r14 + 0x1a0]
0x384feb5: 48ffc1                   inc rcx
0x384feb8: 4839c1                   cmp rcx, rax
0x384febb: 0f8dd5050000             jge 0x3850496
0x384fec1: 4d8bb698010000           mov r14, qword ptr [r14 + 0x198]
0x384fec8: 486bc138                 imul rax, rcx, 0x38
0x384fecc: 48898d58ffffff           mov qword ptr [rbp - 0xa8], rcx
0x384fed3: 410fb60c06               movzx ecx, byte ptr [r14 + rax]
0x384fed8: 3b8d7cffffff             cmp ecx, dword ptr [rbp - 0x84]
0x384fede: 75c0                     jne 0x384fea0
0x384fee0: 4901c6                   add r14, rax
0x384fee3: 488b4598                 mov rax, qword ptr [rbp - 0x68]
0x384fee7: 418b4e10                 mov ecx, dword ptr [r14 + 0x10]
0x384feeb: 8b4008                   mov eax, dword ptr [rax + 8]
0x384feee: 39c1                     cmp ecx, eax
0x384fef0: 752e                     jne 0x384ff20
0x384fef2: 83f902                   cmp ecx, 2
0x384fef5: 7c79                     jl 0x384ff70
0x384fef7: 488b4598                 mov rax, qword ptr [rbp - 0x68]
0x384fefb: 498b7e08                 mov rdi, qword ptr [r14 + 8]
0x384feff: 488b30                   mov rsi, qword ptr [rax]
0x384ff02: e899cd83fc               call 0x8cca0
0x384ff07: 85c0                     test eax, eax
0x384ff09: 7465                     je 0x384ff70
0x384ff0b: 488b4598                 mov rax, qword ptr [rbp - 0x68]
0x384ff0f: 8b4008                   mov eax, dword ptr [rax + 8]
0x384ff12: eb13                     jmp 0x384ff27
0x384ff14: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384ff20: 01c1                     add ecx, eax
0x384ff22: 83f901                   cmp ecx, 1
0x384ff25: 7449                     je 0x384ff70
0x384ff27: 488d3d72f6dd03           lea rdi, [rip + 0x3ddf672]
0x384ff2e: 85c0                     test eax, eax
0x384ff30: 7407                     je 0x384ff39
0x384ff32: 488b4598                 mov rax, qword ptr [rbp - 0x68]
0x384ff36: 488b38                   mov rdi, qword ptr [rax]
0x384ff39: 488d3560f6dd03           lea rsi, [rip + 0x3ddf660]
0x384ff40: e85bcd83fc               call 0x8cca0
0x384ff45: 85c0                     test eax, eax
0x384ff47: 7427                     je 0x384ff70
0x384ff49: 488b4598                 mov rax, qword ptr [rbp - 0x68]
0x384ff4d: 488d3d4cf6dd03           lea rdi, [rip + 0x3ddf64c]
0x384ff54: 83780800                 cmp dword ptr [rax + 8], 0
0x384ff58: 0f8423ffffff             je 0x384fe81
0x384ff5e: 488b38                   mov rdi, qword ptr [rax]
0x384ff61: e91bffffff               jmp 0x384fe81
0x384ff66: 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x384ff70: 41837e2000               cmp dword ptr [r14 + 0x20], 0
0x384ff75: 0f8e25ffffff             jle 0x384fea0
0x384ff7b: 498d4628                 lea rax, [r14 + 0x28]
0x384ff7f: 498d4e34                 lea rcx, [r14 + 0x34]
0x384ff83: 4531ff                   xor r15d, r15d
0x384ff86: 4c897588                 mov qword ptr [rbp - 0x78], r14
0x384ff8a: 48898568ffffff           mov qword ptr [rbp - 0x98], rax
0x384ff91: 48898d70ffffff           mov qword ptr [rbp - 0x90], rcx
0x384ff98: eb16                     jmp 0x384ffb0
0x384ff9a: 660f1f440000             nop word ptr [rax + rax]
0x384ffa0: 49634620                 movsxd rax, dword ptr [r14 + 0x20]
0x384ffa4: 49ffc7                   inc r15
0x384ffa7: 4939c7                   cmp r15, rax
0x384ffaa: 0f8df0feffff             jge 0x384fea0
0x384ffb0: 498b7618                 mov rsi, qword ptr [r14 + 0x18]
0x384ffb4: 4a8d1cfd00000000         lea rbx, [r15*8]
0x384ffbc: 4c89e7                   mov rdi, r12
0x384ffbf: 4801de                   add rsi, rbx
0x384ffc2: e8f9f09efc               call 0x23f0c0
0x384ffc7: 4c89ef                   mov rdi, r13
0x384ffca: 4c89e6                   mov rsi, r12
0x384ffcd: 488d15c8e2d703           lea rdx, [rip + 0x3d7e2c8] ; ['_SF']
0x384ffd4: e8a77681fc               call 0x67680
0x384ffd9: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x384ffdd: 4885ff                   test rdi, rdi
0x384ffe0: 7405                     je 0x384ffe7
0x384ffe2: e8a97a86fc               call 0xb7a90
0x384ffe7: 4c89ef                   mov rdi, r13
0x384ffea: 4c89e6                   mov rsi, r12
0x384ffed: ba01000000               mov edx, 1
0x384fff2: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384fff6: c5f81145b0               vmovups xmmword ptr [rbp - 0x50], xmm0
0x384fffb: e820a3a6fc               call 0x2ba320
0x3850000: 84c0                     test al, al
0x3850002: 753f                     jne 0x3850043
0x3850004: 49035e18                 add rbx, qword ptr [r14 + 0x18]
0x3850008: 4c8d75a0                 lea r14, [rbp - 0x60]
0x385000c: 4c89f7                   mov rdi, r14
0x385000f: 4889de                   mov rsi, rbx
0x3850012: e8a9f09efc               call 0x23f0c0
0x3850017: 4c89f7                   mov rdi, r14
0x385001a: 4c89e6                   mov rsi, r12
0x385001d: ba01000000               mov edx, 1
0x3850022: e8f9a2a6fc               call 0x2ba320
0x3850027: 488b7da0                 mov rdi, qword ptr [rbp - 0x60]
0x385002b: 89c3                     mov ebx, eax
0x385002d: 4885ff                   test rdi, rdi
0x3850030: 7405                     je 0x3850037
0x3850032: e8597a86fc               call 0xb7a90
0x3850037: 4c8b7588                 mov r14, qword ptr [rbp - 0x78]
0x385003b: 84db                     test bl, bl
0x385003d: 0f84ff030000             je 0x3850442
0x3850043: 837db800                 cmp dword ptr [rbp - 0x48], 0
0x3850047: 488d3552f5dd03           lea rsi, [rip + 0x3ddf552]
0x385004e: 7404                     je 0x3850054
0x3850050: 488b75b0                 mov rsi, qword ptr [rbp - 0x50]
0x3850054: 31ff                     xor edi, edi
0x3850056: 31d2                     xor edx, edx
0x3850058: 31c9                     xor ecx, ecx
0x385005a: 4531c0                   xor r8d, r8d
0x385005d: e8dec4c3fc               call 0x48c540
0x3850062: 4889c3                   mov rbx, rax
0x3850065: 8b400c                   mov eax, dword ptr [rax + 0xc]
0x3850068: 488d15a1a0b605           lea rdx, [rip + 0x5b6a0a1]
0x385006f: bf00000000               mov edi, 0
0x3850074: 394224                   cmp dword ptr [rdx + 0x24], eax
0x3850077: 7e17                     jle 0x3850090
0x3850079: 488b5210                 mov rdx, qword ptr [rdx + 0x10]
0x385007d: 89c1                     mov ecx, eax
0x385007f: 0fb7c0                   movzx eax, ax
0x3850082: 488d3c40                 lea rdi, [rax + rax*2]
0x3850086: c1e910                   shr ecx, 0x10
0x3850089: c1e703                   shl edi, 3
0x385008c: 48033cca                 add rdi, qword ptr [rdx + rcx*8]
0x3850090: be00000040               mov esi, 0x40000000
0x3850095: e8d683b1fc               call 0x368470
0x385009a: 4885db                   test rbx, rbx
0x385009d: 7414                     je 0x38500b3
0x385009f: 488d05b6edb405           lea rax, [rip + 0x5b4edb6]
0x38500a6: 803800                   cmp byte ptr [rax], 0
0x38500a9: 7408                     je 0x38500b3
0x38500ab: 4889df                   mov rdi, rbx
0x38500ae: e80d82b1fc               call 0x3682c0
0x38500b3: 418b4630                 mov eax, dword ptr [r14 + 0x30]
0x38500b7: 413b4634                 cmp eax, dword ptr [r14 + 0x34]
0x38500bb: 7518                     jne 0x38500d5
0x38500bd: 488bb568ffffff           mov rsi, qword ptr [rbp - 0x98]
0x38500c4: 488b9570ffffff           mov rdx, qword ptr [rbp - 0x90]
0x38500cb: bf08080000               mov edi, 0x808
0x38500d0: e8cb977dfc               call 0x298a0
0x38500d5: 8d4801                   lea ecx, [rax + 1]
0x38500d8: 4898                     cdqe 
0x38500da: 41894e30                 mov dword ptr [r14 + 0x30], ecx
0x38500de: 498b4e28                 mov rcx, qword ptr [r14 + 0x28]
0x38500e2: 48891cc1                 mov qword ptr [rcx + rax*8], rbx
0x38500e6: 4885db                   test rbx, rbx
0x38500e9: 7414                     je 0x38500ff
0x38500eb: 488d056aedb405           lea rax, [rip + 0x5b4ed6a]
0x38500f2: 803800                   cmp byte ptr [rax], 0
0x38500f5: 7408                     je 0x38500ff
0x38500f7: 4889df                   mov rdi, rbx
0x38500fa: e8c181b1fc               call 0x3682c0
0x38500ff: 48899d50ffffff           mov qword ptr [rbp - 0xb0], rbx
0x3850106: e8a55ab7fc               call 0x3c5bb0
0x385010b: 48894580                 mov qword ptr [rbp - 0x80], rax
0x385010f: e85c68b8fc               call 0x3d6970
0x3850114: 488d0df59fb605           lea rcx, [rip + 0x5b69ff5]
0x385011b: 89c7                     mov edi, eax
0x385011d: 4c89bd40ffffff           mov qword ptr [rbp - 0xc0], r15
0x3850124: 8b4124                   mov eax, dword ptr [rcx + 0x24]
0x3850127: 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x385012b: 89c2                     mov edx, eax
0x385012d: c1fa1f                   sar edx, 0x1f
0x3850130: c4e268f2d8               andn ebx, edx, eax
0x3850135: 31c0                     xor eax, eax
0x3850137: 660f1f840000000000       nop word ptr [rax + rax]
0x3850140: 39c3                     cmp ebx, eax
0x3850142: 742c                     je 0x3850170
0x3850144: 89c2                     mov edx, eax
0x3850146: 0fb7f0                   movzx esi, ax
0x3850149: ffc0                     inc eax
0x385014b: c1ea10                   shr edx, 0x10
0x385014e: 488d3476                 lea rsi, [rsi + rsi*2]
0x3850152: 488b14d1                 mov rdx, qword ptr [rcx + rdx*8]
0x3850156: 48833cf200               cmp qword ptr [rdx + rsi*8], 0
0x385015b: 74e3                     je 0x3850140
0x385015d: ffc8                     dec eax
0x385015f: 4c8d34f2                 lea r14, [rdx + rsi*8]
0x3850163: 4189ff                   mov r15d, edi
0x3850166: 89c3                     mov ebx, eax
0x3850168: eb0c                     jmp 0x3850176
0x385016a: 660f1f440000             nop word ptr [rax + rax]
0x3850170: 4189ff                   mov r15d, edi
0x3850173: 4531f6                   xor r14d, r14d
0x3850176: e8f567b8fc               call 0x3d6970
0x385017b: 4c8d2d8e9fb605           lea r13, [rip + 0x5b69f8e]
0x3850182: 4189c4                   mov r12d, eax
0x3850185: 498d7d30                 lea rdi, [r13 + 0x30]
0x3850189: 4889bd48ffffff           mov qword ptr [rbp - 0xb8], rdi
0x3850190: e84b1dc203               call 0x7471ee0
0x3850195: 85c0                     test eax, eax
0x3850197: 7509                     jne 0x38501a2
0x3850199: e8521dc203               call 0x7471ef0
0x385019e: 49894538                 mov qword ptr [r13 + 0x38], rax
0x38501a2: 49ff4540                 inc qword ptr [r13 + 0x40]
0x38501a6: 488b4580                 mov rax, qword ptr [rbp - 0x80]
0x38501aa: 4489ff                   mov edi, r15d
0x38501ad: 4c8bbd40ffffff           mov r15, qword ptr [rbp - 0xc0]
0x38501b4: 4409e7                   or edi, r12d
0x38501b7: 81cf00000690             or edi, 0x90060000
0x38501bd: 4c8d4030                 lea r8, [rax + 0x30]
0x38501c1: 4d85f6                   test r14, r14
0x38501c4: 741a                     je 0x38501e0
0x38501c6: 4d8b26                   mov r12, qword ptr [r14]
0x38501c9: 418b442408               mov eax, dword ptr [r12 + 8]
0x38501ce: a810                     test al, 0x10
0x38501d0: 741e                     je 0x38501f0
0x38501d2: e999000000               jmp 0x3850270
0x38501d7: 660f1f840000000000       nop word ptr [rax + rax]
0x38501e0: 4531e4                   xor r12d, r12d
0x38501e3: 418b442408               mov eax, dword ptr [r12 + 8]
0x38501e8: a810                     test al, 0x10
0x38501ea: 0f8580000000             jne 0x3850270
0x38501f0: 418b44240c               mov eax, dword ptr [r12 + 0xc]
0x38501f5: 41394524                 cmp dword ptr [r13 + 0x24], eax
0x38501f9: 7e25                     jle 0x3850220
0x38501fb: 498b5510                 mov rdx, qword ptr [r13 + 0x10]
0x38501ff: 89c1                     mov ecx, eax
0x3850201: 0fb7c0                   movzx eax, ax
0x3850204: 488d0440                 lea rax, [rax + rax*2]
0x3850208: c1e910                   shr ecx, 0x10
0x385020b: c1e003                   shl eax, 3
0x385020e: 480304ca                 add rax, qword ptr [rdx + rcx*8]
0x3850212: 8b4008                   mov eax, dword ptr [rax + 8]
0x3850215: 85f8                     test eax, edi
0x3850217: 7557                     jne 0x3850270
0x3850219: eb0e                     jmp 0x3850229
0x385021b: 0f1f440000               nop dword ptr [rax + rax]
0x3850220: 31c0                     xor eax, eax
0x3850222: 8b4008                   mov eax, dword ptr [rax + 8]
0x3850225: 85f8                     test eax, edi
0x3850227: 7547                     jne 0x3850270
0x3850229: 897d94                   mov dword ptr [rbp - 0x6c], edi
0x385022c: 4d89c5                   mov r13, r8
0x385022f: e87c59b7fc               call 0x3c5bb0
0x3850234: 4d89e8                   mov r8, r13
0x3850237: 8b7d94                   mov edi, dword ptr [rbp - 0x6c]
0x385023a: 4c8d2dcf9eb605           lea r13, [rip + 0x5b69ecf]
0x3850241: 48394580                 cmp qword ptr [rbp - 0x80], rax
0x3850245: 0f84d5010000             je 0x3850420
0x385024b: 488b4d80                 mov rcx, qword ptr [rbp - 0x80]
0x385024f: 498b442410               mov rax, qword ptr [r12 + 0x10]
0x3850254: 48634938                 movsxd rcx, dword ptr [rcx + 0x38]
0x3850258: 3b4838                   cmp ecx, dword ptr [rax + 0x38]
0x385025b: 7f13                     jg 0x3850270
0x385025d: 488b4030                 mov rax, qword ptr [rax + 0x30]
0x3850261: 4c3904c8                 cmp qword ptr [rax + rcx*8], r8
0x3850265: 0f84b5010000             je 0x3850420
0x385026b: 0f1f440000               nop dword ptr [rax + rax]
0x3850270: 418b4524                 mov eax, dword ptr [r13 + 0x24]
0x3850274: 498b4d10                 mov rcx, qword ptr [r13 + 0x10]
0x3850278: 0f1f840000000000         nop dword ptr [rax + rax]
0x3850280: ffc3                     inc ebx
0x3850282: 39c3                     cmp ebx, eax
0x3850284: 0f8da6010000             jge 0x3850430
0x385028a: 89da                     mov edx, ebx
0x385028c: 0fb7f3                   movzx esi, bx
0x385028f: c1ea10                   shr edx, 0x10
0x3850292: 488d3476                 lea rsi, [rsi + rsi*2]
0x3850296: 488b14d1                 mov rdx, qword ptr [rcx + rdx*8]
0x385029a: 48833cf200               cmp qword ptr [rdx + rsi*8], 0
0x385029f: 74df                     je 0x3850280
0x38502a1: 4c8d34f2                 lea r14, [rdx + rsi*8]
0x38502a5: 4d85f6                   test r14, r14
0x38502a8: 0f8518ffffff             jne 0x38501c6
0x38502ae: e92dffffff               jmp 0x38501e0
0x38502b3: 666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x38502c0: 498b3e                   mov rdi, qword ptr [r14]
0x38502c3: 488bb550ffffff           mov rsi, qword ptr [rbp - 0xb0]
0x38502ca: e8e173c3fc               call 0x4876b0
0x38502cf: 84c0                     test al, al
0x38502d1: 0f84a4000000             je 0x385037b
0x38502d7: 498b06                   mov rax, qword ptr [r14]
0x38502da: 488d152f9eb605           lea rdx, [rip + 0x5b69e2f]
0x38502e1: 8b400c                   mov eax, dword ptr [rax + 0xc]
0x38502e4: 394224                   cmp dword ptr [rdx + 0x24], eax
0x38502e7: 7e19                     jle 0x3850302
0x38502e9: 488b5210                 mov rdx, qword ptr [rdx + 0x10]
0x38502ed: 89c1                     mov ecx, eax
0x38502ef: 0fb7c0                   movzx eax, ax
0x38502f2: 488d3c40                 lea rdi, [rax + rax*2]
0x38502f6: c1e910                   shr ecx, 0x10
0x38502f9: c1e703                   shl edi, 3
0x38502fc: 48033cca                 add rdi, qword ptr [rdx + rcx*8]
0x3850300: eb02                     jmp 0x3850304
0x3850302: 31ff                     xor edi, edi
0x3850304: be00000040               mov esi, 0x40000000
0x3850309: e86281b1fc               call 0x368470
0x385030e: 4d8b36                   mov r14, qword ptr [r14]
0x3850311: 4d85f6                   test r14, r14
0x3850314: 7414                     je 0x385032a
0x3850316: 488d053febb405           lea rax, [rip + 0x5b4eb3f]
0x385031d: 803800                   cmp byte ptr [rax], 0
0x3850320: 7408                     je 0x385032a
0x3850322: 4c89f7                   mov rdi, r14
0x3850325: e8967fb1fc               call 0x3682c0
0x385032a: 488b4d88                 mov rcx, qword ptr [rbp - 0x78]
0x385032e: 8b4130                   mov eax, dword ptr [rcx + 0x30]
0x3850331: 3b4134                   cmp eax, dword ptr [rcx + 0x34]
0x3850334: 7518                     jne 0x385034e
0x3850336: 488bb568ffffff           mov rsi, qword ptr [rbp - 0x98]
0x385033d: 488b9570ffffff           mov rdx, qword ptr [rbp - 0x90]
0x3850344: bf08080000               mov edi, 0x808
0x3850349: e852957dfc               call 0x298a0
0x385034e: 488b5588                 mov rdx, qword ptr [rbp - 0x78]
0x3850352: 8d4801                   lea ecx, [rax + 1]
0x3850355: 4898                     cdqe 
0x3850357: 894a30                   mov dword ptr [rdx + 0x30], ecx
0x385035a: 488b4a28                 mov rcx, qword ptr [rdx + 0x28]
0x385035e: 4c8934c1                 mov qword ptr [rcx + rax*8], r14
0x3850362: 4d85f6                   test r14, r14
0x3850365: 7414                     je 0x385037b
0x3850367: 488d05eeeab405           lea rax, [rip + 0x5b4eaee]
0x385036e: 803800                   cmp byte ptr [rax], 0
0x3850371: 7408                     je 0x385037b
0x3850373: 4c89f7                   mov rdi, r14
0x3850376: e8457fb1fc               call 0x3682c0
0x385037b: 4c8d2d8e9db605           lea r13, [rip + 0x5b69d8e]
0x3850382: 448b5594                 mov r10d, dword ptr [rbp - 0x6c]
0x3850386: 418b4524                 mov eax, dword ptr [r13 + 0x24]
0x385038a: 498b4d10                 mov rcx, qword ptr [r13 + 0x10]
0x385038e: 8d5301                   lea edx, [rbx + 1]
0x3850391: 39d0                     cmp eax, edx
0x3850393: 0f4fd0                   cmovg edx, eax
0x3850396: ffca                     dec edx
0x3850398: 0f1f840000000000         nop dword ptr [rax + rax]
0x38503a0: 39da                     cmp edx, ebx
0x38503a2: 0f8488000000             je 0x3850430
0x38503a8: ffc3                     inc ebx
0x38503aa: 89de                     mov esi, ebx
0x38503ac: 440fb7c3                 movzx r8d, bx
0x38503b0: c1ee10                   shr esi, 0x10
0x38503b3: 4f8d0440                 lea r8, [r8 + r8*2]
0x38503b7: 488b3cf1                 mov rdi, qword ptr [rcx + rsi*8]
0x38503bb: 4a8b34c7                 mov rsi, qword ptr [rdi + r8*8]
0x38503bf: 4885f6                   test rsi, rsi
0x38503c2: 74dc                     je 0x38503a0
0x38503c4: 4e8d34c7                 lea r14, [rdi + r8*8]
0x38503c8: 4d85f6                   test r14, r14
0x38503cb: 742c                     je 0x38503f9
0x38503cd: 8b5608                   mov edx, dword ptr [rsi + 8]
0x38503d0: f6c210                   test dl, 0x10
0x38503d3: 75b9                     jne 0x385038e
0x38503d5: 8b560c                   mov edx, dword ptr [rsi + 0xc]
0x38503d8: 39d0                     cmp eax, edx
0x38503da: 7e32                     jle 0x385040e
0x38503dc: 89d6                     mov esi, edx
0x38503de: 0fb7d2                   movzx edx, dx
0x38503e1: 488d1452                 lea rdx, [rdx + rdx*2]
0x38503e5: c1ee10                   shr esi, 0x10
0x38503e8: c1e203                   shl edx, 3
0x38503eb: 480314f1                 add rdx, qword ptr [rcx + rsi*8]
0x38503ef: 8b5208                   mov edx, dword ptr [rdx + 8]
0x38503f2: 4485d2                   test edx, r10d
0x38503f5: 7597                     jne 0x385038e
0x38503f7: eb27                     jmp 0x3850420
0x38503f9: 8b142508000000           mov edx, dword ptr [8]
0x3850400: f6c210                   test dl, 0x10
0x3850403: 7589                     jne 0x385038e
0x3850405: 31f6                     xor esi, esi
0x3850407: 8b560c                   mov edx, dword ptr [rsi + 0xc]
0x385040a: 39d0                     cmp eax, edx
0x385040c: 7fce                     jg 0x38503dc
0x385040e: 31d2                     xor edx, edx
0x3850410: 8b5208                   mov edx, dword ptr [rdx + 8]
0x3850413: 4485d2                   test edx, r10d
0x3850416: 0f8572ffffff             jne 0x385038e
0x385041c: 0f1f4000                 nop dword ptr [rax]
0x3850420: 4d85f6                   test r14, r14
0x3850423: 0f8597feffff             jne 0x38502c0
0x3850429: 0f1f8000000000           nop dword ptr [rax]
0x3850430: 49ff4d40                 dec qword ptr [r13 + 0x40]
0x3850434: 4c8b7588                 mov r14, qword ptr [rbp - 0x78]
0x3850438: 4c8d65b0                 lea r12, [rbp - 0x50]
0x385043c: 4c8d6dc0                 lea r13, [rbp - 0x40]
0x3850440: 742e                     je 0x3850470
0x3850442: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x3850446: 4885ff                   test rdi, rdi
0x3850449: 7405                     je 0x3850450
0x385044b: e8407686fc               call 0xb7a90
0x3850450: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x3850454: 4885ff                   test rdi, rdi
0x3850457: 0f8443fbffff             je 0x384ffa0
0x385045d: e82e7686fc               call 0xb7a90
0x3850462: e939fbffff               jmp 0x384ffa0
0x3850467: 660f1f840000000000       nop word ptr [rax + rax]
0x3850470: 488d05999cb605           lea rax, [rip + 0x5b69c99]
0x3850477: 488bbd48ffffff           mov rdi, qword ptr [rbp - 0xb8]
0x385047e: 48c7403800000000         mov qword ptr [rax + 0x38], 0
0x3850486: e8951ac203               call 0x7471f20
0x385048b: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]

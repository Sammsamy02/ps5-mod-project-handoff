0x2fdac20: 55                       push rbp
0x2fdac21: 4889e5                   mov rbp, rsp
0x2fdac24: 4157                     push r15
0x2fdac26: 4156                     push r14
0x2fdac28: 4155                     push r13
0x2fdac2a: 4154                     push r12
0x2fdac2c: 53                       push rbx
0x2fdac2d: 4883ec18                 sub rsp, 0x18
0x2fdac31: 4c8b25d0721506           mov r12, qword ptr [rip + 0x61572d0]
0x2fdac38: 4989f6                   mov r14, rsi
0x2fdac3b: 4889fb                   mov rbx, rdi
0x2fdac3e: 488d3513e58a04           lea rsi, [rip + 0x48ae513] ; ['Sprite']
0x2fdac45: 488d7dc0                 lea rdi, [rbp - 0x40]
0x2fdac49: ba01000000               mov edx, 1
0x2fdac4e: 498b0424                 mov rax, qword ptr [r12]
0x2fdac52: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdac56: e8352926fd               call 0x23d590
0x2fdac5b: 4c8b7dc0                 mov r15, qword ptr [rbp - 0x40]
0x2fdac5f: 4c89f7                   mov rdi, r14
0x2fdac62: 4c89fe                   mov rsi, r15
0x2fdac65: e856834bfd               call 0x492fc0
0x2fdac6a: 498d7e20                 lea rdi, [r14 + 0x20]
0x2fdac6e: 4c89fe                   mov rsi, r15
0x2fdac71: 31d2                     xor edx, edx
0x2fdac73: 31c9                     xor ecx, ecx
0x2fdac75: e806804bfd               call 0x492c80
0x2fdac7a: 4889df                   mov rdi, rbx
0x2fdac7d: 4c89f6                   mov rsi, r14
0x2fdac80: e8eb440900               call 0x306f170
0x2fdac85: 488d0524f44a05           lea rax, [rip + 0x54af424]
0x2fdac8c: 488dbb18030000           lea rdi, [rbx + 0x318]
0x2fdac93: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdac97: 488903                   mov qword ptr [rbx], rax
0x2fdac9a: c5fc1183e8020000         vmovups ymmword ptr [rbx + 0x2e8], ymm0
0x2fdaca2: c5fc1183a8020000         vmovups ymmword ptr [rbx + 0x2a8], ymm0
0x2fdacaa: c5fc1183c8020000         vmovups ymmword ptr [rbx + 0x2c8], ymm0
0x2fdacb2: c5fc1183f8020000         vmovups ymmword ptr [rbx + 0x2f8], ymm0
0x2fdacba: e8911c12fd               call 0xfc950
0x2fdacbf: c6832903000000           mov byte ptr [rbx + 0x329], 0
0x2fdacc6: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdacca: c5f8118330030000         vmovups xmmword ptr [rbx + 0x330], xmm0
0x2fdacd2: 806359f7                 and byte ptr [rbx + 0x59], 0xf7
0x2fdacd6: 0fb68328030000           movzx eax, byte ptr [rbx + 0x328]
0x2fdacdd: 24f9                     and al, 0xf9
0x2fdacdf: 0c04                     or al, 4
0x2fdace1: 888328030000             mov byte ptr [rbx + 0x328], al
0x2fdace7: e894e6f4ff               call 0x2f29380
0x2fdacec: 4c8d3d69413c06           lea r15, [rip + 0x63c4169]
0x2fdacf3: 4989c6                   mov r14, rax
0x2fdacf6: 4885c0                   test rax, rax
0x2fdacf9: 740e                     je 0x2fdad09
0x2fdacfb: 41803f00                 cmp byte ptr [r15], 0
0x2fdacff: 7408                     je 0x2fdad09
0x2fdad01: 4c89f7                   mov rdi, r14
0x2fdad04: e8b7d538fd               call 0x3682c0
0x2fdad09: 4c89b3e0020000           mov qword ptr [rbx + 0x2e0], r14
0x2fdad10: e8abc84400               call 0x34275c0
0x2fdad15: 4989c6                   mov r14, rax
0x2fdad18: 4885c0                   test rax, rax
0x2fdad1b: 740e                     je 0x2fdad2b
0x2fdad1d: 41803f00                 cmp byte ptr [r15], 0
0x2fdad21: 7408                     je 0x2fdad2b
0x2fdad23: 4c89f7                   mov rdi, r14
0x2fdad26: e895d538fd               call 0x3682c0
0x2fdad2b: 4c89b3c8020000           mov qword ptr [rbx + 0x2c8], r14
0x2fdad32: e859664500               call 0x3431390
0x2fdad37: 4989c6                   mov r14, rax
0x2fdad3a: 4885c0                   test rax, rax
0x2fdad3d: 740e                     je 0x2fdad4d
0x2fdad3f: 41803f00                 cmp byte ptr [r15], 0
0x2fdad43: 7408                     je 0x2fdad4d
0x2fdad45: 4c89f7                   mov rdi, r14
0x2fdad48: e873d538fd               call 0x3682c0
0x2fdad4d: 4c89b3d0020000           mov qword ptr [rbx + 0x2d0], r14
0x2fdad54: 4c8b35bd7a5406           mov r14, qword ptr [rip + 0x6547abd]
0x2fdad5b: 4d85f6                   test r14, r14
0x2fdad5e: 0f8583000000             jne 0x2fdade7
0x2fdad64: 488d0535f4b3ff           lea rax, [rip - 0x4c0bcb]
0x2fdad6b: 4c8d356e5302fd           lea r14, [rip - 0x2fdac92]
0x2fdad72: 4c8d25bd496504           lea r12, [rip + 0x46549bd] ; ['Game']
0x2fdad79: 4c8d1560a87cfe           lea r10, [rip - 0x18357a0]
0x2fdad80: 4c8d5dc8                 lea r11, [rbp - 0x38]
0x2fdad84: 4c8d2da5890200           lea r13, [rip + 0x289a5]
0x2fdad8b: 488d3df06d9e04           lea rdi, [rip + 0x49e6df0] ; ['/Script/Engine']
0x2fdad92: 488d3547756e04           lea rsi, [rip + 0x46e7547] ; ['GameStateBase']
0x2fdad99: 488d15787a5406           lea rdx, [rip + 0x6547a78]
0x2fdada0: 488d0da9880200           lea rcx, [rip + 0x288a9]
0x2fdada7: 41b800030000             mov r8d, 0x300
0x2fdadad: 41b908000000             mov r9d, 8
0x2fdadb3: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x2fdadb7: 488d0532172cfd           lea rax, [rip - 0x2d3e8ce]
0x2fdadbe: 50                       push rax
0x2fdadbf: 4152                     push r10
0x2fdadc1: 4153                     push r11
0x2fdadc3: 4156                     push r14
0x2fdadc5: 4155                     push r13
0x2fdadc7: 4154                     push r12
0x2fdadc9: 4c8b2538711506           mov r12, qword ptr [rip + 0x6157138]
0x2fdadd0: 6a00                     push 0
0x2fdadd2: 6804000010               push 0x10000004
0x2fdadd7: e8842a33fd               call 0x30d860
0x2fdaddc: 4883c440                 add rsp, 0x40
0x2fdade0: 4c8b35317a5406           mov r14, qword ptr [rip + 0x6547a31]
0x2fdade7: 4d85f6                   test r14, r14
0x2fdadea: 740e                     je 0x2fdadfa
0x2fdadec: 41803f00                 cmp byte ptr [r15], 0
0x2fdadf0: 7408                     je 0x2fdadfa
0x2fdadf2: 4c89f7                   mov rdi, r14
0x2fdadf5: e8c6d438fd               call 0x3682c0
0x2fdadfa: 4c89b3c0020000           mov qword ptr [rbx + 0x2c0], r14
0x2fdae01: e8daf30800               call 0x306a1e0
0x2fdae06: 4989c6                   mov r14, rax
0x2fdae09: 4885c0                   test rax, rax
0x2fdae0c: 740e                     je 0x2fdae1c
0x2fdae0e: 41803f00                 cmp byte ptr [r15], 0
0x2fdae12: 7408                     je 0x2fdae1c
0x2fdae14: 4c89f7                   mov rdi, r14
0x2fdae17: e8a4d438fd               call 0x3682c0
0x2fdae1c: 4c89b3d8020000           mov qword ptr [rbx + 0x2d8], r14
0x2fdae23: 4c8b353e795406           mov r14, qword ptr [rip + 0x654793e]
0x2fdae2a: 4d85f6                   test r14, r14
0x2fdae2d: 757b                     jne 0x2fdaeaa
0x2fdae2f: 488d056af3b3ff           lea rax, [rip - 0x4c0c96]
0x2fdae36: 4c8d35a35202fd           lea r14, [rip - 0x2fdad5d]
0x2fdae3d: 4c8d159ca77cfe           lea r10, [rip - 0x1835864]
0x2fdae44: 4c8d5dc8                 lea r11, [rbp - 0x38]
0x2fdae48: 4c8d2de7486504           lea r13, [rip + 0x46548e7] ; ['Game']
0x2fdae4f: 488d3d2c6d9e04           lea rdi, [rip + 0x49e6d2c] ; ['/Script/Engine']
0x2fdae56: 488d35c3cc9604           lea rsi, [rip + 0x496ccc3] ; ['GameSession']
0x2fdae5d: 488d1504795406           lea rdx, [rip + 0x6547904]
0x2fdae64: 488d0d250403fd           lea rcx, [rip - 0x2fcfbdb]
0x2fdae6b: 41b8d0020000             mov r8d, 0x2d0
0x2fdae71: 41b908000000             mov r9d, 8
0x2fdae77: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x2fdae7b: 488d056e162cfd           lea rax, [rip - 0x2d3e992]
0x2fdae82: 50                       push rax
0x2fdae83: 4152                     push r10
0x2fdae85: 4153                     push r11
0x2fdae87: 4156                     push r14
0x2fdae89: 488d05f0530200           lea rax, [rip + 0x253f0]
0x2fdae90: 50                       push rax
0x2fdae91: 4155                     push r13
0x2fdae93: 6a00                     push 0
0x2fdae95: 6804000010               push 0x10000004
0x2fdae9a: e8c12933fd               call 0x30d860
0x2fdae9f: 4883c440                 add rsp, 0x40
0x2fdaea3: 4c8b35be785406           mov r14, qword ptr [rip + 0x65478be]
0x2fdaeaa: 4d85f6                   test r14, r14
0x2fdaead: 740e                     je 0x2fdaebd
0x2fdaeaf: 41803f00                 cmp byte ptr [r15], 0
0x2fdaeb3: 7408                     je 0x2fdaebd
0x2fdaeb5: 4c89f7                   mov rdi, r14
0x2fdaeb8: e803d438fd               call 0x3682c0
0x2fdaebd: 4c89b3b8020000           mov qword ptr [rbx + 0x2b8], r14
0x2fdaec4: e847ec6600               call 0x3649b10
0x2fdaec9: 4989c6                   mov r14, rax
0x2fdaecc: 4885c0                   test rax, rax
0x2fdaecf: 740e                     je 0x2fdaedf
0x2fdaed1: 41803f00                 cmp byte ptr [r15], 0
0x2fdaed5: 7408                     je 0x2fdaedf
0x2fdaed7: 4c89f7                   mov rdi, r14
0x2fdaeda: e8e1d338fd               call 0x3682c0
0x2fdaedf: 4c89b3e8020000           mov qword ptr [rbx + 0x2e8], r14
0x2fdaee6: e8d5c64400               call 0x34275c0
0x2fdaeeb: 4989c6                   mov r14, rax
0x2fdaeee: 4885c0                   test rax, rax
0x2fdaef1: 740e                     je 0x2fdaf01
0x2fdaef3: 41803f00                 cmp byte ptr [r15], 0
0x2fdaef7: 7408                     je 0x2fdaf01
0x2fdaef9: 4c89f7                   mov rdi, r14
0x2fdaefc: e8bfd338fd               call 0x3682c0
0x2fdaf01: 4c89b3f0020000           mov qword ptr [rbx + 0x2f0], r14
0x2fdaf08: e80305fcff               call 0x2f9b410
0x2fdaf0d: 4989c6                   mov r14, rax
0x2fdaf10: 4885c0                   test rax, rax
0x2fdaf13: 740e                     je 0x2fdaf23
0x2fdaf15: 41803f00                 cmp byte ptr [r15], 0
0x2fdaf19: 7408                     je 0x2fdaf23
0x2fdaf1b: 4c89f7                   mov rdi, r14
0x2fdaf1e: e89dd338fd               call 0x3682c0
0x2fdaf23: 4c89b3f8020000           mov qword ptr [rbx + 0x2f8], r14
0x2fdaf2a: 498b0424                 mov rax, qword ptr [r12]
0x2fdaf2e: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdaf32: 750f                     jne 0x2fdaf43
0x2fdaf34: 4883c418                 add rsp, 0x18
0x2fdaf38: 5b                       pop rbx
0x2fdaf39: 415c                     pop r12
0x2fdaf3b: 415d                     pop r13
0x2fdaf3d: 415e                     pop r14
0x2fdaf3f: 415f                     pop r15
0x2fdaf41: 5d                       pop rbp
0x2fdaf42: c3                       ret 
0x2fdaf43: e8186f4904               call 0x7471e60
0x2fdaf48: 0f0b                     ud2 
0x2fdaf4a: cc                       int3 
0x2fdaf4b: cc                       int3 
0x2fdaf4c: cc                       int3 
0x2fdaf4d: cc                       int3 
0x2fdaf4e: cc                       int3 
0x2fdaf4f: cc                       int3 
0x2fdaf50: 55                       push rbp
0x2fdaf51: 4889e5                   mov rbp, rsp
0x2fdaf54: 4157                     push r15
0x2fdaf56: 4156                     push r14
0x2fdaf58: 4155                     push r13
0x2fdaf5a: 4154                     push r12
0x2fdaf5c: 53                       push rbx

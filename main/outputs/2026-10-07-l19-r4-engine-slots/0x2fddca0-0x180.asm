0x2fddca0: 55                       push rbp
0x2fddca1: 4889e5                   mov rbp, rsp
0x2fddca4: 4157                     push r15
0x2fddca6: 4156                     push r14
0x2fddca8: 4155                     push r13
0x2fddcaa: 4154                     push r12
0x2fddcac: 53                       push rbx
0x2fddcad: 4881ec98000000           sub rsp, 0x98
0x2fddcb4: 4c8b354d421506           mov r14, qword ptr [rip + 0x615424d]
0x2fddcbb: 4989ff                   mov r15, rdi
0x2fddcbe: 498b06                   mov rax, qword ptr [r14]
0x2fddcc1: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fddcc5: 4883bab002000000         cmp qword ptr [rdx + 0x2b0], 0
0x2fddccd: 7478                     je 0x2fddd47
0x2fddccf: 4c8bb600030000           mov r14, qword ptr [rsi + 0x300]
0x2fddcd6: 48c745b000000000         mov qword ptr [rbp - 0x50], 0
0x2fddcde: 4c89bd50ffffff           mov qword ptr [rbp - 0xb0], r15
0x2fddce5: 4d89c4                   mov r12, r8
0x2fddce8: 4889cb                   mov rbx, rcx
0x2fddceb: 48899558ffffff           mov qword ptr [rbp - 0xa8], rdx
0x2fddcf2: 4889b568ffffff           mov qword ptr [rbp - 0x98], rsi
0x2fddcf9: 4c898d60ffffff           mov qword ptr [rbp - 0xa0], r9
0x2fddd00: 4d8b28                   mov r13, qword ptr [r8]
0x2fddd03: 4d637808                 movsxd r15, dword ptr [r8 + 8]
0x2fddd07: 44897db8                 mov dword ptr [rbp - 0x48], r15d
0x2fddd0b: 4d85ff                   test r15, r15
0x2fddd0e: 744b                     je 0x2fddd5b
0x2fddd10: 488d45bc                 lea rax, [rbp - 0x44]
0x2fddd14: 4c8d45b0                 lea r8, [rbp - 0x50]
0x2fddd18: bf02000000               mov edi, 2
0x2fddd1d: be02000000               mov esi, 2
0x2fddd22: 4489fa                   mov edx, r15d
0x2fddd25: 4589f9                   mov r9d, r15d
0x2fddd28: 31c9                     xor ecx, ecx
0x2fddd2a: 48890424                 mov qword ptr [rsp], rax
0x2fddd2e: e89d9804fd               call 0x275d0
0x2fddd33: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x2fddd37: 4d01ff                   add r15, r15
0x2fddd3a: 4c89ee                   mov rsi, r13
0x2fddd3d: 4c89fa                   mov rdx, r15
0x2fddd40: e80b414904               call 0x7471e50
0x2fddd45: eb1b                     jmp 0x2fddd62
0x2fddd47: 488d3520945904           lea rsi, [rip + 0x4599420] ; ['PlayerState is null']
0x2fddd4e: 4c89ff                   mov rdi, r15
0x2fddd51: e89a6108fd               call 0x63ef0
0x2fddd56: e946030000               jmp 0x2fde0a1
0x2fddd5b: c745bc00000000           mov dword ptr [rbp - 0x44], 0
0x2fddd62: 4c8d7dc0                 lea r15, [rbp - 0x40]
0x2fddd66: 488d35c5f07d04           lea rsi, [rip + 0x47df0c5] ; ['bIsFromInvite']
0x2fddd6d: 4c89ff                   mov rdi, r15
0x2fddd70: e87b6108fd               call 0x63ef0
0x2fddd75: 488d7db0                 lea rdi, [rbp - 0x50]
0x2fddd79: 4c89fe                   mov rsi, r15
0x2fddd7c: e84f030000               call 0x2fde0d0
0x2fddd81: 4c8bad58ffffff           mov r13, qword ptr [rbp - 0xa8]
0x2fddd88: 4d8b06                   mov r8, qword ptr [r14]
0x2fddd8b: 0fb6c8                   movzx ecx, al
0x2fddd8e: 4c89f7                   mov rdi, r14
0x2fddd91: 4889da                   mov rdx, rbx
0x2fddd94: 4c89ee                   mov rsi, r13
0x2fddd97: 41ff9070070000           call qword ptr [r8 + 0x770]
0x2fddd9e: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fddda2: 4885ff                   test rdi, rdi
0x2fddda5: 7405                     je 0x2fdddac
0x2fddda7: e8e49c0dfd               call 0xb7a90
0x2fdddac: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x2fdddb0: 4c8bbd50ffffff           mov r15, qword ptr [rbp - 0xb0]
0x2fdddb7: 4885ff                   test rdi, rdi
0x2fdddba: 7405                     je 0x2fdddc1
0x2fdddbc: e8cf9c0dfd               call 0xb7a90
0x2fdddc1: 488bbd68ffffff           mov rdi, qword ptr [rbp - 0x98]
0x2fdddc8: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdddcc: c4c1781107               vmovups xmmword ptr [r15], xmm0
0x2fdddd1: 488b9560ffffff           mov rdx, qword ptr [rbp - 0xa0]
0x2fdddd8: 4c89ee                   mov rsi, r13
0x2fddddb: 4c89f9                   mov rcx, r15
0x2fdddde: 488b07                   mov rax, qword ptr [rdi]
0x2fddde1: ff90f0080000             call qword ptr [rax + 0x8f0]
0x2fddde7: 48c745a000000000         mov qword ptr [rbp - 0x60], 0
0x2fdddef: 498b1c24                 mov rbx, qword ptr [r12]
0x2fdddf3: 4d63742408               movsxd r14, dword ptr [r12 + 8]
0x2fdddf8: 448975a8                 mov dword ptr [rbp - 0x58], r14d
0x2fdddfc: 4d85f6                   test r14, r14
0x2fdddff: 7437                     je 0x2fdde38
0x2fdde01: 488d45ac                 lea rax, [rbp - 0x54]
0x2fdde05: 4c8d45a0                 lea r8, [rbp - 0x60]
0x2fdde09: bf02000000               mov edi, 2
0x2fdde0e: be02000000               mov esi, 2
0x2fdde13: 4489f2                   mov edx, r14d
0x2fdde16: 4589f1                   mov r9d, r14d
0x2fdde19: 31c9                     xor ecx, ecx
0x2fdde1b: 48890424                 mov qword ptr [rsp], rax

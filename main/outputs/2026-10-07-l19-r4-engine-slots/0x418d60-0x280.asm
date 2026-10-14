0x418d60: 55                       push rbp
0x418d61: 4889e5                   mov rbp, rsp
0x418d64: 4157                     push r15
0x418d66: 4156                     push r14
0x418d68: 4155                     push r13
0x418d6a: 4154                     push r12
0x418d6c: 53                       push rbx
0x418d6d: 4883ec18                 sub rsp, 0x18
0x418d71: 4c8b259091d108           mov r12, qword ptr [rip + 0x8d19190]
0x418d78: 4989d6                   mov r14, rdx
0x418d7b: 4889fb                   mov rbx, rdi
0x418d7e: 498b0424                 mov rax, qword ptr [r12]
0x418d82: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x418d86: 4c8b2e                   mov r13, qword ptr [rsi]
0x418d89: 41f6c501                 test r13b, 1
0x418d8d: 7446                     je 0x418dd5
0x418d8f: 4d89ef                   mov r15, r13
0x418d92: 4983e7fe                 and r15, 0xfffffffffffffffe
0x418d96: 743d                     je 0x418dd5
0x418d98: 4c8d25bd60f808           lea r12, [rip + 0x8f860bd]
0x418d9f: 41803c2401               cmp byte ptr [r12], 1
0x418da4: 0f85f4000000             jne 0x418e9e
0x418daa: 4c89ff                   mov rdi, r15
0x418dad: e80ef5f4ff               call 0x3682c0
0x418db2: 41803c2401               cmp byte ptr [r12], 1
0x418db7: 0f84ec000000             je 0x418ea9
0x418dbd: 41803c2401               cmp byte ptr [r12], 1
0x418dc2: 4c8b253f91d108           mov r12, qword ptr [rip + 0x8d1913f]
0x418dc9: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x418dcd: 0f84f4000000             je 0x418ec7
0x418dd3: eb04                     jmp 0x418dd9
0x418dd5: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x418dd9: 488d75c8                 lea rsi, [rbp - 0x38]
0x418ddd: 48b90002000010000800     movabs rcx, 0x8001000000200
0x418de7: 4889df                   mov rdi, rbx
0x418dea: 4c89f2                   mov rdx, r14
0x418ded: e82e90fcff               call 0x3e1e20
0x418df2: 488d05f768ad07           lea rax, [rip + 0x7ad68f7]
0x418df9: 488903                   mov qword ptr [rbx], rax
0x418dfc: 48c7437000000000         mov qword ptr [rbx + 0x70], 0
0x418e04: 498b4638                 mov rax, qword ptr [r14 + 0x38]
0x418e08: 4885c0                   test rax, rax
0x418e0b: 7420                     je 0x418e2d
0x418e0d: ffd0                     call rax
0x418e0f: 4989c7                   mov r15, rax
0x418e12: 4885c0                   test rax, rax
0x418e15: 7419                     je 0x418e30
0x418e17: 488d053e60f808           lea rax, [rip + 0x8f8603e]
0x418e1e: 803800                   cmp byte ptr [rax], 0
0x418e21: 740d                     je 0x418e30
0x418e23: 4c89ff                   mov rdi, r15
0x418e26: e895f4f4ff               call 0x3682c0
0x418e2b: eb03                     jmp 0x418e30
0x418e2d: 4531ff                   xor r15d, r15d
0x418e30: 488d0581c3ad07           lea rax, [rip + 0x7adc381]
0x418e37: 4889df                   mov rdi, rbx
0x418e3a: be08000000               mov esi, 8
0x418e3f: 4c897b70                 mov qword ptr [rbx + 0x70], r15
0x418e43: 488903                   mov qword ptr [rbx], rax
0x418e46: e8158ffcff               call 0x3e1d60
0x418e4b: 488d0516c1ad07           lea rax, [rip + 0x7adc116]
0x418e52: 488903                   mov qword ptr [rbx], rax
0x418e55: 498b4638                 mov rax, qword ptr [r14 + 0x38]
0x418e59: 4885c0                   test rax, rax
0x418e5c: 7420                     je 0x418e7e
0x418e5e: ffd0                     call rax
0x418e60: 4989c6                   mov r14, rax
0x418e63: 4885c0                   test rax, rax
0x418e66: 7419                     je 0x418e81
0x418e68: 488d05ed5ff808           lea rax, [rip + 0x8f85fed]
0x418e6f: 803800                   cmp byte ptr [rax], 0
0x418e72: 740d                     je 0x418e81
0x418e74: 4c89f7                   mov rdi, r14
0x418e77: e844f4f4ff               call 0x3682c0
0x418e7c: eb03                     jmp 0x418e81
0x418e7e: 4531f6                   xor r14d, r14d
0x418e81: 4c897370                 mov qword ptr [rbx + 0x70], r14
0x418e85: 498b0424                 mov rax, qword ptr [r12]
0x418e89: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x418e8d: 7545                     jne 0x418ed4
0x418e8f: 4883c418                 add rsp, 0x18
0x418e93: 5b                       pop rbx
0x418e94: 415c                     pop r12
0x418e96: 415d                     pop r13
0x418e98: 415e                     pop r14
0x418e9a: 415f                     pop r15
0x418e9c: 5d                       pop rbp
0x418e9d: c3                       ret 
0x418e9e: 41803c2401               cmp byte ptr [r12], 1
0x418ea3: 0f8514ffffff             jne 0x418dbd
0x418ea9: 4c89ff                   mov rdi, r15
0x418eac: e80ff4f4ff               call 0x3682c0
0x418eb1: 41803c2401               cmp byte ptr [r12], 1
0x418eb6: 4c8b254b90d108           mov r12, qword ptr [rip + 0x8d1904b]
0x418ebd: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x418ec1: 0f8512ffffff             jne 0x418dd9
0x418ec7: 4c89ff                   mov rdi, r15
0x418eca: e8f1f3f4ff               call 0x3682c0
0x418ecf: e905ffffff               jmp 0x418dd9
0x418ed4: e8878f0507               call 0x7471e60
0x418ed9: 0f0b                     ud2 
0x418edb: cc                       int3 
0x418edc: cc                       int3 
0x418edd: cc                       int3 
0x418ede: cc                       int3 
0x418edf: cc                       int3 
0x418ee0: 55                       push rbp
0x418ee1: 4889e5                   mov rbp, rsp
0x418ee4: 53                       push rbx
0x418ee5: 50                       push rax
0x418ee6: b838010000               mov eax, 0x138
0x418eeb: 4889fb                   mov rbx, rdi
0x418eee: c4e2f8f74638             bextr rax, qword ptr [rsi + 0x38], rax
0x418ef4: 4183780800               cmp dword ptr [r8 + 8], 0
0x418ef9: 7405                     je 0x418f00
0x418efb: 498b10                   mov rdx, qword ptr [r8]
0x418efe: eb07                     jmp 0x418f07
0x418f00: 488d1599662107           lea rdx, [rip + 0x7216699]
0x418f07: 488d35dae62607           lea rsi, [rip + 0x726e6da] ; ['TObjectPtr<%s>']
0x418f0e: 488d3d631a3907           lea rdi, [rip + 0x7391a63] ; ['%s*']
0x418f15: 4885c0                   test rax, rax
0x418f18: 480f44f7                 cmove rsi, rdi
0x418f1c: f7c100010000             test ecx, 0x100
0x418f22: 480f45f7                 cmovne rsi, rdi
0x418f26: 4889df                   mov rdi, rbx
0x418f29: 31c0                     xor eax, eax
0x418f2b: e8502bc5ff               call 0x6ba80
0x418f30: 4889d8                   mov rax, rbx
0x418f33: 4883c408                 add rsp, 8
0x418f37: 5b                       pop rbx
0x418f38: 5d                       pop rbp
0x418f39: c3                       ret 
0x418f3a: cc                       int3 
0x418f3b: cc                       int3 
0x418f3c: cc                       int3 
0x418f3d: cc                       int3 
0x418f3e: cc                       int3 
0x418f3f: cc                       int3 
0x418f40: 55                       push rbp
0x418f41: 4889e5                   mov rbp, rsp
0x418f44: 4157                     push r15
0x418f46: 4156                     push r14
0x418f48: 4155                     push r13
0x418f4a: 4154                     push r12
0x418f4c: 53                       push rbx
0x418f4d: 4883ec28                 sub rsp, 0x28
0x418f51: 4c8b2db08fd108           mov r13, qword ptr [rip + 0x8d18fb0]
0x418f58: 4989d6                   mov r14, rdx
0x418f5b: 4989f4                   mov r12, rsi
0x418f5e: 4889fb                   mov rbx, rdi
0x418f61: 498b4500                 mov rax, qword ptr [r13]
0x418f65: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x418f69: f6463f01                 test byte ptr [rsi + 0x3f], 1
0x418f6d: 753a                     jne 0x418fa9
0x418f6f: 498b7c2470               mov rdi, qword ptr [r12 + 0x70]
0x418f74: 488b07                   mov rax, qword ptr [rdi]
0x418f77: ff9020030000             call qword ptr [rax + 0x320]
0x418f7d: 498b4c2470               mov rcx, qword ptr [r12 + 0x70]
0x418f82: 4989c7                   mov r15, rax
0x418f85: 4c8d65c0                 lea r12, [rbp - 0x40]
0x418f89: 488d7db0                 lea rdi, [rbp - 0x50]
0x418f8d: 4c89e6                   mov rsi, r12
0x418f90: 488b4118                 mov rax, qword ptr [rcx + 0x18]
0x418f94: 488945c0                 mov qword ptr [rbp - 0x40], rax
0x418f98: e82361e2ff               call 0x23f0c0
0x418f9d: 837db800                 cmp dword ptr [rbp - 0x48], 0
0x418fa1: 7440                     je 0x418fe3
0x418fa3: 488b4db0                 mov rcx, qword ptr [rbp - 0x50]
0x418fa7: eb41                     jmp 0x418fea
0x418fa9: 498b7c2470               mov rdi, qword ptr [r12 + 0x70]
0x418fae: 488b07                   mov rax, qword ptr [rdi]
0x418fb1: ff9020030000             call qword ptr [rax + 0x320]
0x418fb7: 498b4c2470               mov rcx, qword ptr [r12 + 0x70]
0x418fbc: 4989c7                   mov r15, rax
0x418fbf: 4c8d65c0                 lea r12, [rbp - 0x40]
0x418fc3: 488d7db0                 lea rdi, [rbp - 0x50]
0x418fc7: 4c89e6                   mov rsi, r12
0x418fca: 488b4118                 mov rax, qword ptr [rcx + 0x18]
0x418fce: 488945c0                 mov qword ptr [rbp - 0x40], rax
0x418fd2: e8e960e2ff               call 0x23f0c0
0x418fd7: 837db800                 cmp dword ptr [rbp - 0x48], 0
0x418fdb: 7454                     je 0x419031

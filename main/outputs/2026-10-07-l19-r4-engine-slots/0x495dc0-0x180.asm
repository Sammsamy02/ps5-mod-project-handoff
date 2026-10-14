0x495dc0: 55                       push rbp
0x495dc1: 4889e5                   mov rbp, rsp
0x495dc4: 4157                     push r15
0x495dc6: 4156                     push r14
0x495dc8: 4155                     push r13
0x495dca: 4154                     push r12
0x495dcc: 53                       push rbx
0x495dcd: 4883ec18                 sub rsp, 0x18
0x495dd1: 4c8b2530c1c908           mov r12, qword ptr [rip + 0x8c9c130]
0x495dd8: 4889f3                   mov rbx, rsi
0x495ddb: 4989ff                   mov r15, rdi
0x495dde: 498b0424                 mov rax, qword ptr [r12]
0x495de2: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x495de6: 488b4628                 mov rax, qword ptr [rsi + 0x28]
0x495dea: 480b4620                 or rax, qword ptr [rsi + 0x20]
0x495dee: 7571                     jne 0x495e61
0x495df0: bf78000000               mov edi, 0x78
0x495df5: 31f6                     xor esi, esi
0x495df7: e8241cc2ff               call 0xb7a20
0x495dfc: 4989c6                   mov r14, rax
0x495dff: e8ccbff6ff               call 0x401dd0
0x495e04: 4c89f7                   mov rdi, r14
0x495e07: 4889c2                   mov rdx, rax
0x495e0a: 31f6                     xor esi, esi
0x495e0c: e8afe7ecff               call 0x3645c0
0x495e11: 488d0560bea507           lea rax, [rip + 0x7a5be60]
0x495e18: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x495e1c: c4c178114668             vmovups xmmword ptr [r14 + 0x68], xmm0
0x495e22: 4883c010                 add rax, 0x10
0x495e26: 498906                   mov qword ptr [r14], rax
0x495e29: 498b3f                   mov rdi, qword ptr [r15]
0x495e2c: 48897dc0                 mov qword ptr [rbp - 0x40], rdi
0x495e30: 40f6c701                 test dil, 1
0x495e34: 7417                     je 0x495e4d
0x495e36: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x495e3a: 7411                     je 0x495e4d
0x495e3c: 488d051990f008           lea rax, [rip + 0x8f09019]
0x495e43: 803801                   cmp byte ptr [rax], 1
0x495e46: 7505                     jne 0x495e4d
0x495e48: e87324edff               call 0x3682c0
0x495e4d: 488d75c0                 lea rsi, [rbp - 0x40]
0x495e51: 4c89f7                   mov rdi, r14
0x495e54: 4889da                   mov rdx, rbx
0x495e57: e804c3f6ff               call 0x402160
0x495e5c: e9a1000000               jmp 0x495f02
0x495e61: bf88000000               mov edi, 0x88
0x495e66: 31f6                     xor esi, esi
0x495e68: e8b31bc2ff               call 0xb7a20
0x495e6d: 4989c6                   mov r14, rax
0x495e70: e85bbff6ff               call 0x401dd0
0x495e75: 4c89f7                   mov rdi, r14
0x495e78: 4889c2                   mov rdx, rax
0x495e7b: 31f6                     xor esi, esi
0x495e7d: e83ee7ecff               call 0x3645c0
0x495e82: 488d05efbda507           lea rax, [rip + 0x7a5bdef]
0x495e89: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x495e8d: c4c178114668             vmovups xmmword ptr [r14 + 0x68], xmm0
0x495e93: 4883c010                 add rax, 0x10
0x495e97: 498906                   mov qword ptr [r14], rax
0x495e9a: 4d8b2f                   mov r13, qword ptr [r15]
0x495e9d: 41f6c501                 test r13b, 1
0x495ea1: 7437                     je 0x495eda
0x495ea3: 4d89ef                   mov r15, r13
0x495ea6: 4983e7fe                 and r15, 0xfffffffffffffffe
0x495eaa: 742e                     je 0x495eda
0x495eac: 488d05a98ff008           lea rax, [rip + 0x8f08fa9]
0x495eb3: 803801                   cmp byte ptr [rax], 1
0x495eb6: 750f                     jne 0x495ec7
0x495eb8: 4c89ff                   mov rdi, r15
0x495ebb: e80024edff               call 0x3682c0
0x495ec0: 488d05958ff008           lea rax, [rip + 0x8f08f95]
0x495ec7: 803801                   cmp byte ptr [rax], 1
0x495eca: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x495ece: 750e                     jne 0x495ede
0x495ed0: 4c89ff                   mov rdi, r15
0x495ed3: e8e823edff               call 0x3682c0
0x495ed8: eb04                     jmp 0x495ede
0x495eda: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x495ede: 488d75c8                 lea rsi, [rbp - 0x38]
0x495ee2: 4c89f7                   mov rdi, r14
0x495ee5: 4889da                   mov rdx, rbx
0x495ee8: e873c2f6ff               call 0x402160
0x495eed: 488d0514c1a607           lea rax, [rip + 0x7a6c114]
0x495ef4: 498906                   mov qword ptr [r14], rax
0x495ef7: c5f8104320               vmovups xmm0, xmmword ptr [rbx + 0x20]
0x495efc: c4c178114678             vmovups xmmword ptr [r14 + 0x78], xmm0
0x495f02: 498b0424                 mov rax, qword ptr [r12]
0x495f06: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x495f0a: 7512                     jne 0x495f1e
0x495f0c: 4c89f0                   mov rax, r14
0x495f0f: 4883c418                 add rsp, 0x18
0x495f13: 5b                       pop rbx
0x495f14: 415c                     pop r12
0x495f16: 415d                     pop r13
0x495f18: 415e                     pop r14
0x495f1a: 415f                     pop r15
0x495f1c: 5d                       pop rbp
0x495f1d: c3                       ret 
0x495f1e: e83dbffd06               call 0x7471e60
0x495f23: 0f0b                     ud2 
0x495f25: cc                       int3 
0x495f26: cc                       int3 
0x495f27: cc                       int3 
0x495f28: cc                       int3 
0x495f29: cc                       int3 
0x495f2a: cc                       int3 
0x495f2b: cc                       int3 
0x495f2c: cc                       int3 
0x495f2d: cc                       int3 
0x495f2e: cc                       int3 
0x495f2f: cc                       int3 
0x495f30: 55                       push rbp
0x495f31: 4889e5                   mov rbp, rsp
0x495f34: 4157                     push r15
0x495f36: 4156                     push r14
0x495f38: 4155                     push r13
0x495f3a: 4154                     push r12
0x495f3c: 53                       push rbx

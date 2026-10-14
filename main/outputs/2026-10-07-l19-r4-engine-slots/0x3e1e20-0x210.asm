0x3e1e20: 55                       push rbp
0x3e1e21: 4889e5                   mov rbp, rsp
0x3e1e24: 4157                     push r15
0x3e1e26: 4156                     push r14
0x3e1e28: 4155                     push r13
0x3e1e2a: 4154                     push r12
0x3e1e2c: 53                       push rbx
0x3e1e2d: 4881ec48010000           sub rsp, 0x148
0x3e1e34: 4c8b3dcd00d508           mov r15, qword ptr [rip + 0x8d500cd]
0x3e1e3b: 48898d98feffff           mov qword ptr [rbp - 0x168], rcx
0x3e1e42: 4889fb                   mov rbx, rdi
0x3e1e45: 4989d5                   mov r13, rdx
0x3e1e48: 498b07                   mov rax, qword ptr [r15]
0x3e1e4b: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x3e1e4f: 488b3e                   mov rdi, qword ptr [rsi]
0x3e1e52: 4889bdb0feffff           mov qword ptr [rbp - 0x150], rdi
0x3e1e59: 40f6c701                 test dil, 1
0x3e1e5d: 7417                     je 0x3e1e76
0x3e1e5f: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x3e1e63: 7411                     je 0x3e1e76
0x3e1e65: 488d05f0cffb08           lea rax, [rip + 0x8fbcff0]
0x3e1e6c: 803801                   cmp byte ptr [rax], 1
0x3e1e6f: 7505                     jne 0x3e1e76
0x3e1e71: e84a64f8ff               call 0x3682c0
0x3e1e76: 4d8b6500                 mov r12, qword ptr [r13]
0x3e1e7a: 48c745b800000000         mov qword ptr [rbp - 0x48], 0
0x3e1e82: 4d85e4                   test r12, r12
0x3e1e85: 0f8494000000             je 0x3e1f1f
0x3e1e8b: 4c89e7                   mov rdi, r12
0x3e1e8e: 4c89ada0feffff           mov qword ptr [rbp - 0x160], r13
0x3e1e95: e856020907               call 0x74720f0
0x3e1e9a: 4989c5                   mov r13, rax
0x3e1e9d: 4c89e6                   mov rsi, r12
0x3e1ea0: 4531f6                   xor r14d, r14d
0x3e1ea3: 31ff                     xor edi, edi
0x3e1ea5: 4489ea                   mov edx, r13d
0x3e1ea8: e8f3b2caff               call 0x8d1a0
0x3e1ead: 4189c7                   mov r15d, eax
0x3e1eb0: 8945c8                   mov dword ptr [rbp - 0x38], eax
0x3e1eb3: 41ffc7                   inc r15d
0x3e1eb6: 4181ff80000000           cmp r15d, 0x80
0x3e1ebd: 7707                     ja 0x3e1ec6
0x3e1ebf: 48837db800               cmp qword ptr [rbp - 0x48], 0
0x3e1ec4: 7422                     je 0x3e1ee8
0x3e1ec6: 4181ff81000000           cmp r15d, 0x81
0x3e1ecd: 488d7db8                 lea rdi, [rbp - 0x48]
0x3e1ed1: b902000000               mov ecx, 2
0x3e1ed6: 450f43f7                 cmovae r14d, r15d
0x3e1eda: 31f6                     xor esi, esi
0x3e1edc: 4489f2                   mov edx, r14d
0x3e1edf: e87c10c8ff               call 0x62f60
0x3e1ee4: 4c8b75b8                 mov r14, qword ptr [rbp - 0x48]
0x3e1ee8: 4d85f6                   test r14, r14
0x3e1eeb: 488dbdb8feffff           lea rdi, [rbp - 0x148]
0x3e1ef2: 4489fe                   mov esi, r15d
0x3e1ef5: 4c89e2                   mov rdx, r12
0x3e1ef8: 490f45fe                 cmovne rdi, r14
0x3e1efc: 41ffc5                   inc r13d
0x3e1eff: 4489e9                   mov ecx, r13d
0x3e1f02: 48897dc0                 mov qword ptr [rbp - 0x40], rdi
0x3e1f06: e8f5bbcaff               call 0x8db00
0x3e1f0b: 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x3e1f0f: 4c8b3df2ffd408           mov r15, qword ptr [rip + 0x8d4fff2]
0x3e1f16: 4c8bada0feffff           mov r13, qword ptr [rbp - 0x160]
0x3e1f1d: eb11                     jmp 0x3e1f30
0x3e1f1f: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x3e1f27: 31f6                     xor esi, esi
0x3e1f29: c745c800000000           mov dword ptr [rbp - 0x38], 0
0x3e1f30: 4c8db5a8feffff           lea r14, [rbp - 0x158]
0x3e1f37: ba01000000               mov edx, 1
0x3e1f3c: 4c89f7                   mov rdi, r14
0x3e1f3f: e84cb6e5ff               call 0x23d590
0x3e1f44: 418b4d1c                 mov ecx, dword ptr [r13 + 0x1c]
0x3e1f48: 488db5b0feffff           lea rsi, [rbp - 0x150]
0x3e1f4f: 4889df                   mov rdi, rbx
0x3e1f52: 4c89f2                   mov rdx, r14
0x3e1f55: e89626f8ff               call 0x3645f0
0x3e1f5a: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x3e1f5e: 4885ff                   test rdi, rdi
0x3e1f61: 7405                     je 0x3e1f68
0x3e1f63: e8285bcdff               call 0xb7a90
0x3e1f68: 488d05c984b007           lea rax, [rip + 0x7b084c9]
0x3e1f6f: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x3e1f73: 4889df                   mov rdi, rbx
0x3e1f76: 488903                   mov qword ptr [rbx], rax
0x3e1f79: 48c7432c01000000         mov qword ptr [rbx + 0x2c], 1
0x3e1f81: 488b8598feffff           mov rax, qword ptr [rbp - 0x168]
0x3e1f88: 490b4510                 or rax, qword ptr [r13 + 0x10]
0x3e1f8c: 48894338                 mov qword ptr [rbx + 0x38], rax
0x3e1f90: 66c743400000             mov word ptr [rbx + 0x40], 0
0x3e1f96: c6434200                 mov byte ptr [rbx + 0x42], 0
0x3e1f9a: c5fc114344               vmovups ymmword ptr [rbx + 0x44], ymm0
0x3e1f9f: c5fc114350               vmovups ymmword ptr [rbx + 0x50], ymm0
0x3e1fa4: 410fb74532               movzx eax, word ptr [r13 + 0x32]
0x3e1fa9: 894344                   mov dword ptr [rbx + 0x44], eax
0x3e1fac: e82f000000               call 0x3e1fe0
0x3e1fb1: 498b07                   mov rax, qword ptr [r15]
0x3e1fb4: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x3e1fb8: 7512                     jne 0x3e1fcc
0x3e1fba: 4881c448010000           add rsp, 0x148
0x3e1fc1: 5b                       pop rbx
0x3e1fc2: 415c                     pop r12
0x3e1fc4: 415d                     pop r13
0x3e1fc6: 415e                     pop r14
0x3e1fc8: 415f                     pop r15
0x3e1fca: 5d                       pop rbp
0x3e1fcb: c3                       ret 
0x3e1fcc: e88ffe0807               call 0x7471e60
0x3e1fd1: 0f0b                     ud2 
0x3e1fd3: cc                       int3 
0x3e1fd4: cc                       int3 
0x3e1fd5: cc                       int3 
0x3e1fd6: cc                       int3 
0x3e1fd7: cc                       int3 
0x3e1fd8: cc                       int3 
0x3e1fd9: cc                       int3 
0x3e1fda: cc                       int3 
0x3e1fdb: cc                       int3 
0x3e1fdc: cc                       int3 
0x3e1fdd: cc                       int3 
0x3e1fde: cc                       int3 
0x3e1fdf: cc                       int3 
0x3e1fe0: 55                       push rbp
0x3e1fe1: 4889e5                   mov rbp, rsp
0x3e1fe4: 4156                     push r14
0x3e1fe6: 53                       push rbx
0x3e1fe7: 4889fb                   mov rbx, rdi
0x3e1fea: 4c8d7710                 lea r14, [rdi + 0x10]
0x3e1fee: e8bd3bfeff               call 0x3c5bb0
0x3e1ff3: 4c89f7                   mov rdi, r14
0x3e1ff6: 4889c6                   mov rsi, rax
0x3e1ff9: e8321ef8ff               call 0x363e30
0x3e1ffe: 84c0                     test al, al
0x3e2000: 7406                     je 0x3e2008
0x3e2002: 49833e02                 cmp qword ptr [r14], 2
0x3e2006: 7329                     jae 0x3e2031
0x3e2008: e82325f8ff               call 0x364530
0x3e200d: 4c89f7                   mov rdi, r14
0x3e2010: 4889c6                   mov rsi, rax
0x3e2013: e8581ef8ff               call 0x363e70
0x3e2018: 84c0                     test al, al
0x3e201a: 7405                     je 0x3e2021
0x3e201c: 498b3e                   mov rdi, qword ptr [r14]
0x3e201f: eb02                     jmp 0x3e2023
0x3e2021: 31ff                     xor edi, edi
0x3e2023: 488b07                   mov rax, qword ptr [rdi]
0x3e2026: 4889de                   mov rsi, rbx
0x3e2029: ff5040                   call qword ptr [rax + 0x40]
0x3e202c: 5b                       pop rbx
0x3e202d: 415e                     pop r14
0x3e202f: 5d                       pop rbp

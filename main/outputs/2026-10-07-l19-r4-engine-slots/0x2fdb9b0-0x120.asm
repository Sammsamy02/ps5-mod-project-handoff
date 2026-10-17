0x2fdb9b0: 55                       push rbp
0x2fdb9b1: 4889e5                   mov rbp, rsp
0x2fdb9b4: 4157                     push r15
0x2fdb9b6: 4156                     push r14
0x2fdb9b8: 4155                     push r13
0x2fdb9ba: 4154                     push r12
0x2fdb9bc: 53                       push rbx
0x2fdb9bd: 4883ec28                 sub rsp, 0x28
0x2fdb9c1: 488b0540651506           mov rax, qword ptr [rip + 0x6156540]
0x2fdb9c8: 4989fe                   mov r14, rdi
0x2fdb9cb: 488b00                   mov rax, qword ptr [rax]
0x2fdb9ce: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdb9d2: e8c9e8b3ff               call 0x2b1a2a0
0x2fdb9d7: 4889c7                   mov rdi, rax
0x2fdb9da: e8d1cb7b00               call 0x37985b0
0x2fdb9df: 4531ff                   xor r15d, r15d
0x2fdb9e2: 85d2                     test edx, edx
0x2fdb9e4: 0f88b0000000             js 0x2fdba9a
0x2fdb9ea: 4189d4                   mov r12d, edx
0x2fdb9ed: 395008                   cmp dword ptr [rax + 8], edx
0x2fdb9f0: 0f8ea4000000             jle 0x2fdba9a
0x2fdb9f6: 4489e3                   mov ebx, r12d
0x2fdb9f9: 4531ff                   xor r15d, r15d
0x2fdb9fc: 488945b0                 mov qword ptr [rbp - 0x50], rax
0x2fdba00: 48c1e303                 shl rbx, 3
0x2fdba04: eb2c                     jmp 0x2fdba32
0x2fdba06: 498b06                   mov rax, qword ptr [r14]
0x2fdba09: 4c89f7                   mov rdi, r14
0x2fdba0c: 4c89ee                   mov rsi, r13
0x2fdba0f: ff9090070000             call qword ptr [rax + 0x790]
0x2fdba15: 448b7dbc                 mov r15d, dword ptr [rbp - 0x44]
0x2fdba19: 3401                     xor al, 1
0x2fdba1b: 0fb6c0                   movzx eax, al
0x2fdba1e: 4101c7                   add r15d, eax
0x2fdba21: 488b45b0                 mov rax, qword ptr [rbp - 0x50]
0x2fdba25: 41ffc4                   inc r12d
0x2fdba28: 4883c308                 add rbx, 8
0x2fdba2c: 44396008                 cmp dword ptr [rax + 8], r12d
0x2fdba30: 7e68                     jle 0x2fdba9a
0x2fdba32: 488b38                   mov rdi, qword ptr [rax]
0x2fdba35: 4801df                   add rdi, rbx
0x2fdba38: e8c3b24dfd               call 0x4b6d00
0x2fdba3d: 4885c0                   test rax, rax
0x2fdba40: 74df                     je 0x2fdba21
0x2fdba42: 4883b8b002000000         cmp qword ptr [rax + 0x2b0], 0
0x2fdba4a: 4989c5                   mov r13, rax
0x2fdba4d: 74d2                     je 0x2fdba21
0x2fdba4f: 488b354a675406           mov rsi, qword ptr [rip + 0x654674a]
0x2fdba56: 4c89f7                   mov rdi, r14
0x2fdba59: 44897dbc                 mov dword ptr [rbp - 0x44], r15d
0x2fdba5d: e8dedc47fd               call 0x459740
0x2fdba62: 4889c7                   mov rdi, rax
0x2fdba65: 4989c7                   mov r15, rax
0x2fdba68: e893e733fd               call 0x31a200
0x2fdba6d: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdba74: 7590                     jne 0x2fdba06
0x2fdba76: c645c800                 mov byte ptr [rbp - 0x38], 0
0x2fdba7a: 4c896dc0                 mov qword ptr [rbp - 0x40], r13
0x2fdba7e: 4c89f7                   mov rdi, r14
0x2fdba81: 4c89fe                   mov rsi, r15
0x2fdba84: 488d55c0                 lea rdx, [rbp - 0x40]
0x2fdba88: 498b06                   mov rax, qword ptr [r14]
0x2fdba8b: ff9068020000             call qword ptr [rax + 0x268]
0x2fdba91: 0fb645c8                 movzx eax, byte ptr [rbp - 0x38]
0x2fdba95: e97bffffff               jmp 0x2fdba15
0x2fdba9a: 488b0567641506           mov rax, qword ptr [rip + 0x6156467]
0x2fdbaa1: 488b00                   mov rax, qword ptr [rax]
0x2fdbaa4: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdbaa8: 7512                     jne 0x2fdbabc
0x2fdbaaa: 4489f8                   mov eax, r15d
0x2fdbaad: 4883c428                 add rsp, 0x28
0x2fdbab1: 5b                       pop rbx
0x2fdbab2: 415c                     pop r12
0x2fdbab4: 415d                     pop r13
0x2fdbab6: 415e                     pop r14
0x2fdbab8: 415f                     pop r15
0x2fdbaba: 5d                       pop rbp
0x2fdbabb: c3                       ret 
0x2fdbabc: e89f634904               call 0x7471e60
0x2fdbac1: 0f0b                     ud2 
0x2fdbac3: cc                       int3 
0x2fdbac4: cc                       int3 
0x2fdbac5: cc                       int3 
0x2fdbac6: cc                       int3 
0x2fdbac7: cc                       int3 
0x2fdbac8: cc                       int3 
0x2fdbac9: cc                       int3 
0x2fdbaca: cc                       int3 
0x2fdbacb: cc                       int3 
0x2fdbacc: cc                       int3 
0x2fdbacd: cc                       int3 
0x2fdbace: cc                       int3 
0x2fdbacf: cc                       int3 

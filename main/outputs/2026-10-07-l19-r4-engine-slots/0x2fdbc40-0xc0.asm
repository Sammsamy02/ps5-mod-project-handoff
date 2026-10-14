0x2fdbc40: 55                       push rbp
0x2fdbc41: 4889e5                   mov rbp, rsp
0x2fdbc44: 4157                     push r15
0x2fdbc46: 4156                     push r14
0x2fdbc48: 4154                     push r12
0x2fdbc4a: 53                       push rbx
0x2fdbc4b: 488b07                   mov rax, qword ptr [rdi]
0x2fdbc4e: 4989d7                   mov r15, rdx
0x2fdbc51: 4889f3                   mov rbx, rsi
0x2fdbc54: 4989fe                   mov r14, rdi
0x2fdbc57: ff9000080000             call qword ptr [rax + 0x800]
0x2fdbc5d: 84c0                     test al, al
0x2fdbc5f: 0f8489000000             je 0x2fdbcee
0x2fdbc65: 4189c4                   mov r12d, eax
0x2fdbc68: 418b8638030000           mov eax, dword ptr [r14 + 0x338]
0x2fdbc6f: 413b863c030000           cmp eax, dword ptr [r14 + 0x33c]
0x2fdbc76: 7518                     jne 0x2fdbc90
0x2fdbc78: 498db630030000           lea rsi, [r14 + 0x330]
0x2fdbc7f: 498d963c030000           lea rdx, [r14 + 0x33c]
0x2fdbc86: bf10080000               mov edi, 0x810
0x2fdbc8b: e810dc04fd               call 0x298a0
0x2fdbc90: 8d4801                   lea ecx, [rax + 1]
0x2fdbc93: 4898                     cdqe 
0x2fdbc95: 4c89fe                   mov rsi, r15
0x2fdbc98: 41898e38030000           mov dword ptr [r14 + 0x338], ecx
0x2fdbc9f: 48c1e004                 shl rax, 4
0x2fdbca3: 498b8e30030000           mov rcx, qword ptr [r14 + 0x330]
0x2fdbcaa: 488d3c01                 lea rdi, [rcx + rax]
0x2fdbcae: 48c7040100000000         mov qword ptr [rcx + rax], 0
0x2fdbcb6: c744010800000000         mov dword ptr [rcx + rax + 8], 0
0x2fdbcbe: e84d7f05fd               call 0x33c10
0x2fdbcc3: 4c89f7                   mov rdi, r14
0x2fdbcc6: e895b4b5ff               call 0x2b37160
0x2fdbccb: 4883b8a804000000         cmp qword ptr [rax + 0x4a8], 0
0x2fdbcd3: 4889c7                   mov rdi, rax
0x2fdbcd6: 4489e0                   mov eax, r12d
0x2fdbcd9: 7513                     jne 0x2fdbcee
0x2fdbcdb: 488bb3b0020000           mov rsi, qword ptr [rbx + 0x2b0]
0x2fdbce2: 488b07                   mov rax, qword ptr [rdi]
0x2fdbce5: ff90e0070000             call qword ptr [rax + 0x7e0]
0x2fdbceb: 4489e0                   mov eax, r12d
0x2fdbcee: 5b                       pop rbx
0x2fdbcef: 415c                     pop r12
0x2fdbcf1: 415e                     pop r14
0x2fdbcf3: 415f                     pop r15
0x2fdbcf5: 5d                       pop rbp
0x2fdbcf6: c3                       ret 
0x2fdbcf7: cc                       int3 
0x2fdbcf8: cc                       int3 
0x2fdbcf9: cc                       int3 
0x2fdbcfa: cc                       int3 
0x2fdbcfb: cc                       int3 
0x2fdbcfc: cc                       int3 
0x2fdbcfd: cc                       int3 
0x2fdbcfe: cc                       int3 
0x2fdbcff: cc                       int3 

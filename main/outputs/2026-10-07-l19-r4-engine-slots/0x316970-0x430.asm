0x316970: 55                       push rbp
0x316971: 4889e5                   mov rbp, rsp
0x316974: 4157                     push r15
0x316976: 4156                     push r14
0x316978: 4154                     push r12
0x31697a: 53                       push rbx
0x31697b: 4881eca0000000           sub rsp, 0xa0
0x316982: 4c8b257fb5e108           mov r12, qword ptr [rip + 0x8e1b57f]
0x316989: 4c8dbd48ffffff           lea r15, [rbp - 0xb8]
0x316990: 4989fe                   mov r14, rdi
0x316993: 89f3                     mov ebx, esi
0x316995: 4c89ff                   mov rdi, r15
0x316998: 498b0424                 mov rax, qword ptr [r12]
0x31699c: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x3169a0: e85b44f0ff               call 0x21ae00
0x3169a5: 488d0524edbb07           lea rax, [rip + 0x7bbed24]
0x3169ac: 4c89f7                   mov rdi, r14
0x3169af: 4c89fe                   mov rsi, r15
0x3169b2: 89da                     mov edx, ebx
0x3169b4: 48898548ffffff           mov qword ptr [rbp - 0xb8], rax
0x3169bb: 498b06                   mov rax, qword ptr [r14]
0x3169be: ff90d0020000             call qword ptr [rax + 0x2d0]
0x3169c4: 4c89ff                   mov rdi, r15
0x3169c7: e8444bf0ff               call 0x21b510
0x3169cc: 498b0424                 mov rax, qword ptr [r12]
0x3169d0: 483b45d8                 cmp rax, qword ptr [rbp - 0x28]
0x3169d4: 7510                     jne 0x3169e6
0x3169d6: 4881c4a0000000           add rsp, 0xa0
0x3169dd: 5b                       pop rbx
0x3169de: 415c                     pop r12
0x3169e0: 415e                     pop r14
0x3169e2: 415f                     pop r15
0x3169e4: 5d                       pop rbp
0x3169e5: c3                       ret 
0x3169e6: e875b41507               call 0x7471e60
0x3169eb: 0f0b                     ud2 
0x3169ed: cc                       int3 
0x3169ee: cc                       int3 
0x3169ef: cc                       int3 
0x3169f0: 55                       push rbp
0x3169f1: 4889e5                   mov rbp, rsp
0x3169f4: 53                       push rbx
0x3169f5: 50                       push rax
0x3169f6: 4889fb                   mov rbx, rdi
0x3169f9: e862000000               call 0x316a60
0x3169fe: b801000000               mov eax, 1
0x316a03: f00fc183d0000000         lock xadd dword ptr [rbx + 0xd0], eax
0x316a0b: 85c0                     test eax, eax
0x316a0d: 7407                     je 0x316a16
0x316a0f: 4883c408                 add rsp, 8
0x316a13: 5b                       pop rbx
0x316a14: 5d                       pop rbp
0x316a15: c3                       ret 
0x316a16: 8b430c                   mov eax, dword ptr [rbx + 0xc]
0x316a19: 488d0df0360a09           lea rcx, [rip + 0x90a36f0]
0x316a20: 394124                   cmp dword ptr [rcx + 0x24], eax
0x316a23: 7e19                     jle 0x316a3e
0x316a25: 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x316a29: 89c2                     mov edx, eax
0x316a2b: 0fb7c0                   movzx eax, ax
0x316a2e: 488d3c40                 lea rdi, [rax + rax*2]
0x316a32: c1ea10                   shr edx, 0x10
0x316a35: c1e703                   shl edi, 3
0x316a38: 48033cd1                 add rdi, qword ptr [rcx + rdx*8]
0x316a3c: eb02                     jmp 0x316a40
0x316a3e: 31ff                     xor edi, edi
0x316a40: be00000040               mov esi, 0x40000000
0x316a45: e8261a0500               call 0x368470
0x316a4a: 4883c408                 add rsp, 8
0x316a4e: 5b                       pop rbx
0x316a4f: 5d                       pop rbp
0x316a50: c3                       ret 
0x316a51: cc                       int3 
0x316a52: cc                       int3 
0x316a53: cc                       int3 
0x316a54: cc                       int3 
0x316a55: cc                       int3 
0x316a56: cc                       int3 
0x316a57: cc                       int3 
0x316a58: cc                       int3 
0x316a59: cc                       int3 
0x316a5a: cc                       int3 
0x316a5b: cc                       int3 
0x316a5c: cc                       int3 
0x316a5d: cc                       int3 
0x316a5e: cc                       int3 
0x316a5f: cc                       int3 
0x316a60: 55                       push rbp
0x316a61: 4889e5                   mov rbp, rsp
0x316a64: 4157                     push r15
0x316a66: 4156                     push r14
0x316a68: 4155                     push r13
0x316a6a: 4154                     push r12
0x316a6c: 53                       push rbx
0x316a6d: 4883ec18                 sub rsp, 0x18
0x316a71: 8b4758                   mov eax, dword ptr [rdi + 0x58]
0x316a74: 440fbf775c               movsx r14d, word ptr [rdi + 0x5c]
0x316a79: 4989ff                   mov r15, rdi
0x316a7c: 4189d5                   mov r13d, edx
0x316a7f: 4889f7                   mov rdi, rsi
0x316a82: 488975c0                 mov qword ptr [rbp - 0x40], rsi
0x316a86: 31db                     xor ebx, ebx
0x316a88: 31f6                     xor esi, esi
0x316a8a: 428d4430ff               lea eax, [rax + r14 - 1]
0x316a8f: 41f7de                   neg r14d
0x316a92: 4121c6                   and r14d, eax
0x316a95: 4489f0                   mov eax, r14d
0x316a98: 0fafc2                   imul eax, edx
0x316a9b: 4863d0                   movsxd rdx, eax
0x316a9e: e89db31507               call 0x7471e40
0x316aa3: 4d8ba7b8000000           mov r12, qword ptr [r15 + 0xb8]
0x316aaa: 4d85e4                   test r12, r12
0x316aad: 7453                     je 0x316b02
0x316aaf: 498b0424                 mov rax, qword ptr [r12]
0x316ab3: 4c89e7                   mov rdi, r12
0x316ab6: 4c897dc8                 mov qword ptr [rbp - 0x38], r15
0x316aba: ff5010                   call qword ptr [rax + 0x10]
0x316abd: 48c1ea21                 shr rdx, 0x21
0x316ac1: 44896dd4                 mov dword ptr [rbp - 0x2c], r13d
0x316ac5: 83e201                   and edx, 1
0x316ac8: 4585ed                   test r13d, r13d
0x316acb: 0f9ec0                   setle al
0x316ace: 08d0                     or al, dl
0x316ad0: 7523                     jne 0x316af5
0x316ad2: 448b7dd4                 mov r15d, dword ptr [rbp - 0x2c]
0x316ad6: 4c8b6dc0                 mov r13, qword ptr [rbp - 0x40]
0x316ada: 4963de                   movsxd rbx, r14d
0x316add: 0f1f00                   nop dword ptr [rax]
0x316ae0: 498b0424                 mov rax, qword ptr [r12]
0x316ae4: 4c89e7                   mov rdi, r12
0x316ae7: 4c89ee                   mov rsi, r13
0x316aea: ff5018                   call qword ptr [rax + 0x18]
0x316aed: 4901dd                   add r13, rbx
0x316af0: 49ffcf                   dec r15
0x316af3: 75eb                     jne 0x316ae0
0x316af5: 418b5c2408               mov ebx, dword ptr [r12 + 8]
0x316afa: 448b6dd4                 mov r13d, dword ptr [rbp - 0x2c]
0x316afe: 4c8b7dc8                 mov r15, qword ptr [rbp - 0x38]
0x316b02: 895dd4                   mov dword ptr [rbp - 0x2c], ebx
0x316b05: 41395f58                 cmp dword ptr [r15 + 0x58], ebx
0x316b09: 0f8e8f000000             jle 0x316b9e
0x316b0f: 4d8b7f70                 mov r15, qword ptr [r15 + 0x70]
0x316b13: 4d85ff                   test r15, r15
0x316b16: 0f94c0                   sete al
0x316b19: 4585ed                   test r13d, r13d
0x316b1c: 0f9ec1                   setle cl
0x316b1f: 08c1                     or cl, al
0x316b21: 757b                     jne 0x316b9e
0x316b23: 4d63f6                   movsxd r14, r14d
0x316b26: 4489e8                   mov eax, r13d
0x316b29: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x316b2d: 0f1f00                   nop dword ptr [rax]
0x316b30: 418b5f30                 mov ebx, dword ptr [r15 + 0x30]
0x316b34: 410faf5f2c               imul ebx, dword ptr [r15 + 0x2c]
0x316b39: 4c8b65c8                 mov r12, qword ptr [rbp - 0x38]
0x316b3d: 4c8b6dc0                 mov r13, qword ptr [rbp - 0x40]
0x316b41: 41035f44                 add ebx, dword ptr [r15 + 0x44]
0x316b45: 3b5dd4                   cmp ebx, dword ptr [rbp - 0x2c]
0x316b48: 7f1a                     jg 0x316b64
0x316b4a: eb52                     jmp 0x316b9e
0x316b4c: 0f1f4000                 nop dword ptr [rax]
0x316b50: 498b07                   mov rax, qword ptr [r15]
0x316b53: 4c89ff                   mov rdi, r15
0x316b56: ff9058010000             call qword ptr [rax + 0x158]
0x316b5c: 4d01f5                   add r13, r14
0x316b5f: 49ffcc                   dec r12
0x316b62: 742c                     je 0x316b90
0x316b64: 49637744                 movsxd rsi, dword ptr [r15 + 0x44]
0x316b68: 4c01ee                   add rsi, r13
0x316b6b: 41f6473902               test byte ptr [r15 + 0x39], 2
0x316b70: 74de                     je 0x316b50
0x316b72: 49634730                 movsxd rax, dword ptr [r15 + 0x30]
0x316b76: 4963572c                 movsxd rdx, dword ptr [r15 + 0x2c]
0x316b7a: 4889f7                   mov rdi, rsi
0x316b7d: 31f6                     xor esi, esi
0x316b7f: 480fafd0                 imul rdx, rax
0x316b83: e8b8b21507               call 0x7471e40
0x316b88: 4d01f5                   add r13, r14
0x316b8b: 49ffcc                   dec r12
0x316b8e: 75d4                     jne 0x316b64
0x316b90: 4d8b7f48                 mov r15, qword ptr [r15 + 0x48]
0x316b94: 4d85ff                   test r15, r15
0x316b97: 7405                     je 0x316b9e
0x316b99: 3b5dd4                   cmp ebx, dword ptr [rbp - 0x2c]
0x316b9c: 7f92                     jg 0x316b30
0x316b9e: 4883c418                 add rsp, 0x18
0x316ba2: 5b                       pop rbx
0x316ba3: 415c                     pop r12
0x316ba5: 415d                     pop r13
0x316ba7: 415e                     pop r14
0x316ba9: 415f                     pop r15
0x316bab: 5d                       pop rbp
0x316bac: c3                       ret 
0x316bad: cc                       int3 
0x316bae: cc                       int3 
0x316baf: cc                       int3 
0x316bb0: 55                       push rbp
0x316bb1: 4889e5                   mov rbp, rsp
0x316bb4: 53                       push rbx
0x316bb5: 50                       push rax
0x316bb6: 4889fb                   mov rbx, rdi
0x316bb9: e852000000               call 0x316c10
0x316bbe: f0ff8bd0000000           lock dec dword ptr [rbx + 0xd0]
0x316bc5: 7534                     jne 0x316bfb
0x316bc7: 8b430c                   mov eax, dword ptr [rbx + 0xc]
0x316bca: 488d0d3f350a09           lea rcx, [rip + 0x90a353f]
0x316bd1: 394124                   cmp dword ptr [rcx + 0x24], eax
0x316bd4: 7e19                     jle 0x316bef
0x316bd6: 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x316bda: 89c2                     mov edx, eax
0x316bdc: 0fb7c0                   movzx eax, ax
0x316bdf: 488d3c40                 lea rdi, [rax + rax*2]
0x316be3: c1ea10                   shr edx, 0x10
0x316be6: c1e703                   shl edi, 3
0x316be9: 48033cd1                 add rdi, qword ptr [rcx + rdx*8]
0x316bed: eb02                     jmp 0x316bf1
0x316bef: 31ff                     xor edi, edi
0x316bf1: be00000040               mov esi, 0x40000000
0x316bf6: e895190500               call 0x368590
0x316bfb: 4883c408                 add rsp, 8
0x316bff: 5b                       pop rbx
0x316c00: 5d                       pop rbp
0x316c01: c3                       ret 
0x316c02: cc                       int3 
0x316c03: cc                       int3 
0x316c04: cc                       int3 
0x316c05: cc                       int3 
0x316c06: cc                       int3 
0x316c07: cc                       int3 
0x316c08: cc                       int3 
0x316c09: cc                       int3 
0x316c0a: cc                       int3 
0x316c0b: cc                       int3 
0x316c0c: cc                       int3 
0x316c0d: cc                       int3 
0x316c0e: cc                       int3 
0x316c0f: cc                       int3 
0x316c10: 55                       push rbp
0x316c11: 4889e5                   mov rbp, rsp
0x316c14: 4157                     push r15
0x316c16: 4156                     push r14
0x316c18: 4155                     push r13
0x316c1a: 4154                     push r12
0x316c1c: 53                       push rbx
0x316c1d: 4883ec28                 sub rsp, 0x28
0x316c21: f687b100000060           test byte ptr [rdi + 0xb1], 0x60
0x316c28: 488975c0                 mov qword ptr [rbp - 0x40], rsi
0x316c2c: 0f8527010000             jne 0x316d59
0x316c32: 8b4758                   mov eax, dword ptr [rdi + 0x58]
0x316c35: 440fbf6f5c               movsx r13d, word ptr [rdi + 0x5c]
0x316c3a: 4c8bbfb8000000           mov r15, qword ptr [rdi + 0xb8]
0x316c41: 428d4c28ff               lea ecx, [rax + r13 - 1]
0x316c46: 41f7dd                   neg r13d
0x316c49: 4121cd                   and r13d, ecx
0x316c4c: 4d85ff                   test r15, r15
0x316c4f: 745a                     je 0x316cab
0x316c51: 498b07                   mov rax, qword ptr [r15]
0x316c54: 48897dc8                 mov qword ptr [rbp - 0x38], rdi
0x316c58: 4c89ff                   mov rdi, r15
0x316c5b: 89d3                     mov ebx, edx
0x316c5d: ff5010                   call qword ptr [rax + 0x10]
0x316c60: 480fbae222               bt rdx, 0x22
0x316c65: 895dd4                   mov dword ptr [rbp - 0x2c], ebx
0x316c68: 0f93c0                   setae al
0x316c6b: 85db                     test ebx, ebx
0x316c6d: 0f9ec1                   setle cl
0x316c70: 08c1                     or cl, al
0x316c72: 7520                     jne 0x316c94
0x316c74: 8b5dd4                   mov ebx, dword ptr [rbp - 0x2c]
0x316c77: 4c8b65c0                 mov r12, qword ptr [rbp - 0x40]
0x316c7b: 4d63f5                   movsxd r14, r13d
0x316c7e: 6690                     nop 
0x316c80: 498b07                   mov rax, qword ptr [r15]
0x316c83: 4c89ff                   mov rdi, r15
0x316c86: 4c89e6                   mov rsi, r12
0x316c89: ff5028                   call qword ptr [rax + 0x28]
0x316c8c: 4d01f4                   add r12, r14
0x316c8f: 48ffcb                   dec rbx
0x316c92: 75ec                     jne 0x316c80
0x316c94: 488b7dc8                 mov rdi, qword ptr [rbp - 0x38]
0x316c98: 418b7708                 mov esi, dword ptr [r15 + 8]
0x316c9c: 8b55d4                   mov edx, dword ptr [rbp - 0x2c]
0x316c9f: 8b4758                   mov eax, dword ptr [rdi + 0x58]
0x316ca2: 39f0                     cmp eax, esi
0x316ca4: 7f0f                     jg 0x316cb5
0x316ca6: e9ae000000               jmp 0x316d59
0x316cab: 31f6                     xor esi, esi
0x316cad: 39f0                     cmp eax, esi
0x316caf: 0f8ea4000000             jle 0x316d59
0x316cb5: 4c8bbf80000000           mov r15, qword ptr [rdi + 0x80]
0x316cbc: 4d85ff                   test r15, r15
0x316cbf: 0f94c0                   sete al
0x316cc2: 85d2                     test edx, edx
0x316cc4: 0f9ec1                   setle cl
0x316cc7: 08c1                     or cl, al
0x316cc9: 0f858a000000             jne 0x316d59
0x316ccf: 89d2                     mov edx, edx
0x316cd1: 48b80000000010000000     movabs rax, 0x1000000000
0x316cdb: 4963dd                   movsxd rbx, r13d
0x316cde: 8975c8                   mov dword ptr [rbp - 0x38], esi
0x316ce1: 488955b8                 mov qword ptr [rbp - 0x48], rdx
0x316ce5: 66662e0f1f840000000000   nop word ptr cs:[rax + rax]
0x316cf0: 418b5730                 mov edx, dword ptr [r15 + 0x30]
0x316cf4: 410faf572c               imul edx, dword ptr [r15 + 0x2c]
0x316cf9: 41035744                 add edx, dword ptr [r15 + 0x44]
0x316cfd: 39f2                     cmp edx, esi
0x316cff: 7e58                     jle 0x316d59
0x316d01: 4c8b65b8                 mov r12, qword ptr [rbp - 0x48]
0x316d05: 4c8b75c0                 mov r14, qword ptr [rbp - 0x40]
0x316d09: 8955d4                   mov dword ptr [rbp - 0x2c], edx
0x316d0c: 49854738                 test qword ptr [r15 + 0x38], rax
0x316d10: 7426                     je 0x316d38
0x316d12: 4d8b7f58                 mov r15, qword ptr [r15 + 0x58]
0x316d16: 8b75c8                   mov esi, dword ptr [rbp - 0x38]
0x316d19: 8b55d4                   mov edx, dword ptr [rbp - 0x2c]
0x316d1c: 4d85ff                   test r15, r15
0x316d1f: 7438                     je 0x316d59
0x316d21: 39f2                     cmp edx, esi
0x316d23: 7fcb                     jg 0x316cf0
0x316d25: eb32                     jmp 0x316d59
0x316d27: 660f1f840000000000       nop word ptr [rax + rax]
0x316d30: 4901de                   add r14, rbx
0x316d33: 49ffcc                   dec r12
0x316d36: 74da                     je 0x316d12
0x316d38: 49854738                 test qword ptr [r15 + 0x38], rax
0x316d3c: 75f2                     jne 0x316d30
0x316d3e: 49637744                 movsxd rsi, dword ptr [r15 + 0x44]
0x316d42: 4989c5                   mov r13, rax
0x316d45: 498b07                   mov rax, qword ptr [r15]
0x316d48: 4c89ff                   mov rdi, r15
0x316d4b: 4c01f6                   add rsi, r14
0x316d4e: ff9040010000             call qword ptr [rax + 0x140]
0x316d54: 4c89e8                   mov rax, r13
0x316d57: ebd7                     jmp 0x316d30
0x316d59: 4883c428                 add rsp, 0x28
0x316d5d: 5b                       pop rbx
0x316d5e: 415c                     pop r12
0x316d60: 415d                     pop r13
0x316d62: 415e                     pop r14
0x316d64: 415f                     pop r15
0x316d66: 5d                       pop rbp
0x316d67: c3                       ret 
0x316d68: cc                       int3 
0x316d69: cc                       int3 
0x316d6a: cc                       int3 
0x316d6b: cc                       int3 
0x316d6c: cc                       int3 
0x316d6d: cc                       int3 
0x316d6e: cc                       int3 
0x316d6f: cc                       int3 
0x316d70: 55                       push rbp
0x316d71: 4889e5                   mov rbp, rsp
0x316d74: 53                       push rbx
0x316d75: 50                       push rax
0x316d76: 4889fb                   mov rbx, rdi
0x316d79: 8b87d0000000             mov eax, dword ptr [rdi + 0xd0]
0x316d7f: 488bbfa8000000           mov rdi, qword ptr [rdi + 0xa8]
0x316d86: 4885ff                   test rdi, rdi
0x316d89: 7405                     je 0x316d90
0x316d8b: e8000ddaff               call 0xb7a90
0x316d90: 31c0                     xor eax, eax
0x316d92: 488783a8000000           xchg qword ptr [rbx + 0xa8], rax
0x316d99: c7436800000000           mov dword ptr [rbx + 0x68], 0

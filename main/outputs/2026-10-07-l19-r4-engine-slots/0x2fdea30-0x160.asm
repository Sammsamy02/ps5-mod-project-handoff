0x2fdea30: 55                       push rbp
0x2fdea31: 4889e5                   mov rbp, rsp
0x2fdea34: 4157                     push r15
0x2fdea36: 4156                     push r14
0x2fdea38: 4154                     push r12
0x2fdea3a: 53                       push rbx
0x2fdea3b: 4883ec20                 sub rsp, 0x20
0x2fdea3f: 4c8b25c2341506           mov r12, qword ptr [rip + 0x61534c2]
0x2fdea46: 498b0424                 mov rax, qword ptr [r12]
0x2fdea4a: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x2fdea4e: 4885f6                   test rsi, rsi
0x2fdea51: 0f8413010000             je 0x2fdeb6a
0x2fdea57: 4889f3                   mov rbx, rsi
0x2fdea5a: 4989fe                   mov r14, rdi
0x2fdea5d: e85e8b4400               call 0x34275c0
0x2fdea62: 488b4b10                 mov rcx, qword ptr [rbx + 0x10]
0x2fdea66: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdea6a: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdea6d: 0f8ff7000000             jg 0x2fdeb6a
0x2fdea73: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdea77: 4883c030                 add rax, 0x30
0x2fdea7b: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdea7f: 0f85e5000000             jne 0x2fdeb6a
0x2fdea85: 488b359c365406           mov rsi, qword ptr [rip + 0x654369c]
0x2fdea8c: 4c89f7                   mov rdi, r14
0x2fdea8f: e8acac47fd               call 0x459740
0x2fdea94: 4889c7                   mov rdi, rax
0x2fdea97: 4989c7                   mov r15, rax
0x2fdea9a: e861b733fd               call 0x31a200
0x2fdea9f: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdeaa6: 7519                     jne 0x2fdeac1
0x2fdeaa8: 48895dd0                 mov qword ptr [rbp - 0x30], rbx
0x2fdeaac: 488d55d0                 lea rdx, [rbp - 0x30]
0x2fdeab0: 4c89f7                   mov rdi, r14
0x2fdeab3: 4c89fe                   mov rsi, r15
0x2fdeab6: 498b06                   mov rax, qword ptr [r14]
0x2fdeab9: ff9068020000             call qword ptr [rax + 0x268]
0x2fdeabf: eb0f                     jmp 0x2fdead0
0x2fdeac1: 498b06                   mov rax, qword ptr [r14]
0x2fdeac4: 4c89f7                   mov rdi, r14
0x2fdeac7: 4889de                   mov rsi, rbx
0x2fdeaca: ff9050070000             call qword ptr [rax + 0x750]
0x2fdead0: 498b06                   mov rax, qword ptr [r14]
0x2fdead3: 4c89f7                   mov rdi, r14
0x2fdead6: 4889de                   mov rsi, rbx
0x2fdead9: ff9000090000             call qword ptr [rax + 0x900]
0x2fdeadf: 498bbe00030000           mov rdi, qword ptr [r14 + 0x300]
0x2fdeae6: 4885ff                   test rdi, rdi
0x2fdeae9: 741a                     je 0x2fdeb05
0x2fdeaeb: 488b07                   mov rax, qword ptr [rdi]
0x2fdeaee: ff9008080000             call qword ptr [rax + 0x808]
0x2fdeaf4: 488b0b                   mov rcx, qword ptr [rbx]
0x2fdeaf7: 3401                     xor al, 1
0x2fdeaf9: 4889df                   mov rdi, rbx
0x2fdeafc: 0fb6f0                   movzx esi, al
0x2fdeaff: ff91e80c0000             call qword ptr [rcx + 0xce8]
0x2fdeb05: 498b06                   mov rax, qword ptr [r14]
0x2fdeb08: 4c89f7                   mov rdi, r14
0x2fdeb0b: 4889de                   mov rsi, rbx
0x2fdeb0e: ff9018090000             call qword ptr [rax + 0x918]
0x2fdeb14: c645d000                 mov byte ptr [rbp - 0x30], 0
0x2fdeb18: c645cf00                 mov byte ptr [rbp - 0x31], 0
0x2fdeb1c: c645ce00                 mov byte ptr [rbp - 0x32], 0
0x2fdeb20: c645cd00                 mov byte ptr [rbp - 0x33], 0
0x2fdeb24: 488d55d0                 lea rdx, [rbp - 0x30]
0x2fdeb28: 488d4dcf                 lea rcx, [rbp - 0x31]
0x2fdeb2c: 4c8d45ce                 lea r8, [rbp - 0x32]
0x2fdeb30: 4c8d4dcd                 lea r9, [rbp - 0x33]
0x2fdeb34: 4c89f7                   mov rdi, r14
0x2fdeb37: 4889de                   mov rsi, rbx
0x2fdeb3a: 498b06                   mov rax, qword ptr [r14]
0x2fdeb3d: ff90f8080000             call qword ptr [rax + 0x8f8]
0x2fdeb43: 84c0                     test al, al
0x2fdeb45: 7423                     je 0x2fdeb6a
0x2fdeb47: 0fb655d0                 movzx edx, byte ptr [rbp - 0x30]
0x2fdeb4b: 0fb64dcf                 movzx ecx, byte ptr [rbp - 0x31]
0x2fdeb4f: 440fb645ce               movzx r8d, byte ptr [rbp - 0x32]
0x2fdeb54: 440fb64dcd               movzx r9d, byte ptr [rbp - 0x33]
0x2fdeb59: 488b03                   mov rax, qword ptr [rbx]
0x2fdeb5c: 4889df                   mov rdi, rbx
0x2fdeb5f: be01000000               mov esi, 1
0x2fdeb64: ff90080f0000             call qword ptr [rax + 0xf08]
0x2fdeb6a: 498b0424                 mov rax, qword ptr [r12]
0x2fdeb6e: 483b45d8                 cmp rax, qword ptr [rbp - 0x28]
0x2fdeb72: 750d                     jne 0x2fdeb81
0x2fdeb74: 4883c420                 add rsp, 0x20
0x2fdeb78: 5b                       pop rbx
0x2fdeb79: 415c                     pop r12
0x2fdeb7b: 415e                     pop r14
0x2fdeb7d: 415f                     pop r15
0x2fdeb7f: 5d                       pop rbp
0x2fdeb80: c3                       ret 
0x2fdeb81: e8da324904               call 0x7471e60
0x2fdeb86: 0f0b                     ud2 
0x2fdeb88: cc                       int3 
0x2fdeb89: cc                       int3 
0x2fdeb8a: cc                       int3 
0x2fdeb8b: cc                       int3 
0x2fdeb8c: cc                       int3 
0x2fdeb8d: cc                       int3 
0x2fdeb8e: cc                       int3 
0x2fdeb8f: cc                       int3 

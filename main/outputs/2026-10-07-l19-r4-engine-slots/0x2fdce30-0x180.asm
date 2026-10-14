0x2fdce30: 55                       push rbp
0x2fdce31: 4889e5                   mov rbp, rsp
0x2fdce34: 4157                     push r15
0x2fdce36: 4156                     push r14
0x2fdce38: 4155                     push r13
0x2fdce3a: 4154                     push r12
0x2fdce3c: 53                       push rbx
0x2fdce3d: 4883ec28                 sub rsp, 0x28
0x2fdce41: 4c8b35c0501506           mov r14, qword ptr [rip + 0x61550c0]
0x2fdce48: 498b06                   mov rax, qword ptr [r14]
0x2fdce4b: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdce4f: 4885f6                   test rsi, rsi
0x2fdce52: 0f846f010000             je 0x2fdcfc7
0x2fdce58: 8b4608                   mov eax, dword ptr [rsi + 8]
0x2fdce5b: 4885d2                   test rdx, rdx
0x2fdce5e: 0f8463010000             je 0x2fdcfc7
0x2fdce64: 2500000040               and eax, 0x40000000
0x2fdce69: 0f8558010000             jne 0x2fdcfc7
0x2fdce6f: 8b4208                   mov eax, dword ptr [rdx + 8]
0x2fdce72: a900000040               test eax, 0x40000000
0x2fdce77: 0f854a010000             jne 0x2fdcfc7
0x2fdce7d: 488b9e48030000           mov rbx, qword ptr [rsi + 0x348]
0x2fdce84: 4885db                   test rbx, rbx
0x2fdce87: 0f843a010000             je 0x2fdcfc7
0x2fdce8d: 0fb686b9040000           movzx eax, byte ptr [rsi + 0x4b9]
0x2fdce94: 488d0dc11f3c06           lea rcx, [rip + 0x63c1fc1]
0x2fdce9b: 4989ff                   mov r15, rdi
0x2fdce9e: 488975b8                 mov qword ptr [rbp - 0x48], rsi
0x2fdcea2: 8882b9040000             mov byte ptr [rdx + 0x4b9], al
0x2fdcea8: 803901                   cmp byte ptr [rcx], 1
0x2fdceab: 488b8620050000           mov rax, qword ptr [rsi + 0x520]
0x2fdceb2: 7522                     jne 0x2fdced6
0x2fdceb4: 4885c0                   test rax, rax
0x2fdceb7: 741b                     je 0x2fdced4
0x2fdceb9: 4889c7                   mov rdi, rax
0x2fdcebc: 4989d4                   mov r12, rdx
0x2fdcebf: e8fcb338fd               call 0x3682c0
0x2fdcec4: 488b75b8                 mov rsi, qword ptr [rbp - 0x48]
0x2fdcec8: 4c89e2                   mov rdx, r12
0x2fdcecb: 488b8620050000           mov rax, qword ptr [rsi + 0x520]
0x2fdced2: eb02                     jmp 0x2fdced6
0x2fdced4: 31c0                     xor eax, eax
0x2fdced6: 48898220050000           mov qword ptr [rdx + 0x520], rax
0x2fdcedd: b81b010000               mov eax, 0x11b
0x2fdcee2: 4889d7                   mov rdi, rdx
0x2fdcee5: 4989f4                   mov r12, rsi
0x2fdcee8: 4989d5                   mov r13, rdx
0x2fdceeb: c4e278f74658             bextr eax, dword ptr [rsi + 0x58], eax
0x2fdcef1: 89c6                     mov esi, eax
0x2fdcef3: e8d880b4ff               call 0x2b24fd0
0x2fdcef8: 498b4500                 mov rax, qword ptr [r13]
0x2fdcefc: 4c89ef                   mov rdi, r13
0x2fdceff: 4889de                   mov rsi, rbx
0x2fdcf02: ff90280e0000             call qword ptr [rax + 0xe28]
0x2fdcf08: 4c89ef                   mov rdi, r13
0x2fdcf0b: 4c89e6                   mov rsi, r12
0x2fdcf0e: e89d8eb5ff               call 0x2b35db0
0x2fdcf13: 488b3566525406           mov rsi, qword ptr [rip + 0x6545266]
0x2fdcf1a: 4c89ff                   mov rdi, r15
0x2fdcf1d: 4c8965c0                 mov qword ptr [rbp - 0x40], r12
0x2fdcf21: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x2fdcf25: e816c847fd               call 0x459740
0x2fdcf2a: 498b0f                   mov rcx, qword ptr [r15]
0x2fdcf2d: 488d55c0                 lea rdx, [rbp - 0x40]
0x2fdcf31: 4c89ff                   mov rdi, r15
0x2fdcf34: 4889c6                   mov rsi, rax
0x2fdcf37: ff9168020000             call qword ptr [rcx + 0x268]
0x2fdcf3d: e85e081d00               call 0x31ad7a0
0x2fdcf42: 488b4b10                 mov rcx, qword ptr [rbx + 0x10]
0x2fdcf46: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdcf4a: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdcf4d: 7f0e                     jg 0x2fdcf5d
0x2fdcf4f: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdcf53: 4883c030                 add rax, 0x30
0x2fdcf57: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdcf5b: 7430                     je 0x2fdcf8d
0x2fdcf5d: e82e4f2500               call 0x3231e90
0x2fdcf62: 488b4b10                 mov rcx, qword ptr [rbx + 0x10]
0x2fdcf66: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdcf6a: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdcf6d: 7e08                     jle 0x2fdcf77
0x2fdcf6f: 31db                     xor ebx, ebx
0x2fdcf71: 488b45b8                 mov rax, qword ptr [rbp - 0x48]
0x2fdcf75: eb49                     jmp 0x2fdcfc0
0x2fdcf77: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdcf7b: 4883c030                 add rax, 0x30
0x2fdcf7f: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdcf83: 488b45b8                 mov rax, qword ptr [rbp - 0x48]
0x2fdcf87: 7421                     je 0x2fdcfaa
0x2fdcf89: 31db                     xor ebx, ebx
0x2fdcf8b: eb33                     jmp 0x2fdcfc0
0x2fdcf8d: 4c89ff                   mov rdi, r15
0x2fdcf90: e80bd3b3ff               call 0x2b1a2a0
0x2fdcf95: 488b75b8                 mov rsi, qword ptr [rbp - 0x48]
0x2fdcf99: 4889c7                   mov rdi, rax
0x2fdcf9c: b901000000               mov ecx, 1
0x2fdcfa1: 31d2                     xor edx, edx
0x2fdcfa3: e858441a00               call 0x3181400
0x2fdcfa8: eb1d                     jmp 0x2fdcfc7

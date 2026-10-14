0x2fdcce0: 55                       push rbp
0x2fdcce1: 4889e5                   mov rbp, rsp
0x2fdcce4: 4157                     push r15
0x2fdcce6: 4156                     push r14
0x2fdcce8: 4155                     push r13
0x2fdccea: 4154                     push r12
0x2fdccec: 53                       push rbx
0x2fdcced: 50                       push rax
0x2fdccee: 4c8bbf08030000           mov r15, qword ptr [rdi + 0x308]
0x2fdccf5: 4189f4                   mov r12d, esi
0x2fdccf8: 4889d3                   mov rbx, rdx
0x2fdccfb: 438d1464                 lea edx, [r12 + r12*2]
0x2fdccff: 458bafc8020000           mov r13d, dword ptr [r15 + 0x2c8]
0x2fdcd06: 4401ea                   add edx, r13d
0x2fdcd09: 0f8817010000             js 0x2fdce26
0x2fdcd0f: 488d430c                 lea rax, [rbx + 0xc]
0x2fdcd13: 4989fe                   mov r14, rdi
0x2fdcd16: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdcd1a: 39530c                   cmp dword ptr [rbx + 0xc], edx
0x2fdcd1d: 7d28                     jge 0x2fdcd47
0x2fdcd1f: 448b4308                 mov r8d, dword ptr [rbx + 8]
0x2fdcd23: 4c8b4dd0                 mov r9, qword ptr [rbp - 0x30]
0x2fdcd27: bf08000000               mov edi, 8
0x2fdcd2c: be08000000               mov esi, 8
0x2fdcd31: 4889d9                   mov rcx, rbx
0x2fdcd34: e8e79004fd               call 0x25e20
0x2fdcd39: 4d8bbe08030000           mov r15, qword ptr [r14 + 0x308]
0x2fdcd40: 458bafc8020000           mov r13d, dword ptr [r15 + 0x2c8]
0x2fdcd47: 4585ed                   test r13d, r13d
0x2fdcd4a: 7436                     je 0x2fdcd82
0x2fdcd4c: 4c8b4dd0                 mov r9, qword ptr [rbp - 0x30]
0x2fdcd50: 4c8d4308                 lea r8, [rbx + 8]
0x2fdcd54: bf08000000               mov edi, 8
0x2fdcd59: be08000000               mov esi, 8
0x2fdcd5e: 4489ea                   mov edx, r13d
0x2fdcd61: 4889d9                   mov rcx, rbx
0x2fdcd64: e8f79604fd               call 0x26460
0x2fdcd69: 4863f8                   movsxd rdi, eax
0x2fdcd6c: 498bb7c0020000           mov rsi, qword ptr [r15 + 0x2c0]
0x2fdcd73: 4489ea                   mov edx, r13d
0x2fdcd76: 48c1e703                 shl rdi, 3
0x2fdcd7a: 48033b                   add rdi, qword ptr [rbx]
0x2fdcd7d: e89e07ebfd               call 0xe8d520
0x2fdcd82: 4584e4                   test r12b, r12b
0x2fdcd85: 0f848c000000             je 0x2fdce17
0x2fdcd8b: 8b4b08                   mov ecx, dword ptr [rbx + 8]
0x2fdcd8e: 8b530c                   mov edx, dword ptr [rbx + 0xc]
0x2fdcd91: 39d1                     cmp ecx, edx
0x2fdcd93: 7516                     jne 0x2fdcdab
0x2fdcd95: 488b55d0                 mov rdx, qword ptr [rbp - 0x30]
0x2fdcd99: bf08080000               mov edi, 0x808
0x2fdcd9e: 4889de                   mov rsi, rbx
0x2fdcda1: e8faca04fd               call 0x298a0
0x2fdcda6: 8b530c                   mov edx, dword ptr [rbx + 0xc]
0x2fdcda9: 89c1                     mov ecx, eax
0x2fdcdab: 8d4101                   lea eax, [rcx + 1]
0x2fdcdae: 4863c9                   movsxd rcx, ecx
0x2fdcdb1: 894308                   mov dword ptr [rbx + 8], eax
0x2fdcdb4: 488b33                   mov rsi, qword ptr [rbx]
0x2fdcdb7: 4c8934ce                 mov qword ptr [rsi + rcx*8], r14
0x2fdcdbb: 4d8bbe08030000           mov r15, qword ptr [r14 + 0x308]
0x2fdcdc2: 39d0                     cmp eax, edx
0x2fdcdc4: 7514                     jne 0x2fdcdda
0x2fdcdc6: 488b55d0                 mov rdx, qword ptr [rbp - 0x30]
0x2fdcdca: bf08080000               mov edi, 0x808
0x2fdcdcf: 4889de                   mov rsi, rbx
0x2fdcdd2: e8c9ca04fd               call 0x298a0
0x2fdcdd7: 8b530c                   mov edx, dword ptr [rbx + 0xc]
0x2fdcdda: 8d4801                   lea ecx, [rax + 1]
0x2fdcddd: 4898                     cdqe 
0x2fdcddf: 894b08                   mov dword ptr [rbx + 8], ecx
0x2fdcde2: 488b33                   mov rsi, qword ptr [rbx]
0x2fdcde5: 4c893cc6                 mov qword ptr [rsi + rax*8], r15
0x2fdcde9: 4d8bb600030000           mov r14, qword ptr [r14 + 0x300]
0x2fdcdf0: 39d1                     cmp ecx, edx
0x2fdcdf2: 7513                     jne 0x2fdce07
0x2fdcdf4: 488b55d0                 mov rdx, qword ptr [rbp - 0x30]
0x2fdcdf8: bf08080000               mov edi, 0x808
0x2fdcdfd: 4889de                   mov rsi, rbx
0x2fdce00: e89bca04fd               call 0x298a0
0x2fdce05: 89c1                     mov ecx, eax
0x2fdce07: 8d4101                   lea eax, [rcx + 1]
0x2fdce0a: 4863c9                   movsxd rcx, ecx
0x2fdce0d: 894308                   mov dword ptr [rbx + 8], eax
0x2fdce10: 488b03                   mov rax, qword ptr [rbx]
0x2fdce13: 4c8934c8                 mov qword ptr [rax + rcx*8], r14
0x2fdce17: 4883c408                 add rsp, 8
0x2fdce1b: 5b                       pop rbx
0x2fdce1c: 415c                     pop r12
0x2fdce1e: 415d                     pop r13
0x2fdce20: 415e                     pop r14
0x2fdce22: 415f                     pop r15
0x2fdce24: 5d                       pop rbp
0x2fdce25: c3                       ret 
0x2fdce26: 89d7                     mov edi, edx
0x2fdce28: e8d36208fd               call 0x63100
0x2fdce2d: 0f0b                     ud2 
0x2fdce2f: cc                       int3 

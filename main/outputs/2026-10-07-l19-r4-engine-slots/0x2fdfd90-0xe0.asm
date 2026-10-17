0x2fdfd90: 55                       push rbp
0x2fdfd91: 4889e5                   mov rbp, rsp
0x2fdfd94: 4157                     push r15
0x2fdfd96: 4156                     push r14
0x2fdfd98: 4155                     push r13
0x2fdfd9a: 4154                     push r12
0x2fdfd9c: 53                       push rbx
0x2fdfd9d: 4883ec18                 sub rsp, 0x18
0x2fdfda1: 4c8b2560211506           mov r12, qword ptr [rip + 0x6152160]
0x2fdfda8: 498b0424                 mov rax, qword ptr [r12]
0x2fdfdac: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdfdb0: 4885f6                   test rsi, rsi
0x2fdfdb3: 0f8490000000             je 0x2fdfe49
0x2fdfdb9: f6465d02                 test byte ptr [rsi + 0x5d], 2
0x2fdfdbd: 0f8586000000             jne 0x2fdfe49
0x2fdfdc3: 8b4608                   mov eax, dword ptr [rsi + 8]
0x2fdfdc6: a900000040               test eax, 0x40000000
0x2fdfdcb: 757c                     jne 0x2fdfe49
0x2fdfdcd: 488d05ccf76404           lea rax, [rip + 0x464f7cc]
0x2fdfdd4: 4c8d75c0                 lea r14, [rbp - 0x40]
0x2fdfdd8: 4889fb                   mov rbx, rdi
0x2fdfddb: 4989f7                   mov r15, rsi
0x2fdfdde: 4c89f7                   mov rdi, r14
0x2fdfde1: 4889c6                   mov rsi, rax
0x2fdfde4: e8074108fd               call 0x63ef0
0x2fdfde9: 4889df                   mov rdi, rbx
0x2fdfdec: 4c89fe                   mov rsi, r15
0x2fdfdef: 4c89f2                   mov rdx, r14
0x2fdfdf2: 4989dd                   mov r13, rbx
0x2fdfdf5: 4c89fb                   mov rbx, r15
0x2fdfdf8: e8d394ffff               call 0x2fd92d0
0x2fdfdfd: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdfe01: 4989c6                   mov r14, rax
0x2fdfe04: 4885ff                   test rdi, rdi
0x2fdfe07: 7405                     je 0x2fdfe0e
0x2fdfe09: e8827c0dfd               call 0xb7a90
0x2fdfe0e: 4d85f6                   test r14, r14
0x2fdfe11: 7523                     jne 0x2fdfe36
0x2fdfe13: 4c8dbbb8020000           lea r15, [rbx + 0x2b8]
0x2fdfe1a: 4531f6                   xor r14d, r14d
0x2fdfe1d: 31f6                     xor esi, esi
0x2fdfe1f: 4c89ff                   mov rdi, r15
0x2fdfe22: e879cbc3fd               call 0xc1c9a0
0x2fdfe27: 84c0                     test al, al
0x2fdfe29: 750b                     jne 0x2fdfe36
0x2fdfe2b: 4c89ff                   mov rdi, r15
0x2fdfe2e: e8cd6e4dfd               call 0x4b6d00
0x2fdfe33: 4989c6                   mov r14, rax
0x2fdfe36: 498b4500                 mov rax, qword ptr [r13]
0x2fdfe3a: 4c89ef                   mov rdi, r13
0x2fdfe3d: 4889de                   mov rsi, rbx
0x2fdfe40: 4c89f2                   mov rdx, r14
0x2fdfe43: ff90c0080000             call qword ptr [rax + 0x8c0]
0x2fdfe49: 498b0424                 mov rax, qword ptr [r12]
0x2fdfe4d: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdfe51: 750f                     jne 0x2fdfe62
0x2fdfe53: 4883c418                 add rsp, 0x18
0x2fdfe57: 5b                       pop rbx
0x2fdfe58: 415c                     pop r12
0x2fdfe5a: 415d                     pop r13
0x2fdfe5c: 415e                     pop r14
0x2fdfe5e: 415f                     pop r15
0x2fdfe60: 5d                       pop rbp
0x2fdfe61: c3                       ret 
0x2fdfe62: e8f91f4904               call 0x7471e60
0x2fdfe67: 0f0b                     ud2 
0x2fdfe69: cc                       int3 
0x2fdfe6a: cc                       int3 
0x2fdfe6b: cc                       int3 
0x2fdfe6c: cc                       int3 
0x2fdfe6d: cc                       int3 
0x2fdfe6e: cc                       int3 
0x2fdfe6f: cc                       int3 

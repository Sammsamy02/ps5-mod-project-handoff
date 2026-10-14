0x2fdeed0: 55                       push rbp
0x2fdeed1: 4889e5                   mov rbp, rsp
0x2fdeed4: 4157                     push r15
0x2fdeed6: 4156                     push r14
0x2fdeed8: 4155                     push r13
0x2fdeeda: 4154                     push r12
0x2fdeedc: 53                       push rbx
0x2fdeedd: 4883ec18                 sub rsp, 0x18
0x2fdeee1: 4c8b3d20301506           mov r15, qword ptr [rip + 0x6153020]
0x2fdeee8: 498b07                   mov rax, qword ptr [r15]
0x2fdeeeb: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdeeef: 4885f6                   test rsi, rsi
0x2fdeef2: 0f8418010000             je 0x2fdf010
0x2fdeef8: 4889f3                   mov rbx, rsi
0x2fdeefb: 4989fe                   mov r14, rdi
0x2fdeefe: e8bd864400               call 0x34275c0
0x2fdef03: 488b4b10                 mov rcx, qword ptr [rbx + 0x10]
0x2fdef07: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdef0b: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdef0e: 0f8ffc000000             jg 0x2fdf010
0x2fdef14: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdef18: 4883c030                 add rax, 0x30
0x2fdef1c: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdef20: 0f85ea000000             jne 0x2fdf010
0x2fdef26: 8b0d68335406             mov ecx, dword ptr [rip + 0x6543368]
0x2fdef2c: 8d4101                   lea eax, [rcx + 1]
0x2fdef2f: 89055f335406             mov dword ptr [rip + 0x654335f], eax
0x2fdef35: 4863054c335406           movsxd rax, dword ptr [rip + 0x654334c]
0x2fdef3c: 4885c0                   test rax, rax
0x2fdef3f: 0f8e87000000             jle 0x2fdefcc
0x2fdef45: 4531e4                   xor r12d, r12d
0x2fdef48: eb17                     jmp 0x2fdef61
0x2fdef4a: 660f1f440000             nop word ptr [rax + rax]
0x2fdef50: 498d47ff                 lea rax, [r15 - 1]
0x2fdef54: 41b401                   mov r12b, 1
0x2fdef57: 4983ff01                 cmp r15, 1
0x2fdef5b: 0f86c7000000             jbe 0x2fdf028
0x2fdef61: 4989c5                   mov r13, rax
0x2fdef64: 4989c7                   mov r15, rax
0x2fdef67: 49c1e504                 shl r13, 4
0x2fdef6b: 4983c5f8                 add r13, -8
0x2fdef6f: 90                       nop 
0x2fdef70: 488b0509335406           mov rax, qword ptr [rip + 0x6543309]
0x2fdef77: 42833c2800               cmp dword ptr [rax + r13], 0
0x2fdef7c: 74d2                     je 0x2fdef50
0x2fdef7e: 4a8b7c28f8               mov rdi, qword ptr [rax + r13 - 8]
0x2fdef83: 4885ff                   test rdi, rdi
0x2fdef86: 74c8                     je 0x2fdef50
0x2fdef88: 488b07                   mov rax, qword ptr [rdi]
0x2fdef8b: 4c89f6                   mov rsi, r14
0x2fdef8e: 4889da                   mov rdx, rbx
0x2fdef91: ff5068                   call qword ptr [rax + 0x68]
0x2fdef94: 84c0                     test al, al
0x2fdef96: 74b8                     je 0x2fdef50
0x2fdef98: 49ffcf                   dec r15
0x2fdef9b: 4983c5f0                 add r13, -0x10
0x2fdef9f: 498d4701                 lea rax, [r15 + 1]
0x2fdefa3: 4883f801                 cmp rax, 1
0x2fdefa7: 77c7                     ja 0x2fdef70
0x2fdefa9: ff0de5325406             dec dword ptr [rip + 0x65432e5]
0x2fdefaf: 4c8b3d522f1506           mov r15, qword ptr [rip + 0x6152f52]
0x2fdefb6: 41f6c401                 test r12b, 1
0x2fdefba: 7416                     je 0x2fdefd2
0x2fdefbc: 488d3dbd325406           lea rdi, [rip + 0x65432bd]
0x2fdefc3: 31f6                     xor esi, esi
0x2fdefc5: e8261e05fd               call 0x30df0
0x2fdefca: eb06                     jmp 0x2fdefd2
0x2fdefcc: 890dc2325406             mov dword ptr [rip + 0x65432c2], ecx
0x2fdefd2: 488b3587315406           mov rsi, qword ptr [rip + 0x6543187]
0x2fdefd9: 4c89f7                   mov rdi, r14
0x2fdefdc: 48895dc8                 mov qword ptr [rbp - 0x38], rbx
0x2fdefe0: e85ba747fd               call 0x459740
0x2fdefe5: 498b0e                   mov rcx, qword ptr [r14]
0x2fdefe8: 488d55c8                 lea rdx, [rbp - 0x38]
0x2fdefec: 4c89f7                   mov rdi, r14
0x2fdefef: 4889c6                   mov rsi, rax
0x2fdeff2: ff9168020000             call qword ptr [rcx + 0x268]
0x2fdeff8: 498bbe00030000           mov rdi, qword ptr [r14 + 0x300]
0x2fdefff: 4885ff                   test rdi, rdi
0x2fdf002: 740c                     je 0x2fdf010
0x2fdf004: 488b07                   mov rax, qword ptr [rdi]
0x2fdf007: 4889de                   mov rsi, rbx
0x2fdf00a: ff9088070000             call qword ptr [rax + 0x788]
0x2fdf010: 498b07                   mov rax, qword ptr [r15]
0x2fdf013: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdf017: 751e                     jne 0x2fdf037
0x2fdf019: 4883c418                 add rsp, 0x18
0x2fdf01d: 5b                       pop rbx
0x2fdf01e: 415c                     pop r12
0x2fdf020: 415d                     pop r13
0x2fdf022: 415e                     pop r14
0x2fdf024: 415f                     pop r15
0x2fdf026: 5d                       pop rbp
0x2fdf027: c3                       ret 
0x2fdf028: ff0d66325406             dec dword ptr [rip + 0x6543266]
0x2fdf02e: 4c8b3dd32e1506           mov r15, qword ptr [rip + 0x6152ed3]
0x2fdf035: eb85                     jmp 0x2fdefbc
0x2fdf037: e8242e4904               call 0x7471e60
0x2fdf03c: 0f0b                     ud2 
0x2fdf03e: cc                       int3 
0x2fdf03f: cc                       int3 

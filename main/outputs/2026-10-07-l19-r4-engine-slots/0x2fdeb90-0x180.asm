0x2fdeb90: 55                       push rbp
0x2fdeb91: 4889e5                   mov rbp, rsp
0x2fdeb94: 4157                     push r15
0x2fdeb96: 4156                     push r14
0x2fdeb98: 4155                     push r13
0x2fdeb9a: 4154                     push r12
0x2fdeb9c: 53                       push rbx
0x2fdeb9d: 4883ec18                 sub rsp, 0x18
0x2fdeba1: 4c8b2d60331506           mov r13, qword ptr [rip + 0x6153360]
0x2fdeba8: 4989f6                   mov r14, rsi
0x2fdebab: 4889fb                   mov rbx, rdi
0x2fdebae: 498b4500                 mov rax, qword ptr [r13]
0x2fdebb2: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdebb6: 488b07                   mov rax, qword ptr [rdi]
0x2fdebb9: ff9010090000             call qword ptr [rax + 0x910]
0x2fdebbf: 4889df                   mov rdi, rbx
0x2fdebc2: e8d9b6b3ff               call 0x2b1a2a0
0x2fdebc7: 498b8648030000           mov rax, qword ptr [r14 + 0x348]
0x2fdebce: 4c89f7                   mov rdi, r14
0x2fdebd1: 8b7038                   mov esi, dword ptr [rax + 0x38]
0x2fdebd4: e8b70f4500               call 0x342fb90
0x2fdebd9: 488b35c0355406           mov rsi, qword ptr [rip + 0x65435c0]
0x2fdebe0: 4889df                   mov rdi, rbx
0x2fdebe3: e858ab47fd               call 0x459740
0x2fdebe8: 4889c7                   mov rdi, rax
0x2fdebeb: 4989c7                   mov r15, rax
0x2fdebee: e80db633fd               call 0x31a200
0x2fdebf3: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdebfa: 756c                     jne 0x2fdec68
0x2fdebfc: c645c800                 mov byte ptr [rbp - 0x38], 0
0x2fdec00: 4c8975c0                 mov qword ptr [rbp - 0x40], r14
0x2fdec04: 488d55c0                 lea rdx, [rbp - 0x40]
0x2fdec08: 4889df                   mov rdi, rbx
0x2fdec0b: 4c89fe                   mov rsi, r15
0x2fdec0e: 488b03                   mov rax, qword ptr [rbx]
0x2fdec11: ff9068020000             call qword ptr [rax + 0x268]
0x2fdec17: 807dc801                 cmp byte ptr [rbp - 0x38], 1
0x2fdec1b: 745e                     je 0x2fdec7b
0x2fdec1d: 4d8bbeb0020000           mov r15, qword ptr [r14 + 0x2b0]
0x2fdec24: 4c8d25d11fdd04           lea r12, [rip + 0x4dd1fd1]
0x2fdec2b: 410fb687d0020000         movzx eax, byte ptr [r15 + 0x2d0]
0x2fdec33: 420fb60420               movzx eax, byte ptr [rax + r12]
0x2fdec38: 3c01                     cmp al, 1
0x2fdec3a: 0f85da000000             jne 0x2fded1a
0x2fdec40: 498bbfc0020000           mov rdi, qword ptr [r15 + 0x2c0]
0x2fdec47: 4885ff                   test rdi, rdi
0x2fdec4a: 7443                     je 0x2fdec8f
0x2fdec4c: 488b07                   mov rax, qword ptr [rdi]
0x2fdec4f: ff5030                   call qword ptr [rax + 0x30]
0x2fdec52: 84c0                     test al, al
0x2fdec54: 7439                     je 0x2fdec8f
0x2fdec56: 410fb687d0020000         movzx eax, byte ptr [r15 + 0x2d0]
0x2fdec5e: 420fb60420               movzx eax, byte ptr [rax + r12]
0x2fdec63: e9c3000000               jmp 0x2fded2b
0x2fdec68: 488b03                   mov rax, qword ptr [rbx]
0x2fdec6b: 4889df                   mov rdi, rbx
0x2fdec6e: 4c89f6                   mov rsi, r14
0x2fdec71: ff9090070000             call qword ptr [rax + 0x790]
0x2fdec77: 84c0                     test al, al
0x2fdec79: 74a2                     je 0x2fdec1d
0x2fdec7b: bf42010000               mov edi, 0x142
0x2fdec80: e86b1626fd               call 0x2402f0
0x2fdec85: 89c6                     mov esi, eax
0x2fdec87: 4c89f7                   mov rdi, r14
0x2fdec8a: e8a1144500               call 0x3430130
0x2fdec8f: 488bbb00030000           mov rdi, qword ptr [rbx + 0x300]
0x2fdec96: 4885ff                   test rdi, rdi
0x2fdec99: 740c                     je 0x2fdeca7
0x2fdec9b: 488b07                   mov rax, qword ptr [rdi]
0x2fdec9e: 4c89f6                   mov rsi, r14
0x2fdeca1: ff9078070000             call qword ptr [rax + 0x778]
0x2fdeca7: 488b03                   mov rax, qword ptr [rbx]
0x2fdecaa: 4889df                   mov rdi, rbx
0x2fdecad: 4c89f6                   mov rsi, r14
0x2fdecb0: ff9088080000             call qword ptr [rax + 0x888]
0x2fdecb6: 488b354b345406           mov rsi, qword ptr [rip + 0x654344b]
0x2fdecbd: 4889df                   mov rdi, rbx
0x2fdecc0: e87baa47fd               call 0x459740
0x2fdecc5: 4889c7                   mov rdi, rax
0x2fdecc8: 4989c7                   mov r15, rax
0x2fdeccb: e830b533fd               call 0x31a200
0x2fdecd0: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdecd7: 7519                     jne 0x2fdecf2
0x2fdecd9: 4c8975c0                 mov qword ptr [rbp - 0x40], r14
0x2fdecdd: 488d55c0                 lea rdx, [rbp - 0x40]
0x2fdece1: 4889df                   mov rdi, rbx
0x2fdece4: 4c89fe                   mov rsi, r15
0x2fdece7: 488b03                   mov rax, qword ptr [rbx]
0x2fdecea: ff9068020000             call qword ptr [rax + 0x268]
0x2fdecf0: eb0f                     jmp 0x2fded01
0x2fdecf2: 488b03                   mov rax, qword ptr [rbx]
0x2fdecf5: 4889df                   mov rdi, rbx
0x2fdecf8: 4c89f6                   mov rsi, r14
0x2fdecfb: ff9098070000             call qword ptr [rax + 0x798]
0x2fded01: 498b4500                 mov rax, qword ptr [r13]
0x2fded05: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fded09: 756e                     jne 0x2fded79
0x2fded0b: 4883c418                 add rsp, 0x18
0x2fded0f: 5b                       pop rbx

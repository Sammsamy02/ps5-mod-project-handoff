0x2fded80: 55                       push rbp
0x2fded81: 4889e5                   mov rbp, rsp
0x2fded84: 4157                     push r15
0x2fded86: 4156                     push r14
0x2fded88: 4155                     push r13
0x2fded8a: 4154                     push r12
0x2fded8c: 53                       push rbx
0x2fded8d: 4883ec18                 sub rsp, 0x18
0x2fded91: 4c8b3d70311506           mov r15, qword ptr [rip + 0x6153170]
0x2fded98: 498b07                   mov rax, qword ptr [r15]
0x2fded9b: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fded9f: 4885f6                   test rsi, rsi
0x2fdeda2: 7426                     je 0x2fdedca
0x2fdeda4: 4889f3                   mov rbx, rsi
0x2fdeda7: 4989fe                   mov r14, rdi
0x2fdedaa: e811884400               call 0x34275c0
0x2fdedaf: 488b4b10                 mov rcx, qword ptr [rbx + 0x10]
0x2fdedb3: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdedb7: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdedba: 7f0e                     jg 0x2fdedca
0x2fdedbc: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdedc0: 4883c030                 add rax, 0x30
0x2fdedc4: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdedc8: 741c                     je 0x2fdede6
0x2fdedca: 498b07                   mov rax, qword ptr [r15]
0x2fdedcd: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdedd1: 0f85e9000000             jne 0x2fdeec0
0x2fdedd7: 4883c418                 add rsp, 0x18
0x2fdeddb: 5b                       pop rbx
0x2fdeddc: 415c                     pop r12
0x2fdedde: 415d                     pop r13
0x2fdede0: 415e                     pop r14
0x2fdede2: 415f                     pop r15
0x2fdede4: 5d                       pop rbp
0x2fdede5: c3                       ret 
0x2fdede6: 488b35a3335406           mov rsi, qword ptr [rip + 0x65433a3]
0x2fdeded: 4c89f7                   mov rdi, r14
0x2fdedf0: 48895dc8                 mov qword ptr [rbp - 0x38], rbx
0x2fdedf4: e847a947fd               call 0x459740
0x2fdedf9: 498b0e                   mov rcx, qword ptr [r14]
0x2fdedfc: 488d55c8                 lea rdx, [rbp - 0x38]
0x2fdee00: 4c89f7                   mov rdi, r14
0x2fdee03: 4889c6                   mov rsi, rax
0x2fdee06: ff9168020000             call qword ptr [rcx + 0x268]
0x2fdee0c: 8b0d6a345406             mov ecx, dword ptr [rip + 0x654346a]
0x2fdee12: 8d4101                   lea eax, [rcx + 1]
0x2fdee15: 890561345406             mov dword ptr [rip + 0x6543461], eax
0x2fdee1b: 4863054e345406           movsxd rax, dword ptr [rip + 0x654344e]
0x2fdee22: 4885c0                   test rax, rax
0x2fdee25: 0f8e82000000             jle 0x2fdeead
0x2fdee2b: 31c9                     xor ecx, ecx
0x2fdee2d: eb0e                     jmp 0x2fdee3d
0x2fdee2f: 90                       nop 
0x2fdee30: 498d4424ff               lea rax, [r12 - 1]
0x2fdee35: b101                     mov cl, 1
0x2fdee37: 4983fc01                 cmp r12, 1
0x2fdee3b: 767b                     jbe 0x2fdeeb8
0x2fdee3d: 4989c5                   mov r13, rax
0x2fdee40: 4989c4                   mov r12, rax
0x2fdee43: 894dc4                   mov dword ptr [rbp - 0x3c], ecx
0x2fdee46: 49c1e504                 shl r13, 4
0x2fdee4a: 4983c5f8                 add r13, -8
0x2fdee4e: 6690                     nop 
0x2fdee50: 488b0511345406           mov rax, qword ptr [rip + 0x6543411]
0x2fdee57: 42833c2800               cmp dword ptr [rax + r13], 0
0x2fdee5c: 74d2                     je 0x2fdee30
0x2fdee5e: 4a8b7c28f8               mov rdi, qword ptr [rax + r13 - 8]
0x2fdee63: 4885ff                   test rdi, rdi
0x2fdee66: 74c8                     je 0x2fdee30
0x2fdee68: 488b07                   mov rax, qword ptr [rdi]
0x2fdee6b: 4c89f6                   mov rsi, r14
0x2fdee6e: 4889da                   mov rdx, rbx
0x2fdee71: ff5068                   call qword ptr [rax + 0x68]
0x2fdee74: 84c0                     test al, al
0x2fdee76: 74b8                     je 0x2fdee30
0x2fdee78: 49ffcc                   dec r12
0x2fdee7b: 4983c5f0                 add r13, -0x10
0x2fdee7f: 498d442401               lea rax, [r12 + 1]
0x2fdee84: 4883f801                 cmp rax, 1
0x2fdee88: 77c6                     ja 0x2fdee50
0x2fdee8a: ff0dec335406             dec dword ptr [rip + 0x65433ec]
0x2fdee90: f645c401                 test byte ptr [rbp - 0x3c], 1
0x2fdee94: 0f8430ffffff             je 0x2fdedca
0x2fdee9a: 488d3dc7335406           lea rdi, [rip + 0x65433c7]
0x2fdeea1: 31f6                     xor esi, esi
0x2fdeea3: e8481f05fd               call 0x30df0
0x2fdeea8: e91dffffff               jmp 0x2fdedca
0x2fdeead: 890dc9335406             mov dword ptr [rip + 0x65433c9], ecx
0x2fdeeb3: e912ffffff               jmp 0x2fdedca
0x2fdeeb8: ff0dbe335406             dec dword ptr [rip + 0x65433be]
0x2fdeebe: ebda                     jmp 0x2fdee9a
0x2fdeec0: e89b2f4904               call 0x7471e60
0x2fdeec5: 0f0b                     ud2 
0x2fdeec7: cc                       int3 
0x2fdeec8: cc                       int3 
0x2fdeec9: cc                       int3 
0x2fdeeca: cc                       int3 
0x2fdeecb: cc                       int3 
0x2fdeecc: cc                       int3 
0x2fdeecd: cc                       int3 
0x2fdeece: cc                       int3 
0x2fdeecf: cc                       int3 

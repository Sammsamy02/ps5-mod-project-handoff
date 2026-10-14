0x2fdbd00: 55                       push rbp
0x2fdbd01: 4889e5                   mov rbp, rsp
0x2fdbd04: 4157                     push r15
0x2fdbd06: 4156                     push r14
0x2fdbd08: 4155                     push r13
0x2fdbd0a: 4154                     push r12
0x2fdbd0c: 53                       push rbx
0x2fdbd0d: 4883ec38                 sub rsp, 0x38
0x2fdbd11: 4c8b35f0611506           mov r14, qword ptr [rip + 0x61561f0]
0x2fdbd18: 4889fb                   mov rbx, rdi
0x2fdbd1b: 4531ff                   xor r15d, r15d
0x2fdbd1e: 31f6                     xor esi, esi
0x2fdbd20: 498b06                   mov rax, qword ptr [r14]
0x2fdbd23: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdbd27: 488b07                   mov rax, qword ptr [rdi]
0x2fdbd2a: ff9000080000             call qword ptr [rax + 0x800]
0x2fdbd30: 84c0                     test al, al
0x2fdbd32: 7569                     jne 0x2fdbd9d
0x2fdbd34: 8bb338030000             mov esi, dword ptr [rbx + 0x338]
0x2fdbd3a: 85f6                     test esi, esi
0x2fdbd3c: 7e2e                     jle 0x2fdbd6c
0x2fdbd3e: 4989dc                   mov r12, rbx
0x2fdbd41: 8b9b3c030000             mov ebx, dword ptr [rbx + 0x33c]
0x2fdbd47: 498bbc2430030000         mov rdi, qword ptr [r12 + 0x330]
0x2fdbd4f: e86c5605fd               call 0x313c0
0x2fdbd54: 41b701                   mov r15b, 1
0x2fdbd57: 41c784243803000000000000 mov dword ptr [r12 + 0x338], 0
0x2fdbd63: 85db                     test ebx, ebx
0x2fdbd65: 780a                     js 0x2fdbd71
0x2fdbd67: 4c89e3                   mov rbx, r12
0x2fdbd6a: eb31                     jmp 0x2fdbd9d
0x2fdbd6c: 4531ff                   xor r15d, r15d
0x2fdbd6f: eb2c                     jmp 0x2fdbd9d
0x2fdbd71: 4d8d8c243c030000         lea r9, [r12 + 0x33c]
0x2fdbd79: 4c89e3                   mov rbx, r12
0x2fdbd7c: 41833900                 cmp dword ptr [r9], 0
0x2fdbd80: 741b                     je 0x2fdbd9d
0x2fdbd82: 488d8b30030000           lea rcx, [rbx + 0x330]
0x2fdbd89: bf10000000               mov edi, 0x10
0x2fdbd8e: be08000000               mov esi, 8
0x2fdbd93: 31d2                     xor edx, edx
0x2fdbd95: 4531c0                   xor r8d, r8d
0x2fdbd98: e883a004fd               call 0x25e20
0x2fdbd9d: 48638338030000           movsxd rax, dword ptr [rbx + 0x338]
0x2fdbda4: 4885c0                   test rax, rax
0x2fdbda7: 0f8e33020000             jle 0x2fdbfe0
0x2fdbdad: 4989c4                   mov r12, rax
0x2fdbdb0: 4189c5                   mov r13d, eax
0x2fdbdb3: 4989c6                   mov r14, rax
0x2fdbdb6: 48895db8                 mov qword ptr [rbp - 0x48], rbx
0x2fdbdba: 49c1e404                 shl r12, 4
0x2fdbdbe: 41f7dd                   neg r13d
0x2fdbdc1: 4983c4f0                 add r12, -0x10
0x2fdbdc5: eb1e                     jmp 0x2fdbde5
0x2fdbdc7: 660f1f840000000000       nop word ptr [rax + rax]
0x2fdbdd0: 41ffc5                   inc r13d
0x2fdbdd3: 498d4601                 lea rax, [r14 + 1]
0x2fdbdd7: 4983c4f0                 add r12, -0x10
0x2fdbddb: 4883f801                 cmp rax, 1
0x2fdbddf: 0f86ee010000             jbe 0x2fdbfd3
0x2fdbde5: 488bb330030000           mov rsi, qword ptr [rbx + 0x330]
0x2fdbdec: 488d7dc0                 lea rdi, [rbp - 0x40]
0x2fdbdf0: 4c8975b0                 mov qword ptr [rbp - 0x50], r14
0x2fdbdf4: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x2fdbdfc: 49ffce                   dec r14
0x2fdbdff: c745c800000000           mov dword ptr [rbp - 0x38], 0
0x2fdbe06: 4c01e6                   add rsi, r12
0x2fdbe09: e8027e05fd               call 0x33c10
0x2fdbe0e: 837dc800                 cmp dword ptr [rbp - 0x38], 0
0x2fdbe12: 4c8975a8                 mov qword ptr [rbp - 0x58], r14
0x2fdbe16: 7458                     je 0x2fdbe70
0x2fdbe18: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdbe1c: 4885ff                   test rdi, rdi
0x2fdbe1f: 744f                     je 0x2fdbe70
0x2fdbe21: 488b07                   mov rax, qword ptr [rdi]
0x2fdbe24: ff5038                   call qword ptr [rax + 0x38]
0x2fdbe27: 84c0                     test al, al
0x2fdbe29: 7445                     je 0x2fdbe70
0x2fdbe2b: 837dc800                 cmp dword ptr [rbp - 0x38], 0
0x2fdbe2f: 0f841b010000             je 0x2fdbf50
0x2fdbe35: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdbe39: 488b07                   mov rax, qword ptr [rdi]
0x2fdbe3c: ff5060                   call qword ptr [rax + 0x60]
0x2fdbe3f: 84c0                     test al, al
0x2fdbe41: 0f8519010000             jne 0x2fdbf60
0x2fdbe47: 660f1f840000000000       nop word ptr [rax + rax]
0x2fdbe50: 837dc800                 cmp dword ptr [rbp - 0x38], 0
0x2fdbe54: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdbe58: 0f85b4000000             jne 0x2fdbf12
0x2fdbe5e: 4885ff                   test rdi, rdi
0x2fdbe61: 0f85df000000             jne 0x2fdbf46
0x2fdbe67: e964ffffff               jmp 0x2fdbdd0
0x2fdbe6c: 0f1f4000                 nop dword ptr [rax]
0x2fdbe70: 4c8bb330030000           mov r14, qword ptr [rbx + 0x330]
0x2fdbe77: 43837c260800             cmp dword ptr [r14 + r12 + 8], 0

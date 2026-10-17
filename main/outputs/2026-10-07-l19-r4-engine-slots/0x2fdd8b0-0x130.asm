0x2fdd8b0: 55                       push rbp
0x2fdd8b1: 4889e5                   mov rbp, rsp
0x2fdd8b4: 4157                     push r15
0x2fdd8b6: 4156                     push r14
0x2fdd8b8: 4155                     push r13
0x2fdd8ba: 4154                     push r12
0x2fdd8bc: 53                       push rbx
0x2fdd8bd: 4883ec18                 sub rsp, 0x18
0x2fdd8c1: 4c8b2d40461506           mov r13, qword ptr [rip + 0x6154640]
0x2fdd8c8: 89f3                     mov ebx, esi
0x2fdd8ca: 4989fe                   mov r14, rdi
0x2fdd8cd: 488d35b4596104           lea rsi, [rip + 0x46159b4] ; ['SpectatorOnly=1']
0x2fdd8d4: 488d7dc0                 lea rdi, [rbp - 0x40]
0x2fdd8d8: 4989d7                   mov r15, rdx
0x2fdd8db: 498b4500                 mov rax, qword ptr [r13]
0x2fdd8df: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdd8e3: e8086608fd               call 0x63ef0
0x2fdd8e8: 837dc800                 cmp dword ptr [rbp - 0x38], 0
0x2fdd8ec: 740e                     je 0x2fdd8fc
0x2fdd8ee: 4c8b65c0                 mov r12, qword ptr [rbp - 0x40]
0x2fdd8f2: 4d85e4                   test r12, r12
0x2fdd8f5: 750c                     jne 0x2fdd903
0x2fdd8f7: e996000000               jmp 0x2fdd992
0x2fdd8fc: 4c8d259d1c6504           lea r12, [rip + 0x4651c9d]
0x2fdd903: 4c89e7                   mov rdi, r12
0x2fdd906: e885474904               call 0x7472090
0x2fdd90b: 4c89ff                   mov rdi, r15
0x2fdd90e: 4c89e6                   mov rsi, r12
0x2fdd911: 89c2                     mov edx, eax
0x2fdd913: b901000000               mov ecx, 1
0x2fdd918: 41b9ffffffff             mov r9d, 0xffffffff
0x2fdd91e: 4531c0                   xor r8d, r8d
0x2fdd921: e84a7508fd               call 0x64e70
0x2fdd926: 83f8ff                   cmp eax, -1
0x2fdd929: 7459                     je 0x2fdd984
0x2fdd92b: 4d8ba6f0020000           mov r12, qword ptr [r14 + 0x2f0]
0x2fdd932: 41b701                   mov r15b, 1
0x2fdd935: 4d85e4                   test r12, r12
0x2fdd938: 7430                     je 0x2fdd96a
0x2fdd93a: e8819c4400               call 0x34275c0
0x2fdd93f: 4885c0                   test rax, rax
0x2fdd942: 7426                     je 0x2fdd96a
0x2fdd944: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdd948: 413b4c2438               cmp ecx, dword ptr [r12 + 0x38]
0x2fdd94d: 7f1b                     jg 0x2fdd96a
0x2fdd94f: 498b542430               mov rdx, qword ptr [r12 + 0x30]
0x2fdd954: 4883c030                 add rax, 0x30
0x2fdd958: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdd95c: 750c                     jne 0x2fdd96a
0x2fdd95e: 4983bef002000000         cmp qword ptr [r14 + 0x2f0], 0
0x2fdd966: 410f94c7                 sete r15b
0x2fdd96a: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdd96e: 4885ff                   test rdi, rdi
0x2fdd971: 7405                     je 0x2fdd978
0x2fdd973: e818a10dfd               call 0xb7a90
0x2fdd978: b8a8080000               mov eax, 0x8a8
0x2fdd97d: 4584ff                   test r15b, r15b
0x2fdd980: 7510                     jne 0x2fdd992
0x2fdd982: eb13                     jmp 0x2fdd997
0x2fdd984: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdd988: 4885ff                   test rdi, rdi
0x2fdd98b: 7405                     je 0x2fdd992
0x2fdd98d: e8fea00dfd               call 0xb7a90
0x2fdd992: b8a0080000               mov eax, 0x8a0
0x2fdd997: 4d8b06                   mov r8, qword ptr [r14]
0x2fdd99a: 488d15af053506           lea rdx, [rip + 0x63505af]
0x2fdd9a1: 488d0dc0053506           lea rcx, [rip + 0x63505c0]
0x2fdd9a8: 4c89f7                   mov rdi, r14
0x2fdd9ab: 89de                     mov esi, ebx
0x2fdd9ad: 41ff1400                 call qword ptr [r8 + rax]
0x2fdd9b1: 498b4d00                 mov rcx, qword ptr [r13]
0x2fdd9b5: 483b4dd0                 cmp rcx, qword ptr [rbp - 0x30]
0x2fdd9b9: 750f                     jne 0x2fdd9ca
0x2fdd9bb: 4883c418                 add rsp, 0x18
0x2fdd9bf: 5b                       pop rbx
0x2fdd9c0: 415c                     pop r12
0x2fdd9c2: 415d                     pop r13
0x2fdd9c4: 415e                     pop r14
0x2fdd9c6: 415f                     pop r15
0x2fdd9c8: 5d                       pop rbp
0x2fdd9c9: c3                       ret 
0x2fdd9ca: e891444904               call 0x7471e60
0x2fdd9cf: 0f0b                     ud2 
0x2fdd9d1: cc                       int3 
0x2fdd9d2: cc                       int3 
0x2fdd9d3: cc                       int3 
0x2fdd9d4: cc                       int3 
0x2fdd9d5: cc                       int3 
0x2fdd9d6: cc                       int3 
0x2fdd9d7: cc                       int3 
0x2fdd9d8: cc                       int3 
0x2fdd9d9: cc                       int3 
0x2fdd9da: cc                       int3 
0x2fdd9db: cc                       int3 
0x2fdd9dc: cc                       int3 
0x2fdd9dd: cc                       int3 
0x2fdd9de: cc                       int3 
0x2fdd9df: cc                       int3 

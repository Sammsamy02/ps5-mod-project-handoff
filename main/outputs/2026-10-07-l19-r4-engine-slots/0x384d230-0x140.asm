0x384d230: 3b45d0                   cmp eax, dword ptr [rbp - 0x30]
0x384d233: 0f851b020000             jne 0x384d454
0x384d239: 4881c478010000           add rsp, 0x178
0x384d240: 5b                       pop rbx
0x384d241: 415c                     pop r12
0x384d243: 415d                     pop r13
0x384d245: 415e                     pop r14
0x384d247: 415f                     pop r15
0x384d249: 5d                       pop rbp
0x384d24a: c3                       ret 
0x384d24b: 488b8568feffff           mov rax, qword ptr [rbp - 0x198]
0x384d252: 488b1daf4c8e05           mov rbx, qword ptr [rip + 0x58e4caf]
0x384d259: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384d25f: e9d6f9ffff               jmp 0x384cc3a
0x384d264: 488b8568feffff           mov rax, qword ptr [rbp - 0x198]
0x384d26b: 488b1d964c8e05           mov rbx, qword ptr [rip + 0x58e4c96]
0x384d272: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384d278: e99dfcffff               jmp 0x384cf1a
0x384d27d: 488b8568feffff           mov rax, qword ptr [rbp - 0x198]
0x384d284: 488b1d7d4c8e05           mov rbx, qword ptr [rip + 0x58e4c7d]
0x384d28b: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384d291: e960ffffff               jmp 0x384d1f6
0x384d296: 4983c068                 add r8, 0x68
0x384d29a: 4c89c6                   mov rsi, r8
0x384d29d: e80ec7ffff               call 0x38499b0
0x384d2a2: 84c0                     test al, al
0x384d2a4: 0f8493000000             je 0x384d33d
0x384d2aa: 498bb690010000           mov rsi, qword ptr [r14 + 0x190]
0x384d2b1: 80beaa00000000           cmp byte ptr [rsi + 0xaa], 0
0x384d2b8: 0f85b0000000             jne 0x384d36e
0x384d2be: 4c8da578feffff           lea r12, [rbp - 0x188]
0x384d2c5: 4883c640                 add rsi, 0x40
0x384d2c9: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384d2cd: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x384d2d2: 4c89e7                   mov rdi, r12
0x384d2d5: e8865378ff               call 0x2fd2660
0x384d2da: 498b8e90010000           mov rcx, qword ptr [r14 + 0x190]
0x384d2e1: 498b4500                 mov rax, qword ptr [r13]
0x384d2e5: 4c8d45c0                 lea r8, [rbp - 0x40]
0x384d2e9: 4c89ef                   mov rdi, r13
0x384d2ec: 4c89f6                   mov rsi, r14
0x384d2ef: 4c89e2                   mov rdx, r12
0x384d2f2: ff90f8040000             call qword ptr [rax + 0x4f8]
0x384d2f8: 4c89e7                   mov rdi, r12
0x384d2fb: 4189c7                   mov r15d, eax
0x384d2fe: e80dd5f3ff               call 0x378a810
0x384d303: 498bbe90010000           mov rdi, qword ptr [r14 + 0x190]
0x384d30a: 4885ff                   test rdi, rdi
0x384d30d: 0f841a010000             je 0x384d42d
0x384d313: 488b07                   mov rax, qword ptr [rdi]
0x384d316: 410fb6cf                 movzx ecx, r15b
0x384d31a: 4c8d45c0                 lea r8, [rbp - 0x40]
0x384d31e: 4c89ee                   mov rsi, r13
0x384d321: 4c89f2                   mov rdx, r14
0x384d324: ff90f8020000             call qword ptr [rax + 0x2f8]
0x384d32a: 41bf01000000             mov r15d, 1
0x384d330: 84c0                     test al, al
0x384d332: 0f85dcfdffff             jne 0x384d114
0x384d338: e9f6000000               jmp 0x384d433
0x384d33d: 4c89ef                   mov rdi, r13
0x384d340: 4c89f6                   mov rsi, r14
0x384d343: e838f2ffff               call 0x384c580
0x384d348: 498b8e90010000           mov rcx, qword ptr [r14 + 0x190]
0x384d34f: 498bb6c0020000           mov rsi, qword ptr [r14 + 0x2c0]
0x384d356: 4c89ef                   mov rdi, r13
0x384d359: ba03000000               mov edx, 3
0x384d35e: 4883c168                 add rcx, 0x68
0x384d362: e8a9eeffff               call 0x384c210
0x384d367: 498bb690010000           mov rsi, qword ptr [r14 + 0x190]

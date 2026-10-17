0x2fdd4b0: 55                       push rbp
0x2fdd4b1: 4889e5                   mov rbp, rsp
0x2fdd4b4: 4157                     push r15
0x2fdd4b6: 4156                     push r14
0x2fdd4b8: 4155                     push r13
0x2fdd4ba: 4154                     push r12
0x2fdd4bc: 53                       push rbx
0x2fdd4bd: 4883ec28                 sub rsp, 0x28
0x2fdd4c1: 488b05404a1506           mov rax, qword ptr [rip + 0x6154a40]
0x2fdd4c8: 4989ce                   mov r14, rcx
0x2fdd4cb: 4c89c3                   mov rbx, r8
0x2fdd4ce: 4989f4                   mov r12, rsi
0x2fdd4d1: 4989ff                   mov r15, rdi
0x2fdd4d4: 488b00                   mov rax, qword ptr [rax]
0x2fdd4d7: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdd4db: 0fb64118                 movzx eax, byte ptr [rcx + 0x18]
0x2fdd4df: 488d0d1637dd04           lea rcx, [rip + 0x4dd3716]
0x2fdd4e6: 803c0801                 cmp byte ptr [rax + rcx], 1
0x2fdd4ea: 7515                     jne 0x2fdd501
0x2fdd4ec: 498b7e08                 mov rdi, qword ptr [r14 + 8]
0x2fdd4f0: 4885ff                   test rdi, rdi
0x2fdd4f3: 742e                     je 0x2fdd523
0x2fdd4f5: 488b07                   mov rax, qword ptr [rdi]
0x2fdd4f8: ff5030                   call qword ptr [rax + 0x30]
0x2fdd4fb: 84c0                     test al, al
0x2fdd4fd: 750c                     jne 0x2fdd50b
0x2fdd4ff: eb22                     jmp 0x2fdd523
0x2fdd501: 41f74608ffffff00         test dword ptr [r14 + 8], 0xffffff
0x2fdd509: 7418                     je 0x2fdd523
0x2fdd50b: e800d02a00               call 0x328a510
0x2fdd510: 488b08                   mov rcx, qword ptr [rax]
0x2fdd513: 4889c7                   mov rdi, rax
0x2fdd516: 4c89f6                   mov rsi, r14
0x2fdd519: ff91e8020000             call qword ptr [rcx + 0x2e8]
0x2fdd51f: 84c0                     test al, al
0x2fdd521: 7455                     je 0x2fdd578
0x2fdd523: 498bb700030000           mov rsi, qword ptr [r15 + 0x300]
0x2fdd52a: 4c8d6dc0                 lea r13, [rbp - 0x40]
0x2fdd52e: 4c89e2                   mov rdx, r12
0x2fdd531: 4c89ef                   mov rdi, r13
0x2fdd534: 488b06                   mov rax, qword ptr [rsi]
0x2fdd537: ff9068070000             call qword ptr [rax + 0x768]
0x2fdd53d: 4939dd                   cmp r13, rbx
0x2fdd540: 7426                     je 0x2fdd568
0x2fdd542: 488b3b                   mov rdi, qword ptr [rbx]
0x2fdd545: 4885ff                   test rdi, rdi
0x2fdd548: 7405                     je 0x2fdd54f
0x2fdd54a: e841a50dfd               call 0xb7a90
0x2fdd54f: 488b45c0                 mov rax, qword ptr [rbp - 0x40]
0x2fdd553: 488903                   mov qword ptr [rbx], rax
0x2fdd556: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x2fdd55e: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x2fdd562: 48894308                 mov qword ptr [rbx + 8], rax
0x2fdd566: eb1f                     jmp 0x2fdd587
0x2fdd568: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdd56c: 4885ff                   test rdi, rdi
0x2fdd56f: 7416                     je 0x2fdd587
0x2fdd571: e81aa50dfd               call 0xb7a90
0x2fdd576: eb0f                     jmp 0x2fdd587
0x2fdd578: 488d3527e17004           lea rsi, [rip + 0x470e127] ; ['incompatible_unique_net_id']
0x2fdd57f: 4889df                   mov rdi, rbx
0x2fdd582: e8c96f08fd               call 0x64550
0x2fdd587: 8b0dd74c5406             mov ecx, dword ptr [rip + 0x6544cd7]
0x2fdd58d: 8d4101                   lea eax, [rcx + 1]
0x2fdd590: 8905ce4c5406             mov dword ptr [rip + 0x6544cce], eax
0x2fdd596: 486305bb4c5406           movsxd rax, dword ptr [rip + 0x6544cbb]
0x2fdd59d: 4885c0                   test rax, rax
0x2fdd5a0: 0f8e93000000             jle 0x2fdd639
0x2fdd5a6: 31c9                     xor ecx, ecx
0x2fdd5a8: eb17                     jmp 0x2fdd5c1
0x2fdd5aa: 660f1f440000             nop word ptr [rax + rax]
0x2fdd5b0: 498d4424ff               lea rax, [r12 - 1]
0x2fdd5b5: b101                     mov cl, 1
0x2fdd5b7: 4983fc01                 cmp r12, 1
0x2fdd5bb: 0f869d000000             jbe 0x2fdd65e
0x2fdd5c1: 4989c5                   mov r13, rax
0x2fdd5c4: 4989c4                   mov r12, rax
0x2fdd5c7: 894dbc                   mov dword ptr [rbp - 0x44], ecx
0x2fdd5ca: 49c1e504                 shl r13, 4
0x2fdd5ce: 4983c5f8                 add r13, -8
0x2fdd5d2: 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x2fdd5e0: 488b05694c5406           mov rax, qword ptr [rip + 0x6544c69]
0x2fdd5e7: 42833c2800               cmp dword ptr [rax + r13], 0
0x2fdd5ec: 74c2                     je 0x2fdd5b0
0x2fdd5ee: 4a8b7c28f8               mov rdi, qword ptr [rax + r13 - 8]
0x2fdd5f3: 4885ff                   test rdi, rdi
0x2fdd5f6: 74b8                     je 0x2fdd5b0
0x2fdd5f8: 488b07                   mov rax, qword ptr [rdi]
0x2fdd5fb: 4c89fe                   mov rsi, r15
0x2fdd5fe: 4c89f2                   mov rdx, r14
0x2fdd601: 4889d9                   mov rcx, rbx
0x2fdd604: ff5068                   call qword ptr [rax + 0x68]
0x2fdd607: 84c0                     test al, al
0x2fdd609: 74a5                     je 0x2fdd5b0
0x2fdd60b: 49ffcc                   dec r12
0x2fdd60e: 4983c5f0                 add r13, -0x10
0x2fdd612: 498d442401               lea rax, [r12 + 1]
0x2fdd617: 4883f801                 cmp rax, 1
0x2fdd61b: 77c3                     ja 0x2fdd5e0
0x2fdd61d: ff0d414c5406             dec dword ptr [rip + 0x6544c41]
0x2fdd623: f645bc01                 test byte ptr [rbp - 0x44], 1
0x2fdd627: 7416                     je 0x2fdd63f
0x2fdd629: 488d3d204c5406           lea rdi, [rip + 0x6544c20]

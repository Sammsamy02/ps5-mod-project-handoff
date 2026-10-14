0x2fdd6f0: 55                       push rbp
0x2fdd6f1: 4889e5                   mov rbp, rsp
0x2fdd6f4: 4157                     push r15
0x2fdd6f6: 4156                     push r14
0x2fdd6f8: 4155                     push r13
0x2fdd6fa: 4154                     push r12
0x2fdd6fc: 53                       push rbx
0x2fdd6fd: 4883ec28                 sub rsp, 0x28
0x2fdd701: 4c8b2d00481506           mov r13, qword ptr [rip + 0x6154800]
0x2fdd708: 488b5d10                 mov rbx, qword ptr [rbp + 0x10]
0x2fdd70c: 498b4500                 mov rax, qword ptr [r13]
0x2fdd710: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdd714: 488bb700030000           mov rsi, qword ptr [rdi + 0x300]
0x2fdd71b: 4885f6                   test rsi, rsi
0x2fdd71e: 744f                     je 0x2fdd76f
0x2fdd720: 488b06                   mov rax, qword ptr [rsi]
0x2fdd723: 4c8d6dc0                 lea r13, [rbp - 0x40]
0x2fdd727: 4189d4                   mov r12d, edx
0x2fdd72a: 4989fe                   mov r14, rdi
0x2fdd72d: 4c89c2                   mov rdx, r8
0x2fdd730: 4d89c7                   mov r15, r8
0x2fdd733: 4c894db8                 mov qword ptr [rbp - 0x48], r9
0x2fdd737: 48894db0                 mov qword ptr [rbp - 0x50], rcx
0x2fdd73b: 4c89ef                   mov rdi, r13
0x2fdd73e: ff9068070000             call qword ptr [rax + 0x768]
0x2fdd744: 4939dd                   cmp r13, rbx
0x2fdd747: 743a                     je 0x2fdd783
0x2fdd749: 488b3b                   mov rdi, qword ptr [rbx]
0x2fdd74c: 4885ff                   test rdi, rdi
0x2fdd74f: 7405                     je 0x2fdd756
0x2fdd751: e83aa30dfd               call 0xb7a90
0x2fdd756: 488b45c0                 mov rax, qword ptr [rbp - 0x40]
0x2fdd75a: 488903                   mov qword ptr [rbx], rax
0x2fdd75d: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x2fdd765: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x2fdd769: 48894308                 mov qword ptr [rbx + 8], rax
0x2fdd76d: eb22                     jmp 0x2fdd791
0x2fdd76f: 488d35aaca6b04           lea rsi, [rip + 0x46bcaaa] ; ['Failed to spawn player controller, GameSession is null']
0x2fdd776: 4889df                   mov rdi, rbx
0x2fdd779: e8d26d08fd               call 0x64550
0x2fdd77e: e90b010000               jmp 0x2fdd88e
0x2fdd783: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdd787: 4885ff                   test rdi, rdi
0x2fdd78a: 7405                     je 0x2fdd791
0x2fdd78c: e8ffa20dfd               call 0xb7a90
0x2fdd791: 837b0801                 cmp dword ptr [rbx + 8], 1
0x2fdd795: 7f6a                     jg 0x2fdd801
0x2fdd797: 498b06                   mov rax, qword ptr [r14]
0x2fdd79a: 4c89f7                   mov rdi, r14
0x2fdd79d: 4489e6                   mov esi, r12d
0x2fdd7a0: 4c89fa                   mov rdx, r15
0x2fdd7a3: ff9098080000             call qword ptr [rax + 0x898]
0x2fdd7a9: 4885c0                   test rax, rax
0x2fdd7ac: 745f                     je 0x2fdd80d
0x2fdd7ae: 4d8b16                   mov r10, qword ptr [r14]
0x2fdd7b1: 488b4db8                 mov rcx, qword ptr [rbp - 0x48]
0x2fdd7b5: 4c8b4db0                 mov r9, qword ptr [rbp - 0x50]
0x2fdd7b9: 4c89f6                   mov rsi, r14
0x2fdd7bc: 4c89ef                   mov rdi, r13
0x2fdd7bf: 4889c2                   mov rdx, rax
0x2fdd7c2: 4d89f8                   mov r8, r15
0x2fdd7c5: 4989c6                   mov r14, rax
0x2fdd7c8: 41ff9208090000           call qword ptr [r10 + 0x908]
0x2fdd7cf: 4939dd                   cmp r13, rbx
0x2fdd7d2: 7474                     je 0x2fdd848
0x2fdd7d4: 488b3b                   mov rdi, qword ptr [rbx]
0x2fdd7d7: 4c8b2d2a471506           mov r13, qword ptr [rip + 0x615472a]
0x2fdd7de: 4885ff                   test rdi, rdi
0x2fdd7e1: 7405                     je 0x2fdd7e8
0x2fdd7e3: e8a8a20dfd               call 0xb7a90
0x2fdd7e8: 488b45c0                 mov rax, qword ptr [rbp - 0x40]
0x2fdd7ec: 488903                   mov qword ptr [rbx], rax
0x2fdd7ef: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x2fdd7f7: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x2fdd7fb: 48894308                 mov qword ptr [rbx + 8], rax
0x2fdd7ff: eb5c                     jmp 0x2fdd85d
0x2fdd801: 4c8b2d00471506           mov r13, qword ptr [rip + 0x6154700]
0x2fdd808: e981000000               jmp 0x2fdd88e
0x2fdd80d: 488d350c405c04           lea rsi, [rip + 0x45c400c] ; ['Failed to spawn player controller']
0x2fdd814: 4c89ef                   mov rdi, r13
0x2fdd817: 31c0                     xor eax, eax
0x2fdd819: e862e208fd               call 0x6ba80
0x2fdd81e: 4939dd                   cmp r13, rbx
0x2fdd821: 7456                     je 0x2fdd879
0x2fdd823: 488b3b                   mov rdi, qword ptr [rbx]
0x2fdd826: 4c8b2ddb461506           mov r13, qword ptr [rip + 0x61546db]
0x2fdd82d: 4885ff                   test rdi, rdi
0x2fdd830: 7405                     je 0x2fdd837
0x2fdd832: e859a20dfd               call 0xb7a90
0x2fdd837: 488b45c0                 mov rax, qword ptr [rbp - 0x40]
0x2fdd83b: 488903                   mov qword ptr [rbx], rax
0x2fdd83e: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x2fdd842: 48894308                 mov qword ptr [rbx + 8], rax
0x2fdd846: eb46                     jmp 0x2fdd88e
0x2fdd848: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdd84c: 4c8b2db5461506           mov r13, qword ptr [rip + 0x61546b5]
0x2fdd853: 4885ff                   test rdi, rdi
0x2fdd856: 7405                     je 0x2fdd85d
0x2fdd858: e833a20dfd               call 0xb7a90
0x2fdd85d: 837b0802                 cmp dword ptr [rbx + 8], 2
0x2fdd861: 7d05                     jge 0x2fdd868
0x2fdd863: 4c89f0                   mov rax, r14
0x2fdd866: eb28                     jmp 0x2fdd890
0x2fdd868: 4c89f7                   mov rdi, r14
0x2fdd86b: ba01000000               mov edx, 1

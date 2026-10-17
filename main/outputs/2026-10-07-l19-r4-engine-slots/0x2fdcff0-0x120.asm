0x2fdcff0: 55                       push rbp
0x2fdcff1: 4889e5                   mov rbp, rsp
0x2fdcff4: 4157                     push r15
0x2fdcff6: 4156                     push r14
0x2fdcff8: 4155                     push r13
0x2fdcffa: 4154                     push r12
0x2fdcffc: 53                       push rbx
0x2fdcffd: 50                       push rax
0x2fdcffe: 4c8bbec8020000           mov r15, qword ptr [rsi + 0x2c8]
0x2fdd005: 4989d4                   mov r12, rdx
0x2fdd008: 4989f6                   mov r14, rsi
0x2fdd00b: 4889fb                   mov rbx, rdi
0x2fdd00e: 4d85ff                   test r15, r15
0x2fdd011: 7426                     je 0x2fdd039
0x2fdd013: e8a8a54400               call 0x34275c0
0x2fdd018: 4885c0                   test rax, rax
0x2fdd01b: 741c                     je 0x2fdd039
0x2fdd01d: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdd021: 413b4f38                 cmp ecx, dword ptr [r15 + 0x38]
0x2fdd025: 7f12                     jg 0x2fdd039
0x2fdd027: 498b5730                 mov rdx, qword ptr [r15 + 0x30]
0x2fdd02b: 4883c030                 add rax, 0x30
0x2fdd02f: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdd033: 0f8487000000             je 0x2fdd0c0
0x2fdd039: 4531ff                   xor r15d, r15d
0x2fdd03c: 4d85e4                   test r12, r12
0x2fdd03f: 0f848b000000             je 0x2fdd0d0
0x2fdd045: 4d8baef0020000           mov r13, qword ptr [r14 + 0x2f0]
0x2fdd04c: 4d85ed                   test r13, r13
0x2fdd04f: 747f                     je 0x2fdd0d0
0x2fdd051: e86aa54400               call 0x34275c0
0x2fdd056: 4885c0                   test rax, rax
0x2fdd059: 7475                     je 0x2fdd0d0
0x2fdd05b: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdd05f: 413b4d38                 cmp ecx, dword ptr [r13 + 0x38]
0x2fdd063: 7f6b                     jg 0x2fdd0d0
0x2fdd065: 498b5530                 mov rdx, qword ptr [r13 + 0x30]
0x2fdd069: 4883c030                 add rax, 0x30
0x2fdd06d: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdd071: 755d                     jne 0x2fdd0d0
0x2fdd073: 4d8baef0020000           mov r13, qword ptr [r14 + 0x2f0]
0x2fdd07a: 4d85ed                   test r13, r13
0x2fdd07d: 7451                     je 0x2fdd0d0
0x2fdd07f: 498b8424b0020000         mov rax, qword ptr [r12 + 0x2b0]
0x2fdd087: 4885c0                   test rax, rax
0x2fdd08a: 7444                     je 0x2fdd0d0
0x2fdd08c: f680b202000004           test byte ptr [rax + 0x2b2], 4
0x2fdd093: 743b                     je 0x2fdd0d0
0x2fdd095: e826a54400               call 0x34275c0
0x2fdd09a: 4885c0                   test rax, rax
0x2fdd09d: 7418                     je 0x2fdd0b7
0x2fdd09f: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdd0a3: 413b4d38                 cmp ecx, dword ptr [r13 + 0x38]
0x2fdd0a7: 7f0e                     jg 0x2fdd0b7
0x2fdd0a9: 498b5530                 mov rdx, qword ptr [r13 + 0x30]
0x2fdd0ad: 4883c030                 add rax, 0x30
0x2fdd0b1: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdd0b5: 7447                     je 0x2fdd0fe
0x2fdd0b7: 48c70300000000           mov qword ptr [rbx], 0
0x2fdd0be: eb2c                     jmp 0x2fdd0ec
0x2fdd0c0: 4d8bbec8020000           mov r15, qword ptr [r14 + 0x2c8]
0x2fdd0c7: 4d85e4                   test r12, r12
0x2fdd0ca: 0f8575ffffff             jne 0x2fdd045
0x2fdd0d0: 4c893b                   mov qword ptr [rbx], r15
0x2fdd0d3: 4d85ff                   test r15, r15
0x2fdd0d6: 7414                     je 0x2fdd0ec
0x2fdd0d8: 488d057d1d3c06           lea rax, [rip + 0x63c1d7d]
0x2fdd0df: 803800                   cmp byte ptr [rax], 0
0x2fdd0e2: 7408                     je 0x2fdd0ec
0x2fdd0e4: 4c89ff                   mov rdi, r15
0x2fdd0e7: e8d4b138fd               call 0x3682c0
0x2fdd0ec: 4889d8                   mov rax, rbx
0x2fdd0ef: 4883c408                 add rsp, 8
0x2fdd0f3: 5b                       pop rbx
0x2fdd0f4: 415c                     pop r12
0x2fdd0f6: 415d                     pop r13
0x2fdd0f8: 415e                     pop r14
0x2fdd0fa: 415f                     pop r15
0x2fdd0fc: 5d                       pop rbp
0x2fdd0fd: c3                       ret 
0x2fdd0fe: 4d8bbef0020000           mov r15, qword ptr [r14 + 0x2f0]
0x2fdd105: 4c893b                   mov qword ptr [rbx], r15
0x2fdd108: 4d85ff                   test r15, r15
0x2fdd10b: 75cb                     jne 0x2fdd0d8
0x2fdd10d: ebdd                     jmp 0x2fdd0ec
0x2fdd10f: cc                       int3 

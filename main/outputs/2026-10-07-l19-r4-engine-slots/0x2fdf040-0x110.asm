0x2fdf040: 55                       push rbp
0x2fdf041: 4889e5                   mov rbp, rsp
0x2fdf044: 4157                     push r15
0x2fdf046: 4156                     push r14
0x2fdf048: 4154                     push r12
0x2fdf04a: 53                       push rbx
0x2fdf04b: 4883ec20                 sub rsp, 0x20
0x2fdf04f: 4c8b25b22e1506           mov r12, qword ptr [rip + 0x6152eb2]
0x2fdf056: 498b0424                 mov rax, qword ptr [r12]
0x2fdf05a: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x2fdf05e: f6872803000002           test byte ptr [rdi + 0x328], 2
0x2fdf065: 0f85c4000000             jne 0x2fdf12f
0x2fdf06b: 4889f3                   mov rbx, rsi
0x2fdf06e: 488b352b315406           mov rsi, qword ptr [rip + 0x654312b]
0x2fdf075: 4989fe                   mov r14, rdi
0x2fdf078: e8c3a647fd               call 0x459740
0x2fdf07d: 4889c7                   mov rdi, rax
0x2fdf080: 4989c7                   mov r15, rax
0x2fdf083: e878b133fd               call 0x31a200
0x2fdf088: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdf08f: 7523                     jne 0x2fdf0b4
0x2fdf091: c645d000                 mov byte ptr [rbp - 0x30], 0
0x2fdf095: 48895dc8                 mov qword ptr [rbp - 0x38], rbx
0x2fdf099: 488d55c8                 lea rdx, [rbp - 0x38]
0x2fdf09d: 4c89f7                   mov rdi, r14
0x2fdf0a0: 4c89fe                   mov rsi, r15
0x2fdf0a3: 498b06                   mov rax, qword ptr [r14]
0x2fdf0a6: ff9068020000             call qword ptr [rax + 0x268]
0x2fdf0ac: 807dd000                 cmp byte ptr [rbp - 0x30], 0
0x2fdf0b0: 7415                     je 0x2fdf0c7
0x2fdf0b2: eb7b                     jmp 0x2fdf12f
0x2fdf0b4: 498b06                   mov rax, qword ptr [r14]
0x2fdf0b7: 4c89f7                   mov rdi, r14
0x2fdf0ba: 4889de                   mov rsi, rbx
0x2fdf0bd: ff9090070000             call qword ptr [rax + 0x790]
0x2fdf0c3: 84c0                     test al, al
0x2fdf0c5: 7568                     jne 0x2fdf12f
0x2fdf0c7: 488b35e2305406           mov rsi, qword ptr [rip + 0x65430e2]
0x2fdf0ce: 4c89f7                   mov rdi, r14
0x2fdf0d1: e86aa647fd               call 0x459740
0x2fdf0d6: 4889c7                   mov rdi, rax
0x2fdf0d9: 4989c7                   mov r15, rax
0x2fdf0dc: e81fb133fd               call 0x31a200
0x2fdf0e1: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdf0e8: 7523                     jne 0x2fdf10d
0x2fdf0ea: c645d000                 mov byte ptr [rbp - 0x30], 0
0x2fdf0ee: 48895dc8                 mov qword ptr [rbp - 0x38], rbx
0x2fdf0f2: 488d55c8                 lea rdx, [rbp - 0x38]
0x2fdf0f6: 4c89f7                   mov rdi, r14
0x2fdf0f9: 4c89fe                   mov rsi, r15
0x2fdf0fc: 498b06                   mov rax, qword ptr [r14]
0x2fdf0ff: ff9068020000             call qword ptr [rax + 0x268]
0x2fdf105: 807dd001                 cmp byte ptr [rbp - 0x30], 1
0x2fdf109: 7415                     je 0x2fdf120
0x2fdf10b: eb22                     jmp 0x2fdf12f
0x2fdf10d: 498b06                   mov rax, qword ptr [r14]
0x2fdf110: 4c89f7                   mov rdi, r14
0x2fdf113: 4889de                   mov rsi, rbx
0x2fdf116: ff9070070000             call qword ptr [rax + 0x770]
0x2fdf11c: 84c0                     test al, al
0x2fdf11e: 740f                     je 0x2fdf12f
0x2fdf120: 498b06                   mov rax, qword ptr [r14]
0x2fdf123: 4c89f7                   mov rdi, r14
0x2fdf126: 4889de                   mov rsi, rbx
0x2fdf129: ff90b8080000             call qword ptr [rax + 0x8b8]
0x2fdf12f: 498b0424                 mov rax, qword ptr [r12]
0x2fdf133: 483b45d8                 cmp rax, qword ptr [rbp - 0x28]
0x2fdf137: 750d                     jne 0x2fdf146
0x2fdf139: 4883c420                 add rsp, 0x20
0x2fdf13d: 5b                       pop rbx
0x2fdf13e: 415c                     pop r12
0x2fdf140: 415e                     pop r14
0x2fdf142: 415f                     pop r15
0x2fdf144: 5d                       pop rbp
0x2fdf145: c3                       ret 
0x2fdf146: e8152d4904               call 0x7471e60
0x2fdf14b: 0f0b                     ud2 
0x2fdf14d: cc                       int3 
0x2fdf14e: cc                       int3 
0x2fdf14f: cc                       int3 

0x2fdb2b0: 55                       push rbp
0x2fdb2b1: 4889e5                   mov rbp, rsp
0x2fdb2b4: 4157                     push r15
0x2fdb2b6: 4156                     push r14
0x2fdb2b8: 4154                     push r12
0x2fdb2ba: 53                       push rbx
0x2fdb2bb: 4c8b7710                 mov r14, qword ptr [rdi + 0x10]
0x2fdb2bf: 4c8ba708030000           mov r12, qword ptr [rdi + 0x308]
0x2fdb2c6: 4c8d3d8f3b3c06           lea r15, [rip + 0x63c3b8f]
0x2fdb2cd: 4889fb                   mov rbx, rdi
0x2fdb2d0: 4d85f6                   test r14, r14
0x2fdb2d3: 740e                     je 0x2fdb2e3
0x2fdb2d5: 41803f00                 cmp byte ptr [r15], 0
0x2fdb2d9: 7408                     je 0x2fdb2e3
0x2fdb2db: 4c89f7                   mov rdi, r14
0x2fdb2de: e8ddcf38fd               call 0x3682c0
0x2fdb2e3: 4d89b424a8020000         mov qword ptr [r12 + 0x2a8], r14
0x2fdb2eb: 488bbb08030000           mov rdi, qword ptr [rbx + 0x308]
0x2fdb2f2: 488b07                   mov rax, qword ptr [rdi]
0x2fdb2f5: ff9088070000             call qword ptr [rax + 0x788]
0x2fdb2fb: 41803f01                 cmp byte ptr [r15], 1
0x2fdb2ff: 488bbbe8020000           mov rdi, qword ptr [rbx + 0x2e8]
0x2fdb306: 4c8bb308030000           mov r14, qword ptr [rbx + 0x308]
0x2fdb30d: 7515                     jne 0x2fdb324
0x2fdb30f: 4885ff                   test rdi, rdi
0x2fdb312: 740e                     je 0x2fdb322
0x2fdb314: e8a7cf38fd               call 0x3682c0
0x2fdb319: 488bbbe8020000           mov rdi, qword ptr [rbx + 0x2e8]
0x2fdb320: eb02                     jmp 0x2fdb324
0x2fdb322: 31ff                     xor edi, edi
0x2fdb324: 4989beb8020000           mov qword ptr [r14 + 0x2b8], rdi
0x2fdb32b: 488bbb08030000           mov rdi, qword ptr [rbx + 0x308]
0x2fdb332: 488b07                   mov rax, qword ptr [rdi]
0x2fdb335: ff9090070000             call qword ptr [rax + 0x790]
0x2fdb33b: 5b                       pop rbx
0x2fdb33c: 415c                     pop r12
0x2fdb33e: 415e                     pop r14
0x2fdb340: 415f                     pop r15
0x2fdb342: 5d                       pop rbp
0x2fdb343: c3                       ret 
0x2fdb344: cc                       int3 
0x2fdb345: cc                       int3 
0x2fdb346: cc                       int3 
0x2fdb347: cc                       int3 
0x2fdb348: cc                       int3 
0x2fdb349: cc                       int3 
0x2fdb34a: cc                       int3 
0x2fdb34b: cc                       int3 
0x2fdb34c: cc                       int3 
0x2fdb34d: cc                       int3 
0x2fdb34e: cc                       int3 
0x2fdb34f: cc                       int3 

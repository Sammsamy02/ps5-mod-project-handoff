0x402040: 55                       push rbp
0x402041: 4889e5                   mov rbp, rsp
0x402044: 4157                     push r15
0x402046: 4156                     push r14
0x402048: 4155                     push r13
0x40204a: 4154                     push r12
0x40204c: 53                       push rbx
0x40204d: 4883ec18                 sub rsp, 0x18
0x402051: 4c8b25b0fed208           mov r12, qword ptr [rip + 0x8d2feb0]
0x402058: 4989d6                   mov r14, rdx
0x40205b: 4889fb                   mov rbx, rdi
0x40205e: 498b0424                 mov rax, qword ptr [r12]
0x402062: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x402066: 4c8b2e                   mov r13, qword ptr [rsi]
0x402069: 41f6c501                 test r13b, 1
0x40206d: 743b                     je 0x4020aa
0x40206f: 4d89ef                   mov r15, r13
0x402072: 4983e7fe                 and r15, 0xfffffffffffffffe
0x402076: 7432                     je 0x4020aa
0x402078: 4c8d25ddcdf908           lea r12, [rip + 0x8f9cddd]
0x40207f: 41803c2401               cmp byte ptr [r12], 1
0x402084: 7508                     jne 0x40208e
0x402086: 4c89ff                   mov rdi, r15
0x402089: e83262f6ff               call 0x3682c0
0x40208e: 41803c2401               cmp byte ptr [r12], 1
0x402093: 4c8b256efed208           mov r12, qword ptr [rip + 0x8d2fe6e]
0x40209a: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x40209e: 750e                     jne 0x4020ae
0x4020a0: 4c89ff                   mov rdi, r15
0x4020a3: e81862f6ff               call 0x3682c0
0x4020a8: eb04                     jmp 0x4020ae
0x4020aa: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x4020ae: 488d75c8                 lea rsi, [rbp - 0x38]
0x4020b2: 4889df                   mov rdi, rbx
0x4020b5: 4c89f2                   mov rdx, r14
0x4020b8: e8a36c0100               call 0x418d60
0x4020bd: 488d05dcdcae07           lea rax, [rip + 0x7aedcdc]
0x4020c4: 488903                   mov qword ptr [rbx], rax
0x4020c7: 48c7437800000000         mov qword ptr [rbx + 0x78], 0
0x4020cf: 48837b7000               cmp qword ptr [rbx + 0x70], 0
0x4020d4: 7429                     je 0x4020ff
0x4020d6: 498b4640                 mov rax, qword ptr [r14 + 0x40]
0x4020da: 4885c0                   test rax, rax
0x4020dd: 744e                     je 0x40212d
0x4020df: ffd0                     call rax
0x4020e1: 4989c6                   mov r14, rax
0x4020e4: 4885c0                   test rax, rax
0x4020e7: 7447                     je 0x402130
0x4020e9: 488d056ccdf908           lea rax, [rip + 0x8f9cd6c]
0x4020f0: 803800                   cmp byte ptr [rax], 0
0x4020f3: 743b                     je 0x402130
0x4020f5: 4c89f7                   mov rdi, r14
0x4020f8: e8c361f6ff               call 0x3682c0
0x4020fd: eb31                     jmp 0x402130
0x4020ff: e8bc43f2ff               call 0x3264c0
0x402104: 4989c7                   mov r15, rax
0x402107: 4885c0                   test rax, rax
0x40210a: 7414                     je 0x402120
0x40210c: 488d0549cdf908           lea rax, [rip + 0x8f9cd49]
0x402113: 803800                   cmp byte ptr [rax], 0
0x402116: 7408                     je 0x402120
0x402118: 4c89ff                   mov rdi, r15
0x40211b: e8a061f6ff               call 0x3682c0
0x402120: 4c897b70                 mov qword ptr [rbx + 0x70], r15
0x402124: 498b4640                 mov rax, qword ptr [r14 + 0x40]
0x402128: 4885c0                   test rax, rax
0x40212b: 75b2                     jne 0x4020df
0x40212d: 4531f6                   xor r14d, r14d
0x402130: 4c897378                 mov qword ptr [rbx + 0x78], r14
0x402134: 498b0424                 mov rax, qword ptr [r12]
0x402138: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x40213c: 750f                     jne 0x40214d
0x40213e: 4883c418                 add rsp, 0x18
0x402142: 5b                       pop rbx
0x402143: 415c                     pop r12
0x402145: 415d                     pop r13
0x402147: 415e                     pop r14
0x402149: 415f                     pop r15
0x40214b: 5d                       pop rbp
0x40214c: c3                       ret 
0x40214d: e80efd0607               call 0x7471e60
0x402152: 0f0b                     ud2 
0x402154: cc                       int3 
0x402155: cc                       int3 
0x402156: cc                       int3 
0x402157: cc                       int3 
0x402158: cc                       int3 
0x402159: cc                       int3 
0x40215a: cc                       int3 
0x40215b: cc                       int3 
0x40215c: cc                       int3 
0x40215d: cc                       int3 
0x40215e: cc                       int3 
0x40215f: cc                       int3 

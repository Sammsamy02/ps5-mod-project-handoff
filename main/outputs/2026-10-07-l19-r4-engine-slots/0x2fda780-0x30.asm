0x2fda780: 55                       push rbp
0x2fda781: 4889e5                   mov rbp, rsp
0x2fda784: 488b4620                 mov rax, qword ptr [rsi + 0x20]
0x2fda788: 4883f801                 cmp rax, 1
0x2fda78c: 4883d8ff                 sbb rax, -1
0x2fda790: 48894620                 mov qword ptr [rsi + 0x20], rax
0x2fda794: 488b07                   mov rax, qword ptr [rdi]
0x2fda797: ff90d8070000             call qword ptr [rax + 0x7d8]
0x2fda79d: 5d                       pop rbp
0x2fda79e: c3                       ret 
0x2fda79f: cc                       int3 
0x2fda7a0: 55                       push rbp
0x2fda7a1: 4889e5                   mov rbp, rsp
0x2fda7a4: 4157                     push r15
0x2fda7a6: 4156                     push r14
0x2fda7a8: 4154                     push r12
0x2fda7aa: 53                       push rbx
0x2fda7ab: 4883ec10                 sub rsp, 0x10

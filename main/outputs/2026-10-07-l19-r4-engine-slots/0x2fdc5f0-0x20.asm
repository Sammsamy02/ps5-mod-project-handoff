0x2fdc5f0: 488bbf00030000           mov rdi, qword ptr [rdi + 0x300]
0x2fdc5f7: 4885ff                   test rdi, rdi
0x2fdc5fa: 740e                     je 0x2fdc60a
0x2fdc5fc: 55                       push rbp
0x2fdc5fd: 4889e5                   mov rbp, rsp
0x2fdc600: 488b07                   mov rax, qword ptr [rdi]
0x2fdc603: ff90d0070000             call qword ptr [rax + 0x7d0]
0x2fdc609: 5d                       pop rbp
0x2fdc60a: c3                       ret 
0x2fdc60b: cc                       int3 
0x2fdc60c: cc                       int3 
0x2fdc60d: cc                       int3 
0x2fdc60e: cc                       int3 
0x2fdc60f: cc                       int3 

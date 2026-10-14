0x2fd8ef0: 488b0531935406           mov rax, qword ptr [rip + 0x6549331]
0x2fd8ef7: 4885c0                   test rax, rax
0x2fd8efa: 7401                     je 0x2fd8efd
0x2fd8efc: c3                       ret 
0x2fd8efd: 55                       push rbp
0x2fd8efe: 4889e5                   mov rbp, rsp
0x2fd8f01: 488d3d20935406           lea rdi, [rip + 0x6549320]
0x2fd8f08: 488d3551114b05           lea rsi, [rip + 0x54b1151]
0x2fd8f0f: e8fc064cfd               call 0x499610
0x2fd8f14: 488b050d935406           mov rax, qword ptr [rip + 0x654930d]
0x2fd8f1b: 5d                       pop rbp
0x2fd8f1c: c3                       ret 
0x2fd8f1d: cc                       int3 
0x2fd8f1e: cc                       int3 
0x2fd8f1f: cc                       int3 
0x2fd8f20: 488b0591915406           mov rax, qword ptr [rip + 0x6549191]
0x2fd8f27: 4885c0                   test rax, rax
0x2fd8f2a: 7401                     je 0x2fd8f2d
0x2fd8f2c: c3                       ret 
0x2fd8f2d: 55                       push rbp

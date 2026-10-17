0x3001a10: 488b05090e5206           mov rax, qword ptr [rip + 0x6520e09]
0x3001a17: 4885c0                   test rax, rax
0x3001a1a: 7401                     je 0x3001a1d
0x3001a1c: c3                       ret 
0x3001a1d: 55                       push rbp
0x3001a1e: 4889e5                   mov rbp, rsp
0x3001a21: 488d3df80d5206           lea rdi, [rip + 0x6520df8]
0x3001a28: 488d3561944905           lea rsi, [rip + 0x5499461]
0x3001a2f: e8dc7b49fd               call 0x499610
0x3001a34: 488b05e50d5206           mov rax, qword ptr [rip + 0x6520de5]
0x3001a3b: 5d                       pop rbp
0x3001a3c: c3                       ret 
0x3001a3d: cc                       int3 
0x3001a3e: cc                       int3 
0x3001a3f: cc                       int3 

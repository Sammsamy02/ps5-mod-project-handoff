0x2fdbc00: 488bbf08030000           mov rdi, qword ptr [rdi + 0x308]
0x2fdbc07: 4885ff                   test rdi, rdi
0x2fdbc0a: 740f                     je 0x2fdbc1b
0x2fdbc0c: 55                       push rbp
0x2fdbc0d: 4889e5                   mov rbp, rsp
0x2fdbc10: 488b07                   mov rax, qword ptr [rdi]
0x2fdbc13: ff9068070000             call qword ptr [rax + 0x768]
0x2fdbc19: 5d                       pop rbp
0x2fdbc1a: c3                       ret 
0x2fdbc1b: 31c0                     xor eax, eax
0x2fdbc1d: c3                       ret 
0x2fdbc1e: cc                       int3 
0x2fdbc1f: cc                       int3 

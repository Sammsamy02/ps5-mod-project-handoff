0x2fdbc20: 488bbf08030000           mov rdi, qword ptr [rdi + 0x308]
0x2fdbc27: 4885ff                   test rdi, rdi
0x2fdbc2a: 740f                     je 0x2fdbc3b
0x2fdbc2c: 55                       push rbp
0x2fdbc2d: 4889e5                   mov rbp, rsp
0x2fdbc30: 488b07                   mov rax, qword ptr [rdi]
0x2fdbc33: ff9070070000             call qword ptr [rax + 0x770]
0x2fdbc39: 5d                       pop rbp
0x2fdbc3a: c3                       ret 
0x2fdbc3b: 31c0                     xor eax, eax
0x2fdbc3d: c3                       ret 
0x2fdbc3e: cc                       int3 
0x2fdbc3f: cc                       int3 

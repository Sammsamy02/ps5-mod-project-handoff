0x2fdf150: 4885f6                   test rsi, rsi
0x2fdf153: 741e                     je 0x2fdf173
0x2fdf155: 488b86b0020000           mov rax, qword ptr [rsi + 0x2b0]
0x2fdf15c: 4885c0                   test rax, rax
0x2fdf15f: 7412                     je 0x2fdf173
0x2fdf161: 55                       push rbp
0x2fdf162: 4889e5                   mov rbp, rsp
0x2fdf165: 0fb680b2020000           movzx eax, byte ptr [rax + 0x2b2]
0x2fdf16c: 2404                     and al, 4
0x2fdf16e: c0e802                   shr al, 2
0x2fdf171: 5d                       pop rbp
0x2fdf172: c3                       ret 
0x2fdf173: 31c0                     xor eax, eax
0x2fdf175: c3                       ret 
0x2fdf176: cc                       int3 
0x2fdf177: cc                       int3 
0x2fdf178: cc                       int3 
0x2fdf179: cc                       int3 
0x2fdf17a: cc                       int3 
0x2fdf17b: cc                       int3 
0x2fdf17c: cc                       int3 
0x2fdf17d: cc                       int3 
0x2fdf17e: cc                       int3 
0x2fdf17f: cc                       int3 

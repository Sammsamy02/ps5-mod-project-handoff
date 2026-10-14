0x2fdf9b0: 4885f6                   test rsi, rsi
0x2fdf9b3: 7422                     je 0x2fdf9d7
0x2fdf9b5: f6465d02                 test byte ptr [rsi + 0x5d], 2
0x2fdf9b9: 751c                     jne 0x2fdf9d7
0x2fdf9bb: 8b4608                   mov eax, dword ptr [rsi + 8]
0x2fdf9be: a900000040               test eax, 0x40000000
0x2fdf9c3: 7512                     jne 0x2fdf9d7
0x2fdf9c5: 55                       push rbp
0x2fdf9c6: 4889e5                   mov rbp, rsp
0x2fdf9c9: 488b06                   mov rax, qword ptr [rsi]
0x2fdf9cc: 4889f7                   mov rdi, rsi
0x2fdf9cf: ff90000f0000             call qword ptr [rax + 0xf00]
0x2fdf9d5: 5d                       pop rbp
0x2fdf9d6: c3                       ret 
0x2fdf9d7: 31c0                     xor eax, eax
0x2fdf9d9: c3                       ret 
0x2fdf9da: cc                       int3 
0x2fdf9db: cc                       int3 
0x2fdf9dc: cc                       int3 
0x2fdf9dd: cc                       int3 
0x2fdf9de: cc                       int3 
0x2fdf9df: cc                       int3 

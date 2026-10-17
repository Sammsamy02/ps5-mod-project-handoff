0x2fde780: 4885f6                   test rsi, rsi
0x2fde783: 7418                     je 0x2fde79d
0x2fde785: 55                       push rbp
0x2fde786: 4889e5                   mov rbp, rsp
0x2fde789: c6861005000001           mov byte ptr [rsi + 0x510], 1
0x2fde790: 4889f7                   mov rdi, rsi
0x2fde793: 488b06                   mov rax, qword ptr [rsi]
0x2fde796: ff90f80c0000             call qword ptr [rax + 0xcf8]
0x2fde79c: 5d                       pop rbp
0x2fde79d: c3                       ret 
0x2fde79e: cc                       int3 
0x2fde79f: cc                       int3 

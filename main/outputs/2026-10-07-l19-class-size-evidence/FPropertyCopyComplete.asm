0x7e24b0  (f 0x3e24b0)  4839d6                 cmp rsi, rdx
0x7e24b3  (f 0x3e24b3)  7433                   je 0x7e24e8
0x7e24b5  (f 0x3e24b5)  55                     push rbp
0x7e24b6  (f 0x3e24b6)  4889e5                 mov rbp, rsp
0x7e24b9  (f 0x3e24b9)  f6473b40               test byte ptr [rdi + 0x3b], 0x40
0x7e24bd  (f 0x3e24bd)  750e                   jne 0x7e24cd
0x7e24bf  (f 0x3e24bf)  8b4f2c                 mov ecx, dword ptr [rdi + 0x2c]
0x7e24c2  (f 0x3e24c2)  488b07                 mov rax, qword ptr [rdi]
0x7e24c5  (f 0x3e24c5)  ff90f8000000           call qword ptr [rax + 0xf8]
0x7e24cb  (f 0x3e24cb)  eb1a                   jmp 0x7e24e7
0x7e24cd  (f 0x3e24cd)  48634f30               movsxd rcx, dword ptr [rdi + 0x30]
0x7e24d1  (f 0x3e24d1)  4863472c               movsxd rax, dword ptr [rdi + 0x2c]
0x7e24d5  (f 0x3e24d5)  4889f7                 mov rdi, rsi
0x7e24d8  (f 0x3e24d8)  4889d6                 mov rsi, rdx
0x7e24db  (f 0x3e24db)  480fafc1               imul rax, rcx
0x7e24df  (f 0x3e24df)  4889c2                 mov rdx, rax
0x7e24e2  (f 0x3e24e2)  e869f90807             call 0x7871e50
0x7e24e7  (f 0x3e24e7)  5d                     pop rbp
0x7e24e8  (f 0x3e24e8)  c3                     ret 
--- calls: [('0xbe24e2', '0x7871e50')]

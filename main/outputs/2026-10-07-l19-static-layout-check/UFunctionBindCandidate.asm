0x726ec0  (f 0x326ec0)  55                     push rbp
0x726ec1  (f 0x326ec1)  4889e5                 mov rbp, rsp
0x726ec4  (f 0x326ec4)  53                     push rbx
0x726ec5  (f 0x326ec5)  50                     push rax
0x726ec6  (f 0x326ec6)  4889fb                 mov rbx, rdi
0x726ec9  (f 0x326ec9)  e8e268ffff             call 0x71d7b0
0x726ece  (f 0x326ece)  8b4308                 mov eax, dword ptr [rbx + 8]
0x726ed1  (f 0x326ed1)  a810                   test al, 0x10
0x726ed3  (f 0x326ed3)  7519                   jne 0x726eee
0x726ed5  (f 0x326ed5)  488b4320               mov rax, qword ptr [rbx + 0x20]
0x726ed9  (f 0x326ed9)  4885c0                 test rax, rax
0x726edc  (f 0x326edc)  7410                   je 0x726eee
0x726ede  (f 0x326ede)  f683b000000040         test byte ptr [rbx + 0xb0], 0x40
0x726ee5  (f 0x326ee5)  7407                   je 0x726eee
0x726ee7  (f 0x326ee7)  80a0d5000000f7         and byte ptr [rax + 0xd5], 0xf7
0x726eee  (f 0x326eee)  8b83b0000000           mov eax, dword ptr [rbx + 0xb0]
0x726ef4  (f 0x326ef4)  89c1                   mov ecx, eax
0x726ef6  (f 0x326ef6)  f7d1                   not ecx
0x726ef8  (f 0x326ef8)  f7c100200040           test ecx, 0x40002000
0x726efe  (f 0x326efe)  750b                   jne 0x726f0b
0x726f00  (f 0x326f00)  25ffffffbf             and eax, 0xbfffffff
0x726f05  (f 0x326f05)  8983b0000000           mov dword ptr [rbx + 0xb0], eax
0x726f0b  (f 0x326f0b)  4883c408               add rsp, 8
0x726f0f  (f 0x326f0f)  5b                     pop rbx
0x726f10  (f 0x326f10)  5d                     pop rbp
0x726f11  (f 0x326f11)  c3                     ret 
--- calls: [('0xb26ec9', '0x71d7b0')]

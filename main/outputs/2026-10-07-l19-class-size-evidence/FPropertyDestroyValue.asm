0x7e2e60  (f 0x3e2e60)  4885f6                 test rsi, rsi
0x7e2e63  (f 0x3e2e63)  7429                   je 0x7e2e8e
0x7e2e65  (f 0x3e2e65)  55                     push rbp
0x7e2e66  (f 0x3e2e66)  4889e5                 mov rbp, rsp
0x7e2e69  (f 0x3e2e69)  53                     push rbx
0x7e2e6a  (f 0x3e2e6a)  50                     push rax
0x7e2e6b  (f 0x3e2e6b)  f6473c10               test byte ptr [rdi + 0x3c], 0x10
0x7e2e6f  (f 0x3e2e6f)  750f                   jne 0x7e2e80
0x7e2e71  (f 0x3e2e71)  488b07                 mov rax, qword ptr [rdi]
0x7e2e74  (f 0x3e2e74)  4889f3                 mov rbx, rsi
0x7e2e77  (f 0x3e2e77)  ff9040010000           call qword ptr [rax + 0x140]
0x7e2e7d  (f 0x3e2e7d)  4889de                 mov rsi, rbx
0x7e2e80  (f 0x3e2e80)  4889f7                 mov rdi, rsi
0x7e2e83  (f 0x3e2e83)  e8084ccdff             call 0x4b7a90
0x7e2e88  (f 0x3e2e88)  4883c408               add rsp, 8
0x7e2e8c  (f 0x3e2e8c)  5b                     pop rbx
0x7e2e8d  (f 0x3e2e8d)  5d                     pop rbp
0x7e2e8e  (f 0x3e2e8e)  c3                     ret 
--- calls: [('0xbe2e83', '0x4b7a90')]

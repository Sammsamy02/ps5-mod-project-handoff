0x7e2e10  (f 0x3e2e10)  55                     push rbp
0x7e2e11  (f 0x3e2e11)  4889e5                 mov rbp, rsp
0x7e2e14  (f 0x3e2e14)  4156                   push r14
0x7e2e16  (f 0x3e2e16)  53                     push rbx
0x7e2e17  (f 0x3e2e17)  4863472c               movsxd rax, dword ptr [rdi + 0x2c]
0x7e2e1b  (f 0x3e2e1b)  4c637730               movsxd r14, dword ptr [rdi + 0x30]
0x7e2e1f  (f 0x3e2e1f)  4889fb                 mov rbx, rdi
0x7e2e22  (f 0x3e2e22)  4c0faff0               imul r14, rax
0x7e2e26  (f 0x3e2e26)  488b07                 mov rax, qword ptr [rdi]
0x7e2e29  (f 0x3e2e29)  ff9070010000           call qword ptr [rax + 0x170]
0x7e2e2f  (f 0x3e2e2f)  4c89f7                 mov rdi, r14
0x7e2e32  (f 0x3e2e32)  89c6                   mov esi, eax
0x7e2e34  (f 0x3e2e34)  e817c2cdff             call 0x4bf050
0x7e2e39  (f 0x3e2e39)  f6433902               test byte ptr [rbx + 0x39], 2
0x7e2e3d  (f 0x3e2e3d)  7515                   jne 0x7e2e54
0x7e2e3f  (f 0x3e2e3f)  488b0b                 mov rcx, qword ptr [rbx]
0x7e2e42  (f 0x3e2e42)  4889df                 mov rdi, rbx
0x7e2e45  (f 0x3e2e45)  4889c6                 mov rsi, rax
0x7e2e48  (f 0x3e2e48)  4889c3                 mov rbx, rax
0x7e2e4b  (f 0x3e2e4b)  ff9158010000           call qword ptr [rcx + 0x158]
0x7e2e51  (f 0x3e2e51)  4889d8                 mov rax, rbx
0x7e2e54  (f 0x3e2e54)  5b                     pop rbx
0x7e2e55  (f 0x3e2e55)  415e                   pop r14
0x7e2e57  (f 0x3e2e57)  5d                     pop rbp
0x7e2e58  (f 0x3e2e58)  c3                     ret 
--- calls: [('0xbe2e34', '0x4bf050')]

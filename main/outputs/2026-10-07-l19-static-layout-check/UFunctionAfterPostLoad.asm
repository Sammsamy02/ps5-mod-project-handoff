0x726f20  (f 0x326f20)  4885ff                 test rdi, rdi
0x726f23  (f 0x326f23)  0f84b1000000           je 0x726fda
0x726f29  (f 0x326f29)  55                     push rbp
0x726f2a  (f 0x326f2a)  4889e5                 mov rbp, rsp
0x726f2d  (f 0x326f2d)  4889f8                 mov rax, rdi
0x726f30  (f 0x326f30)  488b4850               mov rcx, qword ptr [rax + 0x50]
0x726f34  (f 0x326f34)  4885c9                 test rcx, rcx
0x726f37  (f 0x326f37)  7527                   jne 0x726f60
0x726f39  (f 0x326f39)  0f1f8000000000         nop dword ptr [rax]
0x726f40  (f 0x326f40)  488b08                 mov rcx, qword ptr [rax]
0x726f43  (f 0x326f43)  4889c7                 mov rdi, rax
0x726f46  (f 0x326f46)  ff91c8020000           call qword ptr [rcx + 0x2c8]
0x726f4c  (f 0x326f4c)  4885c0                 test rax, rax
0x726f4f  (f 0x326f4f)  0f847e000000           je 0x726fd3
0x726f55  (f 0x326f55)  488b4850               mov rcx, qword ptr [rax + 0x50]
0x726f59  (f 0x326f59)  4885c9                 test rcx, rcx
0x726f5c  (f 0x326f5c)  74e2                   je 0x726f40
0x726f5e  (f 0x326f5e)  6690                   nop 
0x726f60  (f 0x326f60)  488b5108               mov rdx, qword ptr [rcx + 8]
0x726f64  (f 0x326f64)  8b5210                 mov edx, dword ptr [rdx + 0x10]
0x726f67  (f 0x326f67)  f7d2                   not edx
0x726f69  (f 0x326f69)  f7c201800000           test edx, 0x8001
0x726f6f  (f 0x326f6f)  7416                   je 0x726f87
0x726f71  (f 0x326f71)  488b4918               mov rcx, qword ptr [rcx + 0x18]
0x726f75  (f 0x326f75)  4885c9                 test rcx, rcx
0x726f78  (f 0x326f78)  75e6                   jne 0x726f60
0x726f7a  (f 0x326f7a)  ebc4                   jmp 0x726f40
0x726f80  (f 0x326f80)  31c0                   xor eax, eax
0x726f82  (f 0x326f82)  4885c9                 test rcx, rcx
0x726f85  (f 0x326f85)  744c                   je 0x726fd3
0x726f87  (f 0x326f87)  488b5138               mov rdx, qword ptr [rcx + 0x38]
0x726f8b  (f 0x326f8b)  84d2                   test dl, dl
0x726f8d  (f 0x326f8d)  7944                   jns 0x726fd3
0x726f8f  (f 0x326f8f)  f7c200040000           test edx, 0x400
0x726f95  (f 0x326f95)  753e                   jne 0x726fd5
0x726f97  (f 0x326f97)  488b4918               mov rcx, qword ptr [rcx + 0x18]
0x726f9b  (f 0x326f9b)  4885c0                 test rax, rax
0x726f9e  (f 0x326f9e)  74e0                   je 0x726f80
0x726fa0  (f 0x326fa0)  4885c9                 test rcx, rcx
0x726fa3  (f 0x326fa3)  7417                   je 0x726fbc
0x726fa5  (f 0x326fa5)  488b5108               mov rdx, qword ptr [rcx + 8]
0x726fa9  (f 0x326fa9)  8b5210                 mov edx, dword ptr [rdx + 0x10]
0x726fac  (f 0x326fac)  f7d2                   not edx
0x726fae  (f 0x326fae)  f7c201800000           test edx, 0x8001
0x726fb4  (f 0x326fb4)  74cc                   je 0x726f82
0x726fb6  (f 0x326fb6)  488b4918               mov rcx, qword ptr [rcx + 0x18]
0x726fba  (f 0x326fba)  ebe4                   jmp 0x726fa0
0x726fbc  (f 0x326fbc)  488b08                 mov rcx, qword ptr [rax]
0x726fbf  (f 0x326fbf)  4889c7                 mov rdi, rax
0x726fc2  (f 0x326fc2)  ff91c8020000           call qword ptr [rcx + 0x2c8]
0x726fc8  (f 0x326fc8)  4885c0                 test rax, rax
0x726fcb  (f 0x326fcb)  7406                   je 0x726fd3
0x726fcd  (f 0x326fcd)  488b4850               mov rcx, qword ptr [rax + 0x50]
0x726fd1  (f 0x326fd1)  ebcd                   jmp 0x726fa0
0x726fd3  (f 0x326fd3)  31c9                   xor ecx, ecx
0x726fd5  (f 0x326fd5)  5d                     pop rbp
0x726fd6  (f 0x326fd6)  4889c8                 mov rax, rcx
0x726fd9  (f 0x326fd9)  c3                     ret 
0x726fda  (f 0x326fda)  31c9                   xor ecx, ecx
0x726fdc  (f 0x326fdc)  4889c8                 mov rax, rcx
0x726fdf  (f 0x326fdf)  c3                     ret 
--- calls: []

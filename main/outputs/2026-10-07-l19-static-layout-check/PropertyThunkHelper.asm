0x8577d0  (f 0x4577d0)  55                     push rbp
0x8577d1  (f 0x4577d1)  4889e5                 mov rbp, rsp
0x8577d4  (f 0x4577d4)  f6423901               test byte ptr [rdx + 0x39], 1
0x8577d8  (f 0x4577d8)  7524                   jne 0x8577fe
0x8577da  (f 0x4577da)  488b4728               mov rax, qword ptr [rdi + 0x28]
0x8577de  (f 0x4577de)  48634a44               movsxd rcx, dword ptr [rdx + 0x44]
0x8577e2  (f 0x4577e2)  4801c1                 add rcx, rax
0x8577e5  (f 0x4577e5)  48894f38               mov qword ptr [rdi + 0x38], rcx
0x8577e9  (f 0x4577e9)  48894740               mov qword ptr [rdi + 0x40], rax
0x8577ed  (f 0x4577ed)  4889d7                 mov rdi, rdx
0x8577f0  (f 0x4577f0)  488b0a                 mov rcx, qword ptr [rdx]
0x8577f3  (f 0x4577f3)  4889c2                 mov rdx, rax
0x8577f6  (f 0x4577f6)  ff9118010000           call qword ptr [rcx + 0x118]
0x8577fc  (f 0x4577fc)  5d                     pop rbp
0x8577fd  (f 0x4577fd)  c3                     ret 
0x8577fe  (f 0x4577fe)  488d8780000000         lea rax, [rdi + 0x80]
0x857805  (f 0x457805)  66662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x857810  (f 0x457810)  488b08                 mov rcx, qword ptr [rax]
0x857813  (f 0x457813)  488d4110               lea rax, [rcx + 0x10]
0x857817  (f 0x457817)  483911                 cmp qword ptr [rcx], rdx
0x85781a  (f 0x45781a)  75f4                   jne 0x857810
0x85781c  (f 0x45781c)  488b4108               mov rax, qword ptr [rcx + 8]
0x857820  (f 0x457820)  48894738               mov qword ptr [rdi + 0x38], rax
0x857824  (f 0x457824)  48c7474000000000       mov qword ptr [rdi + 0x40], 0
0x85782c  (f 0x45782c)  5d                     pop rbp
0x85782d  (f 0x45782d)  c3                     ret 
--- calls: []

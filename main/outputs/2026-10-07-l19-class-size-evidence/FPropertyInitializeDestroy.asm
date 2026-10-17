0x7e2580  (f 0x3e2580)  55                     push rbp
0x7e2581  (f 0x3e2581)  4889e5                 mov rbp, rsp
0x7e2584  (f 0x3e2584)  4157                   push r15
0x7e2586  (f 0x3e2586)  4156                   push r14
0x7e2588  (f 0x3e2588)  53                     push rbx
0x7e2589  (f 0x3e2589)  50                     push rax
0x7e258a  (f 0x3e258a)  488b07                 mov rax, qword ptr [rdi]
0x7e258d  (f 0x3e258d)  4889d3                 mov rbx, rdx
0x7e2590  (f 0x3e2590)  4989f6                 mov r14, rsi
0x7e2593  (f 0x3e2593)  4989ff                 mov r15, rdi
0x7e2596  (f 0x3e2596)  ff5078                 call qword ptr [rax + 0x78]
0x7e2599  (f 0x3e2599)  84c0                   test al, al
0x7e259b  (f 0x3e259b)  7414                   je 0x7e25b1
0x7e259d  (f 0x3e259d)  498b07                 mov rax, qword ptr [r15]
0x7e25a0  (f 0x3e25a0)  4c89ff                 mov rdi, r15
0x7e25a3  (f 0x3e25a3)  4c89f6                 mov rsi, r14
0x7e25a6  (f 0x3e25a6)  4889da                 mov rdx, rbx
0x7e25a9  (f 0x3e25a9)  ff9090000000           call qword ptr [rax + 0x90]
0x7e25af  (f 0x3e25af)  eb42                   jmp 0x7e25f3
0x7e25b1  (f 0x3e25b1)  49634744               movsxd rax, dword ptr [r15 + 0x44]
0x7e25b5  (f 0x3e25b5)  4901c6                 add r14, rax
0x7e25b8  (f 0x3e25b8)  4939de                 cmp r14, rbx
0x7e25bb  (f 0x3e25bb)  7436                   je 0x7e25f3
0x7e25bd  (f 0x3e25bd)  41f6473b40             test byte ptr [r15 + 0x3b], 0x40
0x7e25c2  (f 0x3e25c2)  7518                   jne 0x7e25dc
0x7e25c4  (f 0x3e25c4)  418b4f2c               mov ecx, dword ptr [r15 + 0x2c]
0x7e25c8  (f 0x3e25c8)  498b07                 mov rax, qword ptr [r15]
0x7e25cb  (f 0x3e25cb)  4c89ff                 mov rdi, r15
0x7e25ce  (f 0x3e25ce)  4c89f6                 mov rsi, r14
0x7e25d1  (f 0x3e25d1)  4889da                 mov rdx, rbx
0x7e25d4  (f 0x3e25d4)  ff90f8000000           call qword ptr [rax + 0xf8]
0x7e25da  (f 0x3e25da)  eb17                   jmp 0x7e25f3
0x7e25dc  (f 0x3e25dc)  49634730               movsxd rax, dword ptr [r15 + 0x30]
0x7e25e0  (f 0x3e25e0)  4963572c               movsxd rdx, dword ptr [r15 + 0x2c]
0x7e25e4  (f 0x3e25e4)  4c89f7                 mov rdi, r14
0x7e25e7  (f 0x3e25e7)  4889de                 mov rsi, rbx
0x7e25ea  (f 0x3e25ea)  480fafd0               imul rdx, rax
0x7e25ee  (f 0x3e25ee)  e85df80807             call 0x7871e50
0x7e25f3  (f 0x3e25f3)  4883c408               add rsp, 8
0x7e25f7  (f 0x3e25f7)  5b                     pop rbx
0x7e25f8  (f 0x3e25f8)  415e                   pop r14
0x7e25fa  (f 0x3e25fa)  415f                   pop r15
0x7e25fc  (f 0x3e25fc)  5d                     pop rbp
0x7e25fd  (f 0x3e25fd)  c3                     ret 
--- calls: [('0xbe25ee', '0x7871e50')]

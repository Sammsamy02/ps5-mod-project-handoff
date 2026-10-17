0x7e2470  (f 0x3e2470)  4839d6                 cmp rsi, rdx
0x7e2473  (f 0x3e2473)  742d                   je 0x7e24a2
0x7e2475  (f 0x3e2475)  55                     push rbp
0x7e2476  (f 0x3e2476)  4889e5                 mov rbp, rsp
0x7e2479  (f 0x3e2479)  f6473b40               test byte ptr [rdi + 0x3b], 0x40
0x7e247d  (f 0x3e247d)  7510                   jne 0x7e248f
0x7e247f  (f 0x3e247f)  488b07                 mov rax, qword ptr [rdi]
0x7e2482  (f 0x3e2482)  b901000000             mov ecx, 1
0x7e2487  (f 0x3e2487)  ff90f8000000           call qword ptr [rax + 0xf8]
0x7e248d  (f 0x3e248d)  eb12                   jmp 0x7e24a1
0x7e248f  (f 0x3e248f)  48634730               movsxd rax, dword ptr [rdi + 0x30]
0x7e2493  (f 0x3e2493)  4889f7                 mov rdi, rsi
0x7e2496  (f 0x3e2496)  4889d6                 mov rsi, rdx
0x7e2499  (f 0x3e2499)  4889c2                 mov rdx, rax
0x7e249c  (f 0x3e249c)  e8aff90807             call 0x7871e50
0x7e24a1  (f 0x3e24a1)  5d                     pop rbp
0x7e24a2  (f 0x3e24a2)  c3                     ret 
--- calls: [('0xbe249c', '0x7871e50')]

0x2fdb960: 55                       push rbp
0x2fdb961: 4889e5                   mov rbp, rsp
0x2fdb964: 4156                     push r14
0x2fdb966: 53                       push rbx
0x2fdb967: 4c8bb7e0020000           mov r14, qword ptr [rdi + 0x2e0]
0x2fdb96e: 4d85f6                   test r14, r14
0x2fdb971: 7425                     je 0x2fdb998
0x2fdb973: 4889fb                   mov rbx, rdi
0x2fdb976: e8c5e13700               call 0x3359b40
0x2fdb97b: 4885c0                   test rax, rax
0x2fdb97e: 7418                     je 0x2fdb998
0x2fdb980: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdb984: 413b4e38                 cmp ecx, dword ptr [r14 + 0x38]
0x2fdb988: 7f0e                     jg 0x2fdb998
0x2fdb98a: 498b5630                 mov rdx, qword ptr [r14 + 0x30]
0x2fdb98e: 4883c030                 add rax, 0x30
0x2fdb992: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdb996: 7407                     je 0x2fdb99f
0x2fdb998: 31c0                     xor eax, eax
0x2fdb99a: 5b                       pop rbx
0x2fdb99b: 415e                     pop r14
0x2fdb99d: 5d                       pop rbp
0x2fdb99e: c3                       ret 
0x2fdb99f: 488b83e0020000           mov rax, qword ptr [rbx + 0x2e0]
0x2fdb9a6: 5b                       pop rbx
0x2fdb9a7: 415e                     pop r14
0x2fdb9a9: 5d                       pop rbp
0x2fdb9aa: c3                       ret 
0x2fdb9ab: cc                       int3 
0x2fdb9ac: cc                       int3 
0x2fdb9ad: cc                       int3 
0x2fdb9ae: cc                       int3 
0x2fdb9af: cc                       int3 

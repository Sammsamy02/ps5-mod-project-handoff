0x764b80  (f 0x364b80)  55                     push rbp
0x764b81  (f 0x364b81)  4889e5                 mov rbp, rsp
0x764b84  (f 0x364b84)  4157                   push r15
0x764b86  (f 0x364b86)  4156                   push r14
0x764b88  (f 0x364b88)  53                     push rbx
0x764b89  (f 0x364b89)  50                     push rax
0x764b8a  (f 0x364b8a)  4c8b7e10               mov r15, qword ptr [rsi + 0x10]
0x764b8e  (f 0x364b8e)  4989f6                 mov r14, rsi
0x764b91  (f 0x364b91)  4889fb                 mov rbx, rdi
0x764b94  (f 0x364b94)  41f6c701               test r15b, 1
0x764b98  (f 0x364b98)  752a                   jne 0x764bc4
0x764b9a  (f 0x364b9a)  660f1f440000           nop word ptr [rax + rax]
0x764ba0  (f 0x364ba0)  4983ff02               cmp r15, 2
0x764ba4  (f 0x364ba4)  7243                   jb 0x764be9
0x764ba6  (f 0x364ba6)  4d8b7f10               mov r15, qword ptr [r15 + 0x10]
0x764baa  (f 0x364baa)  41f6c701               test r15b, 1
0x764bae  (f 0x364bae)  74f0                   je 0x764ba0
0x764bb0  (f 0x364bb0)  4c89ff                 mov rdi, r15
0x764bb3  (f 0x364bb3)  4883e7fe               and rdi, 0xfffffffffffffffe
0x764bb7  (f 0x364bb7)  7424                   je 0x764bdd
0x764bb9  (f 0x364bb9)  803d9ca2030901         cmp byte ptr [rip + 0x903a29c], 1  ; rip->0x979ee5c (file 0x939ee5c)
0x764bc0  (f 0x364bc0)  7414                   je 0x764bd6
0x764bc2  (f 0x364bc2)  eb1f                   jmp 0x764be3
0x764bc4  (f 0x364bc4)  4c89ff                 mov rdi, r15
0x764bc7  (f 0x364bc7)  4883e7fe               and rdi, 0xfffffffffffffffe
0x764bcb  (f 0x364bcb)  7410                   je 0x764bdd
0x764bcd  (f 0x364bcd)  803d88a2030900         cmp byte ptr [rip + 0x903a288], 0  ; rip->0x979ee5c (file 0x939ee5c)
0x764bd4  (f 0x364bd4)  740d                   je 0x764be3
0x764bd6  (f 0x364bd6)  e8f5360000             call 0x7682d0
0x764bdb  (f 0x364bdb)  eb06                   jmp 0x764be3
0x764bdd  (f 0x364bdd)  41bf01000000           mov r15d, 1
0x764be3  (f 0x364be3)  4983e7fe               and r15, 0xfffffffffffffffe
0x764be7  (f 0x364be7)  eb10                   jmp 0x764bf9
0x764be9  (f 0x364be9)  4531ff                 xor r15d, r15d
0x764bec  (f 0x364bec)  eb0b                   jmp 0x764bf9
0x764bf0  (f 0x364bf0)  4d8b7f20               mov r15, qword ptr [r15 + 0x20]
0x764bf4  (f 0x364bf4)  4d85ff                 test r15, r15
0x764bf7  (f 0x364bf7)  7439                   je 0x764c32
0x764bf9  (f 0x364bf9)  4d85ff                 test r15, r15
0x764bfc  (f 0x364bfc)  74f2                   je 0x764bf0
0x764bfe  (f 0x364bfe)  e8edbefbff             call 0x720af0
0x764c03  (f 0x364c03)  498b4f10               mov rcx, qword ptr [r15 + 0x10]
0x764c07  (f 0x364c07)  48635038               movsxd rdx, dword ptr [rax + 0x38]
0x764c0b  (f 0x364c0b)  3b5138                 cmp edx, dword ptr [rcx + 0x38]
0x764c0e  (f 0x364c0e)  7fe0                   jg 0x764bf0
0x764c10  (f 0x364c10)  488b4930               mov rcx, qword ptr [rcx + 0x30]
0x764c14  (f 0x364c14)  4883c030               add rax, 0x30
0x764c18  (f 0x364c18)  483904d1               cmp qword ptr [rcx + rdx*8], rax
0x764c1c  (f 0x364c1c)  75d2                   jne 0x764bf0
0x764c1e  (f 0x364c1e)  498b07                 mov rax, qword ptr [r15]
0x764c21  (f 0x364c21)  4889df                 mov rdi, rbx
0x764c24  (f 0x364c24)  4c89fe                 mov rsi, r15
0x764c27  (f 0x364c27)  4c89f2                 mov rdx, r14
0x764c2a  (f 0x364c2a)  ff9038030000           call qword ptr [rax + 0x338]
0x764c30  (f 0x364c30)  eb08                   jmp 0x764c3a
0x764c32  (f 0x364c32)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x764c36  (f 0x364c36)  c5f81103               vmovups xmmword ptr [rbx], xmm0
0x764c3a  (f 0x364c3a)  4889d8                 mov rax, rbx
0x764c3d  (f 0x364c3d)  4883c408               add rsp, 8
0x764c41  (f 0x364c41)  5b                     pop rbx
0x764c42  (f 0x364c42)  415e                   pop r14
0x764c44  (f 0x364c44)  415f                   pop r15
0x764c46  (f 0x364c46)  5d                     pop rbp
0x764c47  (f 0x364c47)  c3                     ret 
--- calls: [('0xb64bfe', '0x720af0'), ('0xb64bd6', '0x7682d0')]

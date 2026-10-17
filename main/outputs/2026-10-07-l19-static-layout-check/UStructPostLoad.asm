0x71d7b0  (f 0x31d7b0)  55                     push rbp
0x71d7b1  (f 0x31d7b1)  4889e5                 mov rbp, rsp
0x71d7b4  (f 0x31d7b4)  4157                   push r15
0x71d7b6  (f 0x31d7b6)  4156                   push r14
0x71d7b8  (f 0x31d7b8)  4155                   push r13
0x71d7ba  (f 0x31d7ba)  4154                   push r12
0x71d7bc  (f 0x31d7bc)  53                     push rbx
0x71d7bd  (f 0x31d7bd)  4883ec38               sub rsp, 0x38
0x71d7c1  (f 0x31d7c1)  4c8b3d4047e108         mov r15, qword ptr [rip + 0x8e14740]  ; rip->0x9531f08 (file 0x9131f08)
0x71d7c8  (f 0x31d7c8)  4889fb                 mov rbx, rdi
0x71d7cb  (f 0x31d7cb)  498b07                 mov rax, qword ptr [r15]
0x71d7ce  (f 0x31d7ce)  488945d0               mov qword ptr [rbp - 0x30], rax
0x71d7d2  (f 0x31d7d2)  e8d9910a00             call 0x7c69b0
0x71d7d7  (f 0x31d7d7)  488b03                 mov rax, qword ptr [rbx]
0x71d7da  (f 0x31d7da)  4889df                 mov rdi, rbx
0x71d7dd  (f 0x31d7dd)  ff90c0020000           call qword ptr [rax + 0x2c0]
0x71d7e3  (f 0x31d7e3)  4c8bb3a0000000         mov r14, qword ptr [rbx + 0xa0]
0x71d7ea  (f 0x31d7ea)  4d85f6                 test r14, r14
0x71d7ed  (f 0x31d7ed)  0f84d4010000           je 0x71d9c7
0x71d7f3  (f 0x31d7f3)  49634608               movsxd rax, dword ptr [r14 + 8]
0x71d7f7  (f 0x31d7f7)  4885c0                 test rax, rax
0x71d7fa  (f 0x31d7fa)  0f849b010000           je 0x71d99b
0x71d800  (f 0x31d800)  4d8b36                 mov r14, qword ptr [r14]
0x71d803  (f 0x31d803)  488d0480               lea rax, [rax + rax*4]
0x71d807  (f 0x31d807)  4d8d2cc6               lea r13, [r14 + rax*8]
0x71d80b  (f 0x31d80b)  488d8390000000         lea rax, [rbx + 0x90]
0x71d812  (f 0x31d812)  488945a8               mov qword ptr [rbp - 0x58], rax
0x71d816  (f 0x31d816)  eb15                   jmp 0x71d82d
0x71d820  (f 0x31d820)  4983c628               add r14, 0x28
0x71d824  (f 0x31d824)  4d39ee                 cmp r14, r13
0x71d827  (f 0x31d827)  0f845b010000           je 0x71d988
0x71d82d  (f 0x31d82d)  e8fe6c0400             call 0x764530
0x71d832  (f 0x31d832)  49833e00               cmp qword ptr [r14], 0
0x71d836  (f 0x31d836)  4989c4                 mov r12, rax
0x71d839  (f 0x31d839)  740d                   je 0x71d848
0x71d83b  (f 0x31d83b)  498d7e08               lea rdi, [r14 + 8]
0x71d83f  (f 0x31d83f)  e8fc931900             call 0x8b6c40
0x71d844  (f 0x31d844)  84c0                   test al, al
0x71d846  (f 0x31d846)  7548                   jne 0x71d890
0x71d848  (f 0x31d848)  41837e1800             cmp dword ptr [r14 + 0x18], 0
0x71d84d  (f 0x31d84d)  7441                   je 0x71d890
0x71d84f  (f 0x31d84f)  4c89f7                 mov rdi, r14
0x71d852  (f 0x31d852)  4889de                 mov rsi, rbx
0x71d855  (f 0x31d855)  31d2                   xor edx, edx
0x71d857  (f 0x31d857)  e8a4810400             call 0x765a00
0x71d85c  (f 0x31d85c)  4885c0                 test rax, rax
0x71d85f  (f 0x31d85f)  0f840b010000           je 0x71d970
0x71d865  (f 0x31d865)  4889c7                 mov rdi, rax
0x71d868  (f 0x31d868)  488b4008               mov rax, qword ptr [rax + 8]
0x71d86c  (f 0x31d86c)  498b4c2408             mov rcx, qword ptr [r12 + 8]
0x71d871  (f 0x31d871)  4885c9                 test rcx, rcx
0x71d874  (f 0x31d874)  0f84bb000000           je 0x71d935
0x71d87a  (f 0x31d87a)  48854810               test qword ptr [rax + 0x10], rcx
0x71d87e  (f 0x31d87e)  0f84ec000000           je 0x71d970
0x71d884  (f 0x31d884)  49893e                 mov qword ptr [r14], rdi
0x71d887  (f 0x31d887)  4885ff                 test rdi, rdi
0x71d88a  (f 0x31d88a)  750c                   jne 0x71d898
0x71d88c  (f 0x31d88c)  eb92                   jmp 0x71d820
0x71d890  (f 0x31d890)  498b3e                 mov rdi, qword ptr [r14]
0x71d893  (f 0x31d893)  4885ff                 test rdi, rdi
0x71d896  (f 0x31d896)  7488                   je 0x71d820
0x71d898  (f 0x31d898)  488b4360               mov rax, qword ptr [rbx + 0x60]
0x71d89c  (f 0x31d89c)  49634e20               movsxd rcx, dword ptr [r14 + 0x20]
0x71d8a0  (f 0x31d8a0)  488d15a9c7bb07         lea rdx, [rip + 0x7bbc7a9]  ; rip->0x82da050 (file 0x7eda050)
0x71d8a7  (f 0x31d8a7)  4c8d7db0               lea r15, [rbp - 0x50]
0x71d8ab  (f 0x31d8ab)  4c89fe                 mov rsi, r15
0x71d8ae  (f 0x31d8ae)  48893c08               mov qword ptr [rax + rcx], rdi
0x71d8b2  (f 0x31d8b2)  488b4da8               mov rcx, qword ptr [rbp - 0x58]
0x71d8b6  (f 0x31d8b6)  48c745b800000000       mov qword ptr [rbp - 0x48], 0
0x71d8be  (f 0x31d8be)  488955b0               mov qword ptr [rbp - 0x50], rdx
0x71d8c2  (f 0x31d8c2)  48895dc0               mov qword ptr [rbp - 0x40], rbx
0x71d8c6  (f 0x31d8c6)  48894dc8               mov qword ptr [rbp - 0x38], rcx
0x71d8ca  (f 0x31d8ca)  488b07                 mov rax, qword ptr [rdi]
0x71d8cd  (f 0x31d8cd)  ff5038                 call qword ptr [rax + 0x38]
0x71d8d0  (f 0x31d8d0)  4c89ff                 mov rdi, r15
0x71d8d3  (f 0x31d8d3)  e8a8601700             call 0x893980
0x71d8d8  (f 0x31d8d8)  488d057d150809         lea rax, [rip + 0x908157d]  ; rip->0x979ee5c (file 0x939ee5c)
0x71d8df  (f 0x31d8df)  803801                 cmp byte ptr [rax], 1
0x71d8e2  (f 0x31d8e2)  0f8538ffffff           jne 0x71d820
0x71d8e8  (f 0x31d8e8)  8b8398000000           mov eax, dword ptr [rbx + 0x98]
0x71d8ee  (f 0x31d8ee)  85c0                   test eax, eax
0x71d8f0  (f 0x31d8f0)  0f8e2affffff           jle 0x71d820
0x71d8f6  (f 0x31d8f6)  4c8ba390000000         mov r12, qword ptr [rbx + 0x90]
0x71d8fd  (f 0x31d8fd)  4531ff                 xor r15d, r15d
0x71d900  (f 0x31d900)  eb1d                   jmp 0x71d91f
0x71d910  (f 0x31d910)  49ffc7                 inc r15
0x71d913  (f 0x31d913)  4863c8                 movsxd rcx, eax
0x71d916  (f 0x31d916)  4939cf                 cmp r15, rcx
0x71d919  (f 0x31d919)  0f8d01ffffff           jge 0x71d820
0x71d91f  (f 0x31d91f)  4b8b3cfc               mov rdi, qword ptr [r12 + r15*8]
0x71d923  (f 0x31d923)  4885ff                 test rdi, rdi
0x71d926  (f 0x31d926)  74e8                   je 0x71d910
0x71d928  (f 0x31d928)  e893a90400             call 0x7682c0
0x71d92d  (f 0x31d92d)  8b8398000000           mov eax, dword ptr [rbx + 0x98]
0x71d933  (f 0x31d933)  ebdb                   jmp 0x71d910
0x71d935  (f 0x31d935)  4885c0                 test rax, rax
0x71d938  (f 0x31d938)  0f95c1                 setne cl
0x71d93b  (f 0x31d93b)  0f94c2                 sete dl
0x71d93e  (f 0x31d93e)  4c39e0                 cmp rax, r12
0x71d941  (f 0x31d941)  400f94c6               sete sil
0x71d945  (f 0x31d945)  4008d6                 or sil, dl
0x71d948  (f 0x31d948)  751a                   jne 0x71d964
0x71d94a  (f 0x31d94a)  660f1f440000           nop word ptr [rax + rax]
0x71d950  (f 0x31d950)  488b4020               mov rax, qword ptr [rax + 0x20]
0x71d954  (f 0x31d954)  4885c0                 test rax, rax
0x71d957  (f 0x31d957)  0f95c1                 setne cl
0x71d95a  (f 0x31d95a)  4c39e0                 cmp rax, r12
0x71d95d  (f 0x31d95d)  7405                   je 0x71d964
0x71d95f  (f 0x31d95f)  4885c0                 test rax, rax
0x71d962  (f 0x31d962)  75ec                   jne 0x71d950
0x71d964  (f 0x31d964)  84c9                   test cl, cl
0x71d966  (f 0x31d966)  0f8518ffffff           jne 0x71d884
0x71d96c  (f 0x31d96c)  0f1f4000               nop dword ptr [rax]
0x71d970  (f 0x31d970)  49833e00               cmp qword ptr [r14], 0
0x71d974  (f 0x31d974)  0f84a6feffff           je 0x71d820
0x71d97a  (f 0x31d97a)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71d97e  (f 0x31d97e)  c4c1781106             vmovups xmmword ptr [r14], xmm0
0x71d983  (f 0x31d983)  e998feffff             jmp 0x71d820
0x71d988  (f 0x31d988)  4c8bb3a0000000         mov r14, qword ptr [rbx + 0xa0]
0x71d98f  (f 0x31d98f)  4c8b3d7245e108         mov r15, qword ptr [rip + 0x8e14572]  ; rip->0x9531f08 (file 0x9131f08)
0x71d996  (f 0x31d996)  4d85f6                 test r14, r14
0x71d999  (f 0x31d999)  742c                   je 0x71d9c7
0x71d99b  (f 0x31d99b)  498b3e                 mov rdi, qword ptr [r14]
0x71d99e  (f 0x31d99e)  418b7608               mov esi, dword ptr [r14 + 8]
0x71d9a2  (f 0x31d9a2)  e849020100             call 0x72dbf0
0x71d9a7  (f 0x31d9a7)  498b3e                 mov rdi, qword ptr [r14]
0x71d9aa  (f 0x31d9aa)  4885ff                 test rdi, rdi
0x71d9ad  (f 0x31d9ad)  7405                   je 0x71d9b4
0x71d9af  (f 0x31d9af)  e8dca0d9ff             call 0x4b7a90
0x71d9b4  (f 0x31d9b4)  4c89f7                 mov rdi, r14
0x71d9b7  (f 0x31d9b7)  e804ffd2ff             call 0x44d8c0
0x71d9bc  (f 0x31d9bc)  48c783a000000000000000 mov qword ptr [rbx + 0xa0], 0
0x71d9c7  (f 0x31d9c7)  498b07                 mov rax, qword ptr [r15]
0x71d9ca  (f 0x31d9ca)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x71d9ce  (f 0x31d9ce)  750f                   jne 0x71d9df
0x71d9d0  (f 0x31d9d0)  4883c438               add rsp, 0x38
0x71d9d4  (f 0x31d9d4)  5b                     pop rbx
0x71d9d5  (f 0x31d9d5)  415c                   pop r12
0x71d9d7  (f 0x31d9d7)  415d                   pop r13
0x71d9d9  (f 0x31d9d9)  415e                   pop r14
0x71d9db  (f 0x31d9db)  415f                   pop r15
0x71d9dd  (f 0x31d9dd)  5d                     pop rbp
0x71d9de  (f 0x31d9de)  c3                     ret 
0x71d9df  (f 0x31d9df)  e87c441507             call 0x7871e60
0x71d9e4  (f 0x31d9e4)  0f0b                   ud2 
--- calls: [('0xb1d7d2', '0x7c69b0'), ('0xb1d82d', '0x764530'), ('0xb1d83f', '0x8b6c40'), ('0xb1d857', '0x765a00'), ('0xb1d9a2', '0x72dbf0'), ('0xb1d9af', '0x4b7a90'), ('0xb1d9b7', '0x44d8c0'), ('0xb1d9df', '0x7871e60'), ('0xb1d8d3', '0x893980'), ('0xb1d928', '0x7682c0')]

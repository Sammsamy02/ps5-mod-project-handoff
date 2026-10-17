0x7e1b50  (f 0x3e1b50)  55                     push rbp
0x7e1b51  (f 0x3e1b51)  4889e5                 mov rbp, rsp
0x7e1b54  (f 0x3e1b54)  4157                   push r15
0x7e1b56  (f 0x3e1b56)  4156                   push r14
0x7e1b58  (f 0x3e1b58)  4155                   push r13
0x7e1b5a  (f 0x3e1b5a)  4154                   push r12
0x7e1b5c  (f 0x3e1b5c)  53                     push rbx
0x7e1b5d  (f 0x3e1b5d)  4883ec18               sub rsp, 0x18
0x7e1b61  (f 0x3e1b61)  488b05a003d508         mov rax, qword ptr [rip + 0x8d503a0]  ; rip->0x9531f08 (file 0x9131f08)
0x7e1b68  (f 0x3e1b68)  8955c4                 mov dword ptr [rbp - 0x3c], edx
0x7e1b6b  (f 0x3e1b6b)  4989fc                 mov r12, rdi
0x7e1b6e  (f 0x3e1b6e)  bf70000000             mov edi, 0x70
0x7e1b73  (f 0x3e1b73)  4989f7                 mov r15, rsi
0x7e1b76  (f 0x3e1b76)  31f6                   xor esi, esi
0x7e1b78  (f 0x3e1b78)  488b00                 mov rax, qword ptr [rax]
0x7e1b7b  (f 0x3e1b7b)  488945d0               mov qword ptr [rbp - 0x30], rax
0x7e1b7f  (f 0x3e1b7f)  e89c5ecdff             call 0x4b7a20
0x7e1b84  (f 0x3e1b84)  0fb60de559fc08         movzx ecx, byte ptr [rip + 0x8fc59e5]  ; rip->0x97a7570 (file 0x93a7570)
0x7e1b8b  (f 0x3e1b8b)  4889c3                 mov rbx, rax
0x7e1b8e  (f 0x3e1b8e)  84c9                   test cl, cl
0x7e1b90  (f 0x3e1b90)  0f84c7000000           je 0x7e1c5d
0x7e1b96  (f 0x3e1b96)  488d159359fc08         lea rdx, [rip + 0x8fc5993]  ; rip->0x97a7530 (file 0x93a7530)
0x7e1b9d  (f 0x3e1b9d)  4889df                 mov rdi, rbx
0x7e1ba0  (f 0x3e1ba0)  31f6                   xor esi, esi
0x7e1ba2  (f 0x3e1ba2)  e8192af8ff             call 0x7645c0
0x7e1ba7  (f 0x3e1ba7)  4c8d2d8a88b007         lea r13, [rip + 0x7b0888a]  ; rip->0x82ea438 (file 0x7eea438)
0x7e1bae  (f 0x3e1bae)  4c892b                 mov qword ptr [rbx], r13
0x7e1bb1  (f 0x3e1bb1)  48c7436800000000       mov qword ptr [rbx + 0x68], 0
0x7e1bb9  (f 0x3e1bb9)  4d8b2424               mov r12, qword ptr [r12]
0x7e1bbd  (f 0x3e1bbd)  41f6c401               test r12b, 1
0x7e1bc1  (f 0x3e1bc1)  7437                   je 0x7e1bfa
0x7e1bc3  (f 0x3e1bc3)  4d89e6                 mov r14, r12
0x7e1bc6  (f 0x3e1bc6)  4983e6fe               and r14, 0xfffffffffffffffe
0x7e1bca  (f 0x3e1bca)  742e                   je 0x7e1bfa
0x7e1bcc  (f 0x3e1bcc)  488d0589d2fb08         lea rax, [rip + 0x8fbd289]  ; rip->0x979ee5c (file 0x939ee5c)
0x7e1bd3  (f 0x3e1bd3)  803801                 cmp byte ptr [rax], 1
0x7e1bd6  (f 0x3e1bd6)  750f                   jne 0x7e1be7
0x7e1bd8  (f 0x3e1bd8)  4c89f7                 mov rdi, r14
0x7e1bdb  (f 0x3e1bdb)  e8e066f8ff             call 0x7682c0
0x7e1be0  (f 0x3e1be0)  488d0575d2fb08         lea rax, [rip + 0x8fbd275]  ; rip->0x979ee5c (file 0x939ee5c)
0x7e1be7  (f 0x3e1be7)  803801                 cmp byte ptr [rax], 1
0x7e1bea  (f 0x3e1bea)  4c8965c8               mov qword ptr [rbp - 0x38], r12
0x7e1bee  (f 0x3e1bee)  750e                   jne 0x7e1bfe
0x7e1bf0  (f 0x3e1bf0)  4c89f7                 mov rdi, r14
0x7e1bf3  (f 0x3e1bf3)  e8c866f8ff             call 0x7682c0
0x7e1bf8  (f 0x3e1bf8)  eb04                   jmp 0x7e1bfe
0x7e1bfa  (f 0x3e1bfa)  4c8965c8               mov qword ptr [rbp - 0x38], r12
0x7e1bfe  (f 0x3e1bfe)  8b4dc4                 mov ecx, dword ptr [rbp - 0x3c]
0x7e1c01  (f 0x3e1c01)  488d75c8               lea rsi, [rbp - 0x38]
0x7e1c05  (f 0x3e1c05)  4889df                 mov rdi, rbx
0x7e1c08  (f 0x3e1c08)  4c89fa                 mov rdx, r15
0x7e1c0b  (f 0x3e1c0b)  e8e029f8ff             call 0x7645f0
0x7e1c10  (f 0x3e1c10)  488b05f102d508         mov rax, qword ptr [rip + 0x8d502f1]  ; rip->0x9531f08 (file 0x9131f08)
0x7e1c17  (f 0x3e1c17)  4c892b                 mov qword ptr [rbx], r13
0x7e1c1a  (f 0x3e1c1a)  48c7432c01000000       mov qword ptr [rbx + 0x2c], 1
0x7e1c22  (f 0x3e1c22)  48c7433800000000       mov qword ptr [rbx + 0x38], 0
0x7e1c2a  (f 0x3e1c2a)  66c743400000           mov word ptr [rbx + 0x40], 0
0x7e1c30  (f 0x3e1c30)  c6434200               mov byte ptr [rbx + 0x42], 0
0x7e1c34  (f 0x3e1c34)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x7e1c38  (f 0x3e1c38)  c5fc114344             vmovups ymmword ptr [rbx + 0x44], ymm0
0x7e1c3d  (f 0x3e1c3d)  c5fc114350             vmovups ymmword ptr [rbx + 0x50], ymm0
0x7e1c42  (f 0x3e1c42)  488b00                 mov rax, qword ptr [rax]
0x7e1c45  (f 0x3e1c45)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x7e1c49  (f 0x3e1c49)  757c                   jne 0x7e1cc7
0x7e1c4b  (f 0x3e1c4b)  4889d8                 mov rax, rbx
0x7e1c4e  (f 0x3e1c4e)  4883c418               add rsp, 0x18
0x7e1c52  (f 0x3e1c52)  5b                     pop rbx
0x7e1c53  (f 0x3e1c53)  415c                   pop r12
0x7e1c55  (f 0x3e1c55)  415d                   pop r13
0x7e1c57  (f 0x3e1c57)  415e                   pop r14
0x7e1c59  (f 0x3e1c59)  415f                   pop r15
0x7e1c5b  (f 0x3e1c5b)  5d                     pop rbp
0x7e1c5c  (f 0x3e1c5c)  c3                     ret 
0x7e1c5d  (f 0x3e1c5d)  488d3d0c59fc08         lea rdi, [rip + 0x8fc590c]  ; rip->0x97a7570 (file 0x93a7570)
0x7e1c64  (f 0x3e1c64)  e847020907             call 0x7871eb0
0x7e1c69  (f 0x3e1c69)  85c0                   test eax, eax
0x7e1c6b  (f 0x3e1c6b)  0f8425ffffff           je 0x7e1b96
0x7e1c71  (f 0x3e1c71)  e8ba28f8ff             call 0x764530
0x7e1c76  (f 0x3e1c76)  4c8d35b358fc08         lea r14, [rip + 0x8fc58b3]  ; rip->0x97a7530 (file 0x93a7530)
0x7e1c7d  (f 0x3e1c7d)  488d35fac41a07         lea rsi, [rip + 0x71ac4fa]  ; rip->0x798e17e (file 0x758e17e)
0x7e1c84  (f 0x3e1c84)  4c8d0dc5feffff         lea r9, [rip - 0x13b]  ; rip->0x7e1b50 (file 0x3e1b50)
0x7e1c8b  (f 0x3e1c8b)  ba00800000             mov edx, 0x8000
0x7e1c90  (f 0x3e1c90)  b901800000             mov ecx, 0x8001
0x7e1c95  (f 0x3e1c95)  4989c0                 mov r8, rax
0x7e1c98  (f 0x3e1c98)  4c89f7                 mov rdi, r14
0x7e1c9b  (f 0x3e1c9b)  e8f01cf8ff             call 0x763990
0x7e1ca0  (f 0x3e1ca0)  488d3d591ff8ff         lea rdi, [rip - 0x7e0a7]  ; rip->0x763c00 (file 0x363c00)
0x7e1ca7  (f 0x3e1ca7)  488d1552a3ef08         lea rdx, [rip + 0x8efa352]  ; rip->0x96dc000 (file 0x92dc000)
0x7e1cae  (f 0x3e1cae)  4c89f6                 mov rsi, r14
0x7e1cb1  (f 0x3e1cb1)  e81a020907             call 0x7871ed0
0x7e1cb6  (f 0x3e1cb6)  488d3db358fc08         lea rdi, [rip + 0x8fc58b3]  ; rip->0x97a7570 (file 0x93a7570)
0x7e1cbd  (f 0x3e1cbd)  e8fe010907             call 0x7871ec0
0x7e1cc2  (f 0x3e1cc2)  e9cffeffff             jmp 0x7e1b96
0x7e1cc7  (f 0x3e1cc7)  e894010907             call 0x7871e60
0x7e1ccc  (f 0x3e1ccc)  0f0b                   ud2 
--- calls: [('0xbe1b7f', '0x4b7a20'), ('0xbe1ba2', '0x7645c0'), ('0xbe1bdb', '0x7682c0'), ('0xbe1bf3', '0x7682c0'), ('0xbe1c0b', '0x7645f0'), ('0xbe1cc7', '0x7871e60'), ('0xbe1c64', '0x7871eb0'), ('0xbe1c71', '0x764530'), ('0xbe1c9b', '0x763990'), ('0xbe1cb1', '0x7871ed0'), ('0xbe1cbd', '0x7871ec0')]

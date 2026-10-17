0x647b00  (file 0x247b00)  55                       push rbp
0x647b01  (file 0x247b01)  4889e5                   mov rbp, rsp
0x647b04  (file 0x247b04)  4157                     push r15
0x647b06  (file 0x247b06)  4156                     push r14
0x647b08  (file 0x247b08)  4155                     push r13
0x647b0a  (file 0x247b0a)  4154                     push r12
0x647b0c  (file 0x247b0c)  53                       push rbx
0x647b0d  (file 0x247b0d)  4881ec98100000           sub rsp, 0x1098
0x647b14  (file 0x247b14)  488b05ede3ee08           mov rax, qword ptr [rip + 0x8eee3ed]  ; rip->0x9535f08 (file 0x9135f08)
0x647b1b  (file 0x247b1b)  4889bd68efffff           mov qword ptr [rbp - 0x1098], rdi
0x647b22  (file 0x247b22)  4989d6                   mov r14, rdx
0x647b25  (file 0x247b25)  488dbdd0efffff           lea rdi, [rbp - 0x1030]
0x647b2c  (file 0x247b2c)  ba00100000               mov edx, 0x1000
0x647b31  (file 0x247b31)  4989f7                   mov r15, rsi
0x647b34  (file 0x247b34)  31f6                     xor esi, esi
0x647b36  (file 0x247b36)  4c89c3                   mov rbx, r8
0x647b39  (file 0x247b39)  4189cc                   mov r12d, ecx
0x647b3c  (file 0x247b3c)  488b00                   mov rax, qword ptr [rax]
0x647b3f  (file 0x247b3f)  488945d0                 mov qword ptr [rbp - 0x30], rax
0x647b43  (file 0x247b43)  e8f8e22207               call 0x7875e40  ; -> 0x7875e40 (file 0x7475e40)
0x647b48  (file 0x247b48)  4585e4                   test r12d, r12d
0x647b4b  (file 0x247b4b)  7e56                     jle 0x647ba3  ; -> 0x647ba3 (file 0x247ba3)
0x647b4d  (file 0x247b4d)  4489e0                   mov eax, r12d
0x647b50  (file 0x247b50)  31c9                     xor ecx, ecx
0x647b52  (file 0x247b52)  eb24                     jmp 0x647b78  ; -> 0x647b78 (file 0x247b78)
0x647b54  (file 0x247b54)  6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x647b60  (file 0x247b60)  c1e608                   shl esi, 8
0x647b63  (file 0x247b63)  09fe                     or esi, edi
0x647b65  (file 0x247b65)  488d9415d0efffff         lea rdx, [rbp + rdx - 0x1030]
0x647b6d  (file 0x247b6d)  48ffc1                   inc rcx
0x647b70  (file 0x247b70)  017204                   add dword ptr [rdx + 4], esi
0x647b73  (file 0x247b73)  4839c8                   cmp rax, rcx
0x647b76  (file 0x247b76)  742b                     je 0x647ba3  ; -> 0x647ba3 (file 0x247ba3)
0x647b78  (file 0x247b78)  410fb654cf04             movzx edx, byte ptr [r15 + rcx*8 + 4]
0x647b7e  (file 0x247b7e)  c1e204                   shl edx, 4
0x647b81  (file 0x247b81)  ff8415d0efffff           inc dword ptr [rbp + rdx - 0x1030]
0x647b88  (file 0x247b88)  410fb6344e               movzx esi, byte ptr [r14 + rcx*2]
0x647b8d  (file 0x247b8d)  410fb67c4e01             movzx edi, byte ptr [r14 + rcx*2 + 1]
0x647b93  (file 0x247b93)  4084f6                   test sil, sil
0x647b96  (file 0x247b96)  79c8                     jns 0x647b60  ; -> 0x647b60 (file 0x247b60)
0x647b98  (file 0x247b98)  83e67f                   and esi, 0x7f
0x647b9b  (file 0x247b9b)  c1e609                   shl esi, 9
0x647b9e  (file 0x247b9e)  8d347e                   lea esi, [rsi + rdi*2]
0x647ba1  (file 0x247ba1)  ebc2                     jmp 0x647b65  ; -> 0x647b65 (file 0x247b65)
0x647ba3  (file 0x247ba3)  b808000000               mov eax, 8
0x647ba8  (file 0x247ba8)  c5f1efc9                 vpxor xmm1, xmm1, xmm1
0x647bac  (file 0x247bac)  0f1f4000                 nop dword ptr [rax]
0x647bb0  (file 0x247bb0)  c5f9d68c05d0efffff       vmovq qword ptr [rbp + rax - 0x1030], xmm1
0x647bb9  (file 0x247bb9)  c5fa7e8405c8efffff       vmovq xmm0, qword ptr [rbp + rax - 0x1038]
0x647bc2  (file 0x247bc2)  4883c010                 add rax, 0x10
0x647bc6  (file 0x247bc6)  c5f9fec9                 vpaddd xmm1, xmm0, xmm1
0x647bca  (file 0x247bca)  483d08100000             cmp rax, 0x1008
0x647bd0  (file 0x247bd0)  75de                     jne 0x647bb0  ; -> 0x647bb0 (file 0x247bb0)
0x647bd2  (file 0x247bd2)  c4c1797ecc               vmovd r12d, xmm1
0x647bd7  (file 0x247bd7)  c5f9efc0                 vpxor xmm0, xmm0, xmm0
0x647bdb  (file 0x247bdb)  c5fa7f85c0efffff         vmovdqu xmmword ptr [rbp - 0x1040], xmm0
0x647be3  (file 0x247be3)  c5fa7f85b0efffff         vmovdqu xmmword ptr [rbp - 0x1050], xmm0
0x647beb  (file 0x247beb)  4585e4                   test r12d, r12d
0x647bee  (file 0x247bee)  7e39                     jle 0x647c29  ; -> 0x647c29 (file 0x247c29)
0x647bf0  (file 0x247bf0)  4c8d8dccefffff           lea r9, [rbp - 0x1034]
0x647bf7  (file 0x247bf7)  4c8d85c8efffff           lea r8, [rbp - 0x1038]
0x647bfe  (file 0x247bfe)  488d8dc0efffff           lea rcx, [rbp - 0x1040]
0x647c05  (file 0x247c05)  bf30000000               mov edi, 0x30
0x647c0a  (file 0x247c0a)  be08000000               mov esi, 8
0x647c0f  (file 0x247c0f)  4489e2                   mov edx, r12d
0x647c12  (file 0x247c12)  c5f97f8d80efffff         vmovdqa xmmword ptr [rbp - 0x1080], xmm1
0x647c1a  (file 0x247c1a)  e84128deff               call 0x42a460  ; -> 0x42a460 (file 0x2a460)
0x647c1f  (file 0x247c1f)  c5f96f8d80efffff         vmovdqa xmm1, xmmword ptr [rbp - 0x1080]
0x647c27  (file 0x247c27)  eb06                     jmp 0x647c2f  ; -> 0x647c2f (file 0x247c2f)
0x647c29  (file 0x247c29)  0f88eb020000             js 0x647f1a  ; -> 0x647f1a (file 0x247f1a)
0x647c2f  (file 0x247c2f)  c4e37916ca01             vpextrd edx, xmm1, 1
0x647c35  (file 0x247c35)  85d2                     test edx, edx
0x647c37  (file 0x247c37)  7e29                     jle 0x647c62  ; -> 0x647c62 (file 0x247c62)
0x647c39  (file 0x247c39)  4c8d85b8efffff           lea r8, [rbp - 0x1048]
0x647c40  (file 0x247c40)  4c8d8dbcefffff           lea r9, [rbp - 0x1044]
0x647c47  (file 0x247c47)  488d8db0efffff           lea rcx, [rbp - 0x1050]
0x647c4e  (file 0x247c4e)  bf01000000               mov edi, 1
0x647c53  (file 0x247c53)  be01000000               mov esi, 1
0x647c58  (file 0x247c58)  e80328deff               call 0x42a460  ; -> 0x42a460 (file 0x2a460)
0x647c5d  (file 0x247c5d)  4889de                   mov rsi, rbx
0x647c60  (file 0x247c60)  eb09                     jmp 0x647c6b  ; -> 0x647c6b (file 0x247c6b)
0x647c62  (file 0x247c62)  4889de                   mov rsi, rbx
0x647c65  (file 0x247c65)  0f88b9020000             js 0x647f24  ; -> 0x647f24 (file 0x247f24)
0x647c6b  (file 0x247c6b)  4585e4                   test r12d, r12d
0x647c6e  (file 0x247c6e)  0f8410020000             je 0x647e84  ; -> 0x647e84 (file 0x247e84)
0x647c74  (file 0x247c74)  4489e0                   mov eax, r12d
0x647c77  (file 0x247c77)  31d2                     xor edx, edx
0x647c79  (file 0x247c79)  4c89b5a0efffff           mov qword ptr [rbp - 0x1060], r14
0x647c80  (file 0x247c80)  4c89bd70efffff           mov qword ptr [rbp - 0x1090], r15
0x647c87  (file 0x247c87)  48898578efffff           mov qword ptr [rbp - 0x1088], rax
0x647c8e  (file 0x247c8e)  e9f2000000               jmp 0x647d85  ; -> 0x647d85 (file 0x247d85)
0x647c93  (file 0x247c93)  666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x647ca0  (file 0x247ca0)  81e3000000e0             and ebx, 0xe0000000
0x647ca6  (file 0x247ca6)  4c8bb5a0efffff           mov r14, qword ptr [rbp - 0x1060]
0x647cad  (file 0x247cad)  4489e9                   mov ecx, r13d
0x647cb0  (file 0x247cb0)  09c3                     or ebx, eax
0x647cb2  (file 0x247cb2)  4c89d0                   mov rax, r10
0x647cb5  (file 0x247cb5)  c1e106                   shl ecx, 6
0x647cb8  (file 0x247cb8)  48c1e827                 shr rax, 0x27
0x647cbc  (file 0x247cbc)  83e03e                   and eax, 0x3e
0x647cbf  (file 0x247cbf)  09c8                     or eax, ecx
0x647cc1  (file 0x247cc1)  488b8d60efffff           mov rcx, qword ptr [rbp - 0x10a0]
0x647cc8  (file 0x247cc8)  4c8b8d48efffff           mov r9, qword ptr [rbp - 0x10b8]
0x647ccf  (file 0x247ccf)  488bb568efffff           mov rsi, qword ptr [rbp - 0x1098]
0x647cd6  (file 0x247cd6)  488bbdc0efffff           mov rdi, qword ptr [rbp - 0x1040]
0x647cdd  (file 0x247cdd)  488d940dd0efffff         lea rdx, [rbp + rcx - 0x1030]
0x647ce5  (file 0x247ce5)  49c1e102                 shl r9, 2
0x647ce9  (file 0x247ce9)  4c030e                   add r9, qword ptr [rsi]
0x647cec  (file 0x247cec)  48b90000000000ffffff     movabs rcx, 0xffffff0000000000
0x647cf6  (file 0x247cf6)  4921cc                   and r12, rcx
0x647cf9  (file 0x247cf9)  410fb6cf                 movzx ecx, r15b
0x647cfd  (file 0x247cfd)  448bbdacefffff           mov r15d, dword ptr [rbp - 0x1054]
0x647d04  (file 0x247d04)  48637208                 movsxd rsi, dword ptr [rdx + 8]
0x647d08  (file 0x247d08)  48c1e120                 shl rcx, 0x20
0x647d0c  (file 0x247d0c)  4c09e1                   or rcx, r12
0x647d0f  (file 0x247d0f)  4c09e9                   or rcx, r13
0x647d12  (file 0x247d12)  4989cc                   mov r12, rcx
0x647d15  (file 0x247d15)  4c8d0476                 lea r8, [rsi + rsi*2]
0x647d19  (file 0x247d19)  49c1e004                 shl r8, 4
0x647d1d  (file 0x247d1d)  4e891c07                 mov qword ptr [rdi + r8], r11
0x647d21  (file 0x247d21)  4a894c0708               mov qword ptr [rdi + r8 + 8], rcx
0x647d26  (file 0x247d26)  46897c0710               mov dword ptr [rdi + r8 + 0x10], r15d
0x647d2b  (file 0x247d2b)  4689540714               mov dword ptr [rdi + r8 + 0x14], r10d
0x647d30  (file 0x247d30)  42895c0718               mov dword ptr [rdi + r8 + 0x18], ebx
0x647d35  (file 0x247d35)  66428944071c             mov word ptr [rdi + r8 + 0x1c], ax
0x647d3b  (file 0x247d3b)  8d4601                   lea eax, [rsi + 1]
0x647d3e  (file 0x247d3e)  4e894c0720               mov qword ptr [rdi + r8 + 0x20], r9
0x647d43  (file 0x247d43)  6642c74407280000         mov word ptr [rdi + r8 + 0x28], 0
0x647d4b  (file 0x247d4b)  488bb580efffff           mov rsi, qword ptr [rbp - 0x1080]
0x647d52  (file 0x247d52)  4c8bbd70efffff           mov r15, qword ptr [rbp - 0x1090]
0x647d59  (file 0x247d59)  894208                   mov dword ptr [rdx + 8], eax
0x647d5c  (file 0x247d5c)  488b8558efffff           mov rax, qword ptr [rbp - 0x10a8]
0x647d63  (file 0x247d63)  4801c6                   add rsi, rax
0x647d66  (file 0x247d66)  038550efffff             add eax, dword ptr [rbp - 0x10b0]
0x647d6c  (file 0x247d6c)  89420c                   mov dword ptr [rdx + 0xc], eax
0x647d6f  (file 0x247d6f)  488b9540efffff           mov rdx, qword ptr [rbp - 0x10c0]
0x647d76  (file 0x247d76)  48ffc2                   inc rdx
0x647d79  (file 0x247d79)  399578efffff             cmp dword ptr [rbp - 0x1088], edx
0x647d7f  (file 0x247d7f)  0f84ff000000             je 0x647e84  ; -> 0x647e84 (file 0x247e84)
0x647d85  (file 0x247d85)  498b1cd7                 mov rbx, qword ptr [r15 + rdx*8]
0x647d89  (file 0x247d89)  4863c2                   movsxd rax, edx
0x647d8c  (file 0x247d8c)  48899540efffff           mov qword ptr [rbp - 0x10c0], rdx
0x647d93  (file 0x247d93)  488bbdb0efffff           mov rdi, qword ptr [rbp - 0x1050]
0x647d9a  (file 0x247d9a)  4889b580efffff           mov qword ptr [rbp - 0x1080], rsi
0x647da1  (file 0x247da1)  450fb73446               movzx r14d, word ptr [r14 + rax*2]
0x647da6  (file 0x247da6)  48898548efffff           mov qword ptr [rbp - 0x10b8], rax
0x647dad  (file 0x247dad)  48899d98efffff           mov qword ptr [rbp - 0x1068], rbx
0x647db4  (file 0x247db4)  48c1eb20                 shr rbx, 0x20
0x647db8  (file 0x247db8)  0fb6cb                   movzx ecx, bl
0x647dbb  (file 0x247dbb)  4589f5                   mov r13d, r14d
0x647dbe  (file 0x247dbe)  4489f0                   mov eax, r14d
0x647dc1  (file 0x247dc1)  4589f7                   mov r15d, r14d
0x647dc4  (file 0x247dc4)  898dacefffff             mov dword ptr [rbp - 0x1054], ecx
0x647dca  (file 0x247dca)  c1e104                   shl ecx, 4
0x647dcd  (file 0x247dcd)  4183e57f                 and r13d, 0x7f
0x647dd1  (file 0x247dd1)  c1e808                   shr eax, 8
0x647dd4  (file 0x247dd4)  41c0ef07                 shr r15b, 7
0x647dd8  (file 0x247dd8)  4863940ddcefffff         movsxd rdx, dword ptr [rbp + rcx - 0x1024]
0x647de0  (file 0x247de0)  41c1e508                 shl r13d, 8
0x647de4  (file 0x247de4)  48898d60efffff           mov qword ptr [rbp - 0x10a0], rcx
0x647deb  (file 0x247deb)  4109c5                   or r13d, eax
0x647dee  (file 0x247dee)  4801d7                   add rdi, rdx
0x647df1  (file 0x247df1)  48899550efffff           mov qword ptr [rbp - 0x10b0], rdx
0x647df8  (file 0x247df8)  c4c201f7d5               shlx edx, r13d, r15d
0x647dfd  (file 0x247dfd)  4889bd90efffff           mov qword ptr [rbp - 0x1070], rdi
0x647e04  (file 0x247e04)  48899558efffff           mov qword ptr [rbp - 0x10a8], rdx
0x647e0b  (file 0x247e0b)  e840e02207               call 0x7875e50  ; -> 0x7875e50 (file 0x7475e50)
0x647e10  (file 0x247e10)  4584f6                   test r14b, r14b
0x647e13  (file 0x247e13)  783b                     js 0x647e50  ; -> 0x647e50 (file 0x247e50)
0x647e15  (file 0x247e15)  4c8b9598efffff           mov r10, qword ptr [rbp - 0x1068]
0x647e1c  (file 0x247e1c)  4c8b9d90efffff           mov r11, qword ptr [rbp - 0x1070]
0x647e23  (file 0x247e23)  31c0                     xor eax, eax
0x647e25  (file 0x247e25)  4183fd04                 cmp r13d, 4
0x647e29  (file 0x247e29)  0f8571feffff             jne 0x647ca0  ; -> 0x647ca0 (file 0x247ca0)
0x647e2f  (file 0x247e2f)  418b0b                   mov ecx, dword ptr [r11]
0x647e32  (file 0x247e32)  31c0                     xor eax, eax
0x647e34  (file 0x247e34)  81e1dfdfdfdf             and ecx, 0xdfdfdfdf
0x647e3a  (file 0x247e3a)  81f94e4f4e45             cmp ecx, 0x454e4f4e
0x647e40  (file 0x247e40)  0f94c0                   sete al
0x647e43  (file 0x247e43)  c1e01d                   shl eax, 0x1d
0x647e46  (file 0x247e46)  e955feffff               jmp 0x647ca0  ; -> 0x647ca0 (file 0x247ca0)
0x647e4b  (file 0x247e4b)  0f1f440000               nop dword ptr [rax + rax]
0x647e50  (file 0x247e50)  4c8b9598efffff           mov r10, qword ptr [rbp - 0x1068]
0x647e57  (file 0x247e57)  4c8bb5a0efffff           mov r14, qword ptr [rbp - 0x1060]
0x647e5e  (file 0x247e5e)  4c8b9d90efffff           mov r11, qword ptr [rbp - 0x1070]
0x647e65  (file 0x247e65)  4489e8                   mov eax, r13d
0x647e68  (file 0x247e68)  81e3000000e0             and ebx, 0xe0000000
0x647e6e  (file 0x247e6e)  c1e006                   shl eax, 6
0x647e71  (file 0x247e71)  4c89d1                   mov rcx, r10
0x647e74  (file 0x247e74)  48c1e927                 shr rcx, 0x27
0x647e78  (file 0x247e78)  83e13e                   and ecx, 0x3e
0x647e7b  (file 0x247e7b)  8d440801                 lea eax, [rax + rcx + 1]
0x647e7f  (file 0x247e7f)  e93dfeffff               jmp 0x647cc1  ; -> 0x647cc1 (file 0x247cc1)
0x647e84  (file 0x247e84)  488bb5c0efffff           mov rsi, qword ptr [rbp - 0x1040]
0x647e8b  (file 0x247e8b)  488d1d6e201309           lea rbx, [rip + 0x913206e]  ; rip->0x9779f00 (file 0x9379f00)
0x647e92  (file 0x247e92)  4531f6                   xor r14d, r14d
0x647e95  (file 0x247e95)  66662e0f1f840000000000   nop word ptr cs:[rax + rax]
0x647ea0  (file 0x247ea0)  428b9435d0efffff         mov edx, dword ptr [rbp + r14 - 0x1030]
0x647ea8  (file 0x247ea8)  4889df                   mov rdi, rbx
0x647eab  (file 0x247eab)  4c8d3c52                 lea r15, [rdx + rdx*2]
0x647eaf  (file 0x247eaf)  49c1e704                 shl r15, 4
0x647eb3  (file 0x247eb3)  4901f7                   add r15, rsi
0x647eb6  (file 0x247eb6)  e855f4ffff               call 0x647310  ; -> 0x647310 (file 0x247310)
0x647ebb  (file 0x247ebb)  4983c610                 add r14, 0x10
0x647ebf  (file 0x247ebf)  4883c340                 add rbx, 0x40
0x647ec3  (file 0x247ec3)  4c89fe                   mov rsi, r15
0x647ec6  (file 0x247ec6)  4981fe00100000           cmp r14, 0x1000
0x647ecd  (file 0x247ecd)  75d1                     jne 0x647ea0  ; -> 0x647ea0 (file 0x247ea0)
0x647ecf  (file 0x247ecf)  488bbdb0efffff           mov rdi, qword ptr [rbp - 0x1050]
0x647ed6  (file 0x247ed6)  4885ff                   test rdi, rdi
0x647ed9  (file 0x247ed9)  7405                     je 0x647ee0  ; -> 0x647ee0 (file 0x247ee0)
0x647edb  (file 0x247edb)  e8b03be7ff               call 0x4bba90  ; -> 0x4bba90 (file 0xbba90)
0x647ee0  (file 0x247ee0)  488bbdc0efffff           mov rdi, qword ptr [rbp - 0x1040]
0x647ee7  (file 0x247ee7)  488b1d1ae0ee08           mov rbx, qword ptr [rip + 0x8eee01a]  ; rip->0x9535f08 (file 0x9135f08)
0x647eee  (file 0x247eee)  4885ff                   test rdi, rdi
0x647ef1  (file 0x247ef1)  7405                     je 0x647ef8  ; -> 0x647ef8 (file 0x247ef8)
0x647ef3  (file 0x247ef3)  e8983be7ff               call 0x4bba90  ; -> 0x4bba90 (file 0xbba90)
0x647ef8  (file 0x247ef8)  488b03                   mov rax, qword ptr [rbx]
0x647efb  (file 0x247efb)  483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x647eff  (file 0x247eff)  7512                     jne 0x647f13  ; -> 0x647f13 (file 0x247f13)
0x647f01  (file 0x247f01)  4881c498100000           add rsp, 0x1098
0x647f08  (file 0x247f08)  5b                       pop rbx
0x647f09  (file 0x247f09)  415c                     pop r12
0x647f0b  (file 0x247f0b)  415d                     pop r13
0x647f0d  (file 0x247f0d)  415e                     pop r14
0x647f0f  (file 0x247f0f)  415f                     pop r15
0x647f11  (file 0x247f11)  5d                       pop rbp
0x647f12  (file 0x247f12)  c3                       ret 
0x647f13  (file 0x247f13)  e848df2207               call 0x7875e60  ; -> 0x7875e60 (file 0x7475e60)
0x647f18  (file 0x247f18)  0f0b                     ud2 
0x647f1a  (file 0x247f1a)  4489e7                   mov edi, r12d
0x647f1d  (file 0x247f1d)  e8def1e1ff               call 0x467100  ; -> 0x467100 (file 0x67100)
0x647f22  (file 0x247f22)  0f0b                     ud2 
0x647f24  (file 0x247f24)  89d7                     mov edi, edx
0x647f26  (file 0x247f26)  e8d5f1e1ff               call 0x467100  ; -> 0x467100 (file 0x67100)
0x647f2b  (file 0x247f2b)  0f0b                     ud2 
0x647f2d  (file 0x247f2d)  cc                       int3 
0x647f2e  (file 0x247f2e)  cc                       int3 
0x647f2f  (file 0x247f2f)  cc                       int3 

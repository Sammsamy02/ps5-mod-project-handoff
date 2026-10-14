0x883b40  (f 0x483b40)  55                     push rbp
0x883b41  (f 0x483b41)  4889e5                 mov rbp, rsp
0x883b44  (f 0x483b44)  4157                   push r15
0x883b46  (f 0x483b46)  4156                   push r14
0x883b48  (f 0x483b48)  4155                   push r13
0x883b4a  (f 0x483b4a)  4154                   push r12
0x883b4c  (f 0x483b4c)  53                     push rbx
0x883b4d  (f 0x483b4d)  4883ec48               sub rsp, 0x48
0x883b51  (f 0x483b51)  4c8b2db0e3ca08         mov r13, qword ptr [rip + 0x8cae3b0]  ; rip->0x9531f08 (file 0x9131f08)
0x883b58  (f 0x483b58)  448945a4               mov dword ptr [rbp - 0x5c], r8d
0x883b5c  (f 0x483b5c)  8955a0                 mov dword ptr [rbp - 0x60], edx
0x883b5f  (f 0x483b5f)  4989fe                 mov r14, rdi
0x883b62  (f 0x483b62)  4883c730               add rdi, 0x30
0x883b66  (f 0x483b66)  4189cf                 mov r15d, ecx
0x883b69  (f 0x483b69)  4889f3                 mov rbx, rsi
0x883b6c  (f 0x483b6c)  498b4500               mov rax, qword ptr [r13]
0x883b70  (f 0x483b70)  488945d0               mov qword ptr [rbp - 0x30], rax
0x883b74  (f 0x483b74)  48897d98               mov qword ptr [rbp - 0x68], rdi
0x883b78  (f 0x483b78)  e863e3fe06             call 0x7871ee0
0x883b7d  (f 0x483b7d)  85c0                   test eax, eax
0x883b7f  (f 0x483b7f)  743d                   je 0x883bbe
0x883b81  (f 0x483b81)  49ff4640               inc qword ptr [r14 + 0x40]
0x883b85  (f 0x483b85)  4585ff                 test r15d, r15d
0x883b88  (f 0x483b88)  7846                   js 0x883bd0
0x883b8a  (f 0x483b8a)  45397e24               cmp dword ptr [r14 + 0x24], r15d
0x883b8e  (f 0x483b8e)  0f8e6b010000           jle 0x883cff
0x883b94  (f 0x483b94)  498b5610               mov rdx, qword ptr [r14 + 0x10]
0x883b98  (f 0x483b98)  410fb7cf               movzx ecx, r15w
0x883b9c  (f 0x483b9c)  4489f8                 mov eax, r15d
0x883b9f  (f 0x483b9f)  4c8d2449               lea r12, [rcx + rcx*2]
0x883ba3  (f 0x483ba3)  c1e810                 shr eax, 0x10
0x883ba6  (f 0x483ba6)  41c1e403               shl r12d, 3
0x883baa  (f 0x483baa)  4c0324c2               add r12, qword ptr [rdx + rax*8]
0x883bae  (f 0x483bae)  49833c2400             cmp qword ptr [r12], 0
0x883bb3  (f 0x483bb3)  0f8454010000           je 0x883d0d
0x883bb9  (f 0x483bb9)  e9dc020000             jmp 0x883e9a
0x883bbe  (f 0x483bbe)  e82de3fe06             call 0x7871ef0
0x883bc3  (f 0x483bc3)  49894638               mov qword ptr [r14 + 0x38], rax
0x883bc7  (f 0x483bc7)  49ff4640               inc qword ptr [r14 + 0x40]
0x883bcb  (f 0x483bcb)  4585ff                 test r15d, r15d
0x883bce  (f 0x483bce)  79ba                   jns 0x883b8a
0x883bd0  (f 0x483bd0)  41807e0c01             cmp byte ptr [r14 + 0xc], 1
0x883bd5  (f 0x483bd5)  0f85e6000000           jne 0x883cc1
0x883bdb  (f 0x483bdb)  418b4e08               mov ecx, dword ptr [r14 + 8]
0x883bdf  (f 0x483bdf)  85c9                   test ecx, ecx
0x883be1  (f 0x483be1)  0f8eda000000           jle 0x883cc1
0x883be7  (f 0x483be7)  458b7e04               mov r15d, dword ptr [r14 + 4]
0x883beb  (f 0x483beb)  41ffc7                 inc r15d
0x883bee  (f 0x483bee)  45897e04               mov dword ptr [r14 + 4], r15d
0x883bf2  (f 0x483bf2)  4139cf                 cmp r15d, ecx
0x883bf5  (f 0x483bf5)  7c0a                   jl 0x883c01
0x883bf7  (f 0x483bf7)  41833e00               cmp dword ptr [r14], 0
0x883bfb  (f 0x483bfb)  0f8936030000           jns 0x883f37
0x883c01  (f 0x483c01)  4489f8                 mov eax, r15d
0x883c04  (f 0x483c04)  39c8                   cmp eax, ecx
0x883c06  (f 0x483c06)  7c40                   jl 0x883c48
0x883c08  (f 0x483c08)  458b6620               mov r12d, dword ptr [r14 + 0x20]
0x883c0c  (f 0x483c0c)  458b7e24               mov r15d, dword ptr [r14 + 0x24]
0x883c10  (f 0x483c10)  4539e7                 cmp r15d, r12d
0x883c13  (f 0x483c13)  0f8d4d030000           jge 0x883f66
0x883c19  (f 0x483c19)  418b462c               mov eax, dword ptr [r14 + 0x2c]
0x883c1d  (f 0x483c1d)  418d8fffff0000         lea ecx, [r15 + 0xffff]
0x883c24  (f 0x483c24)  4585ff                 test r15d, r15d
0x883c27  (f 0x483c27)  410f49cf               cmovns ecx, r15d
0x883c2b  (f 0x483c2b)  c1f910                 sar ecx, 0x10
0x883c2e  (f 0x483c2e)  894db0                 mov dword ptr [rbp - 0x50], ecx
0x883c31  (f 0x483c31)  39c1                   cmp ecx, eax
0x883c33  (f 0x483c33)  7d40                   jge 0x883c75
0x883c35  (f 0x483c35)  41ff4624               inc dword ptr [r14 + 0x24]
0x883c39  (f 0x483c39)  4c8b2dc8e2ca08         mov r13, qword ptr [rip + 0x8cae2c8]  ; rip->0x9531f08 (file 0x9131f08)
0x883c40  (f 0x483c40)  418b4604               mov eax, dword ptr [r14 + 4]
0x883c44  (f 0x483c44)  418b4e08               mov ecx, dword ptr [r14 + 8]
0x883c48  (f 0x483c48)  ffc0                   inc eax
0x883c4a  (f 0x483c4a)  39c8                   cmp eax, ecx
0x883c4c  (f 0x483c4c)  0f4ec1                 cmovle eax, ecx
0x883c4f  (f 0x483c4f)  41894608               mov dword ptr [r14 + 8], eax
0x883c53  (f 0x483c53)  45397e24               cmp dword ptr [r14 + 0x24], r15d
0x883c57  (f 0x483c57)  0f8f37ffffff           jg 0x883b94
0x883c5d  (f 0x483c5d)  e99d000000             jmp 0x883cff
0x883c70  (f 0x483c70)  3945b0                 cmp dword ptr [rbp - 0x50], eax
0x883c73  (f 0x483c73)  7cc0                   jl 0x883c35
0x883c75  (f 0x483c75)  498b4e10               mov rcx, qword ptr [r14 + 0x10]
0x883c79  (f 0x483c79)  bf08001800             mov edi, 0x180008
0x883c7e  (f 0x483c7e)  4c63e8                 movsxd r13, eax
0x883c81  (f 0x483c81)  48894da8               mov qword ptr [rbp - 0x58], rcx
0x883c85  (f 0x483c85)  e8f62f4104             call 0x4c96c80
0x883c8a  (f 0x483c8a)  4989c4                 mov r12, rax
0x883c8d  (f 0x483c8d)  ba00001800             mov edx, 0x180000
0x883c92  (f 0x483c92)  48c70000000100         mov qword ptr [rax], 0x10000
0x883c99  (f 0x483c99)  31f6                   xor esi, esi
0x883c9b  (f 0x483c9b)  4983c408               add r12, 8
0x883c9f  (f 0x483c9f)  4c89e7                 mov rdi, r12
0x883ca2  (f 0x483ca2)  e899e1fe06             call 0x7871e40
0x883ca7  (f 0x483ca7)  488b4da8               mov rcx, qword ptr [rbp - 0x58]
0x883cab  (f 0x483cab)  31c0                   xor eax, eax
0x883cad  (f 0x483cad)  f04e0fb124e9           lock cmpxchg qword ptr [rcx + r13*8], r12
0x883cb3  (f 0x483cb3)  418b462c               mov eax, dword ptr [r14 + 0x2c]
0x883cb7  (f 0x483cb7)  75b7                   jne 0x883c70
0x883cb9  (f 0x483cb9)  ffc0                   inc eax
0x883cbb  (f 0x483cbb)  4189462c               mov dword ptr [r14 + 0x2c], eax
0x883cbf  (f 0x483cbf)  ebaf                   jmp 0x883c70
0x883cc1  (f 0x483cc1)  49634650               movsxd rax, dword ptr [r14 + 0x50]
0x883cc5  (f 0x483cc5)  4885c0                 test rax, rax
0x883cc8  (f 0x483cc8)  0f8e2f010000           jle 0x883dfd
0x883cce  (f 0x483cce)  498b4e48               mov rcx, qword ptr [r14 + 0x48]
0x883cd2  (f 0x483cd2)  498d5648               lea rdx, [r14 + 0x48]
0x883cd6  (f 0x483cd6)  4d8d4654               lea r8, [r14 + 0x54]
0x883cda  (f 0x483cda)  bf04000000             mov edi, 4
0x883cdf  (f 0x483cdf)  be04000000             mov esi, 4
0x883ce4  (f 0x483ce4)  448b7c81fc             mov r15d, dword ptr [rcx + rax*4 - 4]
0x883ce9  (f 0x483ce9)  8d48ff                 lea ecx, [rax - 1]
0x883cec  (f 0x483cec)  41894e50               mov dword ptr [r14 + 0x50], ecx
0x883cf0  (f 0x483cf0)  e84b34baff             call 0x427140
0x883cf5  (f 0x483cf5)  45397e24               cmp dword ptr [r14 + 0x24], r15d
0x883cf9  (f 0x483cf9)  0f8f95feffff           jg 0x883b94
0x883cff  (f 0x483cff)  4531e4                 xor r12d, r12d
0x883d02  (f 0x483d02)  49833c2400             cmp qword ptr [r12], 0
0x883d07  (f 0x483d07)  0f858d010000           jne 0x883e9a
0x883d0d  (f 0x483d0d)  488d05fc63f308         lea rax, [rip + 0x8f363fc]  ; rip->0x97ba110 (file 0x93ba110)
0x883d14  (f 0x483d14)  41c744240800000080     mov dword ptr [r12 + 8], 0x80000000
0x883d1d  (f 0x483d1d)  83780800               cmp dword ptr [rax + 8], 0
0x883d21  (f 0x483d21)  7e07                   jle 0x883d2a
0x883d23  (f 0x483d23)  41807e0c00             cmp byte ptr [r14 + 0xc], 0
0x883d28  (f 0x483d28)  7513                   jne 0x883d3d
0x883d2a  (f 0x483d2a)  488d0547dbe508         lea rax, [rip + 0x8e5db47]  ; rip->0x96e1878 (file 0x92e1878)
0x883d31  (f 0x483d31)  b900000080             mov ecx, 0x80000000
0x883d36  (f 0x483d36)  0b08                   or ecx, dword ptr [rax]
0x883d38  (f 0x483d38)  41894c2408             mov dword ptr [r12 + 8], ecx
0x883d3d  (f 0x483d3d)  8b45a4                 mov eax, dword ptr [rbp - 0x5c]
0x883d40  (f 0x483d40)  49891c24               mov qword ptr [r12], rbx
0x883d44  (f 0x483d44)  41c744241400000000     mov dword ptr [r12 + 0x14], 0
0x883d4d  (f 0x483d4d)  41c744240c00000000     mov dword ptr [r12 + 0xc], 0
0x883d56  (f 0x483d56)  4189442410             mov dword ptr [r12 + 0x10], eax
0x883d5b  (f 0x483d5b)  8b45a0                 mov eax, dword ptr [rbp - 0x60]
0x883d5e  (f 0x483d5e)  44897b0c               mov dword ptr [rbx + 0xc], r15d
0x883d62  (f 0x483d62)  85c0                   test eax, eax
0x883d64  (f 0x483d64)  7433                   je 0x883d99
0x883d66  (f 0x483d66)  89c6                   mov esi, eax
0x883d68  (f 0x483d68)  81e6ff3ffecf           and esi, 0xcffe3fff
0x883d6e  (f 0x483d6e)  a90000504e             test eax, 0x4e500000
0x883d73  (f 0x483d73)  7418                   je 0x883d8d
0x883d75  (f 0x483d75)  4c89e7                 mov rdi, r12
0x883d78  (f 0x483d78)  e8f346eeff             call 0x768470
0x883d7d  (f 0x483d7d)  eb1a                   jmp 0x883d99
0x883d80  (f 0x483d80)  89c1                   mov ecx, eax
0x883d82  (f 0x483d82)  09f1                   or ecx, esi
0x883d84  (f 0x483d84)  f0410fb14c2408         lock cmpxchg dword ptr [r12 + 8], ecx
0x883d8b  (f 0x483d8b)  740c                   je 0x883d99
0x883d8d  (f 0x483d8d)  418b442408             mov eax, dword ptr [r12 + 8]
0x883d92  (f 0x483d92)  c4e278f2ce             andn ecx, eax, esi
0x883d97  (f 0x483d97)  75e7                   jne 0x883d80
0x883d99  (f 0x483d99)  49ff4e40               dec qword ptr [r14 + 0x40]
0x883d9d  (f 0x483d9d)  7511                   jne 0x883db0
0x883d9f  (f 0x483d9f)  488b7d98               mov rdi, qword ptr [rbp - 0x68]
0x883da3  (f 0x483da3)  49c7463800000000       mov qword ptr [r14 + 0x38], 0
0x883dab  (f 0x483dab)  e870e1fe06             call 0x7871f20
0x883db0  (f 0x483db0)  41837e6000             cmp dword ptr [r14 + 0x60], 0
0x883db5  (f 0x483db5)  7e29                   jle 0x883de0
0x883db7  (f 0x483db7)  4531e4                 xor r12d, r12d
0x883dba  (f 0x483dba)  660f1f440000           nop word ptr [rax + rax]
0x883dc0  (f 0x483dc0)  498b4658               mov rax, qword ptr [r14 + 0x58]
0x883dc4  (f 0x483dc4)  4889de                 mov rsi, rbx
0x883dc7  (f 0x483dc7)  4489fa                 mov edx, r15d
0x883dca  (f 0x483dca)  4a8b3ce0               mov rdi, qword ptr [rax + r12*8]
0x883dce  (f 0x483dce)  488b07                 mov rax, qword ptr [rdi]
0x883dd1  (f 0x483dd1)  ff5010                 call qword ptr [rax + 0x10]
0x883dd4  (f 0x483dd4)  49634660               movsxd rax, dword ptr [r14 + 0x60]
0x883dd8  (f 0x483dd8)  49ffc4                 inc r12
0x883ddb  (f 0x483ddb)  4939c4                 cmp r12, rax
0x883dde  (f 0x483dde)  7ce0                   jl 0x883dc0
0x883de0  (f 0x483de0)  498b4500               mov rax, qword ptr [r13]
0x883de4  (f 0x483de4)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x883de8  (f 0x483de8)  0f85b5010000           jne 0x883fa3
0x883dee  (f 0x483dee)  4883c448               add rsp, 0x48
0x883df2  (f 0x483df2)  5b                     pop rbx
0x883df3  (f 0x483df3)  415c                   pop r12
0x883df5  (f 0x483df5)  415d                   pop r13
0x883df7  (f 0x483df7)  415e                   pop r14
0x883df9  (f 0x483df9)  415f                   pop r15
0x883dfb  (f 0x483dfb)  5d                     pop rbp
0x883dfc  (f 0x483dfc)  c3                     ret 
0x883dfd  (f 0x483dfd)  458b6620               mov r12d, dword ptr [r14 + 0x20]
0x883e01  (f 0x483e01)  458b7e24               mov r15d, dword ptr [r14 + 0x24]
0x883e05  (f 0x483e05)  4539e7                 cmp r15d, r12d
0x883e08  (f 0x483e08)  0f8dec000000           jge 0x883efa
0x883e0e  (f 0x483e0e)  418b462c               mov eax, dword ptr [r14 + 0x2c]
0x883e12  (f 0x483e12)  418d8fffff0000         lea ecx, [r15 + 0xffff]
0x883e19  (f 0x483e19)  4585ff                 test r15d, r15d
0x883e1c  (f 0x483e1c)  48895db0               mov qword ptr [rbp - 0x50], rbx
0x883e20  (f 0x483e20)  410f49cf               cmovns ecx, r15d
0x883e24  (f 0x483e24)  c1f910                 sar ecx, 0x10
0x883e27  (f 0x483e27)  894da8                 mov dword ptr [rbp - 0x58], ecx
0x883e2a  (f 0x483e2a)  39c1                   cmp ecx, eax
0x883e2c  (f 0x483e2c)  7d27                   jge 0x883e55
0x883e2e  (f 0x483e2e)  41ff4624               inc dword ptr [r14 + 0x24]
0x883e32  (f 0x483e32)  488b5db0               mov rbx, qword ptr [rbp - 0x50]
0x883e36  (f 0x483e36)  4c8b2dcbe0ca08         mov r13, qword ptr [rip + 0x8cae0cb]  ; rip->0x9531f08 (file 0x9131f08)
0x883e3d  (f 0x483e3d)  45397e24               cmp dword ptr [r14 + 0x24], r15d
0x883e41  (f 0x483e41)  0f8f4dfdffff           jg 0x883b94
0x883e47  (f 0x483e47)  e9b3feffff             jmp 0x883cff
0x883e50  (f 0x483e50)  3945a8                 cmp dword ptr [rbp - 0x58], eax
0x883e53  (f 0x483e53)  7cd9                   jl 0x883e2e
0x883e55  (f 0x483e55)  4d8b6e10               mov r13, qword ptr [r14 + 0x10]
0x883e59  (f 0x483e59)  bf08001800             mov edi, 0x180008
0x883e5e  (f 0x483e5e)  4863d8                 movsxd rbx, eax
0x883e61  (f 0x483e61)  e81a2e4104             call 0x4c96c80
0x883e66  (f 0x483e66)  4989c4                 mov r12, rax
0x883e69  (f 0x483e69)  ba00001800             mov edx, 0x180000
0x883e6e  (f 0x483e6e)  48c70000000100         mov qword ptr [rax], 0x10000
0x883e75  (f 0x483e75)  31f6                   xor esi, esi
0x883e77  (f 0x483e77)  4983c408               add r12, 8
0x883e7b  (f 0x483e7b)  4c89e7                 mov rdi, r12
0x883e7e  (f 0x483e7e)  e8bddffe06             call 0x7871e40
0x883e83  (f 0x483e83)  31c0                   xor eax, eax
0x883e85  (f 0x483e85)  f04d0fb164dd00         lock cmpxchg qword ptr [r13 + rbx*8], r12
0x883e8c  (f 0x483e8c)  418b462c               mov eax, dword ptr [r14 + 0x2c]
0x883e90  (f 0x483e90)  75be                   jne 0x883e50
0x883e92  (f 0x483e92)  ffc0                   inc eax
0x883e94  (f 0x483e94)  4189462c               mov dword ptr [r14 + 0x2c], eax
0x883e98  (f 0x483e98)  ebb6                   jmp 0x883e50
0x883e9a  (f 0x483e9a)  488b4318               mov rax, qword ptr [rbx + 0x18]
0x883e9e  (f 0x483e9e)  488d7dc0               lea rdi, [rbp - 0x40]
0x883ea2  (f 0x483ea2)  488d75b8               lea rsi, [rbp - 0x48]
0x883ea6  (f 0x483ea6)  488945b8               mov qword ptr [rbp - 0x48], rax
0x883eaa  (f 0x483eaa)  e811b2dbff             call 0x63f0c0
0x883eaf  (f 0x483eaf)  837dc800               cmp dword ptr [rbp - 0x38], 0
0x883eb3  (f 0x483eb3)  7406                   je 0x883ebb
0x883eb5  (f 0x483eb5)  488b4dc0               mov rcx, qword ptr [rbp - 0x40]
0x883eb9  (f 0x483eb9)  eb07                   jmp 0x883ec2
0x883ebb  (f 0x483ebb)  488d0ddeb61a07         lea rcx, [rip + 0x71ab6de]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x883ec2  (f 0x483ec2)  4d8b0c24               mov r9, qword ptr [r12]
0x883ec6  (f 0x483ec6)  488d3d3c436607         lea rdi, [rip + 0x766433c]  ; rip->0x7ee8209 (file 0x7ae8209)
0x883ecd  (f 0x483ecd)  488d15be534c07         lea rdx, [rip + 0x74c53be]  ; rip->0x7d49292 (file 0x7949292)
0x883ed4  (f 0x483ed4)  be23010000             mov esi, 0x123
0x883ed9  (f 0x483ed9)  4589f8                 mov r8d, r15d
0x883edc  (f 0x483edc)  31c0                   xor eax, eax
0x883ede  (f 0x483ede)  e8cd11cfff             call 0x5750b0
0x883ee3  (f 0x483ee3)  488b7dc0               mov rdi, qword ptr [rbp - 0x40]
0x883ee7  (f 0x483ee7)  4885ff                 test rdi, rdi
0x883eea  (f 0x483eea)  0f841dfeffff           je 0x883d0d
0x883ef0  (f 0x483ef0)  e89b3bc3ff             call 0x4b7a90
0x883ef5  (f 0x483ef5)  e913feffff             jmp 0x883d0d
0x883efa  (f 0x483efa)  833def5cf30800         cmp dword ptr [rip + 0x8f35cef], 0  ; rip->0x97b9bf0 (file 0x93b9bf0)
0x883f01  (f 0x483f01)  740c                   je 0x883f0f
0x883f03  (f 0x483f03)  488d3d0662f308         lea rdi, [rip + 0x8f36206]  ; rip->0x97ba110 (file 0x93ba110)
0x883f0a  (f 0x483f0a)  e8a1050000             call 0x8844b0
0x883f0f  (f 0x483f0f)  488d3df3426607         lea rdi, [rip + 0x76642f3]  ; rip->0x7ee8209 (file 0x7ae8209)
0x883f16  (f 0x483f16)  488d15814a2507         lea rdx, [rip + 0x7254a81]  ; rip->0x7ad899e (file 0x76d899e)
0x883f1d  (f 0x483f1d)  be88020000             mov esi, 0x288
0x883f22  (f 0x483f22)  4489e1                 mov ecx, r12d
0x883f25  (f 0x483f25)  41b801000000           mov r8d, 1
0x883f2b  (f 0x483f2b)  31c0                   xor eax, eax
0x883f2d  (f 0x483f2d)  e87e11cfff             call 0x5750b0
0x883f32  (f 0x483f32)  e9d7feffff             jmp 0x883e0e
0x883f37  (f 0x483f37)  488d3dcb426607         lea rdi, [rip + 0x76642cb]  ; rip->0x7ee8209 (file 0x7ae8209)
0x883f3e  (f 0x483f3e)  488d15552d5907         lea rdx, [rip + 0x7592d55]  ; rip->0x7e16c9a (file 0x7a16c9a)
0x883f45  (f 0x483f45)  be07010000             mov esi, 0x107
0x883f4a  (f 0x483f4a)  31c0                   xor eax, eax
0x883f4c  (f 0x483f4c)  e85f11cfff             call 0x5750b0
0x883f51  (f 0x483f51)  418b4604               mov eax, dword ptr [r14 + 4]
0x883f55  (f 0x483f55)  418b4e08               mov ecx, dword ptr [r14 + 8]
0x883f59  (f 0x483f59)  39c8                   cmp eax, ecx
0x883f5b  (f 0x483f5b)  0f8da7fcffff           jge 0x883c08
0x883f61  (f 0x483f61)  e9e2fcffff             jmp 0x883c48
0x883f66  (f 0x483f66)  833d835cf30800         cmp dword ptr [rip + 0x8f35c83], 0  ; rip->0x97b9bf0 (file 0x93b9bf0)
0x883f6d  (f 0x483f6d)  740c                   je 0x883f7b
0x883f6f  (f 0x483f6f)  488d3d9a61f308         lea rdi, [rip + 0x8f3619a]  ; rip->0x97ba110 (file 0x93ba110)
0x883f76  (f 0x483f76)  e835050000             call 0x8844b0
0x883f7b  (f 0x483f7b)  488d3d87426607         lea rdi, [rip + 0x7664287]  ; rip->0x7ee8209 (file 0x7ae8209)
0x883f82  (f 0x483f82)  488d15154a2507         lea rdx, [rip + 0x7254a15]  ; rip->0x7ad899e (file 0x76d899e)
0x883f89  (f 0x483f89)  be88020000             mov esi, 0x288
0x883f8e  (f 0x483f8e)  4489e1                 mov ecx, r12d
0x883f91  (f 0x483f91)  41b801000000           mov r8d, 1
0x883f97  (f 0x483f97)  31c0                   xor eax, eax
0x883f99  (f 0x483f99)  e81211cfff             call 0x5750b0
0x883f9e  (f 0x483f9e)  e976fcffff             jmp 0x883c19
0x883fa3  (f 0x483fa3)  e8b8defe06             call 0x7871e60
0x883fa8  (f 0x483fa8)  0f0b                   ud2 
--- calls: [('0xc83b78', '0x7871ee0'), ('0xc83eaa', '0x63f0c0'), ('0xc83ede', '0x5750b0'), ('0xc83ef0', '0x4b7a90'), ('0xc83d78', '0x768470'), ('0xc83dab', '0x7871f20'), ('0xc83fa3', '0x7871e60'), ('0xc83c85', '0x4c96c80'), ('0xc83ca2', '0x7871e40'), ('0xc83f76', '0x8844b0'), ('0xc83f99', '0x5750b0'), ('0xc83f4c', '0x5750b0'), ('0xc83cf0', '0x427140'), ('0xc83e61', '0x4c96c80'), ('0xc83e7e', '0x7871e40'), ('0xc83f0a', '0x8844b0'), ('0xc83f2d', '0x5750b0'), ('0xc83bbe', '0x7871ef0')]

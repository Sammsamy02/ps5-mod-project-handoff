0x71d4f0  (f 0x31d4f0)  55                     push rbp
0x71d4f1  (f 0x31d4f1)  4889e5                 mov rbp, rsp
0x71d4f4  (f 0x31d4f4)  4157                   push r15
0x71d4f6  (f 0x31d4f6)  4156                   push r14
0x71d4f8  (f 0x31d4f8)  4155                   push r13
0x71d4fa  (f 0x31d4fa)  4154                   push r12
0x71d4fc  (f 0x31d4fc)  53                     push rbx
0x71d4fd  (f 0x31d4fd)  4883ec48               sub rsp, 0x48
0x71d501  (f 0x31d501)  4c8b25004ae108         mov r12, qword ptr [rip + 0x8e14a00]  ; rip->0x9531f08 (file 0x9131f08)
0x71d508  (f 0x31d508)  4989f6                 mov r14, rsi
0x71d50b  (f 0x31d50b)  4889fb                 mov rbx, rdi
0x71d50e  (f 0x31d50e)  498b0424               mov rax, qword ptr [r12]
0x71d512  (f 0x31d512)  488945d0               mov qword ptr [rbp - 0x30], rax
0x71d516  (f 0x31d516)  c745cc00000000         mov dword ptr [rbp - 0x34], 0
0x71d51d  (f 0x31d51d)  488b4628               mov rax, qword ptr [rsi + 0x28]
0x71d521  (f 0x31d521)  a804                   test al, 4
0x71d523  (f 0x31d523)  7419                   je 0x71d53e
0x71d525  (f 0x31d525)  488b5350               mov rdx, qword ptr [rbx + 0x50]
0x71d529  (f 0x31d529)  31c9                   xor ecx, ecx
0x71d52b  (f 0x31d52b)  4885d2                 test rdx, rdx
0x71d52e  (f 0x31d52e)  740b                   je 0x71d53b
0x71d530  (f 0x31d530)  488b5218               mov rdx, qword ptr [rdx + 0x18]
0x71d534  (f 0x31d534)  ffc1                   inc ecx
0x71d536  (f 0x31d536)  4885d2                 test rdx, rdx
0x71d539  (f 0x31d539)  75f5                   jne 0x71d530
0x71d53b  (f 0x31d53b)  894dcc                 mov dword ptr [rbp - 0x34], ecx
0x71d53e  (f 0x31d53e)  498b4e08               mov rcx, qword ptr [r14 + 8]
0x71d542  (f 0x31d542)  488b31                 mov rsi, qword ptr [rcx]
0x71d545  (f 0x31d545)  488d5604               lea rdx, [rsi + 4]
0x71d549  (f 0x31d549)  483b5108               cmp rdx, qword ptr [rcx + 8]
0x71d54d  (f 0x31d54d)  7625                   jbe 0x71d574
0x71d54f  (f 0x31d54f)  a900000100             test eax, 0x10000
0x71d554  (f 0x31d554)  757e                   jne 0x71d5d4
0x71d556  (f 0x31d556)  498b06                 mov rax, qword ptr [r14]
0x71d559  (f 0x31d559)  488d75cc               lea rsi, [rbp - 0x34]
0x71d55d  (f 0x31d55d)  ba04000000             mov edx, 4
0x71d562  (f 0x31d562)  4c89f7                 mov rdi, r14
0x71d565  (f 0x31d565)  ff9088010000           call qword ptr [rax + 0x188]
0x71d56b  (f 0x31d56b)  41f6462801             test byte ptr [r14 + 0x28], 1
0x71d570  (f 0x31d570)  7411                   je 0x71d583
0x71d572  (f 0x31d572)  eb73                   jmp 0x71d5e7
0x71d574  (f 0x31d574)  8b06                   mov eax, dword ptr [rsi]
0x71d576  (f 0x31d576)  8945cc                 mov dword ptr [rbp - 0x34], eax
0x71d579  (f 0x31d579)  488911                 mov qword ptr [rcx], rdx
0x71d57c  (f 0x31d57c)  41f6462801             test byte ptr [r14 + 0x28], 1
0x71d581  (f 0x31d581)  7564                   jne 0x71d5e7
0x71d583  (f 0x31d583)  488b5b50               mov rbx, qword ptr [rbx + 0x50]
0x71d587  (f 0x31d587)  4885db                 test rbx, rbx
0x71d58a  (f 0x31d58a)  0f849a010000           je 0x71d72a
0x71d590  (f 0x31d590)  4c8d7db0               lea r15, [rbp - 0x50]
0x71d594  (f 0x31d594)  6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x71d5a0  (f 0x31d5a0)  488b4308               mov rax, qword ptr [rbx + 8]
0x71d5a4  (f 0x31d5a4)  4c89f7                 mov rdi, r14
0x71d5a7  (f 0x31d5a7)  4c89fe                 mov rsi, r15
0x71d5aa  (f 0x31d5aa)  488b00                 mov rax, qword ptr [rax]
0x71d5ad  (f 0x31d5ad)  488945b0               mov qword ptr [rbp - 0x50], rax
0x71d5b1  (f 0x31d5b1)  498b06                 mov rax, qword ptr [r14]
0x71d5b4  (f 0x31d5b4)  ff9038010000           call qword ptr [rax + 0x138]
0x71d5ba  (f 0x31d5ba)  488b03                 mov rax, qword ptr [rbx]
0x71d5bd  (f 0x31d5bd)  4889df                 mov rdi, rbx
0x71d5c0  (f 0x31d5c0)  4c89f6                 mov rsi, r14
0x71d5c3  (f 0x31d5c3)  ff5018                 call qword ptr [rax + 0x18]
0x71d5c6  (f 0x31d5c6)  488b5b18               mov rbx, qword ptr [rbx + 0x18]
0x71d5ca  (f 0x31d5ca)  4885db                 test rbx, rbx
0x71d5cd  (f 0x31d5cd)  75d1                   jne 0x71d5a0
0x71d5cf  (f 0x31d5cf)  e956010000             jmp 0x71d72a
0x71d5d4  (f 0x31d5d4)  488d75cc               lea rsi, [rbp - 0x34]
0x71d5d8  (f 0x31d5d8)  4c89f7                 mov rdi, r14
0x71d5db  (f 0x31d5db)  e8d0f4efff             call 0x61cab0
0x71d5e0  (f 0x31d5e0)  41f6462801             test byte ptr [r14 + 0x28], 1
0x71d5e5  (f 0x31d5e5)  749c                   je 0x71d583
0x71d5e7  (f 0x31d5e7)  8b55cc                 mov edx, dword ptr [rbp - 0x34]
0x71d5ea  (f 0x31d5ea)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71d5ee  (f 0x31d5ee)  c5f81145b0             vmovups xmmword ptr [rbp - 0x50], xmm0
0x71d5f3  (f 0x31d5f3)  85d2                   test edx, edx
0x71d5f5  (f 0x31d5f5)  0f884f010000           js 0x71d74a
0x71d5fb  (f 0x31d5fb)  4885d2                 test rdx, rdx
0x71d5fe  (f 0x31d5fe)  0f84e3000000           je 0x71d6e7
0x71d604  (f 0x31d604)  4c8d4dbc               lea r9, [rbp - 0x44]
0x71d608  (f 0x31d608)  488d4db0               lea rcx, [rbp - 0x50]
0x71d60c  (f 0x31d60c)  bf08000000             mov edi, 8
0x71d611  (f 0x31d611)  be08000000             mov esi, 8
0x71d616  (f 0x31d616)  4531ff                 xor r15d, r15d
0x71d619  (f 0x31d619)  4531c0                 xor r8d, r8d
0x71d61c  (f 0x31d61c)  e8ff87d0ff             call 0x425e20
0x71d621  (f 0x31d621)  837dcc00               cmp dword ptr [rbp - 0x34], 0
0x71d625  (f 0x31d625)  0f8ebc000000           jle 0x71d6e7
0x71d62b  (f 0x31d62b)  4889d8                 mov rax, rbx
0x71d62e  (f 0x31d62e)  4c8d6da8               lea r13, [rbp - 0x58]
0x71d632  (f 0x31d632)  4883c801               or rax, 1
0x71d636  (f 0x31d636)  48894590               mov qword ptr [rbp - 0x70], rax
0x71d63a  (f 0x31d63a)  eb24                   jmp 0x71d660
0x71d640  (f 0x31d640)  8d4801                 lea ecx, [rax + 1]
0x71d643  (f 0x31d643)  4898                   cdqe 
0x71d645  (f 0x31d645)  41ffc7                 inc r15d
0x71d648  (f 0x31d648)  894db8                 mov dword ptr [rbp - 0x48], ecx
0x71d64b  (f 0x31d64b)  488b55b0               mov rdx, qword ptr [rbp - 0x50]
0x71d64f  (f 0x31d64f)  4c892cc2               mov qword ptr [rdx + rax*8], r13
0x71d653  (f 0x31d653)  4d89e5                 mov r13, r12
0x71d656  (f 0x31d656)  443b7dcc               cmp r15d, dword ptr [rbp - 0x34]
0x71d65a  (f 0x31d65a)  0f8d8a000000           jge 0x71d6ea
0x71d660  (f 0x31d660)  48c745a800000000       mov qword ptr [rbp - 0x58], 0
0x71d668  (f 0x31d668)  4c89f7                 mov rdi, r14
0x71d66b  (f 0x31d66b)  4c89ee                 mov rsi, r13
0x71d66e  (f 0x31d66e)  498b06                 mov rax, qword ptr [r14]
0x71d671  (f 0x31d671)  ff9038010000           call qword ptr [rax + 0x138]
0x71d677  (f 0x31d677)  488b4590               mov rax, qword ptr [rbp - 0x70]
0x71d67b  (f 0x31d67b)  488945a0               mov qword ptr [rbp - 0x60], rax
0x71d67f  (f 0x31d67f)  4885db                 test rbx, rbx
0x71d682  (f 0x31d682)  7414                   je 0x71d698
0x71d684  (f 0x31d684)  488d05d1170809         lea rax, [rip + 0x90817d1]  ; rip->0x979ee5c (file 0x939ee5c)
0x71d68b  (f 0x31d68b)  803801                 cmp byte ptr [rax], 1
0x71d68e  (f 0x31d68e)  7508                   jne 0x71d698
0x71d690  (f 0x31d690)  4889df                 mov rdi, rbx
0x71d693  (f 0x31d693)  e828ac0400             call 0x7682c0
0x71d698  (f 0x31d698)  4c89ef                 mov rdi, r13
0x71d69b  (f 0x31d69b)  488d75a0               lea rsi, [rbp - 0x60]
0x71d69f  (f 0x31d69f)  488d5598               lea rdx, [rbp - 0x68]
0x71d6a3  (f 0x31d6a3)  31c9                   xor ecx, ecx
0x71d6a5  (f 0x31d6a5)  48c7459800000000       mov qword ptr [rbp - 0x68], 0
0x71d6ad  (f 0x31d6ad)  4d89ec                 mov r12, r13
0x71d6b0  (f 0x31d6b0)  e89b750400             call 0x764c50
0x71d6b5  (f 0x31d6b5)  4989c5                 mov r13, rax
0x71d6b8  (f 0x31d6b8)  488b00                 mov rax, qword ptr [rax]
0x71d6bb  (f 0x31d6bb)  4c89f6                 mov rsi, r14
0x71d6be  (f 0x31d6be)  4c89ef                 mov rdi, r13
0x71d6c1  (f 0x31d6c1)  ff5018                 call qword ptr [rax + 0x18]
0x71d6c4  (f 0x31d6c4)  8b45b8                 mov eax, dword ptr [rbp - 0x48]
0x71d6c7  (f 0x31d6c7)  3b45bc                 cmp eax, dword ptr [rbp - 0x44]
0x71d6ca  (f 0x31d6ca)  0f8570ffffff           jne 0x71d640
0x71d6d0  (f 0x31d6d0)  bf08080000             mov edi, 0x808
0x71d6d5  (f 0x31d6d5)  488d75b0               lea rsi, [rbp - 0x50]
0x71d6d9  (f 0x31d6d9)  488d55bc               lea rdx, [rbp - 0x44]
0x71d6dd  (f 0x31d6dd)  e8bec1d0ff             call 0x4298a0
0x71d6e2  (f 0x31d6e2)  e959ffffff             jmp 0x71d640
0x71d6e7  (f 0x31d6e7)  8b4db8                 mov ecx, dword ptr [rbp - 0x48]
0x71d6ea  (f 0x31d6ea)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x71d6ee  (f 0x31d6ee)  85c9                   test ecx, ecx
0x71d6f0  (f 0x31d6f0)  7e27                   jle 0x71d719
0x71d6f2  (f 0x31d6f2)  488b5350               mov rdx, qword ptr [rbx + 0x50]
0x71d6f6  (f 0x31d6f6)  89c8                   mov eax, ecx
0x71d6f8  (f 0x31d6f8)  48ffc0                 inc rax
0x71d6fb  (f 0x31d6fb)  0f1f440000             nop dword ptr [rax + rax]
0x71d700  (f 0x31d700)  488b4cc7f0             mov rcx, qword ptr [rdi + rax*8 - 0x10]
0x71d705  (f 0x31d705)  48ffc8                 dec rax
0x71d708  (f 0x31d708)  48895118               mov qword ptr [rcx + 0x18], rdx
0x71d70c  (f 0x31d70c)  4889ca                 mov rdx, rcx
0x71d70f  (f 0x31d70f)  48894b50               mov qword ptr [rbx + 0x50], rcx
0x71d713  (f 0x31d713)  4883f801               cmp rax, 1
0x71d717  (f 0x31d717)  77e7                   ja 0x71d700
0x71d719  (f 0x31d719)  4c8b25e847e108         mov r12, qword ptr [rip + 0x8e147e8]  ; rip->0x9531f08 (file 0x9131f08)
0x71d720  (f 0x31d720)  4885ff                 test rdi, rdi
0x71d723  (f 0x31d723)  7405                   je 0x71d72a
0x71d725  (f 0x31d725)  e866a3d9ff             call 0x4b7a90
0x71d72a  (f 0x31d72a)  498b0424               mov rax, qword ptr [r12]
0x71d72e  (f 0x31d72e)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x71d732  (f 0x31d732)  750f                   jne 0x71d743
0x71d734  (f 0x31d734)  4883c448               add rsp, 0x48
0x71d738  (f 0x31d738)  5b                     pop rbx
0x71d739  (f 0x31d739)  415c                   pop r12
0x71d73b  (f 0x31d73b)  415d                   pop r13
0x71d73d  (f 0x31d73d)  415e                   pop r14
0x71d73f  (f 0x31d73f)  415f                   pop r15
0x71d741  (f 0x31d741)  5d                     pop rbp
0x71d742  (f 0x31d742)  c3                     ret 
0x71d743  (f 0x31d743)  e818471507             call 0x7871e60
0x71d748  (f 0x31d748)  0f0b                   ud2 
0x71d74a  (f 0x31d74a)  4889d7                 mov rdi, rdx
0x71d74d  (f 0x31d74d)  e8ae59d4ff             call 0x463100
0x71d752  (f 0x31d752)  0f0b                   ud2 
--- calls: [('0xb1d61c', '0x425e20'), ('0xb1d693', '0x7682c0'), ('0xb1d6b0', '0x764c50'), ('0xb1d6dd', '0x4298a0'), ('0xb1d725', '0x4b7a90'), ('0xb1d743', '0x7871e60'), ('0xb1d74d', '0x463100'), ('0xb1d5db', '0x61cab0')]

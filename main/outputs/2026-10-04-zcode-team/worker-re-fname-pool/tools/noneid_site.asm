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
0x647f30  (file 0x247f30)  55                       push rbp
0x647f31  (file 0x247f31)  4889e5                   mov rbp, rsp
0x647f34  (file 0x247f34)  4157                     push r15
0x647f36  (file 0x247f36)  4156                     push r14
0x647f38  (file 0x247f38)  4155                     push r13
0x647f3a  (file 0x247f3a)  4154                     push r12
0x647f3c  (file 0x247f3c)  53                       push rbx
0x647f3d  (file 0x247f3d)  50                       push rax
0x647f3e  (file 0x247f3e)  4c8b6f10                 mov r13, qword ptr [rdi + 0x10]
0x647f42  (file 0x247f42)  4889cb                   mov rbx, rcx
0x647f45  (file 0x247f45)  4189d6                   mov r14d, edx
0x647f48  (file 0x247f48)  4989f7                   mov r15, rsi
0x647f4b  (file 0x247f4b)  4989fc                   mov r12, rdi
0x647f4e  (file 0x247f4e)  e80de42207               call 0x7876360  ; -> 0x7876360 (file 0x7476360)
0x647f53  (file 0x247f53)  c4e1f96ec0               vmovq xmm0, rax
0x647f58  (file 0x247f58)  c5f96205608f7f07         vpunpckldq xmm0, xmm0, xmmword ptr [rip + 0x77f8f60]  ; rip->0x7e40ec0 (file 0x7a40ec0)
0x647f60  (file 0x247f60)  488d0509810909           lea rax, [rip + 0x9098109]  ; rip->0x96e0070 (file 0x92e0070)
0x647f67  (file 0x247f67)  450fb6442408             movzx r8d, byte ptr [r12 + 8]
0x647f6d  (file 0x247f6d)  450fb64c2409             movzx r9d, byte ptr [r12 + 9]
0x647f73  (file 0x247f73)  4c89ef                   mov rdi, r13
0x647f76  (file 0x247f76)  4c89fe                   mov rsi, r15
0x647f79  (file 0x247f79)  4489f2                   mov edx, r14d
0x647f7c  (file 0x247f7c)  4889d9                   mov rcx, rbx
0x647f7f  (file 0x247f7f)  c5f95c05a9747f07         vsubpd xmm0, xmm0, xmmword ptr [rip + 0x77f74a9]  ; rip->0x7e3f430 (file 0x7a3f430)
0x647f87  (file 0x247f87)  c5f9c6c801               vshufpd xmm1, xmm0, xmm0, 1
0x647f8c  (file 0x247f8c)  c5f358c0                 vaddsd xmm0, xmm1, xmm0
0x647f90  (file 0x247f90)  c5fb5900                 vmulsd xmm0, xmm0, qword ptr [rax]
0x647f94  (file 0x247f94)  e817fdf9ff               call 0x5e7cb0  ; -> 0x5e7cb0 (file 0x1e7cb0)
0x647f99  (file 0x247f99)  4883c408                 add rsp, 8
0x647f9d  (file 0x247f9d)  5b                       pop rbx
0x647f9e  (file 0x247f9e)  415c                     pop r12
0x647fa0  (file 0x247fa0)  415d                     pop r13
0x647fa2  (file 0x247fa2)  415e                     pop r14
0x647fa4  (file 0x247fa4)  415f                     pop r15
0x647fa6  (file 0x247fa6)  5d                       pop rbp
0x647fa7  (file 0x247fa7)  c3                       ret 
0x647fa8  (file 0x247fa8)  cc                       int3 
0x647fa9  (file 0x247fa9)  cc                       int3 
0x647faa  (file 0x247faa)  cc                       int3 
0x647fab  (file 0x247fab)  cc                       int3 
0x647fac  (file 0x247fac)  cc                       int3 
0x647fad  (file 0x247fad)  cc                       int3 
0x647fae  (file 0x247fae)  cc                       int3 
0x647faf  (file 0x247faf)  cc                       int3 
0x647fb0  (file 0x247fb0)  55                       push rbp
0x647fb1  (file 0x247fb1)  4889e5                   mov rbp, rsp
0x647fb4  (file 0x247fb4)  4157                     push r15
0x647fb6  (file 0x247fb6)  4156                     push r14
0x647fb8  (file 0x247fb8)  4155                     push r13
0x647fba  (file 0x247fba)  4154                     push r12
0x647fbc  (file 0x247fbc)  53                       push rbx
0x647fbd  (file 0x247fbd)  4883ec58                 sub rsp, 0x58
0x647fc1  (file 0x247fc1)  4c8b2d40dfee08           mov r13, qword ptr [rip + 0x8eedf40]  ; rip->0x9535f08 (file 0x9135f08)
0x647fc8  (file 0x247fc8)  4c894590                 mov qword ptr [rbp - 0x70], r8
0x647fcc  (file 0x247fcc)  48895580                 mov qword ptr [rbp - 0x80], rdx
0x647fd0  (file 0x247fd0)  48897d98                 mov qword ptr [rbp - 0x68], rdi
0x647fd4  (file 0x247fd4)  4189f6                   mov r14d, esi
0x647fd7  (file 0x247fd7)  498b4500                 mov rax, qword ptr [r13]
0x647fdb  (file 0x247fdb)  488945d0                 mov qword ptr [rbp - 0x30], rax
0x647fdf  (file 0x247fdf)  44894db4                 mov dword ptr [rbp - 0x4c], r9d
0x647fe3  (file 0x247fe3)  8975bc                   mov dword ptr [rbp - 0x44], esi
0x647fe6  (file 0x247fe6)  85f6                     test esi, esi
0x647fe8  (file 0x247fe8)  0f8428020000             je 0x648216  ; -> 0x648216 (file 0x248216)
0x647fee  (file 0x247fee)  4489ca                   mov edx, r9d
0x647ff1  (file 0x247ff1)  83e204                   and edx, 4
0x647ff4  (file 0x247ff4)  85c9                     test ecx, ecx
0x647ff6  (file 0x247ff6)  0f8420020000             je 0x64821c  ; -> 0x64821c (file 0x24821c)
0x647ffc  (file 0x247ffc)  4863c1                   movsxd rax, ecx
0x647fff  (file 0x247fff)  8955a4                   mov dword ptr [rbp - 0x5c], edx
0x648002  (file 0x248002)  31d2                     xor edx, edx
0x648004  (file 0x248004)  48c745a800000000         mov qword ptr [rbp - 0x58], 0
0x64800c  (file 0x24800c)  48c1e004                 shl rax, 4
0x648010  (file 0x248010)  48894588                 mov qword ptr [rbp - 0x78], rax
0x648014  (file 0x248014)  eb20                     jmp 0x648036  ; -> 0x648036 (file 0x248036)
0x648016  (file 0x248016)  662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x648020  (file 0x248020)  8b55b8                   mov edx, dword ptr [rbp - 0x48]
0x648023  (file 0x248023)  448b4db4                 mov r9d, dword ptr [rbp - 0x4c]
0x648027  (file 0x248027)  448b75bc                 mov r14d, dword ptr [rbp - 0x44]
0x64802b  (file 0x24802b)  ffc2                     inc edx
0x64802d  (file 0x24802d)  4439f2                   cmp edx, r14d
0x648030  (file 0x248030)  0f84d0010000             je 0x648206  ; -> 0x648206 (file 0x248206)
0x648036  (file 0x248036)  488b4598                 mov rax, qword ptr [rbp - 0x68]
0x64803a  (file 0x24803a)  4589f5                   mov r13d, r14d
0x64803d  (file 0x24803d)  4c8b7588                 mov r14, qword ptr [rbp - 0x78]
0x648041  (file 0x248041)  4c8b6580                 mov r12, qword ptr [rbp - 0x80]
0x648045  (file 0x248045)  4c63fa                   movsxd r15, edx
0x648048  (file 0x248048)  8955b8                   mov dword ptr [rbp - 0x48], edx
0x64804b  (file 0x24804b)  4129d5                   sub r13d, edx
0x64804e  (file 0x24804e)  4a8d1c78                 lea rbx, [rax + r15*2]
0x648052  (file 0x248052)  41f6c101                 test r9b, 1
0x648056  (file 0x248056)  7566                     jne 0x6480be  ; -> 0x6480be (file 0x2480be)
0x648058  (file 0x248058)  4c8b7588                 mov r14, qword ptr [rbp - 0x78]
0x64805c  (file 0x24805c)  4c8b6580                 mov r12, qword ptr [rbp - 0x80]
0x648060  (file 0x248060)  eb18                     jmp 0x64807a  ; -> 0x64807a (file 0x24807a)
0x648062  (file 0x248062)  66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x648070  (file 0x248070)  4983c410                 add r12, 0x10
0x648074  (file 0x248074)  4983c6f0                 add r14, -0x10
0x648078  (file 0x248078)  74a6                     je 0x648020  ; -> 0x648020 (file 0x248020)
0x64807a  (file 0x24807a)  418b542408               mov edx, dword ptr [r12 + 8]
0x64807f  (file 0x24807f)  4489e8                   mov eax, r13d
0x648082  (file 0x248082)  4439ea                   cmp edx, r13d
0x648085  (file 0x248085)  0f4cc2                   cmovl eax, edx
0x648088  (file 0x248088)  89c1                     mov ecx, eax
0x64808a  (file 0x24808a)  c1f91f                   sar ecx, 0x1f
0x64808d  (file 0x24808d)  c4e270f2c8               andn ecx, ecx, eax
0x648092  (file 0x248092)  39d1                     cmp ecx, edx
0x648094  (file 0x248094)  75da                     jne 0x648070  ; -> 0x648070 (file 0x248070)
0x648096  (file 0x248096)  85c0                     test eax, eax
0x648098  (file 0x248098)  7e54                     jle 0x6480ee  ; -> 0x6480ee (file 0x2480ee)
0x64809a  (file 0x24809a)  498b3c24                 mov rdi, qword ptr [r12]
0x64809e  (file 0x24809e)  4889de                   mov rsi, rbx
0x6480a1  (file 0x2480a1)  e8dae72207               call 0x7876880  ; -> 0x7876880 (file 0x7476880)
0x6480a6  (file 0x2480a6)  85c0                     test eax, eax
0x6480a8  (file 0x2480a8)  75c6                     jne 0x648070  ; -> 0x648070 (file 0x248070)
0x6480aa  (file 0x2480aa)  eb42                     jmp 0x6480ee  ; -> 0x6480ee (file 0x2480ee)
0x6480ac  (file 0x2480ac)  0f1f4000                 nop dword ptr [rax]
0x6480b0  (file 0x2480b0)  4983c410                 add r12, 0x10
0x6480b4  (file 0x2480b4)  4983c6f0                 add r14, -0x10
0x6480b8  (file 0x2480b8)  0f8462ffffff             je 0x648020  ; -> 0x648020 (file 0x248020)
0x6480be  (file 0x2480be)  418b542408               mov edx, dword ptr [r12 + 8]
0x6480c3  (file 0x2480c3)  4489e8                   mov eax, r13d
0x6480c6  (file 0x2480c6)  4439ea                   cmp edx, r13d
0x6480c9  (file 0x2480c9)  0f4cc2                   cmovl eax, edx
0x6480cc  (file 0x2480cc)  89c1                     mov ecx, eax
0x6480ce  (file 0x2480ce)  c1f91f                   sar ecx, 0x1f
0x6480d1  (file 0x2480d1)  c4e270f2c8               andn ecx, ecx, eax
0x6480d6  (file 0x2480d6)  39d1                     cmp ecx, edx
0x6480d8  (file 0x2480d8)  75d6                     jne 0x6480b0  ; -> 0x6480b0 (file 0x2480b0)
0x6480da  (file 0x2480da)  85c0                     test eax, eax
0x6480dc  (file 0x2480dc)  7e10                     jle 0x6480ee  ; -> 0x6480ee (file 0x2480ee)
0x6480de  (file 0x2480de)  498b3c24                 mov rdi, qword ptr [r12]
0x6480e2  (file 0x2480e2)  4889de                   mov rsi, rbx
0x6480e5  (file 0x2480e5)  e8368de4ff               call 0x490e20  ; -> 0x490e20 (file 0x90e20)
0x6480ea  (file 0x2480ea)  85c0                     test eax, eax
0x6480ec  (file 0x2480ec)  75c2                     jne 0x6480b0  ; -> 0x6480b0 (file 0x2480b0)
0x6480ee  (file 0x2480ee)  4d85e4                   test r12, r12
0x6480f1  (file 0x2480f1)  0f8429ffffff             je 0x648020  ; -> 0x648020 (file 0x248020)
0x6480f7  (file 0x2480f7)  8b55b8                   mov edx, dword ptr [rbp - 0x48]
0x6480fa  (file 0x2480fa)  488b45a8                 mov rax, qword ptr [rbp - 0x58]
0x6480fe  (file 0x2480fe)  488b4d98                 mov rcx, qword ptr [rbp - 0x68]
0x648102  (file 0x248102)  4189d5                   mov r13d, edx
0x648105  (file 0x248105)  4129c5                   sub r13d, eax
0x648108  (file 0x248108)  837da400                 cmp dword ptr [rbp - 0x5c], 0
0x64810c  (file 0x24810c)  4898                     cdqe 
0x64810e  (file 0x24810e)  488d1c41                 lea rbx, [rcx + rax*2]
0x648112  (file 0x248112)  7517                     jne 0x64812b  ; -> 0x64812b (file 0x24812b)
0x648114  (file 0x248114)  448b75bc                 mov r14d, dword ptr [rbp - 0x44]
0x648118  (file 0x248118)  448b4db4                 mov r9d, dword ptr [rbp - 0x4c]
0x64811c  (file 0x24811c)  41f6c102                 test r9b, 2
0x648120  (file 0x248120)  0f85aa000000             jne 0x6481d0  ; -> 0x6481d0 (file 0x2481d0)
0x648126  (file 0x248126)  e9aa000000               jmp 0x6481d5  ; -> 0x6481d5 (file 0x2481d5)
0x64812b  (file 0x24812b)  4585ed                   test r13d, r13d
0x64812e  (file 0x24812e)  743b                     je 0x64816b  ; -> 0x64816b (file 0x24816b)
0x648130  (file 0x248130)  4d01ff                   add r15, r15
0x648133  (file 0x248133)  4801c0                   add rax, rax
0x648136  (file 0x248136)  4531f6                   xor r14d, r14d
0x648139  (file 0x248139)  4929c7                   sub r15, rax
0x64813c  (file 0x24813c)  0f1f4000                 nop dword ptr [rax]
0x648140  (file 0x248140)  420fb73c73               movzx edi, word ptr [rbx + r14*2]
0x648145  (file 0x248145)  be09000000               mov esi, 9
0x64814a  (file 0x24814a)  e871e62207               call 0x78767c0  ; -> 0x78767c0 (file 0x74767c0)
0x64814f  (file 0x24814f)  85c0                     test eax, eax
0x648151  (file 0x248151)  7409                     je 0x64815c  ; -> 0x64815c (file 0x24815c)
0x648153  (file 0x248153)  49ffc6                   inc r14
0x648156  (file 0x248156)  4983c7fe                 add r15, -2
0x64815a  (file 0x24815a)  75e4                     jne 0x648140  ; -> 0x648140 (file 0x248140)
0x64815c  (file 0x24815c)  8b55b8                   mov edx, dword ptr [rbp - 0x48]
0x64815f  (file 0x24815f)  4489f0                   mov eax, r14d
0x648162  (file 0x248162)  4529f5                   sub r13d, r14d
0x648165  (file 0x248165)  488d1c43                 lea rbx, [rbx + rax*2]
0x648169  (file 0x248169)  7513                     jne 0x64817e  ; -> 0x64817e (file 0x24817e)
0x64816b  (file 0x24816b)  448b75bc                 mov r14d, dword ptr [rbp - 0x44]
0x64816f  (file 0x24816f)  4531ed                   xor r13d, r13d
0x648172  (file 0x248172)  448b4db4                 mov r9d, dword ptr [rbp - 0x4c]
0x648176  (file 0x248176)  41f6c102                 test r9b, 2
0x64817a  (file 0x24817a)  7554                     jne 0x6481d0  ; -> 0x6481d0 (file 0x2481d0)
0x64817c  (file 0x24817c)  eb57                     jmp 0x6481d5  ; -> 0x6481d5 (file 0x2481d5)
0x64817e  (file 0x24817e)  4d63fd                   movsxd r15, r13d
0x648181  (file 0x248181)  4c8b6da8                 mov r13, qword ptr [rbp - 0x58]
0x648185  (file 0x248185)  4501f5                   add r13d, r14d

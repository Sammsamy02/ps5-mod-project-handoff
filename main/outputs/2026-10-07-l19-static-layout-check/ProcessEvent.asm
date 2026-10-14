0x859830  (f 0x459830)  55                     push rbp
0x859831  (f 0x459831)  4889e5                 mov rbp, rsp
0x859834  (f 0x459834)  4157                   push r15
0x859836  (f 0x459836)  4156                   push r14
0x859838  (f 0x459838)  4155                   push r13
0x85983a  (f 0x45983a)  4154                   push r12
0x85983c  (f 0x45983c)  53                     push rbx
0x85983d  (f 0x45983d)  4883e4e0               and rsp, 0xffffffffffffffe0
0x859841  (f 0x459841)  4881ec20010000         sub rsp, 0x120
0x859848  (f 0x459848)  4889e3                 mov rbx, rsp
0x85984b  (f 0x45984b)  4c8b35b686cd08         mov r14, qword ptr [rip + 0x8cd86b6]  ; rip->0x9531f08 (file 0x9131f08)
0x859852  (f 0x459852)  498b06                 mov rax, qword ptr [r14]
0x859855  (f 0x459855)  48898300010000         mov qword ptr [rbx + 0x100], rax
0x85985c  (f 0x45985c)  8b4708                 mov eax, dword ptr [rdi + 8]
0x85985f  (f 0x45985f)  a900000040             test eax, 0x40000000
0x859864  (f 0x459864)  0f8552030000           jne 0x859bbc
0x85986a  (f 0x45986a)  f686b100000004         test byte ptr [rsi + 0xb1], 4
0x859871  (f 0x459871)  4989d4                 mov r12, rdx
0x859874  (f 0x459874)  750b                   jne 0x859881
0x859876  (f 0x459876)  837e6800               cmp dword ptr [rsi + 0x68], 0
0x85987a  (f 0x45987a)  7550                   jne 0x8598cc
0x85987c  (f 0x45987c)  e93b030000             jmp 0x859bbc
0x859881  (f 0x459881)  488b07                 mov rax, qword ptr [rdi]
0x859884  (f 0x459884)  4c896318               mov qword ptr [rbx + 0x18], r12
0x859888  (f 0x459888)  31d2                   xor edx, edx
0x85988a  (f 0x45988a)  4989fc                 mov r12, rdi
0x85988d  (f 0x45988d)  4989f7                 mov r15, rsi
0x859890  (f 0x459890)  ff9070020000           call qword ptr [rax + 0x270]
0x859896  (f 0x459896)  4189c5                 mov r13d, eax
0x859899  (f 0x459899)  41f6c501               test r13b, 1
0x85989d  (f 0x45989d)  7419                   je 0x8598b8
0x85989f  (f 0x45989f)  498b0424               mov rax, qword ptr [r12]
0x8598a3  (f 0x4598a3)  488b5318               mov rdx, qword ptr [rbx + 0x18]
0x8598a7  (f 0x4598a7)  4c89e7                 mov rdi, r12
0x8598aa  (f 0x4598aa)  4c89fe                 mov rsi, r15
0x8598ad  (f 0x4598ad)  31c9                   xor ecx, ecx
0x8598af  (f 0x4598af)  4531c0                 xor r8d, r8d
0x8598b2  (f 0x4598b2)  ff9078020000           call qword ptr [rax + 0x278]
0x8598b8  (f 0x4598b8)  4c89e7                 mov rdi, r12
0x8598bb  (f 0x4598bb)  4c8b6318               mov r12, qword ptr [rbx + 0x18]
0x8598bf  (f 0x4598bf)  4c89fe                 mov rsi, r15
0x8598c2  (f 0x4598c2)  41f6c502               test r13b, 2
0x8598c6  (f 0x4598c6)  0f84f0020000           je 0x859bbc
0x8598cc  (f 0x4598cc)  4c8bbec8000000         mov r15, qword ptr [rsi + 0xc8]
0x8598d3  (f 0x4598d3)  4d85ff                 test r15, r15
0x8598d6  (f 0x4598d6)  7420                   je 0x8598f8
0x8598d8  (f 0x4598d8)  8b86d0000000           mov eax, dword ptr [rsi + 0xd0]
0x8598de  (f 0x4598de)  4c8d633c               lea r12, [rbx + 0x3c]
0x8598e2  (f 0x4598e2)  89433c                 mov dword ptr [rbx + 0x3c], eax
0x8598e5  (f 0x4598e5)  41f687b100000080       test byte ptr [r15 + 0xb1], 0x80
0x8598ed  (f 0x4598ed)  48897b28               mov qword ptr [rbx + 0x28], rdi
0x8598f1  (f 0x4598f1)  7516                   jne 0x859909
0x8598f3  (f 0x4598f3)  4531f6                 xor r14d, r14d
0x8598f6  (f 0x4598f6)  eb27                   jmp 0x85991f
0x8598f8  (f 0x4598f8)  4989f7                 mov r15, rsi
0x8598fb  (f 0x4598fb)  41f687b100000080       test byte ptr [r15 + 0xb1], 0x80
0x859903  (f 0x459903)  48897b28               mov qword ptr [rbx + 0x28], rdi
0x859907  (f 0x459907)  74ea                   je 0x8598f3
0x859909  (f 0x459909)  4889fe                 mov rsi, rdi
0x85990c  (f 0x45990c)  498b7f20               mov rdi, qword ptr [r15 + 0x20]
0x859910  (f 0x459910)  4c89fa                 mov rdx, r15
0x859913  (f 0x459913)  488b07                 mov rax, qword ptr [rdi]
0x859916  (f 0x459916)  ff90a0030000           call qword ptr [rax + 0x3a0]
0x85991c  (f 0x45991c)  4989c6                 mov r14, rax
0x85991f  (f 0x45991f)  66666664488b042500000000 mov rax, qword ptr fs:[0]
0x85992b  (f 0x45992b)  80b830f9ffff00         cmp byte ptr [rax - 0x6d0], 0
0x859932  (f 0x459932)  48894320               mov qword ptr [rbx + 0x20], rax
0x859936  (f 0x459936)  0f84b1030000           je 0x859ced
0x85993c  (f 0x45993c)  4d89f5                 mov r13, r14
0x85993f  (f 0x45993f)  4d85f6                 test r14, r14
0x859942  (f 0x459942)  0f858f000000           jne 0x8599d7
0x859948  (f 0x459948)  49635758               movsxd rdx, dword ptr [r15 + 0x58]
0x85994c  (f 0x45994c)  4885d2                 test rdx, rdx
0x85994f  (f 0x45994f)  742d                   je 0x85997e
0x859951  (f 0x459951)  410fbf475c             movsx eax, word ptr [r15 + 0x5c]
0x859956  (f 0x459956)  83f810                 cmp eax, 0x10
0x859959  (f 0x459959)  7f34                   jg 0x85998f
0x85995b  (f 0x45995b)  488d421e               lea rax, [rdx + 0x1e]
0x85995f  (f 0x45995f)  4989e5                 mov r13, rsp
0x859962  (f 0x459962)  4883e0f0               and rax, 0xfffffffffffffff0
0x859966  (f 0x459966)  4929c5                 sub r13, rax
0x859969  (f 0x459969)  4983e5e0               and r13, 0xffffffffffffffe0
0x85996d  (f 0x45996d)  4c89ec                 mov rsp, r13
0x859970  (f 0x459970)  410fb787b6000000       movzx eax, word ptr [r15 + 0xb6]
0x859978  (f 0x459978)  39c2                   cmp edx, eax
0x85997a  (f 0x45997a)  754c                   jne 0x8599c8
0x85997c  (f 0x45997c)  eb59                   jmp 0x8599d7
0x85997e  (f 0x45997e)  4531ed                 xor r13d, r13d
0x859981  (f 0x459981)  410fb787b6000000       movzx eax, word ptr [r15 + 0xb6]
0x859989  (f 0x459989)  39c2                   cmp edx, eax
0x85998b  (f 0x45998b)  753b                   jne 0x8599c8
0x85998d  (f 0x45998d)  eb48                   jmp 0x8599d7
0x85998f  (f 0x45998f)  8d4c02ff               lea ecx, [rdx + rax - 1]
0x859993  (f 0x459993)  4889e6                 mov rsi, rsp
0x859996  (f 0x459996)  440fb7e8               movzx r13d, ax
0x85999a  (f 0x45999a)  4863c9                 movsxd rcx, ecx
0x85999d  (f 0x45999d)  4883c10f               add rcx, 0xf
0x8599a1  (f 0x4599a1)  4883e1f0               and rcx, 0xfffffffffffffff0
0x8599a5  (f 0x4599a5)  4829ce                 sub rsi, rcx
0x8599a8  (f 0x4599a8)  4883e6e0               and rsi, 0xffffffffffffffe0
0x8599ac  (f 0x4599ac)  4889f4                 mov rsp, rsi
0x8599af  (f 0x4599af)  4898                   cdqe 
0x8599b1  (f 0x4599b1)  49f7dd                 neg r13
0x8599b4  (f 0x4599b4)  488d4430ff             lea rax, [rax + rsi - 1]
0x8599b9  (f 0x4599b9)  4921c5                 and r13, rax
0x8599bc  (f 0x4599bc)  410fb787b6000000       movzx eax, word ptr [r15 + 0xb6]
0x8599c4  (f 0x4599c4)  39c2                   cmp edx, eax
0x8599c6  (f 0x4599c6)  740f                   je 0x8599d7
0x8599c8  (f 0x4599c8)  89c7                   mov edi, eax
0x8599ca  (f 0x4599ca)  4829c2                 sub rdx, rax
0x8599cd  (f 0x4599cd)  31f6                   xor esi, esi
0x8599cf  (f 0x4599cf)  4c01ef                 add rdi, r13
0x8599d2  (f 0x4599d2)  e869840107             call 0x7871e40
0x8599d7  (f 0x4599d7)  410fb797b6000000       movzx edx, word ptr [r15 + 0xb6]
0x8599df  (f 0x4599df)  4c897330               mov qword ptr [rbx + 0x30], r14
0x8599e3  (f 0x4599e3)  4885d2                 test rdx, rdx
0x8599e6  (f 0x4599e6)  740b                   je 0x8599f3
0x8599e8  (f 0x4599e8)  4c89ef                 mov rdi, r13
0x8599eb  (f 0x4599eb)  4c89e6                 mov rsi, r12
0x8599ee  (f 0x4599ee)  e85d840107             call 0x7871e50
0x8599f3  (f 0x4599f3)  4c896318               mov qword ptr [rbx + 0x18], r12
0x8599f7  (f 0x4599f7)  4d8b6750               mov r12, qword ptr [r15 + 0x50]
0x8599fb  (f 0x4599fb)  4c8d7358               lea r14, [rbx + 0x58]
0x8599ff  (f 0x4599ff)  4c89f7                 mov rdi, r14
0x859a02  (f 0x459a02)  e879acd7ff             call 0x5d4680
0x859a07  (f 0x459a07)  488b5328               mov rdx, qword ptr [rbx + 0x28]
0x859a0b  (f 0x459a0b)  488d055e55aa07         lea rax, [rip + 0x7aa555e]  ; rip->0x82fef70 (file 0x7efef70)
0x859a12  (f 0x459a12)  48b90000000008000000   movabs rcx, 0x800000000
0x859a1c  (f 0x459a1c)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x859a20  (f 0x459a20)  48894358               mov qword ptr [rbx + 0x58], rax
0x859a24  (f 0x459a24)  4c897b68               mov qword ptr [rbx + 0x68], r15
0x859a28  (f 0x459a28)  48895370               mov qword ptr [rbx + 0x70], rdx
0x859a2c  (f 0x459a2c)  498b4760               mov rax, qword ptr [r15 + 0x60]
0x859a30  (f 0x459a30)  48894378               mov qword ptr [rbx + 0x78], rax
0x859a34  (f 0x459a34)  488b4320               mov rax, qword ptr [rbx + 0x20]
0x859a38  (f 0x459a38)  4c89ab80000000         mov qword ptr [rbx + 0x80], r13
0x859a3f  (f 0x459a3f)  48c783c000000000000000 mov qword ptr [rbx + 0xc0], 0
0x859a4a  (f 0x459a4a)  48898bc8000000         mov qword ptr [rbx + 0xc8], rcx
0x859a51  (f 0x459a51)  c5f8118388000000       vmovups xmmword ptr [rbx + 0x88], xmm0
0x859a59  (f 0x459a59)  48c7839800000000000000 mov qword ptr [rbx + 0x98], 0
0x859a64  (f 0x459a64)  c5f81183d0000000       vmovups xmmword ptr [rbx + 0xd0], xmm0
0x859a6c  (f 0x459a6c)  4c89a3e0000000         mov qword ptr [rbx + 0xe0], r12
0x859a73  (f 0x459a73)  48c783e800000000000000 mov qword ptr [rbx + 0xe8], 0
0x859a7e  (f 0x459a7e)  66c783f80000000000     mov word ptr [rbx + 0xf8], 0
0x859a87  (f 0x459a87)  488b8838f9ffff         mov rcx, qword ptr [rax - 0x6c8]
0x859a8e  (f 0x459a8e)  4c89b038f9ffff         mov qword ptr [rax - 0x6c8], r14
0x859a95  (f 0x459a95)  48898bf0000000         mov qword ptr [rbx + 0xf0], rcx
0x859a9c  (f 0x459a9c)  4885c9                 test rcx, rcx
0x859a9f  (f 0x459a9f)  740d                   je 0x859aae
0x859aa1  (f 0x459aa1)  0fb689a1000000         movzx ecx, byte ptr [rcx + 0xa1]
0x859aa8  (f 0x459aa8)  888bf9000000           mov byte ptr [rbx + 0xf9], cl
0x859aae  (f 0x459aae)  41f687b200000040       test byte ptr [r15 + 0xb2], 0x40
0x859ab6  (f 0x459ab6)  4c8b6318               mov r12, qword ptr [rbx + 0x18]
0x859aba  (f 0x459aba)  747f                   je 0x859b3b
0x859abc  (f 0x459abc)  498b4f50               mov rcx, qword ptr [r15 + 0x50]
0x859ac0  (f 0x459ac0)  4885c9                 test rcx, rcx
0x859ac3  (f 0x459ac3)  7476                   je 0x859b3b
0x859ac5  (f 0x459ac5)  488d93d8000000         lea rdx, [rbx + 0xd8]
0x859acc  (f 0x459acc)  eb1b                   jmp 0x859ae9
0x859ace  (f 0x459ace)  488932                 mov qword ptr [rdx], rsi
0x859ad1  (f 0x459ad1)  6666666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x859ae0  (f 0x459ae0)  488b4918               mov rcx, qword ptr [rcx + 0x18]
0x859ae4  (f 0x459ae4)  4885c9                 test rcx, rcx
0x859ae7  (f 0x459ae7)  7442                   je 0x859b2b
0x859ae9  (f 0x459ae9)  488b7138               mov rsi, qword ptr [rcx + 0x38]
0x859aed  (f 0x459aed)  4084f6                 test sil, sil
0x859af0  (f 0x459af0)  7939                   jns 0x859b2b
0x859af2  (f 0x459af2)  f7c600010000           test esi, 0x100
0x859af8  (f 0x459af8)  74e6                   je 0x859ae0
0x859afa  (f 0x459afa)  4889e6                 mov rsi, rsp
0x859afd  (f 0x459afd)  4883c6d0               add rsi, -0x30
0x859b01  (f 0x459b01)  4883e6e0               and rsi, 0xffffffffffffffe0
0x859b05  (f 0x459b05)  4889f4                 mov rsp, rsi
0x859b08  (f 0x459b08)  48637944               movsxd rdi, dword ptr [rcx + 0x44]
0x859b0c  (f 0x459b0c)  4c01e7                 add rdi, r12
0x859b0f  (f 0x459b0f)  48897e08               mov qword ptr [rsi + 8], rdi
0x859b13  (f 0x459b13)  48890e                 mov qword ptr [rsi], rcx
0x859b16  (f 0x459b16)  488b3a                 mov rdi, qword ptr [rdx]
0x859b19  (f 0x459b19)  4885ff                 test rdi, rdi
0x859b1c  (f 0x459b1c)  74b0                   je 0x859ace
0x859b1e  (f 0x459b1e)  48897710               mov qword ptr [rdi + 0x10], rsi
0x859b22  (f 0x459b22)  488b12                 mov rdx, qword ptr [rdx]
0x859b25  (f 0x459b25)  4883c210               add rdx, 0x10
0x859b29  (f 0x459b29)  ebb5                   jmp 0x859ae0
0x859b2b  (f 0x459b2b)  488b0a                 mov rcx, qword ptr [rdx]
0x859b2e  (f 0x459b2e)  4885c9                 test rcx, rcx
0x859b31  (f 0x459b31)  7408                   je 0x859b3b
0x859b33  (f 0x459b33)  48c7411000000000       mov qword ptr [rcx + 0x10], 0
0x859b3b  (f 0x459b3b)  48837b3000             cmp qword ptr [rbx + 0x30], 0
0x859b40  (f 0x459b40)  4c8db038f9ffff         lea r14, [rax - 0x6c8]
0x859b47  (f 0x459b47)  0f848e000000           je 0x859bdb
0x859b4d  (f 0x459b4d)  410fb787b8000000       movzx eax, word ptr [r15 + 0xb8]
0x859b55  (f 0x459b55)  488b7328               mov rsi, qword ptr [rbx + 0x28]
0x859b59  (f 0x459b59)  31c9                   xor ecx, ecx
0x859b5b  (f 0x459b5b)  488d5358               lea rdx, [rbx + 0x58]
0x859b5f  (f 0x459b5f)  4c89ff                 mov rdi, r15
0x859b62  (f 0x459b62)  4901c4                 add r12, rax
0x859b65  (f 0x459b65)  483dffff0000           cmp rax, 0xffff
0x859b6b  (f 0x459b6b)  490f45cc               cmovne rcx, r12
0x859b6f  (f 0x459b6f)  e84ccfecff             call 0x726ac0
0x859b74  (f 0x459b74)  488d05f553aa07         lea rax, [rip + 0x7aa53f5]  ; rip->0x82fef70 (file 0x7efef70)
0x859b7b  (f 0x459b7b)  48894358               mov qword ptr [rbx + 0x58], rax
0x859b7f  (f 0x459b7f)  488b83f0000000         mov rax, qword ptr [rbx + 0xf0]
0x859b86  (f 0x459b86)  498906                 mov qword ptr [r14], rax
0x859b89  (f 0x459b89)  4885c0                 test rax, rax
0x859b8c  (f 0x459b8c)  740d                   je 0x859b9b
0x859b8e  (f 0x459b8e)  0fb68bf9000000         movzx ecx, byte ptr [rbx + 0xf9]
0x859b95  (f 0x459b95)  0888a1000000           or byte ptr [rax + 0xa1], cl
0x859b9b  (f 0x459b9b)  488bbbc0000000         mov rdi, qword ptr [rbx + 0xc0]
0x859ba2  (f 0x459ba2)  4c8b355f83cd08         mov r14, qword ptr [rip + 0x8cd835f]  ; rip->0x9531f08 (file 0x9131f08)
0x859ba9  (f 0x459ba9)  4885ff                 test rdi, rdi
0x859bac  (f 0x459bac)  7405                   je 0x859bb3
0x859bae  (f 0x459bae)  e8dddec5ff             call 0x4b7a90
0x859bb3  (f 0x459bb3)  488d7b58               lea rdi, [rbx + 0x58]
0x859bb7  (f 0x459bb7)  e8d416bbff             call 0x40b290
0x859bbc  (f 0x459bbc)  498b06                 mov rax, qword ptr [r14]
0x859bbf  (f 0x459bbf)  483b8300010000         cmp rax, qword ptr [rbx + 0x100]
0x859bc6  (f 0x459bc6)  0f856a010000           jne 0x859d36
0x859bcc  (f 0x459bcc)  488d65d8               lea rsp, [rbp - 0x28]
0x859bd0  (f 0x459bd0)  5b                     pop rbx
0x859bd1  (f 0x459bd1)  415c                   pop r12
0x859bd3  (f 0x459bd3)  415d                   pop r13
0x859bd5  (f 0x459bd5)  415e                   pop r14
0x859bd7  (f 0x459bd7)  415f                   pop r15
0x859bd9  (f 0x459bd9)  5d                     pop rbp
0x859bda  (f 0x459bda)  c3                     ret 
0x859bdb  (f 0x459bdb)  4d8bafc0000000         mov r13, qword ptr [r15 + 0xc0]
0x859be2  (f 0x459be2)  4d85ed                 test r13, r13
0x859be5  (f 0x459be5)  7528                   jne 0x859c0f
0x859be7  (f 0x459be7)  eb4e                   jmp 0x859c37
0x859bf0  (f 0x459bf0)  49634530               movsxd rax, dword ptr [r13 + 0x30]
0x859bf4  (f 0x459bf4)  4963552c               movsxd rdx, dword ptr [r13 + 0x2c]
0x859bf8  (f 0x459bf8)  4889f7                 mov rdi, rsi
0x859bfb  (f 0x459bfb)  31f6                   xor esi, esi
0x859bfd  (f 0x459bfd)  480fafd0               imul rdx, rax
0x859c01  (f 0x459c01)  e83a820107             call 0x7871e40
0x859c06  (f 0x459c06)  4d8b6d60               mov r13, qword ptr [r13 + 0x60]
0x859c0a  (f 0x459c0a)  4d85ed                 test r13, r13
0x859c0d  (f 0x459c0d)  7428                   je 0x859c37
0x859c0f  (f 0x459c0f)  49637544               movsxd rsi, dword ptr [r13 + 0x44]
0x859c13  (f 0x459c13)  4803b380000000         add rsi, qword ptr [rbx + 0x80]
0x859c1a  (f 0x459c1a)  41f6453902             test byte ptr [r13 + 0x39], 2
0x859c1f  (f 0x459c1f)  75cf                   jne 0x859bf0
0x859c21  (f 0x459c21)  498b4500               mov rax, qword ptr [r13]
0x859c25  (f 0x459c25)  4c89ef                 mov rdi, r13
0x859c28  (f 0x459c28)  ff9058010000           call qword ptr [rax + 0x158]
0x859c2e  (f 0x459c2e)  4d8b6d60               mov r13, qword ptr [r13 + 0x60]
0x859c32  (f 0x459c32)  4d85ed                 test r13, r13
0x859c35  (f 0x459c35)  75d8                   jne 0x859c0f
0x859c37  (f 0x459c37)  410fb787b8000000       movzx eax, word ptr [r15 + 0xb8]
0x859c3f  (f 0x459c3f)  488b7328               mov rsi, qword ptr [rbx + 0x28]
0x859c43  (f 0x459c43)  31c9                   xor ecx, ecx
0x859c45  (f 0x459c45)  4c89ff                 mov rdi, r15
0x859c48  (f 0x459c48)  498d1404               lea rdx, [r12 + rax]
0x859c4c  (f 0x459c4c)  483dffff0000           cmp rax, 0xffff
0x859c52  (f 0x459c52)  480f45ca               cmovne rcx, rdx
0x859c56  (f 0x459c56)  488d5358               lea rdx, [rbx + 0x58]
0x859c5a  (f 0x459c5a)  e861ceecff             call 0x726ac0
0x859c5f  (f 0x459c5f)  4d8ba780000000         mov r12, qword ptr [r15 + 0x80]
0x859c66  (f 0x459c66)  4d85e4                 test r12, r12
0x859c69  (f 0x459c69)  7513                   jne 0x859c7e
0x859c6b  (f 0x459c6b)  e904ffffff             jmp 0x859b74
0x859c70  (f 0x459c70)  4d8b642458             mov r12, qword ptr [r12 + 0x58]
0x859c75  (f 0x459c75)  4d85e4                 test r12, r12
0x859c78  (f 0x459c78)  0f84f6feffff           je 0x859b74
0x859c7e  (f 0x459c7e)  496344242c             movsxd rax, dword ptr [r12 + 0x2c]
0x859c83  (f 0x459c83)  4963542430             movsxd rdx, dword ptr [r12 + 0x30]
0x859c88  (f 0x459c88)  4963742444             movsxd rsi, dword ptr [r12 + 0x44]
0x859c8d  (f 0x459c8d)  410fb78fb6000000       movzx ecx, word ptr [r15 + 0xb6]
0x859c95  (f 0x459c95)  480fafd0               imul rdx, rax
0x859c99  (f 0x459c99)  498b442438             mov rax, qword ptr [r12 + 0x38]
0x859c9e  (f 0x459c9e)  8d3c32                 lea edi, [rdx + rsi]
0x859ca1  (f 0x459ca1)  39cf                   cmp edi, ecx
0x859ca3  (f 0x459ca3)  7e2b                   jle 0x859cd0
0x859ca5  (f 0x459ca5)  480fbae024             bt rax, 0x24
0x859caa  (f 0x459caa)  72c4                   jb 0x859c70
0x859cac  (f 0x459cac)  4803b380000000         add rsi, qword ptr [rbx + 0x80]
0x859cb3  (f 0x459cb3)  498b0424               mov rax, qword ptr [r12]
0x859cb7  (f 0x459cb7)  4c89e7                 mov rdi, r12
0x859cba  (f 0x459cba)  ff9040010000           call qword ptr [rax + 0x140]
0x859cc0  (f 0x459cc0)  4d8b642458             mov r12, qword ptr [r12 + 0x58]
0x859cc5  (f 0x459cc5)  4d85e4                 test r12, r12
0x859cc8  (f 0x459cc8)  75b4                   jne 0x859c7e
0x859cca  (f 0x459cca)  e9a5feffff             jmp 0x859b74
0x859cd0  (f 0x459cd0)  a900010000             test eax, 0x100
0x859cd5  (f 0x459cd5)  7599                   jne 0x859c70
0x859cd7  (f 0x459cd7)  488b4318               mov rax, qword ptr [rbx + 0x18]
0x859cdb  (f 0x459cdb)  488d3c30               lea rdi, [rax + rsi]
0x859cdf  (f 0x459cdf)  4803b380000000         add rsi, qword ptr [rbx + 0x80]
0x859ce6  (f 0x459ce6)  e865810107             call 0x7871e50
0x859ceb  (f 0x459ceb)  eb83                   jmp 0x859c70
0x859ced  (f 0x459ced)  488b4320               mov rax, qword ptr [rbx + 0x20]
0x859cf1  (f 0x459cf1)  31f6                   xor esi, esi
0x859cf3  (f 0x459cf3)  31d2                   xor edx, edx
0x859cf5  (f 0x459cf5)  4c8da8f0f8ffff         lea r13, [rax - 0x710]
0x859cfc  (f 0x459cfc)  4c89ef                 mov rdi, r13
0x859cff  (f 0x459cff)  e8cca5d1ff             call 0x5742d0
0x859d04  (f 0x459d04)  488d3d85d5ffff         lea rdi, [rip - 0x2a7b]  ; rip->0x857290 (file 0x457290)
0x859d0b  (f 0x459d0b)  488d15ee22e808         lea rdx, [rip + 0x8e822ee]  ; rip->0x96dc000 (file 0x92dc000)
0x859d12  (f 0x459d12)  4c89ee                 mov rsi, r13
0x859d15  (f 0x459d15)  e806880107             call 0x7872520
0x859d1a  (f 0x459d1a)  488b4320               mov rax, qword ptr [rbx + 0x20]
0x859d1e  (f 0x459d1e)  c68030f9ffff01         mov byte ptr [rax - 0x6d0], 1
0x859d25  (f 0x459d25)  4d89f5                 mov r13, r14
0x859d28  (f 0x459d28)  4d85f6                 test r14, r14
0x859d2b  (f 0x459d2b)  0f85a6fcffff           jne 0x8599d7
0x859d31  (f 0x459d31)  e912fcffff             jmp 0x859948
0x859d36  (f 0x459d36)  e825810107             call 0x7871e60
0x859d3b  (f 0x459d3b)  0f0b                   ud2 
--- calls: [('0xc59d36', '0x7871e60'), ('0xc599ee', '0x7871e50'), ('0xc59a02', '0x5d4680'), ('0xc59b6f', '0x726ac0'), ('0xc59bae', '0x4b7a90'), ('0xc59bb7', '0x40b290'), ('0xc59c5a', '0x726ac0'), ('0xc59ce6', '0x7871e50'), ('0xc59c01', '0x7871e40'), ('0xc599d2', '0x7871e40'), ('0xc59cff', '0x5742d0'), ('0xc59d15', '0x7872520')]

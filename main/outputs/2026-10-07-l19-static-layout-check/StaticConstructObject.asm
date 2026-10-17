0x890aa0  (f 0x490aa0)  55                     push rbp
0x890aa1  (f 0x490aa1)  4889e5                 mov rbp, rsp
0x890aa4  (f 0x490aa4)  4157                   push r15
0x890aa6  (f 0x490aa6)  4156                   push r14
0x890aa8  (f 0x490aa8)  4155                   push r13
0x890aaa  (f 0x490aaa)  4154                   push r12
0x890aac  (f 0x490aac)  53                     push rbx
0x890aad  (f 0x490aad)  4881ec58020000         sub rsp, 0x258
0x890ab4  (f 0x490ab4)  488b054d14ca08         mov rax, qword ptr [rip + 0x8ca144d]  ; rip->0x9531f08 (file 0x9131f08)
0x890abb  (f 0x490abb)  4889fb                 mov rbx, rdi
0x890abe  (f 0x490abe)  488b00                 mov rax, qword ptr [rax]
0x890ac1  (f 0x490ac1)  488945d0               mov qword ptr [rbp - 0x30], rax
0x890ac5  (f 0x490ac5)  4c8b37                 mov r14, qword ptr [rdi]
0x890ac8  (f 0x490ac8)  488b7708               mov rsi, qword ptr [rdi + 8]
0x890acc  (f 0x490acc)  8b4f18                 mov ecx, dword ptr [rdi + 0x18]
0x890acf  (f 0x490acf)  448b6f70               mov r13d, dword ptr [rdi + 0x70]
0x890ad3  (f 0x490ad3)  41f786d400000080000010 test dword ptr [r14 + 0xd4], 0x10000080
0x890ade  (f 0x490ade)  0f84aa000000           je 0x890b8e
0x890ae4  (f 0x490ae4)  4c8b6328               mov r12, qword ptr [rbx + 0x28]
0x890ae8  (f 0x490ae8)  4d85e4                 test r12, r12
0x890aeb  (f 0x490aeb)  7450                   je 0x890b3d
0x890aed  (f 0x490aed)  488b5310               mov rdx, qword ptr [rbx + 0x10]
0x890af1  (f 0x490af1)  4885d2                 test rdx, rdx
0x890af4  (f 0x490af4)  0f8494000000           je 0x890b8e
0x890afa  (f 0x490afa)  807b2100               cmp byte ptr [rbx + 0x21], 0
0x890afe  (f 0x490afe)  753d                   jne 0x890b3d
0x890b00  (f 0x490b00)  4889b5a8fdffff         mov qword ptr [rbp - 0x258], rsi
0x890b07  (f 0x490b07)  898db4fdffff           mov dword ptr [rbp - 0x24c], ecx
0x890b0d  (f 0x490b0d)  4c89f7                 mov rdi, r14
0x890b10  (f 0x490b10)  4531ff                 xor r15d, r15d
0x890b13  (f 0x490b13)  4531c0                 xor r8d, r8d
0x890b16  (f 0x490b16)  4531c9                 xor r9d, r9d
0x890b19  (f 0x490b19)  488bb5a8fdffff         mov rsi, qword ptr [rbp - 0x258]
0x890b20  (f 0x490b20)  8b8db4fdffff           mov ecx, dword ptr [rbp - 0x24c]
0x890b26  (f 0x490b26)  e89504ffff             call 0x880fc0
0x890b2b  (f 0x490b2b)  488bb5a8fdffff         mov rsi, qword ptr [rbp - 0x258]
0x890b32  (f 0x490b32)  8b8db4fdffff           mov ecx, dword ptr [rbp - 0x24c]
0x890b38  (f 0x490b38)  4939c4                 cmp r12, rax
0x890b3b  (f 0x490b3b)  7554                   jne 0x890b91
0x890b3d  (f 0x490b3d)  41b701                 mov r15b, 1
0x890b40  (f 0x490b40)  f7c100000400           test ecx, 0x40000
0x890b46  (f 0x490b46)  7449                   je 0x890b91
0x890b48  (f 0x490b48)  488d85bcfdffff         lea rax, [rbp - 0x244]
0x890b4f  (f 0x490b4f)  488d153a65e6ff         lea rdx, [rip - 0x199ac6]  ; rip->0x6f7090 (file 0x2f7090)
0x890b56  (f 0x490b56)  4189cf                 mov r15d, ecx
0x890b59  (f 0x490b59)  4989f4                 mov r12, rsi
0x890b5c  (f 0x490b5c)  488985c8fdffff         mov qword ptr [rbp - 0x238], rax
0x890b63  (f 0x490b63)  488995c0fdffff         mov qword ptr [rbp - 0x240], rdx
0x890b6a  (f 0x490b6a)  e851460200             call 0x8b51c0
0x890b6f  (f 0x490b6f)  488dbdc0fdffff         lea rdi, [rbp - 0x240]
0x890b76  (f 0x490b76)  4889c6                 mov rsi, rax
0x890b79  (f 0x490b79)  e8e2dbc2ff             call 0x4be760
0x890b7e  (f 0x490b7e)  83782800               cmp dword ptr [rax + 0x28], 0
0x890b82  (f 0x490b82)  4489f9                 mov ecx, r15d
0x890b85  (f 0x490b85)  4c89e6                 mov rsi, r12
0x890b88  (f 0x490b88)  410f94c7               sete r15b
0x890b8c  (f 0x490b8c)  eb03                   jmp 0x890b91
0x890b8e  (f 0x490b8e)  4531ff                 xor r15d, r15d
0x890b91  (f 0x490b91)  c785bcfdffff00000000   mov dword ptr [rbp - 0x244], 0
0x890b9b  (f 0x490b9b)  c685bbfdffff00         mov byte ptr [rbp - 0x245], 0
0x890ba2  (f 0x490ba2)  488dbdbcfdffff         lea rdi, [rbp - 0x244]
0x890ba9  (f 0x490ba9)  450fb6cf               movzx r9d, r15b
0x890bad  (f 0x490bad)  488b5310               mov rdx, qword ptr [rbx + 0x10]
0x890bb1  (f 0x490bb1)  448b431c               mov r8d, dword ptr [rbx + 0x1c]
0x890bb5  (f 0x490bb5)  488b4338               mov rax, qword ptr [rbx + 0x38]
0x890bb9  (f 0x490bb9)  48897c2420             mov qword ptr [rsp + 0x20], rdi
0x890bbe  (f 0x490bbe)  488dbdbbfdffff         lea rdi, [rbp - 0x245]
0x890bc5  (f 0x490bc5)  44896c2410             mov dword ptr [rsp + 0x10], r13d
0x890bca  (f 0x490bca)  48893c24               mov qword ptr [rsp], rdi
0x890bce  (f 0x490bce)  4c89f7                 mov rdi, r14
0x890bd1  (f 0x490bd1)  4889442408             mov qword ptr [rsp + 8], rax
0x890bd6  (f 0x490bd6)  e8d5030000             call 0x890fb0
0x890bdb  (f 0x490bdb)  80bdbbfdffff00         cmp byte ptr [rbp - 0x245], 0
0x890be2  (f 0x490be2)  4989c7                 mov r15, rax
0x890be5  (f 0x490be5)  7436                   je 0x890c1d
0x890be7  (f 0x490be7)  83bdbcfdffff01         cmp dword ptr [rbp - 0x244], 1
0x890bee  (f 0x490bee)  7560                   jne 0x890c50
0x890bf0  (f 0x490bf0)  e89b74edff             call 0x768090
0x890bf5  (f 0x490bf5)  488b7828               mov rdi, qword ptr [rax + 0x28]
0x890bf9  (f 0x490bf9)  4889c3                 mov rbx, rax
0x890bfc  (f 0x490bfc)  488b07                 mov rax, qword ptr [rdi]
0x890bff  (f 0x490bff)  ff5010                 call qword ptr [rax + 0x10]
0x890c02  (f 0x490c02)  f0ff4b04               lock dec dword ptr [rbx + 4]
0x890c06  (f 0x490c06)  488b05fb12ca08         mov rax, qword ptr [rip + 0x8ca12fb]  ; rip->0x9531f08 (file 0x9131f08)
0x890c0d  (f 0x490c0d)  488b00                 mov rax, qword ptr [rax]
0x890c10  (f 0x490c10)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x890c14  (f 0x490c14)  744a                   je 0x890c60
0x890c16  (f 0x490c16)  e84512fe06             call 0x7871e60
0x890c1b  (f 0x490c1b)  0f0b                   ud2 
0x890c1d  (f 0x490c1d)  4d8ba6b0000000         mov r12, qword ptr [r14 + 0xb0]
0x890c24  (f 0x490c24)  4c8db5c0fdffff         lea r14, [rbp - 0x240]
0x890c2b  (f 0x490c2b)  4c89fe                 mov rsi, r15
0x890c2e  (f 0x490c2e)  4889da                 mov rdx, rbx
0x890c31  (f 0x490c31)  4c89f7                 mov rdi, r14
0x890c34  (f 0x490c34)  e877110000             call 0x891db0
0x890c39  (f 0x490c39)  4c89f7                 mov rdi, r14
0x890c3c  (f 0x490c3c)  41ffd4                 call r12
0x890c3f  (f 0x490c3f)  4c89f7                 mov rdi, r14
0x890c42  (f 0x490c42)  e899130000             call 0x891fe0
0x890c47  (f 0x490c47)  83bdbcfdffff01         cmp dword ptr [rbp - 0x244], 1
0x890c4e  (f 0x490c4e)  74a0                   je 0x890bf0
0x890c50  (f 0x490c50)  488b05b112ca08         mov rax, qword ptr [rip + 0x8ca12b1]  ; rip->0x9531f08 (file 0x9131f08)
0x890c57  (f 0x490c57)  488b00                 mov rax, qword ptr [rax]
0x890c5a  (f 0x490c5a)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x890c5e  (f 0x490c5e)  75b6                   jne 0x890c16
0x890c60  (f 0x490c60)  4c89f8                 mov rax, r15
0x890c63  (f 0x490c63)  4881c458020000         add rsp, 0x258
0x890c6a  (f 0x490c6a)  5b                     pop rbx
0x890c6b  (f 0x490c6b)  415c                   pop r12
0x890c6d  (f 0x490c6d)  415d                   pop r13
0x890c6f  (f 0x490c6f)  415e                   pop r14
0x890c71  (f 0x490c71)  415f                   pop r15
0x890c73  (f 0x490c73)  5d                     pop rbp
0x890c74  (f 0x490c74)  c3                     ret 
--- calls: [('0xc90b26', '0x880fc0'), ('0xc90b6a', '0x8b51c0'), ('0xc90b79', '0x4be760'), ('0xc90bd6', '0x890fb0'), ('0xc90bf0', '0x768090'), ('0xc90c16', '0x7871e60'), ('0xc90c34', '0x891db0'), ('0xc90c42', '0x891fe0')]

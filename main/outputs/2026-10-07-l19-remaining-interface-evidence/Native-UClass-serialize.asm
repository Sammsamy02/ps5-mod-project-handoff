0x323b00 f0x327b00 55                             push rbp
0x323b01 f0x327b01 4889e5                         mov rbp, rsp
0x323b04 f0x327b04 4157                           push r15
0x323b06 f0x327b06 4156                           push r14
0x323b08 f0x327b08 4155                           push r13
0x323b0a f0x327b0a 4154                           push r12
0x323b0c f0x327b0c 53                             push rbx
0x323b0d f0x327b0d 4881ecb8010000                 sub rsp, 0x1b8
0x323b14 f0x327b14 4c8b25ede3e008                 mov r12, qword ptr [rip + 0x8e0e3ed]
0x323b1b f0x327b1b 4989f6                         mov r14, rsi
0x323b1e f0x327b1e 4889fb                         mov rbx, rdi
0x323b21 f0x327b21 498b0424                       mov rax, qword ptr [r12]
0x323b25 f0x327b25 488945d0                       mov qword ptr [rbp - 0x30], rax
0x323b29 f0x327b29 488b4628                       mov rax, qword ptr [rsi + 0x28]
0x323b2d f0x327b2d a801                           test al, 1
0x323b2f f0x327b2f 0f94c1                         sete cl
0x323b32 f0x327b32 2504000002                     and eax, 0x2000004
0x323b37 f0x327b37 3d00000002                     cmp eax, 0x2000000
0x323b3c f0x327b3c 0f95c0                         setne al
0x323b3f f0x327b3f 84c1                           test cl, al
0x323b41 f0x327b41 7508                           jne 0x323b4b
0x323b43 f0x327b43 4889df                         mov rdi, rbx
0x323b46 f0x327b46 e8e5ec1800                     call 0x4b2830
0x323b4b f0x327b4b 4889df                         mov rdi, rbx
0x323b4e f0x327b4e 4c89f6                         mov rsi, r14
0x323b51 f0x327b51 e89a90ffff                     call 0x31cbf0
0x323b56 f0x327b56 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x323b5a f0x327b5a a801                           test al, 1
0x323b5c f0x327b5c 89c2                           mov edx, eax
0x323b5e f0x327b5e 0f94c1                         sete cl
0x323b61 f0x327b61 81e204000002                   and edx, 0x2000004
0x323b67 f0x327b67 81fa00000002                   cmp edx, 0x2000000
0x323b6d f0x327b6d 0f95c2                         setne dl
0x323b70 f0x327b70 84d1                           test cl, dl
0x323b72 f0x327b72 750c                           jne 0x323b80
0x323b74 f0x327b74 4889df                         mov rdi, rbx
0x323b77 f0x327b77 e864e51800                     call 0x4b20e0
0x323b7c f0x327b7c 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x323b80 f0x327b80 480d00200000                   or rax, 0x2000
0x323b86 f0x327b86 b101                           mov cl, 1
0x323b88 f0x327b88 4c8dbb78010000                 lea r15, [rbx + 0x178]
0x323b8f f0x327b8f 49894628                       mov qword ptr [r14 + 0x28], rax
0x323b93 f0x327b93 31c0                           xor eax, eax
0x323b95 f0x327b95 f00fb08b78010000               lock cmpxchg byte ptr [rbx + 0x178], cl
0x323b9d f0x327b9d 0f853c010000                   jne 0x323cdf
0x323ba3 f0x327ba3 488db328010000                 lea rsi, [rbx + 0x128]
0x323baa f0x327baa 4c89f7                         mov rdi, r14
0x323bad f0x327bad e89ee50000                     call 0x332150
0x323bb2 f0x327bb2 b0ff                           mov al, 0xff
0x323bb4 f0x327bb4 f00fc08378010000               lock xadd byte ptr [rbx + 0x178], al
0x323bbc f0x327bbc a802                           test al, 2
0x323bbe f0x327bbe 0f8528010000                   jne 0x323cec
0x323bc4 f0x327bc4 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x323bc8 f0x327bc8 a804                           test al, 4
0x323bca f0x327bca 0f8530010000                   jne 0x323d00
0x323bd0 f0x327bd0 498b4e08                       mov rcx, qword ptr [r14 + 8]
0x323bd4 f0x327bd4 4c8dbbd4000000                 lea r15, [rbx + 0xd4]
0x323bdb f0x327bdb 488b31                         mov rsi, qword ptr [rcx]
0x323bde f0x327bde 488b7908                       mov rdi, qword ptr [rcx + 8]
0x323be2 f0x327be2 488d5604                       lea rdx, [rsi + 4]
0x323be6 f0x327be6 a801                           test al, 1
0x323be8 f0x327be8 7537                           jne 0x323c21
0x323bea f0x327bea 4839fa                         cmp rdx, rdi
0x323bed f0x327bed 7670                           jbe 0x323c5f
0x323bef f0x327bef a900000100                     test eax, 0x10000
0x323bf4 f0x327bf4 0f85a2000000                   jne 0x323c9c
0x323bfa f0x327bfa 498b06                         mov rax, qword ptr [r14]
0x323bfd f0x327bfd ba04000000                     mov edx, 4
0x323c02 f0x327c02 4c89f7                         mov rdi, r14
0x323c05 f0x327c05 4c89fe                         mov rsi, r15
0x323c08 f0x327c08 ff9088010000                   call qword ptr [rax + 0x188]
0x323c0e f0x327c0e 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323c16 f0x327c16 0f8e59010000                   jle 0x323d75
0x323c1c f0x327c1c e980010000                     jmp 0x323da1
0x323c21 f0x327c21 4839fa                         cmp rdx, rdi
0x323c24 f0x327c24 7654                           jbe 0x323c7a
0x323c26 f0x327c26 a900000100                     test eax, 0x10000
0x323c2b f0x327c2b 0f8589000000                   jne 0x323cba
0x323c31 f0x327c31 498b06                         mov rax, qword ptr [r14]
0x323c34 f0x327c34 ba04000000                     mov edx, 4
0x323c39 f0x327c39 4c89f7                         mov rdi, r14
0x323c3c f0x327c3c 4c89fe                         mov rsi, r15
0x323c3f f0x327c3f ff9088010000                   call qword ptr [rax + 0x188]
0x323c45 f0x327c45 4181276ff7bfef                 and dword ptr [r15], 0xefbff76f
0x323c4c f0x327c4c 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323c54 f0x327c54 0f8e1b010000                   jle 0x323d75
0x323c5a f0x327c5a e942010000                     jmp 0x323da1
0x323c5f f0x327c5f 8b06                           mov eax, dword ptr [rsi]
0x323c61 f0x327c61 418907                         mov dword ptr [r15], eax
0x323c64 f0x327c64 488911                         mov qword ptr [rcx], rdx
0x323c67 f0x327c67 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323c6f f0x327c6f 0f8e00010000                   jle 0x323d75
0x323c75 f0x327c75 e927010000                     jmp 0x323da1
0x323c7a f0x327c7a 8b06                           mov eax, dword ptr [rsi]
0x323c7c f0x327c7c 418907                         mov dword ptr [r15], eax
0x323c7f f0x327c7f 488911                         mov qword ptr [rcx], rdx
0x323c82 f0x327c82 4181276ff7bfef                 and dword ptr [r15], 0xefbff76f
0x323c89 f0x327c89 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323c91 f0x327c91 0f8ede000000                   jle 0x323d75
0x323c97 f0x327c97 e905010000                     jmp 0x323da1
0x323c9c f0x327c9c 4c89f7                         mov rdi, r14
0x323c9f f0x327c9f 4c89fe                         mov rsi, r15
0x323ca2 f0x327ca2 e8098eefff                     call 0x21cab0
0x323ca7 f0x327ca7 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323caf f0x327caf 0f8fec000000                   jg 0x323da1
0x323cb5 f0x327cb5 e9bb000000                     jmp 0x323d75
0x323cba f0x327cba 4c89f7                         mov rdi, r14
0x323cbd f0x327cbd 4c89fe                         mov rsi, r15
0x323cc0 f0x327cc0 e8eb8defff                     call 0x21cab0
0x323cc5 f0x327cc5 4181276ff7bfef                 and dword ptr [r15], 0xefbff76f
0x323ccc f0x327ccc 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323cd4 f0x327cd4 0f8e9b000000                   jle 0x323d75
0x323cda f0x327cda e9c2000000                     jmp 0x323da1
0x323cdf f0x327cdf 4c89ff                         mov rdi, r15
0x323ce2 f0x327ce2 e849b9d1ff                     call 0x3f630
0x323ce7 f0x327ce7 e9b7feffff                     jmp 0x323ba3
0x323cec f0x327cec 4c89ff                         mov rdi, r15
0x323cef f0x327cef e8ecbad1ff                     call 0x3f7e0
0x323cf4 f0x327cf4 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x323cf8 f0x327cf8 a804                           test al, 4
0x323cfa f0x327cfa 0f84d0feffff                   je 0x323bd0
0x323d00 f0x327d00 b96ff7bfef                     mov ecx, 0xefbff76f
0x323d05 f0x327d05 238bd4000000                   and ecx, dword ptr [rbx + 0xd4]
0x323d0b f0x327d0b 898d40feffff                   mov dword ptr [rbp - 0x1c0], ecx
0x323d11 f0x327d11 498b4e08                       mov rcx, qword ptr [r14 + 8]
0x323d15 f0x327d15 488b11                         mov rdx, qword ptr [rcx]
0x323d18 f0x327d18 4883c204                       add rdx, 4
0x323d1c f0x327d1c 483b5108                       cmp rdx, qword ptr [rcx + 8]
0x323d20 f0x327d20 762b                           jbe 0x323d4d
0x323d22 f0x327d22 a900000100                     test eax, 0x10000
0x323d27 f0x327d27 7533                           jne 0x323d5c
0x323d29 f0x327d29 498b06                         mov rax, qword ptr [r14]
0x323d2c f0x327d2c 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x323d33 f0x327d33 ba04000000                     mov edx, 4
0x323d38 f0x327d38 4c89f7                         mov rdi, r14
0x323d3b f0x327d3b ff9088010000                   call qword ptr [rax + 0x188]
0x323d41 f0x327d41 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323d49 f0x327d49 7e2a                           jle 0x323d75
0x323d4b f0x327d4b eb54                           jmp 0x323da1
0x323d4d f0x327d4d 488911                         mov qword ptr [rcx], rdx
0x323d50 f0x327d50 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323d58 f0x327d58 7e1b                           jle 0x323d75
0x323d5a f0x327d5a eb45                           jmp 0x323da1
0x323d5c f0x327d5c 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x323d63 f0x327d63 4c89f7                         mov rdi, r14
0x323d66 f0x327d66 e8458defff                     call 0x21cab0
0x323d6b f0x327d6b 41817e403a010000               cmp dword ptr [r14 + 0x40], 0x13a
0x323d73 f0x327d73 7f2c                           jg 0x323da1
0x323d75 f0x327d75 8b83d4000000                   mov eax, dword ptr [rbx + 0xd4]
0x323d7b f0x327d7b 89c1                           mov ecx, eax
0x323d7d f0x327d7d 81f100020000                   xor ecx, 0x200
0x323d83 f0x327d83 898bd4000000                   mov dword ptr [rbx + 0xd4], ecx
0x323d89 f0x327d89 a900020000                     test eax, 0x200
0x323d8e f0x327d8e 7411                           je 0x323da1
0x323d90 f0x327d90 488b4b40                       mov rcx, qword ptr [rbx + 0x40]
0x323d94 f0x327d94 f681d500000002                 test byte ptr [rcx + 0xd5], 2
0x323d9b f0x327d9b 0f853c060000                   jne 0x3243dd
0x323da1 f0x327da1 498b06                         mov rax, qword ptr [r14]
0x323da4 f0x327da4 488db3e0000000                 lea rsi, [rbx + 0xe0]
0x323dab f0x327dab 4c89f7                         mov rdi, r14
0x323dae f0x327dae ff9060010000                   call qword ptr [rax + 0x160]
0x323db4 f0x327db4 498b06                         mov rax, qword ptr [r14]
0x323db7 f0x327db7 488db3e8000000                 lea rsi, [rbx + 0xe8]
0x323dbe f0x327dbe 4c89f7                         mov rdi, r14
0x323dc1 f0x327dc1 ff9038010000                   call qword ptr [rax + 0x138]
0x323dc7 f0x327dc7 c7853cfeffff00000000           mov dword ptr [rbp - 0x1c4], 0
0x323dd1 f0x327dd1 41f6462801                     test byte ptr [r14 + 0x28], 1
0x323dd6 f0x327dd6 7453                           je 0x323e2b
0x323dd8 f0x327dd8 c783e001000000000000           mov dword ptr [rbx + 0x1e0], 0
0x323de2 f0x327de2 83bbe401000000                 cmp dword ptr [rbx + 0x1e4], 0
0x323de9 f0x327de9 7422                           je 0x323e0d
0x323deb f0x327deb 4c8d8be4010000                 lea r9, [rbx + 0x1e4]
0x323df2 f0x327df2 488d8bd8010000                 lea rcx, [rbx + 0x1d8]
0x323df9 f0x327df9 bf10000000                     mov edi, 0x10
0x323dfe f0x327dfe be08000000                     mov esi, 8
0x323e03 f0x327e03 31d2                           xor edx, edx
0x323e05 f0x327e05 4531c0                         xor r8d, r8d
0x323e08 f0x327e08 e81320d0ff                     call 0x25e20
0x323e0d f0x327e0d 41817e4068010000               cmp dword ptr [r14 + 0x40], 0x168
0x323e15 f0x327e15 7f14                           jg 0x323e2b
0x323e17 f0x327e17 488d3df14b0309                 lea rdi, [rip + 0x9034bf1]
0x323e1e f0x327e1e e8bd99e8ff                     call 0x1ad7e0
0x323e23 f0x327e23 84c0                           test al, al
0x323e25 f0x327e25 0f8414020000                   je 0x32403f
0x323e2b f0x327e2b 4531ff                         xor r15d, r15d
0x323e2e f0x327e2e 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x323e32 f0x327e32 a900002000                     test eax, 0x200000
0x323e37 f0x327e37 7522                           jne 0x323e5b
0x323e39 f0x327e39 48c78540feffff00000000         mov qword ptr [rbp - 0x1c0], 0
0x323e44 f0x327e44 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x323e4b f0x327e4b 4c89f7                         mov rdi, r14
0x323e4e f0x327e4e 498b06                         mov rax, qword ptr [r14]
0x323e51 f0x327e51 ff9048010000                   call qword ptr [rax + 0x148]
0x323e57 f0x327e57 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x323e5b f0x327e5b a801                           test al, 1
0x323e5d f0x327e5d 7425                           je 0x323e84
0x323e5f f0x327e5f 81a3d40000006fffbfef           and dword ptr [rbx + 0xd4], 0xefbfff6f
0x323e69 f0x327e69 41f6463510                     test byte ptr [r14 + 0x35], 0x10
0x323e6e f0x327e6e 7514                           jne 0x323e84
0x323e70 f0x327e70 488b03                         mov rax, qword ptr [rbx]
0x323e73 f0x327e73 4889df                         mov rdi, rbx
0x323e76 f0x327e76 4c89f6                         mov rsi, r14
0x323e79 f0x327e79 ba01000000                     mov edx, 1
0x323e7e f0x327e7e ff90d0020000                   call qword ptr [rax + 0x2d0]
0x323e84 f0x327e84 41f6462801                     test byte ptr [r14 + 0x28], 1
0x323e89 f0x327e89 7514                           jne 0x323e9f
0x323e8b f0x327e8b 488db3d8010000                 lea rsi, [rbx + 0x1d8]
0x323e92 f0x327e92 4c89f7                         mov rdi, r14
0x323e95 f0x327e95 e866e70000                     call 0x332600
0x323e9a f0x327e9a e901010000                     jmp 0x323fa0
0x323e9f f0x327e9f 498b06                         mov rax, qword ptr [r14]
0x323ea2 f0x327ea2 4c89f7                         mov rdi, r14
0x323ea5 f0x327ea5 ff5030                         call qword ptr [rax + 0x30]
0x323ea8 f0x327ea8 41817e4068010000               cmp dword ptr [r14 + 0x40], 0x168
0x323eb0 f0x327eb0 4989c4                         mov r12, rax
0x323eb3 f0x327eb3 7f1f                           jg 0x323ed4
0x323eb5 f0x327eb5 488d3d534b0309                 lea rdi, [rip + 0x9034b53]
0x323ebc f0x327ebc e81f99e8ff                     call 0x1ad7e0
0x323ec1 f0x327ec1 84c0                           test al, al
0x323ec3 f0x327ec3 750f                           jne 0x323ed4
0x323ec5 f0x327ec5 498b06                         mov rax, qword ptr [r14]
0x323ec8 f0x327ec8 4c89f7                         mov rdi, r14
0x323ecb f0x327ecb 4c89fe                         mov rsi, r15
0x323ece f0x327ece ff90b8010000                   call qword ptr [rax + 0x1b8]
0x323ed4 f0x327ed4 4c8dad40feffff                 lea r13, [rbp - 0x1c0]
0x323edb f0x327edb 4c89f7                         mov rdi, r14
0x323ede f0x327ede c5f857c0                       vxorps xmm0, xmm0, xmm0
0x323ee2 f0x327ee2 c5f8118540feffff               vmovups xmmword ptr [rbp - 0x1c0], xmm0
0x323eea f0x327eea 4c89ee                         mov rsi, r13
0x323eed f0x327eed e80ee70000                     call 0x332600
0x323ef2 f0x327ef2 83bbe001000000                 cmp dword ptr [rbx + 0x1e0], 0
0x323ef9 f0x327ef9 7564                           jne 0x323f5f
0x323efb f0x327efb 448bbd48feffff                 mov r15d, dword ptr [rbp - 0x1b8]
0x323f02 f0x327f02 4c8d83d8010000                 lea r8, [rbx + 0x1d8]
0x323f09 f0x327f09 4585ff                         test r15d, r15d
0x323f0c f0x327f0c 0f9ec0                         setle al
0x323f0f f0x327f0f 4d39e8                         cmp r8, r13
0x323f12 f0x327f12 0f94c1                         sete cl
0x323f15 f0x327f15 08c1                           or cl, al
0x323f17 f0x327f17 7546                           jne 0x323f5f
0x323f19 f0x327f19 8b8be4010000                   mov ecx, dword ptr [rbx + 0x1e4]
0x323f1f f0x327f1f 4c8bad40feffff                 mov r13, qword ptr [rbp - 0x1c0]
0x323f26 f0x327f26 488d83e4010000                 lea rax, [rbx + 0x1e4]
0x323f2d f0x327f2d bf10000000                     mov edi, 0x10
0x323f32 f0x327f32 be08000000                     mov esi, 8
0x323f37 f0x327f37 4489fa                         mov edx, r15d
0x323f3a f0x327f3a 4589f9                         mov r9d, r15d
0x323f3d f0x327f3d 4489bbe0010000                 mov dword ptr [rbx + 0x1e0], r15d
0x323f44 f0x327f44 48890424                       mov qword ptr [rsp], rax
0x323f48 f0x327f48 e88336d0ff                     call 0x275d0
0x323f4d f0x327f4d 488bbbd8010000                 mov rdi, qword ptr [rbx + 0x1d8]
0x323f54 f0x327f54 4c89ee                         mov rsi, r13
0x323f57 f0x327f57 4489fa                         mov edx, r15d
0x323f5a f0x327f5a e8f1e80000                     call 0x332850
0x323f5f f0x327f5f 41817e4068010000               cmp dword ptr [r14 + 0x40], 0x168
0x323f67 f0x327f67 7f1f                           jg 0x323f88
0x323f69 f0x327f69 488d3d9f4a0309                 lea rdi, [rip + 0x9034a9f]
0x323f70 f0x327f70 e86b98e8ff                     call 0x1ad7e0
0x323f75 f0x327f75 84c0                           test al, al
0x323f77 f0x327f77 750f                           jne 0x323f88
0x323f79 f0x327f79 498b06                         mov rax, qword ptr [r14]
0x323f7c f0x327f7c 4c89f7                         mov rdi, r14
0x323f7f f0x327f7f 4c89e6                         mov rsi, r12
0x323f82 f0x327f82 ff90b8010000                   call qword ptr [rax + 0x1b8]
0x323f88 f0x327f88 488bbd40feffff                 mov rdi, qword ptr [rbp - 0x1c0]
0x323f8f f0x327f8f 4885ff                         test rdi, rdi
0x323f92 f0x327f92 7405                           je 0x323f99
0x323f94 f0x327f94 e8f73ad9ff                     call 0xb7a90
0x323f99 f0x327f99 4c8b2568dfe008                 mov r12, qword ptr [rip + 0x8e0df68]
0x323fa0 f0x327fa0 498b4608                       mov rax, qword ptr [r14 + 8]
0x323fa4 f0x327fa4 488b08                         mov rcx, qword ptr [rax]
0x323fa7 f0x327fa7 4883c104                       add rcx, 4
0x323fab f0x327fab 483b4808                       cmp rcx, qword ptr [rax + 8]
0x323faf f0x327faf 7635                           jbe 0x323fe6
0x323fb1 f0x327fb1 c78540feffff00000000           mov dword ptr [rbp - 0x1c0], 0
0x323fbb f0x327fbb 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x323fc2 f0x327fc2 ba04000000                     mov edx, 4
0x323fc7 f0x327fc7 4c89f7                         mov rdi, r14
0x323fca f0x327fca 498b06                         mov rax, qword ptr [r14]
0x323fcd f0x327fcd ff9088010000                   call qword ptr [rax + 0x188]
0x323fd3 f0x327fd3 83bd40feffff02                 cmp dword ptr [rbp - 0x1c0], 2
0x323fda f0x327fda 720d                           jb 0x323fe9
0x323fdc f0x327fdc 4c89f7                         mov rdi, r14
0x323fdf f0x327fdf e87c76efff                     call 0x21b660
0x323fe4 f0x327fe4 eb03                           jmp 0x323fe9
0x323fe6 f0x327fe6 488908                         mov qword ptr [rax], rcx
0x323fe9 f0x327fe9 48c78530feffff00000000         mov qword ptr [rbp - 0x1d0], 0
0x323ff4 f0x327ff4 488db530feffff                 lea rsi, [rbp - 0x1d0]
0x323ffb f0x327ffb 4c89f7                         mov rdi, r14
0x323ffe f0x327ffe 498b06                         mov rax, qword ptr [r14]
0x324001 f0x328001 ff9038010000                   call qword ptr [rax + 0x138]
0x324007 f0x328007 41817e40f1000000               cmp dword ptr [r14 + 0x40], 0xf1
0x32400f f0x32800f 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x324013 f0x328013 0f8c3e010000                   jl 0x324157
0x324019 f0x328019 a804                           test al, 4
0x32401b f0x32801b 0f84c4000000                   je 0x3240e5
0x324021 f0x328021 80bbd000000000                 cmp byte ptr [rbx + 0xd0], 0
0x324028 f0x328028 b101                           mov cl, 1
0x32402a f0x32802a 756a                           jne 0x324096
0x32402c f0x32802c 498b4e68                       mov rcx, qword ptr [r14 + 0x68]
0x324030 f0x328030 4885c9                         test rcx, rcx
0x324033 f0x328033 745f                           je 0x324094
0x324035 f0x328035 4883791000                     cmp qword ptr [rcx + 0x10], 0
0x32403a f0x32803a 0f95c1                         setne cl
0x32403d f0x32803d eb57                           jmp 0x324096
0x32403f f0x32803f 498b06                         mov rax, qword ptr [r14]
0x324042 f0x328042 4c89f7                         mov rdi, r14
0x324045 f0x328045 ff5030                         call qword ptr [rax + 0x30]
0x324048 f0x328048 4989c7                         mov r15, rax
0x32404b f0x32804b 498b4608                       mov rax, qword ptr [r14 + 8]
0x32404f f0x32804f 488b10                         mov rdx, qword ptr [rax]
0x324052 f0x328052 488d4a04                       lea rcx, [rdx + 4]
0x324056 f0x328056 483b4808                       cmp rcx, qword ptr [rax + 8]
0x32405a f0x32805a 7628                           jbe 0x324084
0x32405c f0x32805c 41f6462a01                     test byte ptr [r14 + 0x2a], 1
0x324061 f0x328061 0f8506030000                   jne 0x32436d
0x324067 f0x328067 498b06                         mov rax, qword ptr [r14]
0x32406a f0x32806a 488db53cfeffff                 lea rsi, [rbp - 0x1c4]
0x324071 f0x328071 ba04000000                     mov edx, 4
0x324076 f0x328076 4c89f7                         mov rdi, r14
0x324079 f0x328079 ff9088010000                   call qword ptr [rax + 0x188]
0x32407f f0x32807f e9f8020000                     jmp 0x32437c
0x324084 f0x328084 8b12                           mov edx, dword ptr [rdx]
0x324086 f0x328086 89953cfeffff                   mov dword ptr [rbp - 0x1c4], edx
0x32408c f0x32808c 488908                         mov qword ptr [rax], rcx
0x32408f f0x32808f e9e8020000                     jmp 0x32437c
0x324094 f0x328094 31c9                           xor ecx, ecx
0x324096 f0x328096 498b5608                       mov rdx, qword ptr [r14 + 8]
0x32409a f0x32809a 488b32                         mov rsi, qword ptr [rdx]
0x32409d f0x32809d 4883c604                       add rsi, 4
0x3240a1 f0x3280a1 483b7208                       cmp rsi, qword ptr [rdx + 8]
0x3240a5 f0x3280a5 763b                           jbe 0x3240e2
0x3240a7 f0x3280a7 a801                           test al, 1
0x3240a9 f0x3280a9 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x3240b0 f0x3280b0 ba04000000                     mov edx, 4
0x3240b5 f0x3280b5 4c89f7                         mov rdi, r14
0x3240b8 f0x3280b8 0f94c0                         sete al
0x3240bb f0x3280bb 20c8                           and al, cl
0x3240bd f0x3280bd 0fb6c0                         movzx eax, al
0x3240c0 f0x3280c0 898540feffff                   mov dword ptr [rbp - 0x1c0], eax
0x3240c6 f0x3280c6 498b06                         mov rax, qword ptr [r14]
0x3240c9 f0x3280c9 ff9088010000                   call qword ptr [rax + 0x188]
0x3240cf f0x3280cf 83bd40feffff02                 cmp dword ptr [rbp - 0x1c0], 2
0x3240d6 f0x3280d6 720d                           jb 0x3240e5
0x3240d8 f0x3280d8 4c89f7                         mov rdi, r14
0x3240db f0x3280db e88075efff                     call 0x21b660
0x3240e0 f0x3280e0 eb03                           jmp 0x3240e5
0x3240e2 f0x3280e2 488932                         mov qword ptr [rdx], rsi
0x3240e5 f0x3280e5 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x3240e9 f0x3280e9 a801                           test al, 1
0x3240eb f0x3280eb 746a                           je 0x324157
0x3240ed f0x3280ed 498b4e08                       mov rcx, qword ptr [r14 + 8]
0x3240f1 f0x3280f1 488b01                         mov rax, qword ptr [rcx]
0x3240f4 f0x3280f4 488d5004                       lea rdx, [rax + 4]
0x3240f8 f0x3280f8 483b5108                       cmp rdx, qword ptr [rcx + 8]
0x3240fc f0x3280fc 7646                           jbe 0x324144
0x3240fe f0x3280fe c78540feffff00000000           mov dword ptr [rbp - 0x1c0], 0
0x324108 f0x328108 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x32410f f0x32810f ba04000000                     mov edx, 4
0x324114 f0x328114 4c89f7                         mov rdi, r14
0x324117 f0x328117 498b06                         mov rax, qword ptr [r14]
0x32411a f0x32811a ff9088010000                   call qword ptr [rax + 0x188]
0x324120 f0x328120 8b8540feffff                   mov eax, dword ptr [rbp - 0x1c0]
0x324126 f0x328126 83f802                         cmp eax, 2
0x324129 f0x328129 720e                           jb 0x324139
0x32412b f0x32812b 4c89f7                         mov rdi, r14
0x32412e f0x32812e e82d75efff                     call 0x21b660
0x324133 f0x328133 8b8540feffff                   mov eax, dword ptr [rbp - 0x1c0]
0x324139 f0x328139 85c0                           test eax, eax
0x32413b f0x32813b 0f95c0                         setne al
0x32413e f0x32813e 41224628                       and al, byte ptr [r14 + 0x28]
0x324142 f0x328142 eb09                           jmp 0x32414d
0x324144 f0x328144 833800                         cmp dword ptr [rax], 0
0x324147 f0x328147 488911                         mov qword ptr [rcx], rdx
0x32414a f0x32814a 0f95c0                         setne al
0x32414d f0x32814d 8883d0000000                   mov byte ptr [rbx + 0xd0], al
0x324153 f0x328153 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x324157 f0x328157 41ff4630                       inc dword ptr [r14 + 0x30]
0x32415b f0x32815b a801                           test al, 1
0x32415d f0x32815d 751c                           jne 0x32417b
0x32415f f0x32815f 488db310010000                 lea rsi, [rbx + 0x110]
0x324166 f0x328166 a900000200                     test eax, 0x20000
0x32416b f0x32816b 756f                           jne 0x3241dc
0x32416d f0x32816d 498b06                         mov rax, qword ptr [r14]
0x324170 f0x328170 4c89f7                         mov rdi, r14
0x324173 f0x328173 ff9060010000                   call qword ptr [rax + 0x160]
0x324179 f0x328179 eb75                           jmp 0x3241f0
0x32417b f0x32817b 4c8bbb10010000                 mov r15, qword ptr [rbx + 0x110]
0x324182 f0x328182 48c78540feffff00000000         mov qword ptr [rbp - 0x1c0], 0
0x32418d f0x32818d 488db540feffff                 lea rsi, [rbp - 0x1c0]
0x324194 f0x328194 4c89f7                         mov rdi, r14
0x324197 f0x328197 498b06                         mov rax, qword ptr [r14]
0x32419a f0x32819a ff9048010000                   call qword ptr [rax + 0x148]
0x3241a0 f0x3281a0 4c39bb10010000                 cmp qword ptr [rbx + 0x110], r15
0x3241a7 f0x3281a7 7527                           jne 0x3241d0
0x3241a9 f0x3281a9 4c8bbd40feffff                 mov r15, qword ptr [rbp - 0x1c0]
0x3241b0 f0x3281b0 4d85ff                         test r15, r15
0x3241b3 f0x3281b3 7414                           je 0x3241c9
0x3241b5 f0x3281b5 488d05a0ac0709                 lea rax, [rip + 0x907aca0]
0x3241bc f0x3281bc 803800                         cmp byte ptr [rax], 0
0x3241bf f0x3281bf 7408                           je 0x3241c9
0x3241c1 f0x3281c1 4c89ff                         mov rdi, r15
0x3241c4 f0x3281c4 e8f7400400                     call 0x3682c0
0x3241c9 f0x3281c9 4c89bb10010000                 mov qword ptr [rbx + 0x110], r15
0x3241d0 f0x3281d0 c783c800000000000000           mov dword ptr [rbx + 0xc8], 0
0x3241da f0x3281da eb14                           jmp 0x3241f0
0x3241dc f0x3281dc 488b3e                         mov rdi, qword ptr [rsi]
0x3241df f0x3281df 4885ff                         test rdi, rdi
0x3241e2 f0x3281e2 740c                           je 0x3241f0
0x3241e4 f0x3281e4 488b07                         mov rax, qword ptr [rdi]
0x3241e7 f0x3281e7 4c89f6                         mov rsi, r14
0x3241ea f0x3281ea ff90c8000000                   call qword ptr [rax + 0xc8]
0x3241f0 f0x3281f0 41f6462805                     test byte ptr [r14 + 0x28], 5
0x3241f5 f0x3281f5 0f84be000000                   je 0x3242b9
0x3241fb f0x3281fb 41ff4e30                       dec dword ptr [r14 + 0x30]
0x3241ff f0x3281ff 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x324203 f0x328203 a801                           test al, 1
0x324205 f0x328205 0f8442010000                   je 0x32434d
0x32420b f0x32820b 4883bb1001000000               cmp qword ptr [rbx + 0x110], 0
0x324213 f0x328213 0f8534010000                   jne 0x32434d
0x324219 f0x328219 a802                           test al, 2
0x32421b f0x32821b 0f85f9000000                   jne 0x32431a
0x324221 f0x328221 488d0548480309                 lea rax, [rip + 0x9034848]
0x324228 f0x328228 803800                         cmp byte ptr [rax], 0
0x32422b f0x32822b 0f85e9000000                   jne 0x32431a
0x324231 f0x328231 410fb74635                     movzx eax, word ptr [r14 + 0x35]
0x324236 f0x328236 a910400000                     test eax, 0x4010
0x32423b f0x32823b 0f850c010000                   jne 0x32434d
0x324241 f0x328241 498b06                         mov rax, qword ptr [r14]
0x324244 f0x328244 4c89f7                         mov rdi, r14
0x324247 f0x328247 ff5028                         call qword ptr [rax + 0x28]
0x32424a f0x32824a 31ff                           xor edi, edi
0x32424c f0x32824c 4885c0                         test rax, rax
0x32424f f0x32824f 740b                           je 0x32425c
0x324251 f0x328251 83b8b800000001                 cmp dword ptr [rax + 0xb8], 1
0x324258 f0x328258 480f44f8                       cmove rdi, rax
0x32425c f0x32825c f683d600000004                 test byte ptr [rbx + 0xd6], 4
0x324263 f0x328263 0f8544010000                   jne 0x3243ad
0x324269 f0x328269 4c8bbb10010000                 mov r15, qword ptr [rbx + 0x110]
0x324270 f0x328270 4d85ff                         test r15, r15
0x324273 f0x328273 7513                           jne 0x324288
0x324275 f0x328275 488b03                         mov rax, qword ptr [rbx]
0x324278 f0x328278 4889df                         mov rdi, rbx
0x32427b f0x32827b ff9000040000                   call qword ptr [rax + 0x400]
0x324281 f0x328281 4c8bbb10010000                 mov r15, qword ptr [rbx + 0x110]
0x324288 f0x328288 4d85ff                         test r15, r15
0x32428b f0x32828b 7414                           je 0x3242a1
0x32428d f0x32828d 488d05c8ab0709                 lea rax, [rip + 0x907abc8]
0x324294 f0x328294 803800                         cmp byte ptr [rax], 0
0x324297 f0x328297 7408                           je 0x3242a1
0x324299 f0x328299 4c89ff                         mov rdi, r15
0x32429c f0x32829c e81f400400                     call 0x3682c0
0x3242a1 f0x3282a1 4c89bb10010000                 mov qword ptr [rbx + 0x110], r15
0x3242a8 f0x3282a8 4c89f7                         mov rdi, r14
0x3242ab f0x3282ab 498b06                         mov rax, qword ptr [r14]
0x3242ae f0x3282ae ff9080010000                   call qword ptr [rax + 0x180]
0x3242b4 f0x3282b4 e994000000                     jmp 0x32434d
0x3242b9 f0x3282b9 498b06                         mov rax, qword ptr [r14]
0x3242bc f0x3282bc 488db320010000                 lea rsi, [rbx + 0x120]
0x3242c3 f0x3282c3 4c89f7                         mov rdi, r14
0x3242c6 f0x3282c6 ff9060010000                   call qword ptr [rax + 0x160]
0x3242cc f0x3282cc 4883bb2001000000               cmp qword ptr [rbx + 0x120], 0
0x3242d4 f0x3282d4 0f8421ffffff                   je 0x3241fb
0x3242da f0x3282da 4883bb1801000000               cmp qword ptr [rbx + 0x118], 0
0x3242e2 f0x3282e2 0f8413ffffff                   je 0x3241fb
0x3242e8 f0x3282e8 4c8dbd40feffff                 lea r15, [rbp - 0x1c0]
0x3242ef f0x3282ef 4c89f6                         mov rsi, r14
0x3242f2 f0x3282f2 4c89ff                         mov rdi, r15
0x3242f5 f0x3282f5 e84694f0ff                     call 0x22d740
0x3242fa f0x3282fa 4c89ff                         mov rdi, r15
0x3242fd f0x3282fd e82e5ed0ff                     call 0x2a130
0x324302 f0x328302 4889df                         mov rdi, rbx
0x324305 f0x328305 4889c6                         mov rsi, rax
0x324308 f0x328308 e8e3000000                     call 0x3243f0
0x32430d f0x32830d 4c89ff                         mov rdi, r15
0x324310 f0x328310 e87b6fceff                     call 0xb290
0x324315 f0x328315 e9e1feffff                     jmp 0x3241fb
0x32431a f0x32831a 488b03                         mov rax, qword ptr [rbx]
0x32431d f0x32831d 4889df                         mov rdi, rbx
0x324320 f0x328320 ff9000040000                   call qword ptr [rax + 0x400]
0x324326 f0x328326 4c8bb310010000                 mov r14, qword ptr [rbx + 0x110]
0x32432d f0x32832d 4d85f6                         test r14, r14
0x324330 f0x328330 7414                           je 0x324346
0x324332 f0x328332 488d0523ab0709                 lea rax, [rip + 0x907ab23]
0x324339 f0x328339 803800                         cmp byte ptr [rax], 0
0x32433c f0x32833c 7408                           je 0x324346
0x32433e f0x32833e 4c89f7                         mov rdi, r14
0x324341 f0x328341 e87a3f0400                     call 0x3682c0
0x324346 f0x328346 4c89b310010000                 mov qword ptr [rbx + 0x110], r14
0x32434d f0x32834d 498b0424                       mov rax, qword ptr [r12]
0x324351 f0x328351 483b45d0                       cmp rax, qword ptr [rbp - 0x30]
0x324355 f0x328355 0f858d000000                   jne 0x3243e8
0x32435b f0x32835b 4881c4b8010000                 add rsp, 0x1b8
0x324362 f0x328362 5b                             pop rbx
0x324363 f0x328363 415c                           pop r12
0x324365 f0x328365 415d                           pop r13
0x324367 f0x328367 415e                           pop r14
0x324369 f0x328369 415f                           pop r15
0x32436b f0x32836b 5d                             pop rbp
0x32436c f0x32836c c3                             ret 
0x32436d f0x32836d 488db53cfeffff                 lea rsi, [rbp - 0x1c4]
0x324374 f0x328374 4c89f7                         mov rdi, r14
0x324377 f0x328377 e83487efff                     call 0x21cab0
0x32437c f0x32837c 4863853cfeffff                 movsxd rax, dword ptr [rbp - 0x1c4]
0x324383 f0x328383 4d8b2e                         mov r13, qword ptr [r14]
0x324386 f0x328386 4c89f7                         mov rdi, r14
0x324389 f0x328389 488d0440                       lea rax, [rax + rax*2]
0x32438d f0x32838d 498d748704                     lea rsi, [r15 + rax*4 + 4]
0x324392 f0x328392 41ff95b8010000                 call qword ptr [r13 + 0x1b8]
0x324399 f0x328399 498b4628                       mov rax, qword ptr [r14 + 0x28]
0x32439d f0x32839d a900002000                     test eax, 0x200000
0x3243a2 f0x3283a2 0f8491faffff                   je 0x323e39
0x3243a8 f0x3283a8 e9aefaffff                     jmp 0x323e5b
0x3243ad f0x3283ad 4885ff                         test rdi, rdi
0x3243b0 f0x3283b0 0f84b3feffff                   je 0x324269
0x3243b6 f0x3283b6 488b4310                       mov rax, qword ptr [rbx + 0x10]
0x3243ba f0x3283ba b900000200                     mov ecx, 0x20000
0x3243bf f0x3283bf 2388d4000000                   and ecx, dword ptr [rax + 0xd4]
0x3243c5 f0x3283c5 0f849efeffff                   je 0x324269
0x3243cb f0x3283cb e8c0c7f8ff                     call 0x2b0b90
0x3243d0 f0x3283d0 84c0                           test al, al
0x3243d2 f0x3283d2 0f8491feffff                   je 0x324269
0x3243d8 f0x3283d8 e970ffffff                     jmp 0x32434d
0x3243dd f0x3283dd 8983d4000000                   mov dword ptr [rbx + 0xd4], eax
0x3243e3 f0x3283e3 e9b9f9ffff                     jmp 0x323da1
0x3243e8 f0x3283e8 e873da1407                     call 0x7471e60
0x3243ed f0x3283ed 0f0b                           ud2 

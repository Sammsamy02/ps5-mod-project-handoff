0x323d70 f0x327d70 0100                           add dword ptr [rax], eax
0x323d72 f0x327d72 007f2c                         add byte ptr [rdi + 0x2c], bh
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
0x323e17 f0x327e17 488d3df14b0309                 lea rdi, [rip + 0x9034bf1] rip->0x9358a0f
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
0x323eb5 f0x327eb5 488d3d534b0309                 lea rdi, [rip + 0x9034b53] rip->0x9358a0f
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
0x323f69 f0x327f69 488d3d9f4a0309                 lea rdi, [rip + 0x9034a9f] rip->0x9358a0f
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
0x323f99 f0x327f99 4c8b2568dfe008                 mov r12, qword ptr [rip + 0x8e0df68] rip->0x9131f08

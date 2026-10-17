0x70d860  (f 0x30d860)  55                     push rbp
0x70d861  (f 0x30d861)  4889e5                 mov rbp, rsp
0x70d864  (f 0x30d864)  4157                   push r15
0x70d866  (f 0x30d866)  4156                   push r14
0x70d868  (f 0x30d868)  4155                   push r13
0x70d86a  (f 0x30d86a)  4154                   push r12
0x70d86c  (f 0x30d86c)  53                     push rbx
0x70d86d  (f 0x30d86d)  4883ec28               sub rsp, 0x28
0x70d871  (f 0x30d871)  488b059046e208         mov rax, qword ptr [rip + 0x8e24690]  ; rip->0x9531f08 (file 0x9131f08)
0x70d878  (f 0x30d878)  48894db0               mov qword ptr [rbp - 0x50], rcx
0x70d87c  (f 0x30d87c)  488955c0               mov qword ptr [rbp - 0x40], rdx
0x70d880  (f 0x30d880)  48897db8               mov qword ptr [rbp - 0x48], rdi
0x70d884  (f 0x30d884)  488d3de6c20a09         lea rdi, [rip + 0x90ac2e6]  ; rip->0x97b9b71 (file 0x93b9b71)
0x70d88b  (f 0x30d88b)  4989f6                 mov r14, rsi
0x70d88e  (f 0x30d88e)  be00020000             mov esi, 0x200
0x70d893  (f 0x30d893)  ba08000000             mov edx, 8
0x70d898  (f 0x30d898)  b901000000             mov ecx, 1
0x70d89d  (f 0x30d89d)  4589cf                 mov r15d, r9d
0x70d8a0  (f 0x30d8a0)  4589c5                 mov r13d, r8d
0x70d8a3  (f 0x30d8a3)  488b00                 mov rax, qword ptr [rax]
0x70d8a6  (f 0x30d8a6)  488945d0               mov qword ptr [rbp - 0x30], rax
0x70d8aa  (f 0x30d8aa)  e8c1341700             call 0x880d70
0x70d8af  (f 0x30d8af)  488d7dc8               lea rdi, [rbp - 0x38]
0x70d8b3  (f 0x30d8b3)  4c89f6                 mov rsi, r14
0x70d8b6  (f 0x30d8b6)  ba01000000             mov edx, 1
0x70d8bb  (f 0x30d8bb)  4989c4                 mov r12, rax
0x70d8be  (f 0x30d8be)  e8cdfcf2ff             call 0x63d590
0x70d8c3  (f 0x30d8c3)  4c89e7                 mov rdi, r12
0x70d8c6  (f 0x30d8c6)  bac7000000             mov edx, 0xc7
0x70d8cb  (f 0x30d8cb)  31f6                   xor esi, esi
0x70d8cd  (f 0x30d8cd)  31db                   xor ebx, ebx
0x70d8cf  (f 0x30d8cf)  e8dc870b00             call 0x7c60b0
0x70d8d4  (f 0x30d8d4)  c5f8104d28             vmovups xmm1, xmmword ptr [rbp + 0x28]
0x70d8d9  (f 0x30d8d9)  488d0548a1bc07         lea rax, [rip + 0x7bca148]  ; rip->0x82d7a28 (file 0x7ed7a28)
0x70d8e0  (f 0x30d8e0)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x70d8e4  (f 0x30d8e4)  c4c17811442428         vmovups xmmword ptr [r12 + 0x28], xmm0
0x70d8eb  (f 0x30d8eb)  41c7442438ffffffff     mov dword ptr [r12 + 0x38], 0xffffffff
0x70d8f4  (f 0x30d8f4)  c4c17811442440         vmovups xmmword ptr [r12 + 0x40], xmm0
0x70d8fb  (f 0x30d8fb)  49c744245000000000     mov qword ptr [r12 + 0x50], 0
0x70d904  (f 0x30d904)  45896c2458             mov dword ptr [r12 + 0x58], r13d
0x70d909  (f 0x30d909)  6645897c245c           mov word ptr [r12 + 0x5c], r15w
0x70d90f  (f 0x30d90f)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x70d913  (f 0x30d913)  c4c17c1144247e         vmovups ymmword ptr [r12 + 0x7e], ymm0
0x70d91a  (f 0x30d91a)  c4c17c1144245e         vmovups ymmword ptr [r12 + 0x5e], ymm0
0x70d921  (f 0x30d921)  c4c17c11842490000000   vmovups ymmword ptr [r12 + 0x90], ymm0
0x70d92b  (f 0x30d92b)  ba80000000             mov edx, 0x80
0x70d930  (f 0x30d930)  0b5510                 or edx, dword ptr [rbp + 0x10]
0x70d933  (f 0x30d933)  488b4d18               mov rcx, qword ptr [rbp + 0x18]
0x70d937  (f 0x30d937)  488b7520               mov rsi, qword ptr [rbp + 0x20]
0x70d93b  (f 0x30d93b)  498dbc24e8000000       lea rdi, [r12 + 0xe8]
0x70d943  (f 0x30d943)  49890424               mov qword ptr [r12], rax
0x70d947  (f 0x30d947)  488b4538               mov rax, qword ptr [rbp + 0x38]
0x70d94b  (f 0x30d94b)  c4c178118c24b0000000   vmovups xmmword ptr [r12 + 0xb0], xmm1
0x70d955  (f 0x30d955)  488b00                 mov rax, qword ptr [rax]
0x70d958  (f 0x30d958)  49898424c0000000       mov qword ptr [r12 + 0xc0], rax
0x70d960  (f 0x30d960)  49c78424c800000000000000 mov qword ptr [r12 + 0xc8], 0
0x70d96c  (f 0x30d96c)  6641c78424d00000000000 mov word ptr [r12 + 0xd0], 0
0x70d977  (f 0x30d977)  41899424d4000000       mov dword ptr [r12 + 0xd4], edx
0x70d97f  (f 0x30d97f)  ba01000000             mov edx, 1
0x70d984  (f 0x30d984)  49898c24d8000000       mov qword ptr [r12 + 0xd8], rcx
0x70d98c  (f 0x30d98c)  49c78424e000000000000000 mov qword ptr [r12 + 0xe0], 0
0x70d998  (f 0x30d998)  e8f3fbf2ff             call 0x63d590
0x70d99d  (f 0x30d99d)  c5f8101d7bf07207       vmovups xmm3, xmmword ptr [rip + 0x772f07b]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x70d9a5  (f 0x30d9a5)  488b45c0               mov rax, qword ptr [rbp - 0x40]
0x70d9a9  (f 0x30d9a9)  49c784244801000000000000 mov qword ptr [r12 + 0x148], 0
0x70d9b5  (f 0x30d9b5)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x70d9b9  (f 0x30d9b9)  c4c17c118424f0000000   vmovups ymmword ptr [r12 + 0xf0], ymm0
0x70d9c3  (f 0x30d9c3)  c4c17c11842410010000   vmovups ymmword ptr [r12 + 0x110], ymm0
0x70d9cd  (f 0x30d9cd)  49c784243001000000000000 mov qword ptr [r12 + 0x130], 0
0x70d9d9  (f 0x30d9d9)  c5e857d2               vxorps xmm2, xmm2, xmm2
0x70d9dd  (f 0x30d9dd)  c5f057c9               vxorps xmm1, xmm1, xmm1
0x70d9e1  (f 0x30d9e1)  c4c178119c2450010000   vmovups xmmword ptr [r12 + 0x150], xmm3
0x70d9eb  (f 0x30d9eb)  49c784246801000000000000 mov qword ptr [r12 + 0x168], 0
0x70d9f7  (f 0x30d9f7)  41c784247001000000000000 mov dword ptr [r12 + 0x170], 0
0x70da03  (f 0x30da03)  41c684247801000000     mov byte ptr [r12 + 0x178], 0
0x70da0c  (f 0x30da0c)  49c78424a001000000000000 mov qword ptr [r12 + 0x1a0], 0
0x70da18  (f 0x30da18)  c4c178118c2480010000   vmovups xmmword ptr [r12 + 0x180], xmm1
0x70da22  (f 0x30da22)  c4c178119c24a8010000   vmovups xmmword ptr [r12 + 0x1a8], xmm3
0x70da2c  (f 0x30da2c)  49c78424c001000000000000 mov qword ptr [r12 + 0x1c0], 0
0x70da38  (f 0x30da38)  41c78424c801000000000000 mov dword ptr [r12 + 0x1c8], 0
0x70da44  (f 0x30da44)  41c68424d001000000     mov byte ptr [r12 + 0x1d0], 0
0x70da4d  (f 0x30da4d)  c4c17c119424d8010000   vmovups ymmword ptr [r12 + 0x1d8], ymm2
0x70da57  (f 0x30da57)  49c78424f801000000000000 mov qword ptr [r12 + 0x1f8], 0
0x70da63  (f 0x30da63)  4c8920                 mov qword ptr [rax], r12
0x70da66  (f 0x30da66)  ff5540                 call qword ptr [rbp + 0x40]
0x70da69  (f 0x30da69)  4989c5                 mov r13, rax
0x70da6c  (f 0x30da6c)  ff5548                 call qword ptr [rbp + 0x48]
0x70da6f  (f 0x30da6f)  4989c7                 mov r15, rax
0x70da72  (f 0x30da72)  498b0424               mov rax, qword ptr [r12]
0x70da76  (f 0x30da76)  4d39e5                 cmp r13, r12
0x70da79  (f 0x30da79)  4c89e7                 mov rdi, r12
0x70da7c  (f 0x30da7c)  490f45dd               cmovne rbx, r13
0x70da80  (f 0x30da80)  4889de                 mov rsi, rbx
0x70da83  (f 0x30da83)  ff9028030000           call qword ptr [rax + 0x328]
0x70da89  (f 0x30da89)  4d85ff                 test r15, r15
0x70da8c  (f 0x30da8c)  7414                   je 0x70daa2
0x70da8e  (f 0x30da8e)  488d05c7130909         lea rax, [rip + 0x90913c7]  ; rip->0x979ee5c (file 0x939ee5c)
0x70da95  (f 0x30da95)  803800                 cmp byte ptr [rax], 0
0x70da98  (f 0x30da98)  7408                   je 0x70daa2
0x70da9a  (f 0x30da9a)  4c89ff                 mov rdi, r15
0x70da9d  (f 0x30da9d)  e81ea80500             call 0x7682c0
0x70daa2  (f 0x30daa2)  4d89bc24e0000000       mov qword ptr [r12 + 0xe0], r15
0x70daaa  (f 0x30daaa)  4c89e7                 mov rdi, r12
0x70daad  (f 0x30daad)  498b0424               mov rax, qword ptr [r12]
0x70dab1  (f 0x30dab1)  ff5010                 call qword ptr [rax + 0x10]
0x70dab4  (f 0x30dab4)  488b55b8               mov rdx, qword ptr [rbp - 0x48]
0x70dab8  (f 0x30dab8)  488d3581f3f8ff         lea rsi, [rip - 0x70c7f]  ; rip->0x69ce40 (file 0x29ce40)
0x70dabf  (f 0x30dabf)  4c89e7                 mov rdi, r12
0x70dac2  (f 0x30dac2)  4c89f1                 mov rcx, r14
0x70dac5  (f 0x30dac5)  e8d6811700             call 0x885ca0
0x70daca  (f 0x30daca)  ff55b0                 call qword ptr [rbp - 0x50]
0x70dacd  (f 0x30dacd)  488b053444e208         mov rax, qword ptr [rip + 0x8e24434]  ; rip->0x9531f08 (file 0x9131f08)
0x70dad4  (f 0x30dad4)  488b00                 mov rax, qword ptr [rax]
0x70dad7  (f 0x30dad7)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x70dadb  (f 0x30dadb)  750f                   jne 0x70daec
0x70dadd  (f 0x30dadd)  4883c428               add rsp, 0x28
0x70dae1  (f 0x30dae1)  5b                     pop rbx
0x70dae2  (f 0x30dae2)  415c                   pop r12
0x70dae4  (f 0x30dae4)  415d                   pop r13
0x70dae6  (f 0x30dae6)  415e                   pop r14
0x70dae8  (f 0x30dae8)  415f                   pop r15
0x70daea  (f 0x30daea)  5d                     pop rbp
0x70daeb  (f 0x30daeb)  c3                     ret 
0x70daec  (f 0x30daec)  e86f431607             call 0x7871e60
0x70daf1  (f 0x30daf1)  0f0b                   ud2 
--- calls: [('0xb0d8aa', '0x880d70'), ('0xb0d8be', '0x63d590'), ('0xb0d8cf', '0x7c60b0'), ('0xb0d998', '0x63d590'), ('0xb0da9d', '0x7682c0'), ('0xb0dac5', '0x885ca0'), ('0xb0daec', '0x7871e60')]

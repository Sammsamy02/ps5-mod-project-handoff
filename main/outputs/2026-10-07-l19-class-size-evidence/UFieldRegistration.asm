0x71a4f0  (f 0x31a4f0)  55                     push rbp
0x71a4f1  (f 0x31a4f1)  4889e5                 mov rbp, rsp
0x71a4f4  (f 0x31a4f4)  4157                   push r15
0x71a4f6  (f 0x31a4f6)  4156                   push r14
0x71a4f8  (f 0x31a4f8)  4155                   push r13
0x71a4fa  (f 0x31a4fa)  4154                   push r12
0x71a4fc  (f 0x31a4fc)  53                     push rbx
0x71a4fd  (f 0x31a4fd)  4883ec18               sub rsp, 0x18
0x71a501  (f 0x31a501)  4c8b2d007ae108         mov r13, qword ptr [rip + 0x8e17a00]  ; rip->0x9531f08 (file 0x9131f08)
0x71a508  (f 0x31a508)  498b4500               mov rax, qword ptr [r13]
0x71a50c  (f 0x31a50c)  488945d0               mov qword ptr [rbp - 0x30], rax
0x71a510  (f 0x31a510)  488b05e13e0809         mov rax, qword ptr [rip + 0x9083ee1]  ; rip->0x979e3f8 (file 0x939e3f8)
0x71a517  (f 0x31a517)  4885c0                 test rax, rax
0x71a51a  (f 0x31a51a)  0f8527020000           jne 0x71a747
0x71a520  (f 0x31a520)  488d3d4af60909         lea rdi, [rip + 0x909f64a]  ; rip->0x97b9b71 (file 0x93b9b71)
0x71a527  (f 0x31a527)  be00020000             mov esi, 0x200
0x71a52c  (f 0x31a52c)  ba08000000             mov edx, 8
0x71a531  (f 0x31a531)  b901000000             mov ecx, 1
0x71a536  (f 0x31a536)  e835681600             call 0x880d70
0x71a53b  (f 0x31a53b)  488d35aef94807         lea rsi, [rip + 0x748f9ae]  ; rip->0x7ba9ef0 (file 0x77a9ef0)
0x71a542  (f 0x31a542)  488d7dc8               lea rdi, [rbp - 0x38]
0x71a546  (f 0x31a546)  ba01000000             mov edx, 1
0x71a54b  (f 0x31a54b)  4889c3                 mov rbx, rax
0x71a54e  (f 0x31a54e)  e83d30f2ff             call 0x63d590
0x71a553  (f 0x31a553)  4889df                 mov rdi, rbx
0x71a556  (f 0x31a556)  bac7000000             mov edx, 0xc7
0x71a55b  (f 0x31a55b)  31f6                   xor esi, esi
0x71a55d  (f 0x31a55d)  4531f6                 xor r14d, r14d
0x71a560  (f 0x31a560)  e84bbb0a00             call 0x7c60b0
0x71a565  (f 0x31a565)  488d05bcd4bb07         lea rax, [rip + 0x7bbd4bc]  ; rip->0x82d7a28 (file 0x7ed7a28)
0x71a56c  (f 0x31a56c)  488d0dfd010000         lea rcx, [rip + 0x1fd]  ; rip->0x71a770 (file 0x31a770)
0x71a573  (f 0x31a573)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71a577  (f 0x31a577)  c5f8114328             vmovups xmmword ptr [rbx + 0x28], xmm0
0x71a57c  (f 0x31a57c)  c74338ffffffff         mov dword ptr [rbx + 0x38], 0xffffffff
0x71a583  (f 0x31a583)  c5f8114340             vmovups xmmword ptr [rbx + 0x40], xmm0
0x71a588  (f 0x31a588)  48c7435000000000       mov qword ptr [rbx + 0x50], 0
0x71a590  (f 0x31a590)  c7435830000000         mov dword ptr [rbx + 0x58], 0x30
0x71a597  (f 0x31a597)  66c7435c0800           mov word ptr [rbx + 0x5c], 8
0x71a59d  (f 0x31a59d)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71a5a1  (f 0x31a5a1)  c5fc11437e             vmovups ymmword ptr [rbx + 0x7e], ymm0
0x71a5a6  (f 0x31a5a6)  c5fc11435e             vmovups ymmword ptr [rbx + 0x5e], ymm0
0x71a5ab  (f 0x31a5ab)  c5fc118390000000       vmovups ymmword ptr [rbx + 0x90], ymm0
0x71a5b3  (f 0x31a5b3)  488d35e6010000         lea rsi, [rip + 0x1e6]  ; rip->0x71a7a0 (file 0x31a7a0)
0x71a5ba  (f 0x31a5ba)  488dbbe8000000         lea rdi, [rbx + 0xe8]
0x71a5c1  (f 0x31a5c1)  ba01000000             mov edx, 1
0x71a5c6  (f 0x31a5c6)  488903                 mov qword ptr [rbx], rax
0x71a5c9  (f 0x31a5c9)  48898bb0000000         mov qword ptr [rbx + 0xb0], rcx
0x71a5d0  (f 0x31a5d0)  488d0db90ccfff         lea rcx, [rip - 0x30f347]  ; rip->0x40b290 (file 0xb290)
0x71a5d7  (f 0x31a5d7)  4889b3b8000000         mov qword ptr [rbx + 0xb8], rsi
0x71a5de  (f 0x31a5de)  488d3589ca5007         lea rsi, [rip + 0x750ca89]  ; rip->0x7c2706e (file 0x782706e)
0x71a5e5  (f 0x31a5e5)  48898bc0000000         mov qword ptr [rbx + 0xc0], rcx
0x71a5ec  (f 0x31a5ec)  48c783c800000000000000 mov qword ptr [rbx + 0xc8], 0
0x71a5f7  (f 0x31a5f7)  66c783d00000000000     mov word ptr [rbx + 0xd0], 0
0x71a600  (f 0x31a600)  c783d400000081000010   mov dword ptr [rbx + 0xd4], 0x10000081
0x71a60a  (f 0x31a60a)  48c783d800000001000000 mov qword ptr [rbx + 0xd8], 1
0x71a615  (f 0x31a615)  48c783e000000000000000 mov qword ptr [rbx + 0xe0], 0
0x71a620  (f 0x31a620)  e86b2ff2ff             call 0x63d590
0x71a625  (f 0x31a625)  c5f8101df3237207       vmovups xmm3, xmmword ptr [rip + 0x77223f3]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x71a62d  (f 0x31a62d)  48c7834801000000000000 mov qword ptr [rbx + 0x148], 0
0x71a638  (f 0x31a638)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71a63c  (f 0x31a63c)  c5fc1183f0000000       vmovups ymmword ptr [rbx + 0xf0], ymm0
0x71a644  (f 0x31a644)  c5fc118310010000       vmovups ymmword ptr [rbx + 0x110], ymm0
0x71a64c  (f 0x31a64c)  48c7833001000000000000 mov qword ptr [rbx + 0x130], 0
0x71a657  (f 0x31a657)  c5e857d2               vxorps xmm2, xmm2, xmm2
0x71a65b  (f 0x31a65b)  c5f057c9               vxorps xmm1, xmm1, xmm1
0x71a65f  (f 0x31a65f)  c5f8119b50010000       vmovups xmmword ptr [rbx + 0x150], xmm3
0x71a667  (f 0x31a667)  48c7836801000000000000 mov qword ptr [rbx + 0x168], 0
0x71a672  (f 0x31a672)  c7837001000000000000   mov dword ptr [rbx + 0x170], 0
0x71a67c  (f 0x31a67c)  c6837801000000         mov byte ptr [rbx + 0x178], 0
0x71a683  (f 0x31a683)  48c783a001000000000000 mov qword ptr [rbx + 0x1a0], 0
0x71a68e  (f 0x31a68e)  c5f8118b80010000       vmovups xmmword ptr [rbx + 0x180], xmm1
0x71a696  (f 0x31a696)  c5f8119ba8010000       vmovups xmmword ptr [rbx + 0x1a8], xmm3
0x71a69e  (f 0x31a69e)  48c783c001000000000000 mov qword ptr [rbx + 0x1c0], 0
0x71a6a9  (f 0x31a6a9)  c783c801000000000000   mov dword ptr [rbx + 0x1c8], 0
0x71a6b3  (f 0x31a6b3)  c683d001000000         mov byte ptr [rbx + 0x1d0], 0
0x71a6ba  (f 0x31a6ba)  c5fc1193d8010000       vmovups ymmword ptr [rbx + 0x1d8], ymm2
0x71a6c2  (f 0x31a6c2)  48c783f801000000000000 mov qword ptr [rbx + 0x1f8], 0
0x71a6cd  (f 0x31a6cd)  48891d243d0809         mov qword ptr [rip + 0x9083d24], rbx  ; rip->0x979e3f8 (file 0x939e3f8)
0x71a6d4  (f 0x31a6d4)  e8d7b40a00             call 0x7c5bb0
0x71a6d9  (f 0x31a6d9)  4989c4                 mov r12, rax
0x71a6dc  (f 0x31a6dc)  e8cfb40a00             call 0x7c5bb0
0x71a6e1  (f 0x31a6e1)  4989c7                 mov r15, rax
0x71a6e4  (f 0x31a6e4)  488b03                 mov rax, qword ptr [rbx]
0x71a6e7  (f 0x31a6e7)  4939dc                 cmp r12, rbx
0x71a6ea  (f 0x31a6ea)  4889df                 mov rdi, rbx
0x71a6ed  (f 0x31a6ed)  4d0f45f4               cmovne r14, r12
0x71a6f1  (f 0x31a6f1)  4c89f6                 mov rsi, r14
0x71a6f4  (f 0x31a6f4)  ff9028030000           call qword ptr [rax + 0x328]
0x71a6fa  (f 0x31a6fa)  4d85ff                 test r15, r15
0x71a6fd  (f 0x31a6fd)  7414                   je 0x71a713
0x71a6ff  (f 0x31a6ff)  488d0556470809         lea rax, [rip + 0x9084756]  ; rip->0x979ee5c (file 0x939ee5c)
0x71a706  (f 0x31a706)  803800                 cmp byte ptr [rax], 0
0x71a709  (f 0x31a709)  7408                   je 0x71a713
0x71a70b  (f 0x31a70b)  4c89ff                 mov rdi, r15
0x71a70e  (f 0x31a70e)  e8addb0400             call 0x7682c0
0x71a713  (f 0x31a713)  4c89bbe0000000         mov qword ptr [rbx + 0xe0], r15
0x71a71a  (f 0x31a71a)  4889df                 mov rdi, rbx
0x71a71d  (f 0x31a71d)  488b03                 mov rax, qword ptr [rbx]
0x71a720  (f 0x31a720)  ff5010                 call qword ptr [rax + 0x10]
0x71a723  (f 0x31a723)  488d351627f8ff         lea rsi, [rip - 0x7d8ea]  ; rip->0x69ce40 (file 0x29ce40)
0x71a72a  (f 0x31a72a)  488d1593c95007         lea rdx, [rip + 0x750c993]  ; rip->0x7c270c4 (file 0x78270c4)
0x71a731  (f 0x31a731)  488d0db8f74807         lea rcx, [rip + 0x748f7b8]  ; rip->0x7ba9ef0 (file 0x77a9ef0)
0x71a738  (f 0x31a738)  4889df                 mov rdi, rbx
0x71a73b  (f 0x31a73b)  e860b51600             call 0x885ca0
0x71a740  (f 0x31a740)  488b05b13c0809         mov rax, qword ptr [rip + 0x9083cb1]  ; rip->0x979e3f8 (file 0x939e3f8)
0x71a747  (f 0x31a747)  498b4d00               mov rcx, qword ptr [r13]
0x71a74b  (f 0x31a74b)  483b4dd0               cmp rcx, qword ptr [rbp - 0x30]
0x71a74f  (f 0x31a74f)  750f                   jne 0x71a760
0x71a751  (f 0x31a751)  4883c418               add rsp, 0x18
0x71a755  (f 0x31a755)  5b                     pop rbx
0x71a756  (f 0x31a756)  415c                   pop r12
0x71a758  (f 0x31a758)  415d                   pop r13
0x71a75a  (f 0x31a75a)  415e                   pop r14
0x71a75c  (f 0x31a75c)  415f                   pop r15
0x71a75e  (f 0x31a75e)  5d                     pop rbp
0x71a75f  (f 0x31a75f)  c3                     ret 
0x71a760  (f 0x31a760)  e8fb761507             call 0x7871e60
0x71a765  (f 0x31a765)  0f0b                   ud2 
--- calls: [('0xb1a536', '0x880d70'), ('0xb1a54e', '0x63d590'), ('0xb1a560', '0x7c60b0'), ('0xb1a620', '0x63d590'), ('0xb1a6d4', '0x7c5bb0'), ('0xb1a6dc', '0x7c5bb0'), ('0xb1a70e', '0x7682c0'), ('0xb1a73b', '0x885ca0'), ('0xb1a760', '0x7871e60')]

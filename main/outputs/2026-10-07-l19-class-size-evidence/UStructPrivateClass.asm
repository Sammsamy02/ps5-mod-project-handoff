0x720af0  (f 0x320af0)  55                     push rbp
0x720af1  (f 0x320af1)  4889e5                 mov rbp, rsp
0x720af4  (f 0x320af4)  4157                   push r15
0x720af6  (f 0x320af6)  4156                   push r14
0x720af8  (f 0x320af8)  4154                   push r12
0x720afa  (f 0x320afa)  53                     push rbx
0x720afb  (f 0x320afb)  4883ec20               sub rsp, 0x20
0x720aff  (f 0x320aff)  4c8b3d0214e108         mov r15, qword ptr [rip + 0x8e11402]  ; rip->0x9531f08 (file 0x9131f08)
0x720b06  (f 0x320b06)  498b07                 mov rax, qword ptr [r15]
0x720b09  (f 0x320b09)  488945d8               mov qword ptr [rbp - 0x28], rax
0x720b0d  (f 0x320b0d)  488b05fcd80709         mov rax, qword ptr [rip + 0x907d8fc]  ; rip->0x979e410 (file 0x939e410)
0x720b14  (f 0x320b14)  4885c0                 test rax, rax
0x720b17  (f 0x320b17)  0f8597020000           jne 0x720db4
0x720b1d  (f 0x320b1d)  488d3d4d900909         lea rdi, [rip + 0x909904d]  ; rip->0x97b9b71 (file 0x93b9b71)
0x720b24  (f 0x320b24)  be00020000             mov esi, 0x200
0x720b29  (f 0x320b29)  ba08000000             mov edx, 8
0x720b2e  (f 0x320b2e)  b901000000             mov ecx, 1
0x720b33  (f 0x320b33)  e838021600             call 0x880d70
0x720b38  (f 0x320b38)  488d3561c15a07         lea rsi, [rip + 0x75ac161]  ; rip->0x7cccca0 (file 0x78ccca0)
0x720b3f  (f 0x320b3f)  488d7dc8               lea rdi, [rbp - 0x38]
0x720b43  (f 0x320b43)  ba01000000             mov edx, 1
0x720b48  (f 0x320b48)  4889c3                 mov rbx, rax
0x720b4b  (f 0x320b4b)  e840caf1ff             call 0x63d590
0x720b50  (f 0x320b50)  4889df                 mov rdi, rbx
0x720b53  (f 0x320b53)  bac7000000             mov edx, 0xc7
0x720b58  (f 0x320b58)  31f6                   xor esi, esi
0x720b5a  (f 0x320b5a)  e851550a00             call 0x7c60b0
0x720b5f  (f 0x320b5f)  488d05c26ebb07         lea rax, [rip + 0x7bb6ec2]  ; rip->0x82d7a28 (file 0x7ed7a28)
0x720b66  (f 0x320b66)  488d0d73020000         lea rcx, [rip + 0x273]  ; rip->0x720de0 (file 0x320de0)
0x720b6d  (f 0x320b6d)  488d35cc020000         lea rsi, [rip + 0x2cc]  ; rip->0x720e40 (file 0x320e40)
0x720b74  (f 0x320b74)  4c8d35f3645007         lea r14, [rip + 0x75064f3]  ; rip->0x7c2706e (file 0x782706e)
0x720b7b  (f 0x320b7b)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x720b7f  (f 0x320b7f)  c5f8114328             vmovups xmmword ptr [rbx + 0x28], xmm0
0x720b84  (f 0x320b84)  c74338ffffffff         mov dword ptr [rbx + 0x38], 0xffffffff
0x720b8b  (f 0x320b8b)  c5f8114340             vmovups xmmword ptr [rbx + 0x40], xmm0
0x720b90  (f 0x320b90)  48c7435000000000       mov qword ptr [rbx + 0x50], 0
0x720b98  (f 0x320b98)  c74358b0000000         mov dword ptr [rbx + 0x58], 0xb0
0x720b9f  (f 0x320b9f)  66c7435c0800           mov word ptr [rbx + 0x5c], 8
0x720ba5  (f 0x320ba5)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x720ba9  (f 0x320ba9)  c5fc11437e             vmovups ymmword ptr [rbx + 0x7e], ymm0
0x720bae  (f 0x320bae)  c5fc11435e             vmovups ymmword ptr [rbx + 0x5e], ymm0
0x720bb3  (f 0x320bb3)  c5fc118390000000       vmovups ymmword ptr [rbx + 0x90], ymm0
0x720bbb  (f 0x320bbb)  488dbbe8000000         lea rdi, [rbx + 0xe8]
0x720bc2  (f 0x320bc2)  ba01000000             mov edx, 1
0x720bc7  (f 0x320bc7)  488903                 mov qword ptr [rbx], rax
0x720bca  (f 0x320bca)  48898bb0000000         mov qword ptr [rbx + 0xb0], rcx
0x720bd1  (f 0x320bd1)  4889b3b8000000         mov qword ptr [rbx + 0xb8], rsi
0x720bd8  (f 0x320bd8)  488d0df160ceff         lea rcx, [rip - 0x319f0f]  ; rip->0x406cd0 (file 0x6cd0)
0x720bdf  (f 0x320bdf)  4c89f6                 mov rsi, r14
0x720be2  (f 0x320be2)  48898bc0000000         mov qword ptr [rbx + 0xc0], rcx
0x720be9  (f 0x320be9)  48c783c800000000000000 mov qword ptr [rbx + 0xc8], 0
0x720bf4  (f 0x320bf4)  66c783d00000000000     mov word ptr [rbx + 0xd0], 0
0x720bfd  (f 0x320bfd)  c783d4000000a0000010   mov dword ptr [rbx + 0xd4], 0x100000a0
0x720c07  (f 0x320c07)  48c783d800000008000000 mov qword ptr [rbx + 0xd8], 8
0x720c12  (f 0x320c12)  48c783e000000000000000 mov qword ptr [rbx + 0xe0], 0
0x720c1d  (f 0x320c1d)  e86ec9f1ff             call 0x63d590
0x720c22  (f 0x320c22)  c5f8101df6bd7107       vmovups xmm3, xmmword ptr [rip + 0x771bdf6]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x720c2a  (f 0x320c2a)  48c7834801000000000000 mov qword ptr [rbx + 0x148], 0
0x720c35  (f 0x320c35)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x720c39  (f 0x320c39)  c5fc1183f0000000       vmovups ymmword ptr [rbx + 0xf0], ymm0
0x720c41  (f 0x320c41)  c5fc118310010000       vmovups ymmword ptr [rbx + 0x110], ymm0
0x720c49  (f 0x320c49)  48c7833001000000000000 mov qword ptr [rbx + 0x130], 0
0x720c54  (f 0x320c54)  c5e857d2               vxorps xmm2, xmm2, xmm2
0x720c58  (f 0x320c58)  c5f057c9               vxorps xmm1, xmm1, xmm1
0x720c5c  (f 0x320c5c)  c5f8119b50010000       vmovups xmmword ptr [rbx + 0x150], xmm3
0x720c64  (f 0x320c64)  48c7836801000000000000 mov qword ptr [rbx + 0x168], 0
0x720c6f  (f 0x320c6f)  c7837001000000000000   mov dword ptr [rbx + 0x170], 0
0x720c79  (f 0x320c79)  c6837801000000         mov byte ptr [rbx + 0x178], 0
0x720c80  (f 0x320c80)  48c783a001000000000000 mov qword ptr [rbx + 0x1a0], 0
0x720c8b  (f 0x320c8b)  c5f8118b80010000       vmovups xmmword ptr [rbx + 0x180], xmm1
0x720c93  (f 0x320c93)  c5f8119ba8010000       vmovups xmmword ptr [rbx + 0x1a8], xmm3
0x720c9b  (f 0x320c9b)  48c783c001000000000000 mov qword ptr [rbx + 0x1c0], 0
0x720ca6  (f 0x320ca6)  c783c801000000000000   mov dword ptr [rbx + 0x1c8], 0
0x720cb0  (f 0x320cb0)  c683d001000000         mov byte ptr [rbx + 0x1d0], 0
0x720cb7  (f 0x320cb7)  c5fc1193d8010000       vmovups ymmword ptr [rbx + 0x1d8], ymm2
0x720cbf  (f 0x320cbf)  48c783f801000000000000 mov qword ptr [rbx + 0x1f8], 0
0x720cca  (f 0x320cca)  48891d3fd70709         mov qword ptr [rip + 0x907d73f], rbx  ; rip->0x979e410 (file 0x939e410)
0x720cd1  (f 0x320cd1)  4c8b2520d70709         mov r12, qword ptr [rip + 0x907d720]  ; rip->0x979e3f8 (file 0x939e3f8)
0x720cd8  (f 0x320cd8)  4d85e4                 test r12, r12
0x720cdb  (f 0x320cdb)  756d                   jne 0x720d4a
0x720cdd  (f 0x320cdd)  488d05aca5ceff         lea rax, [rip - 0x315a54]  ; rip->0x40b290 (file 0xb290)
0x720ce4  (f 0x320ce4)  4c8d25859affff         lea r12, [rip - 0x657b]  ; rip->0x71a770 (file 0x31a770)
0x720ceb  (f 0x320ceb)  4c8d55d0               lea r10, [rbp - 0x30]
0x720cef  (f 0x320cef)  4c8d1daa9affff         lea r11, [rip - 0x6556]  ; rip->0x71a7a0 (file 0x31a7a0)
0x720cf6  (f 0x320cf6)  488d3dc7635007         lea rdi, [rip + 0x75063c7]  ; rip->0x7c270c4 (file 0x78270c4)
0x720cfd  (f 0x320cfd)  488d35ec914807         lea rsi, [rip + 0x74891ec]  ; rip->0x7ba9ef0 (file 0x77a9ef0)
0x720d04  (f 0x320d04)  488d15edd60709         lea rdx, [rip + 0x907d6ed]  ; rip->0x979e3f8 (file 0x939e3f8)
0x720d0b  (f 0x320d0b)  488d0d7ea5ceff         lea rcx, [rip - 0x315a82]  ; rip->0x40b290 (file 0xb290)
0x720d12  (f 0x320d12)  41b830000000           mov r8d, 0x30
0x720d18  (f 0x320d18)  41b908000000           mov r9d, 8
0x720d1e  (f 0x320d1e)  488945d0               mov qword ptr [rbp - 0x30], rax
0x720d22  (f 0x320d22)  488d05c7b7f7ff         lea rax, [rip - 0x84839]  ; rip->0x69c4f0 (file 0x29c4f0)
0x720d29  (f 0x320d29)  50                     push rax
0x720d2a  (f 0x320d2a)  50                     push rax
0x720d2b  (f 0x320d2b)  4152                   push r10
0x720d2d  (f 0x320d2d)  4153                   push r11
0x720d2f  (f 0x320d2f)  4154                   push r12
0x720d31  (f 0x320d31)  4156                   push r14
0x720d33  (f 0x320d33)  6a01                   push 1
0x720d35  (f 0x320d35)  6801000010             push 0x10000001
0x720d3a  (f 0x320d3a)  e821cbfeff             call 0x70d860
0x720d3f  (f 0x320d3f)  4883c440               add rsp, 0x40
0x720d43  (f 0x320d43)  4c8b25aed60709         mov r12, qword ptr [rip + 0x907d6ae]  ; rip->0x979e3f8 (file 0x939e3f8)
0x720d4a  (f 0x320d4a)  e8614e0a00             call 0x7c5bb0
0x720d4f  (f 0x320d4f)  4989c6                 mov r14, rax
0x720d52  (f 0x320d52)  488b03                 mov rax, qword ptr [rbx]
0x720d55  (f 0x320d55)  31f6                   xor esi, esi
0x720d57  (f 0x320d57)  4939dc                 cmp r12, rbx
0x720d5a  (f 0x320d5a)  4889df                 mov rdi, rbx
0x720d5d  (f 0x320d5d)  490f45f4               cmovne rsi, r12
0x720d61  (f 0x320d61)  ff9028030000           call qword ptr [rax + 0x328]
0x720d67  (f 0x320d67)  4d85f6                 test r14, r14
0x720d6a  (f 0x320d6a)  7414                   je 0x720d80
0x720d6c  (f 0x320d6c)  488d05e9e00709         lea rax, [rip + 0x907e0e9]  ; rip->0x979ee5c (file 0x939ee5c)
0x720d73  (f 0x320d73)  803800                 cmp byte ptr [rax], 0
0x720d76  (f 0x320d76)  7408                   je 0x720d80
0x720d78  (f 0x320d78)  4c89f7                 mov rdi, r14
0x720d7b  (f 0x320d7b)  e840750400             call 0x7682c0
0x720d80  (f 0x320d80)  4c89b3e0000000         mov qword ptr [rbx + 0xe0], r14
0x720d87  (f 0x320d87)  4889df                 mov rdi, rbx
0x720d8a  (f 0x320d8a)  488b03                 mov rax, qword ptr [rbx]
0x720d8d  (f 0x320d8d)  ff5010                 call qword ptr [rax + 0x10]
0x720d90  (f 0x320d90)  488d35a9c0f7ff         lea rsi, [rip - 0x83f57]  ; rip->0x69ce40 (file 0x29ce40)
0x720d97  (f 0x320d97)  488d1526635007         lea rdx, [rip + 0x7506326]  ; rip->0x7c270c4 (file 0x78270c4)
0x720d9e  (f 0x320d9e)  488d0dfbbe5a07         lea rcx, [rip + 0x75abefb]  ; rip->0x7cccca0 (file 0x78ccca0)
0x720da5  (f 0x320da5)  4889df                 mov rdi, rbx
0x720da8  (f 0x320da8)  e8f34e1600             call 0x885ca0
0x720dad  (f 0x320dad)  488b055cd60709         mov rax, qword ptr [rip + 0x907d65c]  ; rip->0x979e410 (file 0x939e410)
0x720db4  (f 0x320db4)  498b0f                 mov rcx, qword ptr [r15]
0x720db7  (f 0x320db7)  483b4dd8               cmp rcx, qword ptr [rbp - 0x28]
0x720dbb  (f 0x320dbb)  750d                   jne 0x720dca
0x720dbd  (f 0x320dbd)  4883c420               add rsp, 0x20
0x720dc1  (f 0x320dc1)  5b                     pop rbx
0x720dc2  (f 0x320dc2)  415c                   pop r12
0x720dc4  (f 0x320dc4)  415e                   pop r14
0x720dc6  (f 0x320dc6)  415f                   pop r15
0x720dc8  (f 0x320dc8)  5d                     pop rbp
0x720dc9  (f 0x320dc9)  c3                     ret 
0x720dca  (f 0x320dca)  e891101507             call 0x7871e60
0x720dcf  (f 0x320dcf)  0f0b                   ud2 
--- calls: [('0xb20b33', '0x880d70'), ('0xb20b4b', '0x63d590'), ('0xb20b5a', '0x7c60b0'), ('0xb20c1d', '0x63d590'), ('0xb20d3a', '0x70d860'), ('0xb20d4a', '0x7c5bb0'), ('0xb20d7b', '0x7682c0'), ('0xb20da8', '0x885ca0'), ('0xb20dca', '0x7871e60')]

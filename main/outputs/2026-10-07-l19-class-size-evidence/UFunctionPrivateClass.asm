0x7284b0  (f 0x3284b0)  55                     push rbp
0x7284b1  (f 0x3284b1)  4889e5                 mov rbp, rsp
0x7284b4  (f 0x3284b4)  4157                   push r15
0x7284b6  (f 0x3284b6)  4156                   push r14
0x7284b8  (f 0x3284b8)  4155                   push r13
0x7284ba  (f 0x3284ba)  4154                   push r12
0x7284bc  (f 0x3284bc)  53                     push rbx
0x7284bd  (f 0x3284bd)  4883ec18               sub rsp, 0x18
0x7284c1  (f 0x3284c1)  4c8b3d409ae008         mov r15, qword ptr [rip + 0x8e09a40]  ; rip->0x9531f08 (file 0x9131f08)
0x7284c8  (f 0x3284c8)  498b07                 mov rax, qword ptr [r15]
0x7284cb  (f 0x3284cb)  488945d0               mov qword ptr [rbp - 0x30], rax
0x7284cf  (f 0x3284cf)  488b05d2620709         mov rax, qword ptr [rip + 0x90762d2]  ; rip->0x979e7a8 (file 0x939e7a8)
0x7284d6  (f 0x3284d6)  4885c0                 test rax, rax
0x7284d9  (f 0x3284d9)  0f859a020000           jne 0x728779
0x7284df  (f 0x3284df)  488d3d8b160909         lea rdi, [rip + 0x909168b]  ; rip->0x97b9b71 (file 0x93b9b71)
0x7284e6  (f 0x3284e6)  be00020000             mov esi, 0x200
0x7284eb  (f 0x3284eb)  ba08000000             mov edx, 8
0x7284f0  (f 0x3284f0)  b901000000             mov ecx, 1
0x7284f5  (f 0x3284f5)  e876881500             call 0x880d70
0x7284fa  (f 0x3284fa)  488d3531cd4207         lea rsi, [rip + 0x742cd31]  ; rip->0x7b55232 (file 0x7755232)
0x728501  (f 0x328501)  488d7dc0               lea rdi, [rbp - 0x40]
0x728505  (f 0x328505)  ba01000000             mov edx, 1
0x72850a  (f 0x32850a)  4889c3                 mov rbx, rax
0x72850d  (f 0x32850d)  e87e50f1ff             call 0x63d590
0x728512  (f 0x328512)  4889df                 mov rdi, rbx
0x728515  (f 0x328515)  bac7000000             mov edx, 0xc7
0x72851a  (f 0x32851a)  31f6                   xor esi, esi
0x72851c  (f 0x32851c)  e88fdb0900             call 0x7c60b0
0x728521  (f 0x328521)  488d0500f5ba07         lea rax, [rip + 0x7baf500]  ; rip->0x82d7a28 (file 0x7ed7a28)
0x728528  (f 0x328528)  488d3571020000         lea rsi, [rip + 0x271]  ; rip->0x7287a0 (file 0x3287a0)
0x72852f  (f 0x32852f)  4c8d3538eb4f07         lea r14, [rip + 0x74feb38]  ; rip->0x7c2706e (file 0x782706e)
0x728536  (f 0x328536)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x72853a  (f 0x32853a)  c5f8114328             vmovups xmmword ptr [rbx + 0x28], xmm0
0x72853f  (f 0x32853f)  c74338ffffffff         mov dword ptr [rbx + 0x38], 0xffffffff
0x728546  (f 0x328546)  c5f8114340             vmovups xmmword ptr [rbx + 0x40], xmm0
0x72854b  (f 0x32854b)  48c7435000000000       mov qword ptr [rbx + 0x50], 0
0x728553  (f 0x328553)  c74358e0000000         mov dword ptr [rbx + 0x58], 0xe0
0x72855a  (f 0x32855a)  66c7435c0800           mov word ptr [rbx + 0x5c], 8
0x728560  (f 0x328560)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x728564  (f 0x328564)  c5fc11437e             vmovups ymmword ptr [rbx + 0x7e], ymm0
0x728569  (f 0x328569)  c5fc11435e             vmovups ymmword ptr [rbx + 0x5e], ymm0
0x72856e  (f 0x32856e)  c5fc118390000000       vmovups ymmword ptr [rbx + 0x90], ymm0
0x728576  (f 0x328576)  488d0d83020000         lea rcx, [rip + 0x283]  ; rip->0x728800 (file 0x328800)
0x72857d  (f 0x32857d)  4c8d254ce7cdff         lea r12, [rip - 0x3218b4]  ; rip->0x406cd0 (file 0x6cd0)
0x728584  (f 0x328584)  488dbbe8000000         lea rdi, [rbx + 0xe8]
0x72858b  (f 0x32858b)  ba01000000             mov edx, 1
0x728590  (f 0x328590)  488903                 mov qword ptr [rbx], rax
0x728593  (f 0x328593)  4889b3b0000000         mov qword ptr [rbx + 0xb0], rsi
0x72859a  (f 0x32859a)  4c89f6                 mov rsi, r14
0x72859d  (f 0x32859d)  48898bb8000000         mov qword ptr [rbx + 0xb8], rcx
0x7285a4  (f 0x3285a4)  4c89a3c0000000         mov qword ptr [rbx + 0xc0], r12
0x7285ab  (f 0x3285ab)  48c783c800000000000000 mov qword ptr [rbx + 0xc8], 0
0x7285b6  (f 0x3285b6)  66c783d00000000000     mov word ptr [rbx + 0xd0], 0
0x7285bf  (f 0x3285bf)  c783d400000080000010   mov dword ptr [rbx + 0xd4], 0x10000080
0x7285c9  (f 0x3285c9)  48c783d800000000000800 mov qword ptr [rbx + 0xd8], 0x80000
0x7285d4  (f 0x3285d4)  48c783e000000000000000 mov qword ptr [rbx + 0xe0], 0
0x7285df  (f 0x3285df)  e8ac4ff1ff             call 0x63d590
0x7285e4  (f 0x3285e4)  c5f8101d34447107       vmovups xmm3, xmmword ptr [rip + 0x7714434]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x7285ec  (f 0x3285ec)  48c7834801000000000000 mov qword ptr [rbx + 0x148], 0
0x7285f7  (f 0x3285f7)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x7285fb  (f 0x3285fb)  c5fc1183f0000000       vmovups ymmword ptr [rbx + 0xf0], ymm0
0x728603  (f 0x328603)  c5fc118310010000       vmovups ymmword ptr [rbx + 0x110], ymm0
0x72860b  (f 0x32860b)  48c7833001000000000000 mov qword ptr [rbx + 0x130], 0
0x728616  (f 0x328616)  c5e857d2               vxorps xmm2, xmm2, xmm2
0x72861a  (f 0x32861a)  c5f057c9               vxorps xmm1, xmm1, xmm1
0x72861e  (f 0x32861e)  c5f8119b50010000       vmovups xmmword ptr [rbx + 0x150], xmm3
0x728626  (f 0x328626)  48c7836801000000000000 mov qword ptr [rbx + 0x168], 0
0x728631  (f 0x328631)  c7837001000000000000   mov dword ptr [rbx + 0x170], 0
0x72863b  (f 0x32863b)  c6837801000000         mov byte ptr [rbx + 0x178], 0
0x728642  (f 0x328642)  48c783a001000000000000 mov qword ptr [rbx + 0x1a0], 0
0x72864d  (f 0x32864d)  c5f8118b80010000       vmovups xmmword ptr [rbx + 0x180], xmm1
0x728655  (f 0x328655)  c5f8119ba8010000       vmovups xmmword ptr [rbx + 0x1a8], xmm3
0x72865d  (f 0x32865d)  48c783c001000000000000 mov qword ptr [rbx + 0x1c0], 0
0x728668  (f 0x328668)  c783c801000000000000   mov dword ptr [rbx + 0x1c8], 0
0x728672  (f 0x328672)  c683d001000000         mov byte ptr [rbx + 0x1d0], 0
0x728679  (f 0x328679)  c5fc1193d8010000       vmovups ymmword ptr [rbx + 0x1d8], ymm2
0x728681  (f 0x328681)  48c783f801000000000000 mov qword ptr [rbx + 0x1f8], 0
0x72868c  (f 0x32868c)  48891d15610709         mov qword ptr [rip + 0x9076115], rbx  ; rip->0x979e7a8 (file 0x939e7a8)
0x728693  (f 0x328693)  4c8b2d765d0709         mov r13, qword ptr [rip + 0x9075d76]  ; rip->0x979e410 (file 0x939e410)
0x72869a  (f 0x32869a)  4d85ed                 test r13, r13
0x72869d  (f 0x32869d)  756e                   jne 0x72870d
0x72869f  (f 0x32869f)  488d054a3ef7ff         lea rax, [rip - 0x8c1b6]  ; rip->0x69c4f0 (file 0x29c4f0)
0x7286a6  (f 0x3286a6)  4c8d2d3387ffff         lea r13, [rip - 0x78cd]  ; rip->0x720de0 (file 0x320de0)
0x7286ad  (f 0x3286ad)  4c8965c8               mov qword ptr [rbp - 0x38], r12
0x7286b1  (f 0x3286b1)  4c8d155821ffff         lea r10, [rip - 0xdea8]  ; rip->0x71a810 (file 0x31a810)
0x7286b8  (f 0x3286b8)  4c8d5dc8               lea r11, [rbp - 0x38]
0x7286bc  (f 0x3286bc)  4c8d257d87ffff         lea r12, [rip - 0x7883]  ; rip->0x720e40 (file 0x320e40)
0x7286c3  (f 0x3286c3)  488d3dfae94f07         lea rdi, [rip + 0x74fe9fa]  ; rip->0x7c270c4 (file 0x78270c4)
0x7286ca  (f 0x3286ca)  488d35cf455a07         lea rsi, [rip + 0x75a45cf]  ; rip->0x7cccca0 (file 0x78ccca0)
0x7286d1  (f 0x3286d1)  488d15385d0709         lea rdx, [rip + 0x9075d38]  ; rip->0x979e410 (file 0x939e410)
0x7286d8  (f 0x3286d8)  488d0db12bceff         lea rcx, [rip - 0x31d44f]  ; rip->0x40b290 (file 0xb290)
0x7286df  (f 0x3286df)  41b8b0000000           mov r8d, 0xb0
0x7286e5  (f 0x3286e5)  41b908000000           mov r9d, 8
0x7286eb  (f 0x3286eb)  50                     push rax
0x7286ec  (f 0x3286ec)  4152                   push r10
0x7286ee  (f 0x3286ee)  4153                   push r11
0x7286f0  (f 0x3286f0)  4154                   push r12
0x7286f2  (f 0x3286f2)  4155                   push r13
0x7286f4  (f 0x3286f4)  4156                   push r14
0x7286f6  (f 0x3286f6)  6a08                   push 8
0x7286f8  (f 0x3286f8)  6820000010             push 0x10000020
0x7286fd  (f 0x3286fd)  e85e51feff             call 0x70d860
0x728702  (f 0x328702)  4883c440               add rsp, 0x40
0x728706  (f 0x328706)  4c8b2d035d0709         mov r13, qword ptr [rip + 0x9075d03]  ; rip->0x979e410 (file 0x939e410)
0x72870d  (f 0x32870d)  e8aeddffff             call 0x7264c0
0x728712  (f 0x328712)  4c8b23                 mov r12, qword ptr [rbx]
0x728715  (f 0x328715)  31f6                   xor esi, esi
0x728717  (f 0x328717)  4939dd                 cmp r13, rbx
0x72871a  (f 0x32871a)  4889df                 mov rdi, rbx
0x72871d  (f 0x32871d)  4989c6                 mov r14, rax
0x728720  (f 0x328720)  490f45f5               cmovne rsi, r13
0x728724  (f 0x328724)  41ff942428030000       call qword ptr [r12 + 0x328]
0x72872c  (f 0x32872c)  4d85f6                 test r14, r14
0x72872f  (f 0x32872f)  7414                   je 0x728745
0x728731  (f 0x328731)  488d0524670709         lea rax, [rip + 0x9076724]  ; rip->0x979ee5c (file 0x939ee5c)
0x728738  (f 0x328738)  803800                 cmp byte ptr [rax], 0
0x72873b  (f 0x32873b)  7408                   je 0x728745
0x72873d  (f 0x32873d)  4c89f7                 mov rdi, r14
0x728740  (f 0x328740)  e87bfb0300             call 0x7682c0
0x728745  (f 0x328745)  4c89b3e0000000         mov qword ptr [rbx + 0xe0], r14
0x72874c  (f 0x32874c)  4889df                 mov rdi, rbx
0x72874f  (f 0x32874f)  488b03                 mov rax, qword ptr [rbx]
0x728752  (f 0x328752)  ff5010                 call qword ptr [rax + 0x10]
0x728755  (f 0x328755)  488d35e446f7ff         lea rsi, [rip - 0x8b91c]  ; rip->0x69ce40 (file 0x29ce40)
0x72875c  (f 0x32875c)  488d1561e94f07         lea rdx, [rip + 0x74fe961]  ; rip->0x7c270c4 (file 0x78270c4)
0x728763  (f 0x328763)  488d0dc8ca4207         lea rcx, [rip + 0x742cac8]  ; rip->0x7b55232 (file 0x7755232)
0x72876a  (f 0x32876a)  4889df                 mov rdi, rbx
0x72876d  (f 0x32876d)  e82ed51500             call 0x885ca0
0x728772  (f 0x328772)  488b052f600709         mov rax, qword ptr [rip + 0x907602f]  ; rip->0x979e7a8 (file 0x939e7a8)
0x728779  (f 0x328779)  498b0f                 mov rcx, qword ptr [r15]
0x72877c  (f 0x32877c)  483b4dd0               cmp rcx, qword ptr [rbp - 0x30]
0x728780  (f 0x328780)  750f                   jne 0x728791
0x728782  (f 0x328782)  4883c418               add rsp, 0x18
0x728786  (f 0x328786)  5b                     pop rbx
0x728787  (f 0x328787)  415c                   pop r12
0x728789  (f 0x328789)  415d                   pop r13
0x72878b  (f 0x32878b)  415e                   pop r14
0x72878d  (f 0x32878d)  415f                   pop r15
0x72878f  (f 0x32878f)  5d                     pop rbp
0x728790  (f 0x328790)  c3                     ret 
0x728791  (f 0x328791)  e8ca961407             call 0x7871e60
0x728796  (f 0x328796)  0f0b                   ud2 
--- calls: [('0xb284f5', '0x880d70'), ('0xb2850d', '0x63d590'), ('0xb2851c', '0x7c60b0'), ('0xb285df', '0x63d590'), ('0xb286fd', '0x70d860'), ('0xb2870d', '0x7264c0'), ('0xb28740', '0x7682c0'), ('0xb2876d', '0x885ca0'), ('0xb28791', '0x7871e60')]

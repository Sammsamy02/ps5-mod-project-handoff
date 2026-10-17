0x7264c0  (f 0x3264c0)  55                     push rbp
0x7264c1  (f 0x3264c1)  4889e5                 mov rbp, rsp
0x7264c4  (f 0x3264c4)  4157                   push r15
0x7264c6  (f 0x3264c6)  4156                   push r14
0x7264c8  (f 0x3264c8)  4155                   push r13
0x7264ca  (f 0x3264ca)  4154                   push r12
0x7264cc  (f 0x3264cc)  53                     push rbx
0x7264cd  (f 0x3264cd)  4883ec18               sub rsp, 0x18
0x7264d1  (f 0x3264d1)  4c8b3d30bae008         mov r15, qword ptr [rip + 0x8e0ba30]  ; rip->0x9531f08 (file 0x9131f08)
0x7264d8  (f 0x3264d8)  498b07                 mov rax, qword ptr [r15]
0x7264db  (f 0x3264db)  488945d0               mov qword ptr [rbp - 0x30], rax
0x7264df  (f 0x3264df)  488b056a800709         mov rax, qword ptr [rip + 0x907806a]  ; rip->0x979e550 (file 0x939e550)
0x7264e6  (f 0x3264e6)  4885c0                 test rax, rax
0x7264e9  (f 0x3264e9)  0f85a0020000           jne 0x72678f
0x7264ef  (f 0x3264ef)  488d3d7b360909         lea rdi, [rip + 0x909367b]  ; rip->0x97b9b71 (file 0x93b9b71)
0x7264f6  (f 0x3264f6)  be00020000             mov esi, 0x200
0x7264fb  (f 0x3264fb)  ba08000000             mov edx, 8
0x726500  (f 0x326500)  b901000000             mov ecx, 1
0x726505  (f 0x326505)  e866a81500             call 0x880d70
0x72650a  (f 0x32650a)  488d353d823807         lea rsi, [rip + 0x738823d]  ; rip->0x7aae74e (file 0x76ae74e)
0x726511  (f 0x326511)  488d7dc0               lea rdi, [rbp - 0x40]
0x726515  (f 0x326515)  ba01000000             mov edx, 1
0x72651a  (f 0x32651a)  4889c3                 mov rbx, rax
0x72651d  (f 0x32651d)  e86e70f1ff             call 0x63d590
0x726522  (f 0x326522)  4889df                 mov rdi, rbx
0x726525  (f 0x326525)  bac7000000             mov edx, 0xc7
0x72652a  (f 0x32652a)  31f6                   xor esi, esi
0x72652c  (f 0x32652c)  e87ffb0900             call 0x7c60b0
0x726531  (f 0x326531)  488d05f014bb07         lea rax, [rip + 0x7bb14f0]  ; rip->0x82d7a28 (file 0x7ed7a28)
0x726538  (f 0x326538)  488d0d71020000         lea rcx, [rip + 0x271]  ; rip->0x7267b0 (file 0x3267b0)
0x72653f  (f 0x32653f)  488d35ba030000         lea rsi, [rip + 0x3ba]  ; rip->0x726900 (file 0x326900)
0x726546  (f 0x326546)  4c8d35210b5007         lea r14, [rip + 0x7500b21]  ; rip->0x7c2706e (file 0x782706e)
0x72654d  (f 0x32654d)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x726551  (f 0x326551)  c5f8114328             vmovups xmmword ptr [rbx + 0x28], xmm0
0x726556  (f 0x326556)  c74338ffffffff         mov dword ptr [rbx + 0x38], 0xffffffff
0x72655d  (f 0x32655d)  c5f8114340             vmovups xmmword ptr [rbx + 0x40], xmm0
0x726562  (f 0x326562)  48c7435000000000       mov qword ptr [rbx + 0x50], 0
0x72656a  (f 0x32656a)  c7435800020000         mov dword ptr [rbx + 0x58], 0x200
0x726571  (f 0x326571)  66c7435c0800           mov word ptr [rbx + 0x5c], 8
0x726577  (f 0x326577)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x72657b  (f 0x32657b)  c5fc11437e             vmovups ymmword ptr [rbx + 0x7e], ymm0
0x726580  (f 0x326580)  c5fc11435e             vmovups ymmword ptr [rbx + 0x5e], ymm0
0x726585  (f 0x326585)  c5fc118390000000       vmovups ymmword ptr [rbx + 0x90], ymm0
0x72658d  (f 0x32658d)  488dbbe8000000         lea rdi, [rbx + 0xe8]
0x726594  (f 0x326594)  ba01000000             mov edx, 1
0x726599  (f 0x326599)  488903                 mov qword ptr [rbx], rax
0x72659c  (f 0x32659c)  48898bb0000000         mov qword ptr [rbx + 0xb0], rcx
0x7265a3  (f 0x3265a3)  4889b3b8000000         mov qword ptr [rbx + 0xb8], rsi
0x7265aa  (f 0x3265aa)  488d0d3f37ffff         lea rcx, [rip - 0xc8c1]  ; rip->0x719cf0 (file 0x319cf0)
0x7265b1  (f 0x3265b1)  4c89f6                 mov rsi, r14
0x7265b4  (f 0x3265b4)  48898bc0000000         mov qword ptr [rbx + 0xc0], rcx
0x7265bb  (f 0x3265bb)  48c783c800000000000000 mov qword ptr [rbx + 0xc8], 0
0x7265c6  (f 0x3265c6)  66c783d00000000000     mov word ptr [rbx + 0xd0], 0
0x7265cf  (f 0x3265cf)  c783d400000080000010   mov dword ptr [rbx + 0xd4], 0x10000080
0x7265d9  (f 0x3265d9)  48c783d800000020000000 mov qword ptr [rbx + 0xd8], 0x20
0x7265e4  (f 0x3265e4)  48c783e000000000000000 mov qword ptr [rbx + 0xe0], 0
0x7265ef  (f 0x3265ef)  e89c6ff1ff             call 0x63d590
0x7265f4  (f 0x3265f4)  c5f8101d24647107       vmovups xmm3, xmmword ptr [rip + 0x7716424]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x7265fc  (f 0x3265fc)  48c7834801000000000000 mov qword ptr [rbx + 0x148], 0
0x726607  (f 0x326607)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x72660b  (f 0x32660b)  c5fc1183f0000000       vmovups ymmword ptr [rbx + 0xf0], ymm0
0x726613  (f 0x326613)  c5fc118310010000       vmovups ymmword ptr [rbx + 0x110], ymm0
0x72661b  (f 0x32661b)  48c7833001000000000000 mov qword ptr [rbx + 0x130], 0
0x726626  (f 0x326626)  c5e857d2               vxorps xmm2, xmm2, xmm2
0x72662a  (f 0x32662a)  c5f057c9               vxorps xmm1, xmm1, xmm1
0x72662e  (f 0x32662e)  c5f8119b50010000       vmovups xmmword ptr [rbx + 0x150], xmm3
0x726636  (f 0x326636)  48c7836801000000000000 mov qword ptr [rbx + 0x168], 0
0x726641  (f 0x326641)  c7837001000000000000   mov dword ptr [rbx + 0x170], 0
0x72664b  (f 0x32664b)  c6837801000000         mov byte ptr [rbx + 0x178], 0
0x726652  (f 0x326652)  48c783a001000000000000 mov qword ptr [rbx + 0x1a0], 0
0x72665d  (f 0x32665d)  c5f8118b80010000       vmovups xmmword ptr [rbx + 0x180], xmm1
0x726665  (f 0x326665)  c5f8119ba8010000       vmovups xmmword ptr [rbx + 0x1a8], xmm3
0x72666d  (f 0x32666d)  48c783c001000000000000 mov qword ptr [rbx + 0x1c0], 0
0x726678  (f 0x326678)  c783c801000000000000   mov dword ptr [rbx + 0x1c8], 0
0x726682  (f 0x326682)  c683d001000000         mov byte ptr [rbx + 0x1d0], 0
0x726689  (f 0x326689)  c5fc1193d8010000       vmovups ymmword ptr [rbx + 0x1d8], ymm2
0x726691  (f 0x326691)  48c783f801000000000000 mov qword ptr [rbx + 0x1f8], 0
0x72669c  (f 0x32669c)  48891dad7e0709         mov qword ptr [rip + 0x9077ead], rbx  ; rip->0x979e550 (file 0x939e550)
0x7266a3  (f 0x3266a3)  4c8b25667d0709         mov r12, qword ptr [rip + 0x9077d66]  ; rip->0x979e410 (file 0x939e410)
0x7266aa  (f 0x3266aa)  4d85e4                 test r12, r12
0x7266ad  (f 0x3266ad)  7575                   jne 0x726724
0x7266af  (f 0x3266af)  488d051a06ceff         lea rax, [rip - 0x31f9e6]  ; rip->0x406cd0 (file 0x6cd0)
0x7266b6  (f 0x3266b6)  4c8d2583a7ffff         lea r12, [rip - 0x587d]  ; rip->0x720e40 (file 0x320e40)
0x7266bd  (f 0x3266bd)  4c8d154c41ffff         lea r10, [rip - 0xbeb4]  ; rip->0x71a810 (file 0x31a810)
0x7266c4  (f 0x3266c4)  4c8d5dc8               lea r11, [rbp - 0x38]
0x7266c8  (f 0x3266c8)  4c8d2d11a7ffff         lea r13, [rip - 0x58ef]  ; rip->0x720de0 (file 0x320de0)
0x7266cf  (f 0x3266cf)  488d3dee095007         lea rdi, [rip + 0x75009ee]  ; rip->0x7c270c4 (file 0x78270c4)
0x7266d6  (f 0x3266d6)  488d35c3655a07         lea rsi, [rip + 0x75a65c3]  ; rip->0x7cccca0 (file 0x78ccca0)
0x7266dd  (f 0x3266dd)  488d152c7d0709         lea rdx, [rip + 0x9077d2c]  ; rip->0x979e410 (file 0x939e410)
0x7266e4  (f 0x3266e4)  488d0da54bceff         lea rcx, [rip - 0x31b45b]  ; rip->0x40b290 (file 0xb290)
0x7266eb  (f 0x3266eb)  41b8b0000000           mov r8d, 0xb0
0x7266f1  (f 0x3266f1)  41b908000000           mov r9d, 8
0x7266f7  (f 0x3266f7)  488945c8               mov qword ptr [rbp - 0x38], rax
0x7266fb  (f 0x3266fb)  488d05ee5df7ff         lea rax, [rip - 0x8a212]  ; rip->0x69c4f0 (file 0x29c4f0)
0x726702  (f 0x326702)  50                     push rax
0x726703  (f 0x326703)  4152                   push r10
0x726705  (f 0x326705)  4153                   push r11
0x726707  (f 0x326707)  4154                   push r12
0x726709  (f 0x326709)  4155                   push r13
0x72670b  (f 0x32670b)  4156                   push r14
0x72670d  (f 0x32670d)  6a08                   push 8
0x72670f  (f 0x32670f)  6820000010             push 0x10000020
0x726714  (f 0x326714)  e84771feff             call 0x70d860
0x726719  (f 0x326719)  4883c440               add rsp, 0x40
0x72671d  (f 0x32671d)  4c8b25ec7c0709         mov r12, qword ptr [rip + 0x9077cec]  ; rip->0x979e410 (file 0x939e410)
0x726724  (f 0x326724)  e8772f0b00             call 0x7d96a0
0x726729  (f 0x326729)  4c8b2b                 mov r13, qword ptr [rbx]
0x72672c  (f 0x32672c)  31f6                   xor esi, esi
0x72672e  (f 0x32672e)  4939dc                 cmp r12, rbx
0x726731  (f 0x326731)  4889df                 mov rdi, rbx
0x726734  (f 0x326734)  4989c6                 mov r14, rax
0x726737  (f 0x326737)  490f45f4               cmovne rsi, r12
0x72673b  (f 0x32673b)  41ff9528030000         call qword ptr [r13 + 0x328]
0x726742  (f 0x326742)  4d85f6                 test r14, r14
0x726745  (f 0x326745)  7414                   je 0x72675b
0x726747  (f 0x326747)  488d050e870709         lea rax, [rip + 0x907870e]  ; rip->0x979ee5c (file 0x939ee5c)
0x72674e  (f 0x32674e)  803800                 cmp byte ptr [rax], 0
0x726751  (f 0x326751)  7408                   je 0x72675b
0x726753  (f 0x326753)  4c89f7                 mov rdi, r14
0x726756  (f 0x326756)  e8651b0400             call 0x7682c0
0x72675b  (f 0x32675b)  4c89b3e0000000         mov qword ptr [rbx + 0xe0], r14
0x726762  (f 0x326762)  4889df                 mov rdi, rbx
0x726765  (f 0x326765)  488b03                 mov rax, qword ptr [rbx]
0x726768  (f 0x326768)  ff5010                 call qword ptr [rax + 0x10]
0x72676b  (f 0x32676b)  488d35ce66f7ff         lea rsi, [rip - 0x89932]  ; rip->0x69ce40 (file 0x29ce40)
0x726772  (f 0x326772)  488d154b095007         lea rdx, [rip + 0x750094b]  ; rip->0x7c270c4 (file 0x78270c4)
0x726779  (f 0x326779)  488d0dce7f3807         lea rcx, [rip + 0x7387fce]  ; rip->0x7aae74e (file 0x76ae74e)
0x726780  (f 0x326780)  4889df                 mov rdi, rbx
0x726783  (f 0x326783)  e818f51500             call 0x885ca0
0x726788  (f 0x326788)  488b05c17d0709         mov rax, qword ptr [rip + 0x9077dc1]  ; rip->0x979e550 (file 0x939e550)
0x72678f  (f 0x32678f)  498b0f                 mov rcx, qword ptr [r15]
0x726792  (f 0x326792)  483b4dd0               cmp rcx, qword ptr [rbp - 0x30]
0x726796  (f 0x326796)  750f                   jne 0x7267a7
0x726798  (f 0x326798)  4883c418               add rsp, 0x18
0x72679c  (f 0x32679c)  5b                     pop rbx
0x72679d  (f 0x32679d)  415c                   pop r12
0x72679f  (f 0x32679f)  415d                   pop r13
0x7267a1  (f 0x3267a1)  415e                   pop r14
0x7267a3  (f 0x3267a3)  415f                   pop r15
0x7267a5  (f 0x3267a5)  5d                     pop rbp
0x7267a6  (f 0x3267a6)  c3                     ret 
0x7267a7  (f 0x3267a7)  e8b4b61407             call 0x7871e60
0x7267ac  (f 0x3267ac)  0f0b                   ud2 
--- calls: [('0xb26505', '0x880d70'), ('0xb2651d', '0x63d590'), ('0xb2652c', '0x7c60b0'), ('0xb265ef', '0x63d590'), ('0xb26714', '0x70d860'), ('0xb26724', '0x7d96a0'), ('0xb26756', '0x7682c0'), ('0xb26783', '0x885ca0'), ('0xb267a7', '0x7871e60')]

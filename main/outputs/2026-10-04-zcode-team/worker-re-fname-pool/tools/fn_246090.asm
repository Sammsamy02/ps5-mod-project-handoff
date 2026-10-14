0x646090  (file 0x246090)  55                       push rbp
0x646091  (file 0x246091)  4889e5                   mov rbp, rsp
0x646094  (file 0x246094)  4157                     push r15
0x646096  (file 0x246096)  4156                     push r14
0x646098  (file 0x246098)  4155                     push r13
0x64609a  (file 0x24609a)  4154                     push r12
0x64609c  (file 0x24609c)  53                       push rbx
0x64609d  (file 0x24609d)  4883ec18                 sub rsp, 0x18
0x6460a1  (file 0x2460a1)  4c8b2d60feee08           mov r13, qword ptr [rip + 0x8eefe60]  ; rip->0x9535f08 (file 0x9135f08)
0x6460a8  (file 0x2460a8)  4889fb                   mov rbx, rdi
0x6460ab  (file 0x2460ab)  498b4500                 mov rax, qword ptr [r13]
0x6460af  (file 0x2460af)  488945d0                 mov qword ptr [rbp - 0x30], rax
0x6460b3  (file 0x2460b3)  40f6c601                 test sil, 1
0x6460b7  (file 0x2460b7)  0f8595000000             jne 0x646152  ; -> 0x646152 (file 0x246152)
0x6460bd  (file 0x2460bd)  4c8b7310                 mov r14, qword ptr [rbx + 0x10]
0x6460c1  (file 0x2460c1)  e85a2af3ff               call 0x578b20  ; -> 0x578b20 (file 0x178b20)
0x6460c6  (file 0x2460c6)  488d3595555e07           lea rsi, [rip + 0x75e5595]  ; rip->0x7c2b662 (file 0x782b662)
0x6460cd  (file 0x2460cd)  4889c7                   mov rdi, rax
0x6460d0  (file 0x2460d0)  4989c7                   mov r15, rax
0x6460d3  (file 0x2460d3)  e86886f9ff               call 0x5de740  ; -> 0x5de740 (file 0x1de740)
0x6460d8  (file 0x2460d8)  84c0                     test al, al
0x6460da  (file 0x2460da)  0f85fe010000             jne 0x6462de  ; -> 0x6462de (file 0x2462de)
0x6460e0  (file 0x2460e0)  488d35cfeb6d07           lea rsi, [rip + 0x76debcf]  ; rip->0x7d24cb6 (file 0x7924cb6)
0x6460e7  (file 0x2460e7)  4c89ff                   mov rdi, r15
0x6460ea  (file 0x2460ea)  e85186f9ff               call 0x5de740  ; -> 0x5de740 (file 0x1de740)
0x6460ef  (file 0x2460ef)  84c0                     test al, al
0x6460f1  (file 0x2460f1)  0f85e7010000             jne 0x6462de  ; -> 0x6462de (file 0x2462de)
0x6460f7  (file 0x2460f7)  4c8b3daaad1309           mov r15, qword ptr [rip + 0x913adaa]  ; rip->0x9780ea8 (file 0x9380ea8)
0x6460fe  (file 0x2460fe)  e89da2e7ff               call 0x4c03a0  ; -> 0x4c03a0 (file 0xc03a0)
0x646103  (file 0x246103)  84c0                     test al, al
0x646105  (file 0x246105)  740e                     je 0x646115  ; -> 0x646115 (file 0x246115)
0x646107  (file 0x246107)  31c0                     xor eax, eax
0x646109  (file 0x246109)  41803c0700               cmp byte ptr [r15 + rax], 0
0x64610e  (file 0x24610e)  741a                     je 0x64612a  ; -> 0x64612a (file 0x24612a)
0x646110  (file 0x246110)  e9c9010000               jmp 0x6462de  ; -> 0x6462de (file 0x2462de)
0x646115  (file 0x246115)  e8766fe7ff               call 0x4bd090  ; -> 0x4bd090 (file 0xbd090)
0x64611a  (file 0x24611a)  3401                     xor al, 1
0x64611c  (file 0x24611c)  0fb6c0                   movzx eax, al
0x64611f  (file 0x24611f)  41803c0700               cmp byte ptr [r15 + rax], 0
0x646124  (file 0x246124)  0f85b4010000             jne 0x6462de  ; -> 0x6462de (file 0x2462de)
0x64612a  (file 0x24612a)  488d359deb6d07           lea rsi, [rip + 0x76deb9d]  ; rip->0x7d24cce (file 0x7924cce)
0x646131  (file 0x246131)  488d15ec9d4107           lea rdx, [rip + 0x7419dec]  ; rip->0x7a5ff24 (file 0x765ff24)
0x646138  (file 0x246138)  488d4dc8                 lea rcx, [rbp - 0x38]
0x64613c  (file 0x24613c)  4c89f7                   mov rdi, r14
0x64613f  (file 0x24613f)  c645c800                 mov byte ptr [rbp - 0x38], 0
0x646143  (file 0x246143)  e80884f4ff               call 0x58e550  ; -> 0x58e550 (file 0x18e550)
0x646148  (file 0x246148)  807dc800                 cmp byte ptr [rbp - 0x38], 0
0x64614c  (file 0x24614c)  0f858c010000             jne 0x6462de  ; -> 0x6462de (file 0x2462de)
0x646152  (file 0x246152)  0fb6057fad1309           movzx eax, byte ptr [rip + 0x913ad7f]  ; rip->0x9780ed8 (file 0x9380ed8)
0x646159  (file 0x246159)  84c0                     test al, al
0x64615b  (file 0x24615b)  0f849d010000             je 0x6462fe  ; -> 0x6462fe (file 0x2462fe)
0x646161  (file 0x246161)  488d3d58ad1309           lea rdi, [rip + 0x913ad58]  ; rip->0x9780ec0 (file 0x9380ec0)
0x646168  (file 0x246168)  e873fd2207               call 0x7875ee0  ; -> 0x7875ee0 (file 0x7475ee0)
0x64616d  (file 0x24616d)  85c0                     test eax, eax
0x64616f  (file 0x24616f)  7417                     je 0x646188  ; -> 0x646188 (file 0x246188)
0x646171  (file 0x246171)  48ff0558ad1309           inc qword ptr [rip + 0x913ad58]  ; rip->0x9780ed0 (file 0x9380ed0)
0x646178  (file 0x246178)  48833d30ad130900         cmp qword ptr [rip + 0x913ad30], 0  ; rip->0x9780eb0 (file 0x9380eb0)
0x646180  (file 0x246180)  0f8538010000             jne 0x6462be  ; -> 0x6462be (file 0x2462be)
0x646186  (file 0x246186)  eb21                     jmp 0x6461a9  ; -> 0x6461a9 (file 0x2461a9)
0x646188  (file 0x246188)  e863fd2207               call 0x7875ef0  ; -> 0x7875ef0 (file 0x7475ef0)
0x64618d  (file 0x24618d)  48890534ad1309           mov qword ptr [rip + 0x913ad34], rax  ; rip->0x9780ec8 (file 0x9380ec8)
0x646194  (file 0x246194)  48ff0535ad1309           inc qword ptr [rip + 0x913ad35]  ; rip->0x9780ed0 (file 0x9380ed0)
0x64619b  (file 0x24619b)  48833d0dad130900         cmp qword ptr [rip + 0x913ad0d], 0  ; rip->0x9780eb0 (file 0x9380eb0)
0x6461a3  (file 0x2461a3)  0f8515010000             jne 0x6462be  ; -> 0x6462be (file 0x2462be)
0x6461a9  (file 0x2461a9)  488b7b10                 mov rdi, qword ptr [rbx + 0x10]
0x6461ad  (file 0x2461ad)  e80efcffff               call 0x645dc0  ; -> 0x645dc0 (file 0x245dc0)
0x6461b2  (file 0x2461b2)  4885c0                   test rax, rax
0x6461b5  (file 0x2461b5)  746f                     je 0x646226  ; -> 0x646226 (file 0x246226)
0x6461b7  (file 0x2461b7)  4889c7                   mov rdi, rax
0x6461ba  (file 0x2461ba)  4989c6                   mov r14, rax
0x6461bd  (file 0x2461bd)  e8befaffff               call 0x645c80  ; -> 0x645c80 (file 0x245c80)
0x6461c2  (file 0x2461c2)  4885c0                   test rax, rax
0x6461c5  (file 0x2461c5)  745f                     je 0x646226  ; -> 0x646226 (file 0x246226)
0x6461c7  (file 0x2461c7)  488b08                   mov rcx, qword ptr [rax]
0x6461ca  (file 0x2461ca)  488d7dc8                 lea rdi, [rbp - 0x38]
0x6461ce  (file 0x2461ce)  4889c6                   mov rsi, rax
0x6461d1  (file 0x2461d1)  ff11                     call qword ptr [rcx]
0x6461d3  (file 0x2461d3)  488b7dc8                 mov rdi, qword ptr [rbp - 0x38]
0x6461d7  (file 0x2461d7)  4889de                   mov rsi, rbx
0x6461da  (file 0x2461da)  488b07                   mov rax, qword ptr [rdi]
0x6461dd  (file 0x2461dd)  ff5010                   call qword ptr [rax + 0x10]
0x6461e0  (file 0x2461e0)  488b7dc8                 mov rdi, qword ptr [rbp - 0x38]
0x6461e4  (file 0x2461e4)  84c0                     test al, al
0x6461e6  (file 0x2461e6)  7433                     je 0x64621b  ; -> 0x64621b (file 0x24621b)
0x6461e8  (file 0x2461e8)  488b05c1ac1309           mov rax, qword ptr [rip + 0x913acc1]  ; rip->0x9780eb0 (file 0x9380eb0)
0x6461ef  (file 0x2461ef)  48893dbaac1309           mov qword ptr [rip + 0x913acba], rdi  ; rip->0x9780eb0 (file 0x9380eb0)
0x6461f6  (file 0x2461f6)  48c745c800000000         mov qword ptr [rbp - 0x38], 0
0x6461fe  (file 0x2461fe)  4885c0                   test rax, rax
0x646201  (file 0x246201)  740f                     je 0x646212  ; -> 0x646212 (file 0x246212)
0x646203  (file 0x246203)  488b08                   mov rcx, qword ptr [rax]
0x646206  (file 0x246206)  4889c7                   mov rdi, rax
0x646209  (file 0x246209)  ff5108                   call qword ptr [rcx + 8]
0x64620c  (file 0x24620c)  488b7dc8                 mov rdi, qword ptr [rbp - 0x38]
0x646210  (file 0x246210)  eb02                     jmp 0x646214  ; -> 0x646214 (file 0x246214)
0x646212  (file 0x246212)  31ff                     xor edi, edi
0x646214  (file 0x246214)  4c89359dac1309           mov qword ptr [rip + 0x913ac9d], r14  ; rip->0x9780eb8 (file 0x9380eb8)
0x64621b  (file 0x24621b)  4885ff                   test rdi, rdi
0x64621e  (file 0x24621e)  7406                     je 0x646226  ; -> 0x646226 (file 0x246226)
0x646220  (file 0x246220)  488b07                   mov rax, qword ptr [rdi]
0x646223  (file 0x246223)  ff5008                   call qword ptr [rax + 8]
0x646226  (file 0x246226)  48833d82ac130900         cmp qword ptr [rip + 0x913ac82], 0  ; rip->0x9780eb0 (file 0x9380eb0)
0x64622e  (file 0x24622e)  0f858a000000             jne 0x6462be  ; -> 0x6462be (file 0x2462be)
0x646234  (file 0x246234)  bf38000000               mov edi, 0x38
0x646239  (file 0x246239)  e8424a6504               call 0x4c9ac80  ; -> 0x4c9ac80 (file 0x489ac80)
0x64623e  (file 0x24623e)  4c8d65c8                 lea r12, [rbp - 0x38]
0x646242  (file 0x246242)  4989c6                   mov r14, rax
0x646245  (file 0x246245)  488d05349ec807           lea rax, [rip + 0x7c89e34]  ; rip->0x82d0080 (file 0x7ed0080)
0x64624c  (file 0x24624c)  c5f857c0                 vxorps xmm0, xmm0, xmm0
0x646250  (file 0x246250)  4c89e7                   mov rdi, r12
0x646253  (file 0x246253)  498906                   mov qword ptr [r14], rax
0x646256  (file 0x246256)  4d8d7e08                 lea r15, [r14 + 8]
0x64625a  (file 0x24625a)  c4c178114610             vmovups xmmword ptr [r14 + 0x10], xmm0
0x646260  (file 0x246260)  e86bfd2207               call 0x7875fd0  ; -> 0x7875fd0 (file 0x7475fd0)
0x646265  (file 0x246265)  4c89e7                   mov rdi, r12
0x646268  (file 0x246268)  be04000000               mov esi, 4
0x64626d  (file 0x24626d)  e86efd2207               call 0x7875fe0  ; -> 0x7875fe0 (file 0x7475fe0)
0x646272  (file 0x246272)  4c89ff                   mov rdi, r15
0x646275  (file 0x246275)  4c89e6                   mov rsi, r12
0x646278  (file 0x246278)  31d2                     xor edx, edx
0x64627a  (file 0x24627a)  e871fd2207               call 0x7875ff0  ; -> 0x7875ff0 (file 0x7475ff0)
0x64627f  (file 0x24627f)  4c89e7                   mov rdi, r12
0x646282  (file 0x246282)  e879fd2207               call 0x7876000  ; -> 0x7876000 (file 0x7476000)
0x646287  (file 0x246287)  c5f857c0                 vxorps xmm0, xmm0, xmm0
0x64628b  (file 0x24628b)  c4c178114620             vmovups xmmword ptr [r14 + 0x20], xmm0
0x646291  (file 0x246291)  49c7463002000000         mov qword ptr [r14 + 0x30], 2
0x646299  (file 0x246299)  4c89f7                   mov rdi, r14
0x64629c  (file 0x24629c)  4889de                   mov rsi, rbx
0x64629f  (file 0x24629f)  498b06                   mov rax, qword ptr [r14]
0x6462a2  (file 0x2462a2)  ff5010                   call qword ptr [rax + 0x10]
0x6462a5  (file 0x2462a5)  488b3d04ac1309           mov rdi, qword ptr [rip + 0x913ac04]  ; rip->0x9780eb0 (file 0x9380eb0)
0x6462ac  (file 0x2462ac)  4c8935fdab1309           mov qword ptr [rip + 0x913abfd], r14  ; rip->0x9780eb0 (file 0x9380eb0)
0x6462b3  (file 0x2462b3)  4885ff                   test rdi, rdi
0x6462b6  (file 0x2462b6)  7406                     je 0x6462be  ; -> 0x6462be (file 0x2462be)
0x6462b8  (file 0x2462b8)  488b07                   mov rax, qword ptr [rdi]
0x6462bb  (file 0x2462bb)  ff5008                   call qword ptr [rax + 8]
0x6462be  (file 0x2462be)  48ff0d0bac1309           dec qword ptr [rip + 0x913ac0b]  ; rip->0x9780ed0 (file 0x9380ed0)
0x6462c5  (file 0x2462c5)  7517                     jne 0x6462de  ; -> 0x6462de (file 0x2462de)
0x6462c7  (file 0x2462c7)  488d3df2ab1309           lea rdi, [rip + 0x913abf2]  ; rip->0x9780ec0 (file 0x9380ec0)
0x6462ce  (file 0x2462ce)  48c705efab130900000000   mov qword ptr [rip + 0x913abef], 0  ; rip->0x9780ec8 (file 0x9380ec8)
0x6462d9  (file 0x2462d9)  e842fc2207               call 0x7875f20  ; -> 0x7875f20 (file 0x7475f20)
0x6462de  (file 0x2462de)  498b4500                 mov rax, qword ptr [r13]
0x6462e2  (file 0x2462e2)  483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x6462e6  (file 0x2462e6)  750f                     jne 0x6462f7  ; -> 0x6462f7 (file 0x2462f7)
0x6462e8  (file 0x2462e8)  4883c418                 add rsp, 0x18
0x6462ec  (file 0x2462ec)  5b                       pop rbx
0x6462ed  (file 0x2462ed)  415c                     pop r12
0x6462ef  (file 0x2462ef)  415d                     pop r13
0x6462f1  (file 0x2462f1)  415e                     pop r14
0x6462f3  (file 0x2462f3)  415f                     pop r15
0x6462f5  (file 0x2462f5)  5d                       pop rbp
0x6462f6  (file 0x2462f6)  c3                       ret 
0x6462f7  (file 0x2462f7)  e864fb2207               call 0x7875e60  ; -> 0x7875e60 (file 0x7475e60)
0x6462fc  (file 0x2462fc)  0f0b                     ud2 
0x6462fe  (file 0x2462fe)  488d3dd3ab1309           lea rdi, [rip + 0x913abd3]  ; rip->0x9780ed8 (file 0x9380ed8)
0x646305  (file 0x246305)  e8a6fb2207               call 0x7875eb0  ; -> 0x7875eb0 (file 0x7475eb0)
0x64630a  (file 0x24630a)  85c0                     test eax, eax
0x64630c  (file 0x24630c)  0f844ffeffff             je 0x646161  ; -> 0x646161 (file 0x246161)
0x646312  (file 0x246312)  4c8d7dc8                 lea r15, [rbp - 0x38]
0x646316  (file 0x246316)  c5f857c0                 vxorps xmm0, xmm0, xmm0
0x64631a  (file 0x24631a)  c5f81105a6ab1309         vmovups xmmword ptr [rip + 0x913aba6], xmm0  ; rip->0x9780ec8 (file 0x9380ec8)
0x646322  (file 0x246322)  4c8d3597ab1309           lea r14, [rip + 0x913ab97]  ; rip->0x9780ec0 (file 0x9380ec0)
0x646329  (file 0x246329)  4c89ff                   mov rdi, r15
0x64632c  (file 0x24632c)  e89ffc2207               call 0x7875fd0  ; -> 0x7875fd0 (file 0x7475fd0)
0x646331  (file 0x246331)  4c89ff                   mov rdi, r15
0x646334  (file 0x246334)  be04000000               mov esi, 4
0x646339  (file 0x246339)  e8a2fc2207               call 0x7875fe0  ; -> 0x7875fe0 (file 0x7475fe0)
0x64633e  (file 0x24633e)  4c89f7                   mov rdi, r14
0x646341  (file 0x246341)  4c89fe                   mov rsi, r15
0x646344  (file 0x246344)  31d2                     xor edx, edx
0x646346  (file 0x246346)  e8a5fc2207               call 0x7875ff0  ; -> 0x7875ff0 (file 0x7475ff0)
0x64634b  (file 0x24634b)  4c89ff                   mov rdi, r15
0x64634e  (file 0x24634e)  e8adfc2207               call 0x7876000  ; -> 0x7876000 (file 0x7476000)
0x646353  (file 0x246353)  488d3d3629dcff           lea rdi, [rip - 0x23d6ca]  ; rip->0x408c90 (file 0x8c90)
0x64635a  (file 0x24635a)  488d159f9c0909           lea rdx, [rip + 0x9099c9f]  ; rip->0x96e0000 (file 0x92e0000)
0x646361  (file 0x246361)  4c89f6                   mov rsi, r14
0x646364  (file 0x246364)  e867fb2207               call 0x7875ed0  ; -> 0x7875ed0 (file 0x7475ed0)
0x646369  (file 0x246369)  488d3d68ab1309           lea rdi, [rip + 0x913ab68]  ; rip->0x9780ed8 (file 0x9380ed8)
0x646370  (file 0x246370)  e84bfb2207               call 0x7875ec0  ; -> 0x7875ec0 (file 0x7475ec0)
0x646375  (file 0x246375)  e9e7fdffff               jmp 0x646161  ; -> 0x646161 (file 0x246161)
0x64637a  (file 0x24637a)  cc                       int3 
0x64637b  (file 0x24637b)  cc                       int3 
0x64637c  (file 0x24637c)  cc                       int3 
0x64637d  (file 0x24637d)  cc                       int3 
0x64637e  (file 0x24637e)  cc                       int3 
0x64637f  (file 0x24637f)  cc                       int3 
0x646380  (file 0x246380)  55                       push rbp
0x646381  (file 0x246381)  4889e5                   mov rbp, rsp
0x646384  (file 0x246384)  48833d24ab130900         cmp qword ptr [rip + 0x913ab24], 0  ; rip->0x9780eb0 (file 0x9380eb0)
0x64638c  (file 0x24638c)  0f95c0                   setne al
0x64638f  (file 0x24638f)  5d                       pop rbp
0x646390  (file 0x246390)  c3                       ret 
0x646391  (file 0x246391)  cc                       int3 
0x646392  (file 0x246392)  cc                       int3 
0x646393  (file 0x246393)  cc                       int3 
0x646394  (file 0x246394)  cc                       int3 
0x646395  (file 0x246395)  cc                       int3 
0x646396  (file 0x246396)  cc                       int3 
0x646397  (file 0x246397)  cc                       int3 
0x646398  (file 0x246398)  cc                       int3 
0x646399  (file 0x246399)  cc                       int3 
0x64639a  (file 0x24639a)  cc                       int3 
0x64639b  (file 0x24639b)  cc                       int3 
0x64639c  (file 0x24639c)  cc                       int3 
0x64639d  (file 0x24639d)  cc                       int3 
0x64639e  (file 0x24639e)  cc                       int3 
0x64639f  (file 0x24639f)  cc                       int3 
0x6463a0  (file 0x2463a0)  488b0509ab1309           mov rax, qword ptr [rip + 0x913ab09]  ; rip->0x9780eb0 (file 0x9380eb0)
0x6463a7  (file 0x2463a7)  4885c0                   test rax, rax
0x6463aa  (file 0x2463aa)  7401                     je 0x6463ad  ; -> 0x6463ad (file 0x2463ad)
0x6463ac  (file 0x2463ac)  c3                       ret 
0x6463ad  (file 0x2463ad)  55                       push rbp

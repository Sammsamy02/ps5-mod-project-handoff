0x880fc0  (f 0x480fc0)  55                     push rbp
0x880fc1  (f 0x480fc1)  4889e5                 mov rbp, rsp
0x880fc4  (f 0x480fc4)  4157                   push r15
0x880fc6  (f 0x480fc6)  4156                   push r14
0x880fc8  (f 0x480fc8)  4155                   push r13
0x880fca  (f 0x480fca)  4154                   push r12
0x880fcc  (f 0x480fcc)  53                     push rbx
0x880fcd  (f 0x480fcd)  4883ec58               sub rsp, 0x58
0x880fd1  (f 0x480fd1)  488b1d300fcb08         mov rbx, qword ptr [rip + 0x8cb0f30]  ; rip->0x9531f08 (file 0x9131f08)
0x880fd8  (f 0x480fd8)  4189cc                 mov r12d, ecx
0x880fdb  (f 0x480fdb)  4989f7                 mov r15, rsi
0x880fde  (f 0x480fde)  4989fe                 mov r14, rdi
0x880fe1  (f 0x480fe1)  488b03                 mov rax, qword ptr [rbx]
0x880fe4  (f 0x480fe4)  488945d0               mov qword ptr [rbp - 0x30], rax
0x880fe8  (f 0x480fe8)  41f6c410               test r12b, 0x10
0x880fec  (f 0x480fec)  7569                   jne 0x881057
0x880fee  (f 0x480fee)  48895580               mov qword ptr [rbp - 0x80], rdx
0x880ff2  (f 0x480ff2)  44894594               mov dword ptr [rbp - 0x6c], r8d
0x880ff6  (f 0x480ff6)  4d85ff                 test r15, r15
0x880ff9  (f 0x480ff9)  0f8405010000           je 0x881104
0x880fff  (f 0x480fff)  498b5f10               mov rbx, qword ptr [r15 + 0x10]
0x881003  (f 0x481003)  4d89cd                 mov r13, r9
0x881006  (f 0x481006)  e89586f5ff             call 0x7d96a0
0x88100b  (f 0x48100b)  4839c3                 cmp rbx, rax
0x88100e  (f 0x48100e)  488b1df30ecb08         mov rbx, qword ptr [rip + 0x8cb0ef3]  ; rip->0x9531f08 (file 0x9131f08)
0x881015  (f 0x481015)  0f84e9000000           je 0x881104
0x88101b  (f 0x48101b)  e810370300             call 0x8b4730
0x881020  (f 0x481020)  4c89ff                 mov rdi, r15
0x881023  (f 0x481023)  4c89ee                 mov rsi, r13
0x881026  (f 0x481026)  4c897d98               mov qword ptr [rbp - 0x68], r15
0x88102a  (f 0x48102a)  e881050000             call 0x8815b0
0x88102f  (f 0x48102f)  488b5580               mov rdx, qword ptr [rbp - 0x80]
0x881033  (f 0x481033)  4889c7                 mov rdi, rax
0x881036  (f 0x481036)  4c89f6                 mov rsi, r14
0x881039  (f 0x481039)  4989c7                 mov r15, rax
0x88103c  (f 0x48103c)  e88fed0200             call 0x8afdd0
0x881041  (f 0x481041)  4989c5                 mov r13, rax
0x881044  (f 0x481044)  4885c0                 test rax, rax
0x881047  (f 0x481047)  7441                   je 0x88108a
0x881049  (f 0x481049)  e862370300             call 0x8b47b0
0x88104e  (f 0x48104e)  4c8b7d98               mov r15, qword ptr [rbp - 0x68]
0x881052  (f 0x481052)  e9d8020000             jmp 0x88132f
0x881057  (f 0x481057)  498b06                 mov rax, qword ptr [r14]
0x88105a  (f 0x48105a)  4c89f7                 mov rdi, r14
0x88105d  (f 0x48105d)  4584c0                 test r8b, r8b
0x881060  (f 0x481060)  741a                   je 0x88107c
0x881062  (f 0x481062)  ff9070030000           call qword ptr [rax + 0x370]
0x881068  (f 0x481068)  488b08                 mov rcx, qword ptr [rax]
0x88106b  (f 0x48106b)  4889c7                 mov rdi, rax
0x88106e  (f 0x48106e)  ff91f0030000           call qword ptr [rcx + 0x3f0]
0x881074  (f 0x481074)  4989c5                 mov r13, rax
0x881077  (f 0x481077)  e9b3020000             jmp 0x88132f
0x88107c  (f 0x48107c)  ff90f0030000           call qword ptr [rax + 0x3f0]
0x881082  (f 0x481082)  4989c5                 mov r13, rax
0x881085  (f 0x481085)  e9a5020000             jmp 0x88132f
0x88108a  (f 0x48108a)  41f7c400004000         test r12d, 0x400000
0x881091  (f 0x481091)  7428                   je 0x8810bb
0x881093  (f 0x481093)  e82854eaff             call 0x7264c0
0x881098  (f 0x481098)  488b4d98               mov rcx, qword ptr [rbp - 0x68]
0x88109c  (f 0x48109c)  48635038               movsxd rdx, dword ptr [rax + 0x38]
0x8810a0  (f 0x4810a0)  488b4910               mov rcx, qword ptr [rcx + 0x10]
0x8810a4  (f 0x4810a4)  3b5138                 cmp edx, dword ptr [rcx + 0x38]
0x8810a7  (f 0x4810a7)  7f12                   jg 0x8810bb
0x8810a9  (f 0x4810a9)  488b4930               mov rcx, qword ptr [rcx + 0x30]
0x8810ad  (f 0x4810ad)  4883c030               add rax, 0x30
0x8810b1  (f 0x4810b1)  483904d1               cmp qword ptr [rcx + rdx*8], rax
0x8810b5  (f 0x4810b5)  0f8498000000           je 0x881153
0x8810bb  (f 0x4810bb)  488d05ae79ed08         lea rax, [rip + 0x8ed79ae]  ; rip->0x9758a70 (file 0x9358a70)
0x8810c2  (f 0x4810c2)  4d89fc                 mov r12, r15
0x8810c5  (f 0x4810c5)  803801                 cmp byte ptr [rax], 1
0x8810c8  (f 0x4810c8)  7510                   jne 0x8810da
0x8810ca  (f 0x4810ca)  418b442408             mov eax, dword ptr [r12 + 8]
0x8810cf  (f 0x4810cf)  a900040000             test eax, 0x400
0x8810d4  (f 0x4810d4)  0f85ad030000           jne 0x881487
0x8810da  (f 0x4810da)  498b7c2410             mov rdi, qword ptr [r12 + 0x10]
0x8810df  (f 0x4810df)  488b5580               mov rdx, qword ptr [rbp - 0x80]
0x8810e3  (f 0x4810e3)  4c89f6                 mov rsi, r14
0x8810e6  (f 0x4810e6)  488b07                 mov rax, qword ptr [rdi]
0x8810e9  (f 0x4810e9)  ff90e8030000           call qword ptr [rax + 0x3e8]
0x8810ef  (f 0x4810ef)  4989c5                 mov r13, rax
0x8810f2  (f 0x4810f2)  e8b9360300             call 0x8b47b0
0x8810f7  (f 0x4810f7)  4c8b7d98               mov r15, qword ptr [rbp - 0x68]
0x8810fb  (f 0x4810fb)  4d85ed                 test r13, r13
0x8810fe  (f 0x4810fe)  0f852b020000           jne 0x88132f
0x881104  (f 0x481104)  807d9400               cmp byte ptr [rbp - 0x6c], 0
0x881108  (f 0x481108)  0f8406020000           je 0x881314
0x88110e  (f 0x48110e)  498b06                 mov rax, qword ptr [r14]
0x881111  (f 0x481111)  4c89f7                 mov rdi, r14
0x881114  (f 0x481114)  ff9070030000           call qword ptr [rax + 0x370]
0x88111a  (f 0x48111a)  4c8ba810010000         mov r13, qword ptr [rax + 0x110]
0x881121  (f 0x481121)  4d85ed                 test r13, r13
0x881124  (f 0x481124)  0f8505020000           jne 0x88132f
0x88112a  (f 0x48112a)  4889c7                 mov rdi, rax
0x88112d  (f 0x48112d)  4d89f4                 mov r12, r14
0x881130  (f 0x481130)  4d89fe                 mov r14, r15
0x881133  (f 0x481133)  4989df                 mov r15, rbx
0x881136  (f 0x481136)  4889c3                 mov rbx, rax
0x881139  (f 0x481139)  e87224eaff             call 0x7235b0
0x88113e  (f 0x48113e)  4c8bab10010000         mov r13, qword ptr [rbx + 0x110]
0x881145  (f 0x481145)  4c89fb                 mov rbx, r15
0x881148  (f 0x481148)  4d89f7                 mov r15, r14
0x88114b  (f 0x48114b)  4d89e6                 mov r14, r12
0x88114e  (f 0x48114e)  e9dc010000             jmp 0x88132f
0x881153  (f 0x481153)  488b4598               mov rax, qword ptr [rbp - 0x68]
0x881157  (f 0x481157)  8b4d94                 mov ecx, dword ptr [rbp - 0x6c]
0x88115a  (f 0x48115a)  488b7840               mov rdi, qword ptr [rax + 0x40]
0x88115e  (f 0x48115e)  4885ff                 test rdi, rdi
0x881161  (f 0x481161)  0f94c0                 sete al
0x881164  (f 0x481164)  80f101                 xor cl, 1
0x881167  (f 0x481167)  08c1                   or cl, al
0x881169  (f 0x481169)  750c                   jne 0x881177
0x88116b  (f 0x48116b)  488b07                 mov rax, qword ptr [rdi]
0x88116e  (f 0x48116e)  ff9070030000           call qword ptr [rax + 0x370]
0x881174  (f 0x481174)  4889c7                 mov rdi, rax
0x881177  (f 0x481177)  4885ff                 test rdi, rdi
0x88117a  (f 0x48117a)  0f8481010000           je 0x881301
0x881180  (f 0x481180)  f687d600000004         test byte ptr [rdi + 0xd6], 4
0x881187  (f 0x481187)  7508                   jne 0x881191
0x881189  (f 0x481189)  4531ed                 xor r13d, r13d
0x88118c  (f 0x48118c)  e961ffffff             jmp 0x8810f2
0x881191  (f 0x481191)  488d05d878ed08         lea rax, [rip + 0x8ed78d8]  ; rip->0x9758a70 (file 0x9358a70)
0x881198  (f 0x481198)  803801                 cmp byte ptr [rax], 1
0x88119b  (f 0x48119b)  750a                   jne 0x8811a7
0x88119d  (f 0x48119d)  8b4708                 mov eax, dword ptr [rdi + 8]
0x8811a0  (f 0x4811a0)  a900040000             test eax, 0x400
0x8811a5  (f 0x4811a5)  754a                   jne 0x8811f1
0x8811a7  (f 0x4811a7)  488b5580               mov rdx, qword ptr [rbp - 0x80]
0x8811ab  (f 0x4811ab)  4c89f6                 mov rsi, r14
0x8811ae  (f 0x4811ae)  4989fc                 mov r12, rdi
0x8811b1  (f 0x4811b1)  e81aec0200             call 0x8afdd0
0x8811b6  (f 0x4811b6)  4989c5                 mov r13, rax
0x8811b9  (f 0x4811b9)  4885c0                 test rax, rax
0x8811bc  (f 0x4811bc)  7417                   je 0x8811d5
0x8811be  (f 0x4811be)  418b4508               mov eax, dword ptr [r13 + 8]
0x8811c2  (f 0x4811c2)  a900000040             test eax, 0x40000000
0x8811c7  (f 0x4811c7)  750c                   jne 0x8811d5
0x8811c9  (f 0x4811c9)  418b4508               mov eax, dword ptr [r13 + 8]
0x8811cd  (f 0x4811cd)  a840                   test al, 0x40
0x8811cf  (f 0x4811cf)  0f8474feffff           je 0x881049
0x8811d5  (f 0x4811d5)  498b7c2440             mov rdi, qword ptr [r12 + 0x40]
0x8811da  (f 0x4811da)  4885ff                 test rdi, rdi
0x8811dd  (f 0x4811dd)  0f840fffffff           je 0x8810f2
0x8811e3  (f 0x4811e3)  f687d600000004         test byte ptr [rdi + 0xd6], 4
0x8811ea  (f 0x4811ea)  75a5                   jne 0x881191
0x8811ec  (f 0x4811ec)  e901ffffff             jmp 0x8810f2
0x8811f1  (f 0x4811f1)  48897d88               mov qword ptr [rbp - 0x78], rdi
0x8811f5  (f 0x4811f5)  4d85ff                 test r15, r15
0x8811f8  (f 0x4811f8)  740a                   je 0x881204
0x8811fa  (f 0x4811fa)  49837f1000             cmp qword ptr [r15 + 0x10], 0
0x8811ff  (f 0x4811ff)  4c89fe                 mov rsi, r15
0x881202  (f 0x481202)  7512                   jne 0x881216
0x881204  (f 0x481204)  488d7dc0               lea rdi, [rbp - 0x40]
0x881208  (f 0x481208)  488d350f713f07         lea rsi, [rip + 0x73f710f]  ; rip->0x7c7831e (file 0x787831e)
0x88120f  (f 0x48120f)  e8dc2cbeff             call 0x463ef0
0x881214  (f 0x481214)  eb0d                   jmp 0x881223
0x881216  (f 0x481216)  488d7dc0               lea rdi, [rbp - 0x40]
0x88121a  (f 0x48121a)  31d2                   xor edx, edx
0x88121c  (f 0x48121c)  31c9                   xor ecx, ecx
0x88121e  (f 0x48121e)  e86d5d0000             call 0x886f90
0x881223  (f 0x481223)  837dc800               cmp dword ptr [rbp - 0x38], 0
0x881227  (f 0x481227)  4c8d2572e31a07         lea r12, [rip + 0x71ae372]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x88122e  (f 0x48122e)  7404                   je 0x881234
0x881230  (f 0x481230)  4c8b65c0               mov r12, qword ptr [rbp - 0x40]
0x881234  (f 0x481234)  4d85f6                 test r14, r14
0x881237  (f 0x481237)  7407                   je 0x881240
0x881239  (f 0x481239)  49837e1000             cmp qword ptr [r14 + 0x10], 0
0x88123e  (f 0x48123e)  7512                   jne 0x881252
0x881240  (f 0x481240)  488d7db0               lea rdi, [rbp - 0x50]
0x881244  (f 0x481244)  488d35d3703f07         lea rsi, [rip + 0x73f70d3]  ; rip->0x7c7831e (file 0x787831e)
0x88124b  (f 0x48124b)  e8a02cbeff             call 0x463ef0
0x881250  (f 0x481250)  eb10                   jmp 0x881262
0x881252  (f 0x481252)  488d7db0               lea rdi, [rbp - 0x50]
0x881256  (f 0x481256)  4c89f6                 mov rsi, r14
0x881259  (f 0x481259)  31d2                   xor edx, edx
0x88125b  (f 0x48125b)  31c9                   xor ecx, ecx
0x88125d  (f 0x48125d)  e82e5d0000             call 0x886f90
0x881262  (f 0x481262)  837db800               cmp dword ptr [rbp - 0x48], 0
0x881266  (f 0x481266)  4c8d2d33e31a07         lea r13, [rip + 0x71ae333]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x88126d  (f 0x48126d)  7404                   je 0x881273
0x88126f  (f 0x48126f)  4c8b6db0               mov r13, qword ptr [rbp - 0x50]
0x881273  (f 0x481273)  488b7598               mov rsi, qword ptr [rbp - 0x68]
0x881277  (f 0x481277)  48837e1000             cmp qword ptr [rsi + 0x10], 0
0x88127c  (f 0x48127c)  7512                   jne 0x881290
0x88127e  (f 0x48127e)  488d7da0               lea rdi, [rbp - 0x60]
0x881282  (f 0x481282)  488d3595703f07         lea rsi, [rip + 0x73f7095]  ; rip->0x7c7831e (file 0x787831e)
0x881289  (f 0x481289)  e8622cbeff             call 0x463ef0
0x88128e  (f 0x48128e)  eb0d                   jmp 0x88129d
0x881290  (f 0x481290)  488d7da0               lea rdi, [rbp - 0x60]
0x881294  (f 0x481294)  31d2                   xor edx, edx
0x881296  (f 0x481296)  31c9                   xor ecx, ecx
0x881298  (f 0x481298)  e8f35c0000             call 0x886f90
0x88129d  (f 0x48129d)  837da800               cmp dword ptr [rbp - 0x58], 0
0x8812a1  (f 0x4812a1)  4c8d0df8e21a07         lea r9, [rip + 0x71ae2f8]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x8812a8  (f 0x4812a8)  7404                   je 0x8812ae
0x8812aa  (f 0x4812aa)  4c8b4da0               mov r9, qword ptr [rbp - 0x60]
0x8812ae  (f 0x4812ae)  488d3da40e6107         lea rdi, [rip + 0x7610ea4]  ; rip->0x7e92159 (file 0x7a92159)
0x8812b5  (f 0x4812b5)  be92000000             mov esi, 0x92
0x8812ba  (f 0x4812ba)  488d15f57c3a07         lea rdx, [rip + 0x73a7cf5]  ; rip->0x7c28fb6 (file 0x7828fb6)
0x8812c1  (f 0x4812c1)  4c89e1                 mov rcx, r12
0x8812c4  (f 0x4812c4)  4d89e8                 mov r8, r13
0x8812c7  (f 0x4812c7)  31c0                   xor eax, eax
0x8812c9  (f 0x4812c9)  e8e23dcfff             call 0x5750b0
0x8812ce  (f 0x4812ce)  488b7da0               mov rdi, qword ptr [rbp - 0x60]
0x8812d2  (f 0x4812d2)  4885ff                 test rdi, rdi
0x8812d5  (f 0x4812d5)  7405                   je 0x8812dc
0x8812d7  (f 0x4812d7)  e8b467c3ff             call 0x4b7a90
0x8812dc  (f 0x4812dc)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x8812e0  (f 0x4812e0)  4885ff                 test rdi, rdi
0x8812e3  (f 0x4812e3)  7405                   je 0x8812ea
0x8812e5  (f 0x4812e5)  e8a667c3ff             call 0x4b7a90
0x8812ea  (f 0x4812ea)  488b7dc0               mov rdi, qword ptr [rbp - 0x40]
0x8812ee  (f 0x4812ee)  4885ff                 test rdi, rdi
0x8812f1  (f 0x4812f1)  7405                   je 0x8812f8
0x8812f3  (f 0x4812f3)  e89867c3ff             call 0x4b7a90
0x8812f8  (f 0x4812f8)  488b7d88               mov rdi, qword ptr [rbp - 0x78]
0x8812fc  (f 0x4812fc)  e9a6feffff             jmp 0x8811a7
0x881301  (f 0x481301)  e8aa340300             call 0x8b47b0
0x881306  (f 0x481306)  4c8b7d98               mov r15, qword ptr [rbp - 0x68]
0x88130a  (f 0x48130a)  807d9400               cmp byte ptr [rbp - 0x6c], 0
0x88130e  (f 0x48130e)  0f85fafdffff           jne 0x88110e
0x881314  (f 0x481314)  4d8bae10010000         mov r13, qword ptr [r14 + 0x110]
0x88131b  (f 0x48131b)  4d85ed                 test r13, r13
0x88131e  (f 0x48131e)  750f                   jne 0x88132f
0x881320  (f 0x481320)  4c89f7                 mov rdi, r14
0x881323  (f 0x481323)  e88822eaff             call 0x7235b0
0x881328  (f 0x481328)  4d8bae10010000         mov r13, qword ptr [r14 + 0x110]
0x88132f  (f 0x48132f)  4d85ed                 test r13, r13
0x881332  (f 0x481332)  7417                   je 0x88134b
0x881334  (f 0x481334)  488d053577ed08         lea rax, [rip + 0x8ed7735]  ; rip->0x9758a70 (file 0x9358a70)
0x88133b  (f 0x48133b)  803800                 cmp byte ptr [rax], 0
0x88133e  (f 0x48133e)  740b                   je 0x88134b
0x881340  (f 0x481340)  418b4508               mov eax, dword ptr [r13 + 8]
0x881344  (f 0x481344)  a900040000             test eax, 0x400
0x881349  (f 0x481349)  751f                   jne 0x88136a
0x88134b  (f 0x48134b)  488b03                 mov rax, qword ptr [rbx]
0x88134e  (f 0x48134e)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x881352  (f 0x481352)  0f854a020000           jne 0x8815a2
0x881358  (f 0x481358)  4c89e8                 mov rax, r13
0x88135b  (f 0x48135b)  4883c458               add rsp, 0x58
0x88135f  (f 0x48135f)  5b                     pop rbx
0x881360  (f 0x481360)  415c                   pop r12
0x881362  (f 0x481362)  415d                   pop r13
0x881364  (f 0x481364)  415e                   pop r14
0x881366  (f 0x481366)  415f                   pop r15
0x881368  (f 0x481368)  5d                     pop rbp
0x881369  (f 0x481369)  c3                     ret 
0x88136a  (f 0x48136a)  49837d1000             cmp qword ptr [r13 + 0x10], 0
0x88136f  (f 0x48136f)  7512                   jne 0x881383
0x881371  (f 0x481371)  488d35a66f3f07         lea rsi, [rip + 0x73f6fa6]  ; rip->0x7c7831e (file 0x787831e)
0x881378  (f 0x481378)  488d7dc0               lea rdi, [rbp - 0x40]
0x88137c  (f 0x48137c)  e86f2bbeff             call 0x463ef0
0x881381  (f 0x481381)  eb10                   jmp 0x881393
0x881383  (f 0x481383)  488d7dc0               lea rdi, [rbp - 0x40]
0x881387  (f 0x481387)  4c89ee                 mov rsi, r13
0x88138a  (f 0x48138a)  31d2                   xor edx, edx
0x88138c  (f 0x48138c)  31c9                   xor ecx, ecx
0x88138e  (f 0x48138e)  e8fd5b0000             call 0x886f90
0x881393  (f 0x481393)  837dc800               cmp dword ptr [rbp - 0x38], 0
0x881397  (f 0x481397)  4d89fc                 mov r12, r15
0x88139a  (f 0x48139a)  7406                   je 0x8813a2
0x88139c  (f 0x48139c)  4c8b7dc0               mov r15, qword ptr [rbp - 0x40]
0x8813a0  (f 0x4813a0)  eb07                   jmp 0x8813a9
0x8813a2  (f 0x4813a2)  4c8d3df7e11a07         lea r15, [rip + 0x71ae1f7]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x8813a9  (f 0x4813a9)  4d85f6                 test r14, r14
0x8813ac  (f 0x4813ac)  7407                   je 0x8813b5
0x8813ae  (f 0x4813ae)  49837e1000             cmp qword ptr [r14 + 0x10], 0
0x8813b3  (f 0x4813b3)  751c                   jne 0x8813d1
0x8813b5  (f 0x4813b5)  488d35626f3f07         lea rsi, [rip + 0x73f6f62]  ; rip->0x7c7831e (file 0x787831e)
0x8813bc  (f 0x4813bc)  488d7db0               lea rdi, [rbp - 0x50]
0x8813c0  (f 0x4813c0)  e82b2bbeff             call 0x463ef0
0x8813c5  (f 0x4813c5)  837db800               cmp dword ptr [rbp - 0x48], 0
0x8813c9  (f 0x4813c9)  741c                   je 0x8813e7
0x8813cb  (f 0x4813cb)  4c8b75b0               mov r14, qword ptr [rbp - 0x50]
0x8813cf  (f 0x4813cf)  eb1d                   jmp 0x8813ee
0x8813d1  (f 0x4813d1)  488d7db0               lea rdi, [rbp - 0x50]
0x8813d5  (f 0x4813d5)  4c89f6                 mov rsi, r14
0x8813d8  (f 0x4813d8)  31d2                   xor edx, edx
0x8813da  (f 0x4813da)  31c9                   xor ecx, ecx
0x8813dc  (f 0x4813dc)  e8af5b0000             call 0x886f90
0x8813e1  (f 0x4813e1)  837db800               cmp dword ptr [rbp - 0x48], 0
0x8813e5  (f 0x4813e5)  75e4                   jne 0x8813cb
0x8813e7  (f 0x4813e7)  4c8d35b2e11a07         lea r14, [rip + 0x71ae1b2]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x8813ee  (f 0x4813ee)  4d85e4                 test r12, r12
0x8813f1  (f 0x4813f1)  740b                   je 0x8813fe
0x8813f3  (f 0x4813f3)  49837c241000           cmp qword ptr [r12 + 0x10], 0
0x8813f9  (f 0x4813f9)  4c89e6                 mov rsi, r12
0x8813fc  (f 0x4813fc)  751c                   jne 0x88141a
0x8813fe  (f 0x4813fe)  488d35196f3f07         lea rsi, [rip + 0x73f6f19]  ; rip->0x7c7831e (file 0x787831e)
0x881405  (f 0x481405)  488d7da0               lea rdi, [rbp - 0x60]
0x881409  (f 0x481409)  e8e22abeff             call 0x463ef0
0x88140e  (f 0x48140e)  837da800               cmp dword ptr [rbp - 0x58], 0
0x881412  (f 0x481412)  7419                   je 0x88142d
0x881414  (f 0x481414)  4c8b4da0               mov r9, qword ptr [rbp - 0x60]
0x881418  (f 0x481418)  eb1a                   jmp 0x881434
0x88141a  (f 0x48141a)  488d7da0               lea rdi, [rbp - 0x60]
0x88141e  (f 0x48141e)  31d2                   xor edx, edx
0x881420  (f 0x481420)  31c9                   xor ecx, ecx
0x881422  (f 0x481422)  e8695b0000             call 0x886f90
0x881427  (f 0x481427)  837da800               cmp dword ptr [rbp - 0x58], 0
0x88142b  (f 0x48142b)  75e7                   jne 0x881414
0x88142d  (f 0x48142d)  4c8d0d6ce11a07         lea r9, [rip + 0x71ae16c]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x881434  (f 0x481434)  488d3d1e0d6107         lea rdi, [rip + 0x7610d1e]  ; rip->0x7e92159 (file 0x7a92159)
0x88143b  (f 0x48143b)  488d1550772507         lea rdx, [rip + 0x7257750]  ; rip->0x7ad8b92 (file 0x76d8b92)
0x881442  (f 0x481442)  bebf000000             mov esi, 0xbf
0x881447  (f 0x481447)  4c89f9                 mov rcx, r15
0x88144a  (f 0x48144a)  4d89f0                 mov r8, r14
0x88144d  (f 0x48144d)  31c0                   xor eax, eax
0x88144f  (f 0x48144f)  e85c3ccfff             call 0x5750b0
0x881454  (f 0x481454)  488b7da0               mov rdi, qword ptr [rbp - 0x60]
0x881458  (f 0x481458)  4885ff                 test rdi, rdi
0x88145b  (f 0x48145b)  7405                   je 0x881462
0x88145d  (f 0x48145d)  e82e66c3ff             call 0x4b7a90
0x881462  (f 0x481462)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x881466  (f 0x481466)  4885ff                 test rdi, rdi
0x881469  (f 0x481469)  7405                   je 0x881470
0x88146b  (f 0x48146b)  e82066c3ff             call 0x4b7a90
0x881470  (f 0x481470)  488b7dc0               mov rdi, qword ptr [rbp - 0x40]
0x881474  (f 0x481474)  4885ff                 test rdi, rdi
0x881477  (f 0x481477)  0f84cefeffff           je 0x88134b
0x88147d  (f 0x48147d)  e80e66c3ff             call 0x4b7a90
0x881482  (f 0x481482)  e9c4feffff             jmp 0x88134b
0x881487  (f 0x481487)  4d85e4                 test r12, r12
0x88148a  (f 0x48148a)  7408                   je 0x881494
0x88148c  (f 0x48148c)  49837c241000           cmp qword ptr [r12 + 0x10], 0
0x881492  (f 0x481492)  7512                   jne 0x8814a6
0x881494  (f 0x481494)  488d35836e3f07         lea rsi, [rip + 0x73f6e83]  ; rip->0x7c7831e (file 0x787831e)
0x88149b  (f 0x48149b)  488d7dc0               lea rdi, [rbp - 0x40]
0x88149f  (f 0x48149f)  e84c2abeff             call 0x463ef0
0x8814a4  (f 0x4814a4)  eb10                   jmp 0x8814b6
0x8814a6  (f 0x4814a6)  488d7dc0               lea rdi, [rbp - 0x40]
0x8814aa  (f 0x4814aa)  4c89e6                 mov rsi, r12
0x8814ad  (f 0x4814ad)  31d2                   xor edx, edx
0x8814af  (f 0x4814af)  31c9                   xor ecx, ecx
0x8814b1  (f 0x4814b1)  e8da5a0000             call 0x886f90
0x8814b6  (f 0x4814b6)  837dc800               cmp dword ptr [rbp - 0x38], 0
0x8814ba  (f 0x4814ba)  4c8b7d98               mov r15, qword ptr [rbp - 0x68]
0x8814be  (f 0x4814be)  7406                   je 0x8814c6
0x8814c0  (f 0x4814c0)  488b45c0               mov rax, qword ptr [rbp - 0x40]
0x8814c4  (f 0x4814c4)  eb07                   jmp 0x8814cd
0x8814c6  (f 0x4814c6)  488d05d3e01a07         lea rax, [rip + 0x71ae0d3]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x8814cd  (f 0x4814cd)  48894588               mov qword ptr [rbp - 0x78], rax
0x8814d1  (f 0x4814d1)  4d85f6                 test r14, r14
0x8814d4  (f 0x4814d4)  7407                   je 0x8814dd
0x8814d6  (f 0x4814d6)  49837e1000             cmp qword ptr [r14 + 0x10], 0
0x8814db  (f 0x4814db)  7512                   jne 0x8814ef
0x8814dd  (f 0x4814dd)  488d353a6e3f07         lea rsi, [rip + 0x73f6e3a]  ; rip->0x7c7831e (file 0x787831e)
0x8814e4  (f 0x4814e4)  488d7db0               lea rdi, [rbp - 0x50]
0x8814e8  (f 0x4814e8)  e8032abeff             call 0x463ef0
0x8814ed  (f 0x4814ed)  eb10                   jmp 0x8814ff
0x8814ef  (f 0x4814ef)  488d7db0               lea rdi, [rbp - 0x50]
0x8814f3  (f 0x4814f3)  4c89f6                 mov rsi, r14
0x8814f6  (f 0x4814f6)  31d2                   xor edx, edx
0x8814f8  (f 0x4814f8)  31c9                   xor ecx, ecx
0x8814fa  (f 0x4814fa)  e8915a0000             call 0x886f90
0x8814ff  (f 0x4814ff)  837db800               cmp dword ptr [rbp - 0x48], 0
0x881503  (f 0x481503)  7406                   je 0x88150b
0x881505  (f 0x481505)  4c8b6db0               mov r13, qword ptr [rbp - 0x50]
0x881509  (f 0x481509)  eb07                   jmp 0x881512
0x88150b  (f 0x48150b)  4c8d2d8ee01a07         lea r13, [rip + 0x71ae08e]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x881512  (f 0x481512)  49837f1000             cmp qword ptr [r15 + 0x10], 0
0x881517  (f 0x481517)  7512                   jne 0x88152b
0x881519  (f 0x481519)  488d35fe6d3f07         lea rsi, [rip + 0x73f6dfe]  ; rip->0x7c7831e (file 0x787831e)
0x881520  (f 0x481520)  488d7da0               lea rdi, [rbp - 0x60]
0x881524  (f 0x481524)  e8c729beff             call 0x463ef0
0x881529  (f 0x481529)  eb10                   jmp 0x88153b
0x88152b  (f 0x48152b)  488d7da0               lea rdi, [rbp - 0x60]
0x88152f  (f 0x48152f)  4c89fe                 mov rsi, r15
0x881532  (f 0x481532)  31d2                   xor edx, edx
0x881534  (f 0x481534)  31c9                   xor ecx, ecx
0x881536  (f 0x481536)  e8555a0000             call 0x886f90
0x88153b  (f 0x48153b)  837da800               cmp dword ptr [rbp - 0x58], 0
0x88153f  (f 0x48153f)  7406                   je 0x881547
0x881541  (f 0x481541)  4c8b4da0               mov r9, qword ptr [rbp - 0x60]
0x881545  (f 0x481545)  eb07                   jmp 0x88154e
0x881547  (f 0x481547)  4c8d0d52e01a07         lea r9, [rip + 0x71ae052]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x88154e  (f 0x48154e)  488b4d88               mov rcx, qword ptr [rbp - 0x78]
0x881552  (f 0x481552)  488d3d000c6107         lea rdi, [rip + 0x7610c00]  ; rip->0x7e92159 (file 0x7a92159)
0x881559  (f 0x481559)  488d15763a0e07         lea rdx, [rip + 0x70e3a76]  ; rip->0x7964fd6 (file 0x7564fd6)
0x881560  (f 0x481560)  bea3000000             mov esi, 0xa3
0x881565  (f 0x481565)  4d89e8                 mov r8, r13
0x881568  (f 0x481568)  31c0                   xor eax, eax
0x88156a  (f 0x48156a)  e8413bcfff             call 0x5750b0
0x88156f  (f 0x48156f)  488b7da0               mov rdi, qword ptr [rbp - 0x60]
0x881573  (f 0x481573)  4885ff                 test rdi, rdi
0x881576  (f 0x481576)  7405                   je 0x88157d
0x881578  (f 0x481578)  e81365c3ff             call 0x4b7a90
0x88157d  (f 0x48157d)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x881581  (f 0x481581)  4885ff                 test rdi, rdi
0x881584  (f 0x481584)  7405                   je 0x88158b
0x881586  (f 0x481586)  e80565c3ff             call 0x4b7a90
0x88158b  (f 0x48158b)  488b7dc0               mov rdi, qword ptr [rbp - 0x40]
0x88158f  (f 0x48158f)  4885ff                 test rdi, rdi
0x881592  (f 0x481592)  0f8442fbffff           je 0x8810da
0x881598  (f 0x481598)  e8f364c3ff             call 0x4b7a90
0x88159d  (f 0x48159d)  e938fbffff             jmp 0x8810da
0x8815a2  (f 0x4815a2)  e8b908ff06             call 0x7871e60
0x8815a7  (f 0x4815a7)  0f0b                   ud2 
--- calls: [('0xc81006', '0x7d96a0'), ('0xc8101b', '0x8b4730'), ('0xc8102a', '0x8815b0'), ('0xc8103c', '0x8afdd0'), ('0xc81049', '0x8b47b0'), ('0xc815a2', '0x7871e60'), ('0xc8137c', '0x463ef0'), ('0xc813c0', '0x463ef0'), ('0xc81409', '0x463ef0'), ('0xc8144f', '0x5750b0'), ('0xc8145d', '0x4b7a90'), ('0xc8146b', '0x4b7a90'), ('0xc8147d', '0x4b7a90'), ('0xc81422', '0x886f90'), ('0xc813dc', '0x886f90'), ('0xc8138e', '0x886f90'), ('0xc81093', '0x7264c0'), ('0xc810f2', '0x8b47b0'), ('0xc81139', '0x7235b0'), ('0xc81323', '0x7235b0'), ('0xc8149f', '0x463ef0'), ('0xc814e8', '0x463ef0'), ('0xc81524', '0x463ef0'), ('0xc8156a', '0x5750b0'), ('0xc81578', '0x4b7a90'), ('0xc81586', '0x4b7a90'), ('0xc81598', '0x4b7a90'), ('0xc81536', '0x886f90'), ('0xc814fa', '0x886f90'), ('0xc814b1', '0x886f90'), ('0xc811b1', '0x8afdd0'), ('0xc8120f', '0x463ef0'), ('0xc8124b', '0x463ef0'), ('0xc81289', '0x463ef0'), ('0xc812c9', '0x5750b0'), ('0xc812d7', '0x4b7a90'), ('0xc812e5', '0x4b7a90'), ('0xc812f3', '0x4b7a90'), ('0xc81298', '0x886f90'), ('0xc8125d', '0x886f90'), ('0xc8121e', '0x886f90'), ('0xc81301', '0x8b47b0')]

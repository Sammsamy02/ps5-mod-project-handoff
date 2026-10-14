0x7222a0  (f 0x3222a0)  55                     push rbp
0x7222a1  (f 0x3222a1)  4889e5                 mov rbp, rsp
0x7222a4  (f 0x3222a4)  4157                   push r15
0x7222a6  (f 0x3222a6)  4156                   push r14
0x7222a8  (f 0x3222a8)  4155                   push r13
0x7222aa  (f 0x3222aa)  4154                   push r12
0x7222ac  (f 0x3222ac)  53                     push rbx
0x7222ad  (f 0x3222ad)  4881ec88100000         sub rsp, 0x1088
0x7222b4  (f 0x3222b4)  4c8b2d4dfce008         mov r13, qword ptr [rip + 0x8e0fc4d]  ; rip->0x9531f08 (file 0x9131f08)
0x7222bb  (f 0x3222bb)  4889fb                 mov rbx, rdi
0x7222be  (f 0x3222be)  498b4500               mov rax, qword ptr [r13]
0x7222c2  (f 0x3222c2)  488945d0               mov qword ptr [rbp - 0x30], rax
0x7222c6  (f 0x3222c6)  4883bf1001000000       cmp qword ptr [rdi + 0x110], 0
0x7222ce  (f 0x3222ce)  0f8551030000           jne 0x722625
0x7222d4  (f 0x3222d4)  4c8b7b40               mov r15, qword ptr [rbx + 0x40]
0x7222d8  (f 0x3222d8)  4d85ff                 test r15, r15
0x7222db  (f 0x3222db)  7431                   je 0x72230e
0x7222dd  (f 0x3222dd)  4c89ff                 mov rdi, r15
0x7222e0  (f 0x3222e0)  be01000000             mov esi, 1
0x7222e5  (f 0x3222e5)  e8063b1600             call 0x885df0
0x7222ea  (f 0x3222ea)  4d8bb710010000         mov r14, qword ptr [r15 + 0x110]
0x7222f1  (f 0x3222f1)  4d85f6                 test r14, r14
0x7222f4  (f 0x3222f4)  7529                   jne 0x72231f
0x7222f6  (f 0x3222f6)  498b07                 mov rax, qword ptr [r15]
0x7222f9  (f 0x3222f9)  4c89ff                 mov rdi, r15
0x7222fc  (f 0x3222fc)  ff9000040000           call qword ptr [rax + 0x400]
0x722302  (f 0x322302)  4d8bb710010000         mov r14, qword ptr [r15 + 0x110]
0x722309  (f 0x322309)  4d85f6                 test r14, r14
0x72230c  (f 0x32230c)  7511                   jne 0x72231f
0x72230e  (f 0x32230e)  e89d380a00             call 0x7c5bb0
0x722313  (f 0x322313)  4839d8                 cmp rax, rbx
0x722316  (f 0x322316)  0f8509030000           jne 0x722625
0x72231c  (f 0x32231c)  4531f6                 xor r14d, r14d
0x72231f  (f 0x32231f)  f683d600000004         test byte ptr [rbx + 0xd6], 4
0x722326  (f 0x322326)  7470                   je 0x722398
0x722328  (f 0x322328)  48837b7000             cmp qword ptr [rbx + 0x70], 0
0x72232d  (f 0x32232d)  7569                   jne 0x722398
0x72232f  (f 0x32232f)  488d3dd9660309         lea rdi, [rip + 0x90366d9]  ; rip->0x9758a0f (file 0x9358a0f)
0x722336  (f 0x322336)  e8a5b4e8ff             call 0x5ad7e0
0x72233b  (f 0x32233b)  84c0                   test al, al
0x72233d  (f 0x32233d)  7559                   jne 0x722398
0x72233f  (f 0x32233f)  4889df                 mov rdi, rbx
0x722342  (f 0x322342)  e8b9291900             call 0x8b4d00
0x722347  (f 0x322347)  4885c0                 test rax, rax
0x72234a  (f 0x32234a)  744c                   je 0x722398
0x72234c  (f 0x32234c)  488d0d1d670309         lea rcx, [rip + 0x903671d]  ; rip->0x9758a70 (file 0x9358a70)
0x722353  (f 0x322353)  4989c7                 mov r15, rax
0x722356  (f 0x322356)  803900                 cmp byte ptr [rcx], 0
0x722359  (f 0x322359)  0f84e9020000           je 0x722648
0x72235f  (f 0x32235f)  4c8dbda0f7ffff         lea r15, [rbp - 0x860]
0x722366  (f 0x322366)  4c89ff                 mov rdi, r15
0x722369  (f 0x322369)  e8928aefff             call 0x61ae00
0x72236e  (f 0x32236e)  488d055b33bb07         lea rax, [rip + 0x7bb335b]  ; rip->0x82d56d0 (file 0x7ed56d0)
0x722375  (f 0x322375)  4889df                 mov rdi, rbx
0x722378  (f 0x322378)  4c89fe                 mov rsi, r15
0x72237b  (f 0x32237b)  ba01000000             mov edx, 1
0x722380  (f 0x322380)  488985a0f7ffff         mov qword ptr [rbp - 0x860], rax
0x722387  (f 0x322387)  488b03                 mov rax, qword ptr [rbx]
0x72238a  (f 0x32238a)  ff90d0020000           call qword ptr [rax + 0x2d0]
0x722390  (f 0x322390)  4c89ff                 mov rdi, r15
0x722393  (f 0x322393)  e87891efff             call 0x61b510
0x722398  (f 0x322398)  4883bb1001000000       cmp qword ptr [rbx + 0x110], 0
0x7223a0  (f 0x3223a0)  0f857f020000           jne 0x722625
0x7223a6  (f 0x3223a6)  488b7320               mov rsi, qword ptr [rbx + 0x20]
0x7223aa  (f 0x3223aa)  4889df                 mov rdi, rbx
0x7223ad  (f 0x3223ad)  b931000000             mov ecx, 0x31
0x7223b2  (f 0x3223b2)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x7223b6  (f 0x3223b6)  c5f811442418           vmovups xmmword ptr [rsp + 0x18], xmm0
0x7223bc  (f 0x3223bc)  c5f8110424             vmovups xmmword ptr [rsp], xmm0
0x7223c1  (f 0x3223c1)  c744241000000000       mov dword ptr [rsp + 0x10], 0
0x7223c9  (f 0x3223c9)  31d2                   xor edx, edx
0x7223cb  (f 0x3223cb)  4531c0                 xor r8d, r8d
0x7223ce  (f 0x3223ce)  4531c9                 xor r9d, r9d
0x7223d1  (f 0x3223d1)  e8daeb1600             call 0x890fb0
0x7223d6  (f 0x3223d6)  4989c7                 mov r15, rax
0x7223d9  (f 0x3223d9)  4885c0                 test rax, rax
0x7223dc  (f 0x3223dc)  7414                   je 0x7223f2
0x7223de  (f 0x3223de)  488d0577ca0709         lea rax, [rip + 0x907ca77]  ; rip->0x979ee5c (file 0x939ee5c)
0x7223e5  (f 0x3223e5)  803800                 cmp byte ptr [rax], 0
0x7223e8  (f 0x3223e8)  7408                   je 0x7223f2
0x7223ea  (f 0x3223ea)  4c89ff                 mov rdi, r15
0x7223ed  (f 0x3223ed)  e8ce5e0400             call 0x7682c0
0x7223f2  (f 0x3223f2)  4c89bb10010000         mov qword ptr [rbx + 0x110], r15
0x7223f9  (f 0x3223f9)  4885db                 test rbx, rbx
0x7223fc  (f 0x3223fc)  0f84b1000000           je 0x7224b3
0x722402  (f 0x322402)  4c8b6350               mov r12, qword ptr [rbx + 0x50]
0x722406  (f 0x322406)  4d85e4                 test r12, r12
0x722409  (f 0x322409)  0f84a4000000           je 0x7224b3
0x72240f  (f 0x32240f)  49bf0180000200000800   movabs r15, 0x8000002008001
0x722419  (f 0x322419)  eb13                   jmp 0x72242e
0x722420  (f 0x322420)  4d8b642418             mov r12, qword ptr [r12 + 0x18]
0x722425  (f 0x322425)  4d85e4                 test r12, r12
0x722428  (f 0x322428)  0f8485000000           je 0x7224b3
0x72242e  (f 0x32242e)  498b442408             mov rax, qword ptr [r12 + 8]
0x722433  (f 0x322433)  488b4010               mov rax, qword ptr [rax + 0x10]
0x722437  (f 0x322437)  c4c2f8f2c7             andn rax, rax, r15
0x72243c  (f 0x32243c)  75e2                   jne 0x722420
0x72243e  (f 0x32243e)  41f644243b20           test byte ptr [r12 + 0x3b], 0x20
0x722444  (f 0x322444)  75da                   jne 0x722420
0x722446  (f 0x322446)  4885db                 test rbx, rbx
0x722449  (f 0x322449)  7441                   je 0x72248c
0x72244b  (f 0x32244b)  498b442470             mov rax, qword ptr [r12 + 0x70]
0x722450  (f 0x322450)  488bbb10010000         mov rdi, qword ptr [rbx + 0x110]
0x722457  (f 0x322457)  4963542444             movsxd rdx, dword ptr [r12 + 0x44]
0x72245c  (f 0x32245c)  488bb0e8000000         mov rsi, qword ptr [rax + 0xe8]
0x722463  (f 0x322463)  e8882a1400             call 0x864ef0
0x722468  (f 0x322468)  4d8b642418             mov r12, qword ptr [r12 + 0x18]
0x72246d  (f 0x32246d)  4d85e4                 test r12, r12
0x722470  (f 0x322470)  7441                   je 0x7224b3
0x722472  (f 0x322472)  498b442408             mov rax, qword ptr [r12 + 8]
0x722477  (f 0x322477)  488b4010               mov rax, qword ptr [rax + 0x10]
0x72247b  (f 0x32247b)  c4c2f8f2c7             andn rax, rax, r15
0x722480  (f 0x322480)  75e6                   jne 0x722468
0x722482  (f 0x322482)  41f644243b20           test byte ptr [r12 + 0x3b], 0x20
0x722488  (f 0x322488)  74c1                   je 0x72244b
0x72248a  (f 0x32248a)  ebdc                   jmp 0x722468
0x72248c  (f 0x32248c)  498b442470             mov rax, qword ptr [r12 + 0x70]
0x722491  (f 0x322491)  488bbb10010000         mov rdi, qword ptr [rbx + 0x110]
0x722498  (f 0x322498)  4963542444             movsxd rdx, dword ptr [r12 + 0x44]
0x72249d  (f 0x32249d)  488bb0e8000000         mov rsi, qword ptr [rax + 0xe8]
0x7224a4  (f 0x3224a4)  e8472a1400             call 0x864ef0
0x7224a9  (f 0x3224a9)  4d8b642418             mov r12, qword ptr [r12 + 0x18]
0x7224ae  (f 0x3224ae)  4d85e4                 test r12, r12
0x7224b1  (f 0x3224b1)  75d9                   jne 0x72248c
0x7224b3  (f 0x3224b3)  31c9                   xor ecx, ecx
0x7224b5  (f 0x3224b5)  f783d400000080000010   test dword ptr [rbx + 0xd4], 0x10000080
0x7224bf  (f 0x3224bf)  488bb310010000         mov rsi, qword ptr [rbx + 0x110]
0x7224c6  (f 0x3224c6)  4c8ba3b0000000         mov r12, qword ptr [rbx + 0xb0]
0x7224cd  (f 0x3224cd)  4c8dbda0f7ffff         lea r15, [rbp - 0x860]
0x7224d4  (f 0x3224d4)  4c89f2                 mov rdx, r14
0x7224d7  (f 0x3224d7)  4c89ff                 mov rdi, r15
0x7224da  (f 0x3224da)  0f94c1                 sete cl
0x7224dd  (f 0x3224dd)  4531c0                 xor r8d, r8d
0x7224e0  (f 0x3224e0)  01c9                   add ecx, ecx
0x7224e2  (f 0x3224e2)  e839fa1600             call 0x891f20
0x7224e7  (f 0x3224e7)  4c89ff                 mov rdi, r15
0x7224ea  (f 0x3224ea)  41ffd4                 call r12
0x7224ed  (f 0x3224ed)  4c89ff                 mov rdi, r15
0x7224f0  (f 0x3224f0)  e8ebfa1600             call 0x891fe0
0x7224f5  (f 0x3224f5)  4889df                 mov rdi, rbx
0x7224f8  (f 0x3224f8)  e8533c1600             call 0x886150
0x7224fd  (f 0x3224fd)  f6402c10               test byte ptr [rax + 0x2c], 0x10
0x722501  (f 0x322501)  0f8411010000           je 0x722618
0x722507  (f 0x322507)  4889df                 mov rdi, rbx
0x72250a  (f 0x32250a)  e8413c1600             call 0x886150
0x72250f  (f 0x32250f)  f6402f20               test byte ptr [rax + 0x2f], 0x20
0x722513  (f 0x322513)  0f85ff000000           jne 0x722618
0x722519  (f 0x322519)  4889df                 mov rdi, rbx
0x72251c  (f 0x32251c)  e82f3c1600             call 0x886150
0x722521  (f 0x322521)  488b4018               mov rax, qword ptr [rax + 0x18]
0x722525  (f 0x322525)  4c8db580efffff         lea r14, [rbp - 0x1080]
0x72252c  (f 0x32252c)  488dbda0f7ffff         lea rdi, [rbp - 0x860]
0x722533  (f 0x322533)  ba0b040000             mov edx, 0x40b
0x722538  (f 0x322538)  4c89f6                 mov rsi, r14
0x72253b  (f 0x32253b)  488985a0f7ffff         mov qword ptr [rbp - 0x860], rax
0x722542  (f 0x322542)  e8b9d4f1ff             call 0x63fa00
0x722547  (f 0x322547)  488dbdbaf7ffff         lea rdi, [rbp - 0x846]
0x72254e  (f 0x32254e)  488d45d0               lea rax, [rbp - 0x30]
0x722552  (f 0x322552)  488d3599565007         lea rsi, [rip + 0x7505699]  ; rip->0x7c27bf2 (file 0x7827bf2)
0x722559  (f 0x322559)  ba12000000             mov edx, 0x12
0x72255e  (f 0x32255e)  c685b8f7ffff00         mov byte ptr [rbp - 0x848], 0
0x722565  (f 0x322565)  4889bda0f7ffff         mov qword ptr [rbp - 0x860], rdi
0x72256c  (f 0x32256c)  4889bda8f7ffff         mov qword ptr [rbp - 0x858], rdi
0x722573  (f 0x322573)  488985b0f7ffff         mov qword ptr [rbp - 0x850], rax
0x72257a  (f 0x32257a)  e8b1aad6ff             call 0x48d030
0x72257f  (f 0x32257f)  4883c012               add rax, 0x12
0x722583  (f 0x322583)  488dbd78efffff         lea rdi, [rbp - 0x1088]
0x72258a  (f 0x32258a)  4c89fe                 mov rsi, r15
0x72258d  (f 0x32258d)  488985a8f7ffff         mov qword ptr [rbp - 0x858], rax
0x722594  (f 0x322594)  488b4318               mov rax, qword ptr [rbx + 0x18]
0x722598  (f 0x322598)  48898578efffff         mov qword ptr [rbp - 0x1088], rax
0x72259f  (f 0x32259f)  e89cd0f1ff             call 0x63f640
0x7225a4  (f 0x3225a4)  488b85a8f7ffff         mov rax, qword ptr [rbp - 0x858]
0x7225ab  (f 0x3225ab)  488dbd78efffff         lea rdi, [rbp - 0x1088]
0x7225b2  (f 0x3225b2)  ba01000000             mov edx, 1
0x7225b7  (f 0x3225b7)  66c7000000             mov word ptr [rax], 0
0x7225bc  (f 0x3225bc)  488bb5a0f7ffff         mov rsi, qword ptr [rbp - 0x860]
0x7225c3  (f 0x3225c3)  e8c8aff1ff             call 0x63d590
0x7225c8  (f 0x3225c8)  4c89ff                 mov rdi, r15
0x7225cb  (f 0x3225cb)  e8101beeff             call 0x6040e0
0x7225d0  (f 0x3225d0)  488b8578efffff         mov rax, qword ptr [rbp - 0x1088]
0x7225d7  (f 0x3225d7)  488dbd78efffff         lea rdi, [rbp - 0x1088]
0x7225de  (f 0x3225de)  4c89fe                 mov rsi, r15
0x7225e1  (f 0x3225e1)  ba0b040000             mov edx, 0x40b
0x7225e6  (f 0x3225e6)  48898578efffff         mov qword ptr [rbp - 0x1088], rax
0x7225ed  (f 0x3225ed)  e80ed4f1ff             call 0x63fa00
0x7225f2  (f 0x3225f2)  488b8310010000         mov rax, qword ptr [rbx + 0x110]
0x7225f9  (f 0x3225f9)  4c89f7                 mov rdi, r14
0x7225fc  (f 0x3225fc)  4c89fe                 mov rsi, r15
0x7225ff  (f 0x3225ff)  ba01000000             mov edx, 1
0x722604  (f 0x322604)  b902000000             mov ecx, 2
0x722609  (f 0x322609)  4531c0                 xor r8d, r8d
0x72260c  (f 0x32260c)  4531c9                 xor r9d, r9d
0x72260f  (f 0x32260f)  48890424               mov qword ptr [rsp], rax
0x722613  (f 0x322613)  e8088ffdff             call 0x6fb520
0x722618  (f 0x322618)  488bbb10010000         mov rdi, qword ptr [rbx + 0x110]
0x72261f  (f 0x32261f)  488b07                 mov rax, qword ptr [rdi]
0x722622  (f 0x322622)  ff5068                 call qword ptr [rax + 0x68]
0x722625  (f 0x322625)  488b8310010000         mov rax, qword ptr [rbx + 0x110]
0x72262c  (f 0x32262c)  498b4d00               mov rcx, qword ptr [r13]
0x722630  (f 0x322630)  483b4dd0               cmp rcx, qword ptr [rbp - 0x30]
0x722634  (f 0x322634)  754a                   jne 0x722680
0x722636  (f 0x322636)  4881c488100000         add rsp, 0x1088
0x72263d  (f 0x32263d)  5b                     pop rbx
0x72263e  (f 0x32263e)  415c                   pop r12
0x722640  (f 0x322640)  415d                   pop r13
0x722642  (f 0x322642)  415e                   pop r14
0x722644  (f 0x322644)  415f                   pop r15
0x722646  (f 0x322646)  5d                     pop rbp
0x722647  (f 0x322647)  c3                     ret 
0x722648  (f 0x322648)  4c8b6348               mov r12, qword ptr [rbx + 0x48]
0x72264c  (f 0x32264c)  eb05                   jmp 0x722653
0x72264e  (f 0x32264e)  4d8b642428             mov r12, qword ptr [r12 + 0x28]
0x722653  (f 0x322653)  4d85e4                 test r12, r12
0x722656  (f 0x322656)  0f8403fdffff           je 0x72235f
0x72265c  (f 0x32265c)  49395c2420             cmp qword ptr [r12 + 0x20], rbx
0x722661  (f 0x322661)  0f85f8fcffff           jne 0x72235f
0x722667  (f 0x322667)  418b442408             mov eax, dword ptr [r12 + 8]
0x72266c  (f 0x32266c)  a900040000             test eax, 0x400
0x722671  (f 0x322671)  74db                   je 0x72264e
0x722673  (f 0x322673)  4c89ff                 mov rdi, r15
0x722676  (f 0x322676)  4c89e6                 mov rsi, r12
0x722679  (f 0x322679)  e802e30800             call 0x7b0980
0x72267e  (f 0x32267e)  ebce                   jmp 0x72264e
0x722680  (f 0x322680)  e8dbf71407             call 0x7871e60
0x722685  (f 0x322685)  0f0b                   ud2 
--- calls: [('0xb222e5', '0x885df0'), ('0xb2230e', '0x7c5bb0'), ('0xb22336', '0x5ad7e0'), ('0xb22342', '0x8b4d00'), ('0xb22369', '0x61ae00'), ('0xb22393', '0x61b510'), ('0xb223d1', '0x890fb0'), ('0xb223ed', '0x7682c0'), ('0xb22463', '0x864ef0'), ('0xb224e2', '0x891f20'), ('0xb224f0', '0x891fe0'), ('0xb224f8', '0x886150'), ('0xb2250a', '0x886150'), ('0xb2251c', '0x886150'), ('0xb22542', '0x63fa00'), ('0xb2257a', '0x48d030'), ('0xb2259f', '0x63f640'), ('0xb225c3', '0x63d590'), ('0xb225cb', '0x6040e0'), ('0xb225ed', '0x63fa00'), ('0xb22613', '0x6fb520'), ('0xb22680', '0x7871e60'), ('0xb224a4', '0x864ef0'), ('0xb22679', '0x7b0980')]

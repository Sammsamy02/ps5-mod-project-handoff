0x2fe03e0: 55                       push rbp
0x2fe03e1: 4889e5                   mov rbp, rsp
0x2fe03e4: 4157                     push r15
0x2fe03e6: 4156                     push r14
0x2fe03e8: 4155                     push r13
0x2fe03ea: 4154                     push r12
0x2fe03ec: 53                       push rbx
0x2fe03ed: 4883e4e0                 and rsp, 0xffffffffffffffe0
0x2fe03f1: 4881ec00010000           sub rsp, 0x100
0x2fe03f8: 488b0d091b1506           mov rcx, qword ptr [rip + 0x6151b09]
0x2fe03ff: 488b01                   mov rax, qword ptr [rcx]
0x2fe0402: 48898424e0000000         mov qword ptr [rsp + 0xe0], rax
0x2fe040a: 4885f6                   test rsi, rsi
0x2fe040d: 0f8432020000             je 0x2fe0645
0x2fe0413: f6465d02                 test byte ptr [rsi + 0x5d], 2
0x2fe0417: 4889f3                   mov rbx, rsi
0x2fe041a: 0f8525020000             jne 0x2fe0645
0x2fe0420: 8b4308                   mov eax, dword ptr [rbx + 8]
0x2fe0423: a900000040               test eax, 0x40000000
0x2fe0428: 0f8517020000             jne 0x2fe0645
0x2fe042e: 4989fe                   mov r14, rdi
0x2fe0431: 4989d7                   mov r15, rdx
0x2fe0434: e887714400               call 0x34275c0
0x2fe0439: 488b5310                 mov rdx, qword ptr [rbx + 0x10]
0x2fe043d: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fe0441: 3b4a38                   cmp ecx, dword ptr [rdx + 0x38]
0x2fe0444: 7f11                     jg 0x2fe0457
0x2fe0446: 488b5230                 mov rdx, qword ptr [rdx + 0x30]
0x2fe044a: 4883c030                 add rax, 0x30
0x2fe044e: 4989dc                   mov r12, rbx
0x2fe0451: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fe0455: 7403                     je 0x2fe045a
0x2fe0457: 4531e4                   xor r12d, r12d
0x2fe045a: 488b353f1d5406           mov rsi, qword ptr [rip + 0x6541d3f]
0x2fe0461: 4c89f7                   mov rdi, r14
0x2fe0464: e8d79247fd               call 0x459740
0x2fe0469: 4889c7                   mov rdi, rax
0x2fe046c: 4989c5                   mov r13, rax
0x2fe046f: e88c9d33fd               call 0x31a200
0x2fe0474: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fe047b: 7534                     jne 0x2fe04b1
0x2fe047d: c644246800               mov byte ptr [rsp + 0x68], 0
0x2fe0482: 4c89642460               mov qword ptr [rsp + 0x60], r12
0x2fe0487: 488d542460               lea rdx, [rsp + 0x60]
0x2fe048c: 4c89f7                   mov rdi, r14
0x2fe048f: 4c89ee                   mov rsi, r13
0x2fe0492: 498b06                   mov rax, qword ptr [r14]
0x2fe0495: ff9068020000             call qword ptr [rax + 0x268]
0x2fe049b: 807c246800               cmp byte ptr [rsp + 0x68], 0
0x2fe04a0: 488b0d611a1506           mov rcx, qword ptr [rip + 0x6151a61]
0x2fe04a7: 4c89f8                   mov rax, r15
0x2fe04aa: 7426                     je 0x2fe04d2
0x2fe04ac: e994010000               jmp 0x2fe0645
0x2fe04b1: 498b06                   mov rax, qword ptr [r14]
0x2fe04b4: 4c89f7                   mov rdi, r14
0x2fe04b7: 4c89e6                   mov rsi, r12
0x2fe04ba: ff9090070000             call qword ptr [rax + 0x790]
0x2fe04c0: 84c0                     test al, al
0x2fe04c2: 488b0d3f1a1506           mov rcx, qword ptr [rip + 0x6151a3f]
0x2fe04c9: 4c89f8                   mov rax, r15
0x2fe04cc: 0f8573010000             jne 0x2fe0645
0x2fe04d2: c5fc1000                 vmovups ymm0, ymmword ptr [rax]
0x2fe04d6: 488d7c2420               lea rdi, [rsp + 0x20]
0x2fe04db: 488d742460               lea rsi, [rsp + 0x60]
0x2fe04e0: c5fc11442460             vmovups ymmword ptr [rsp + 0x60], ymm0
0x2fe04e6: e8151b17fd               call 0x152000
0x2fe04eb: 4c8d2d6ae93b06           lea r13, [rip + 0x63be96a]
0x2fe04f2: 4c8ba3e8020000           mov r12, qword ptr [rbx + 0x2e8]
0x2fe04f9: 410fb64500               movzx eax, byte ptr [r13]
0x2fe04fe: 4d85e4                   test r12, r12
0x2fe0501: 0f95c1                   setne cl
0x2fe0504: 20c1                     and cl, al
0x2fe0506: 80f901                   cmp cl, 1
0x2fe0509: 7516                     jne 0x2fe0521
0x2fe050b: 4c89e7                   mov rdi, r12
0x2fe050e: e8ad7d38fd               call 0x3682c0
0x2fe0513: 4c8ba3e8020000           mov r12, qword ptr [rbx + 0x2e8]
0x2fe051a: 410fb64500               movzx eax, byte ptr [r13]
0x2fe051f: eb09                     jmp 0x2fe052a
0x2fe0521: 4d85e4                   test r12, r12
0x2fe0524: 0f843b010000             je 0x2fe0665
0x2fe052a: 4d85e4                   test r12, r12
0x2fe052d: 0f95c1                   setne cl
0x2fe0530: 20c1                     and cl, al
0x2fe0532: 80f901                   cmp cl, 1
0x2fe0535: 7508                     jne 0x2fe053f
0x2fe0537: 4c89e7                   mov rdi, r12
0x2fe053a: e8817d38fd               call 0x3682c0
0x2fe053f: 4d8ba424b8010000         mov r12, qword ptr [r12 + 0x1b8]
0x2fe0547: 4d85e4                   test r12, r12
0x2fe054a: 746e                     je 0x2fe05ba
0x2fe054c: c4c17d108424e0010000     vmovupd ymm0, ymmword ptr [r12 + 0x1e0]
0x2fe0556: c5fd11442460             vmovupd ymmword ptr [rsp + 0x60], ymm0

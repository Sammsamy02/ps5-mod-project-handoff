0x384d460: 55                       push rbp
0x384d461: 4889e5                   mov rbp, rsp
0x384d464: 4157                     push r15
0x384d466: 4156                     push r14
0x384d468: 4155                     push r13
0x384d46a: 4154                     push r12
0x384d46c: 53                       push rbx
0x384d46d: 4881ec88010000           sub rsp, 0x188
0x384d474: 488b058d4a8e05           mov rax, qword ptr [rip + 0x58e4a8d]
0x384d47b: 48898d60feffff           mov qword ptr [rbp - 0x1a0], rcx
0x384d482: 4889b598feffff           mov qword ptr [rbp - 0x168], rsi
0x384d489: 4889bd88feffff           mov qword ptr [rbp - 0x178], rdi
0x384d490: 4989d7                   mov r15, rdx
0x384d493: 4c8d6a28                 lea r13, [rdx + 0x28]
0x384d497: 488b00                   mov rax, qword ptr [rax]
0x384d49a: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384d49e: 4c898580feffff           mov qword ptr [rbp - 0x180], r8
0x384d4a5: 837a3000                 cmp dword ptr [rdx + 0x30], 0
0x384d4a9: 7406                     je 0x384d4b1
0x384d4ab: 498b5500                 mov rdx, qword ptr [r13]
0x384d4af: eb07                     jmp 0x384d4b8
0x384d4b1: 488d15e820de03           lea rdx, [rip + 0x3de20e8]
0x384d4b8: 488d35cda10e04           lea rsi, [rip + 0x40ea1cd] ; ['Loading Map %s']
0x384d4bf: 488d7dcf                 lea rdi, [rbp - 0x31]
0x384d4c3: 31c0                     xor eax, eax
0x384d4c5: e8a60196fc               call 0x1ad670
0x384d4ca: 48c745b800000000         mov qword ptr [rbp - 0x48], 0
0x384d4d2: 4d8b7728                 mov r14, qword ptr [r15 + 0x28]
0x384d4d6: 4d636730                 movsxd r12, dword ptr [r15 + 0x30]
0x384d4da: 448965c0                 mov dword ptr [rbp - 0x40], r12d
0x384d4de: 4d85e4                   test r12, r12
0x384d4e1: 7437                     je 0x384d51a
0x384d4e3: 488d45c4                 lea rax, [rbp - 0x3c]
0x384d4e7: 4c8d45b8                 lea r8, [rbp - 0x48]
0x384d4eb: bf02000000               mov edi, 2
0x384d4f0: be02000000               mov esi, 2
0x384d4f5: 4489e2                   mov edx, r12d
0x384d4f8: 4589e1                   mov r9d, r12d
0x384d4fb: 31c9                     xor ecx, ecx
0x384d4fd: 48890424                 mov qword ptr [rsp], rax
0x384d501: e8caa07dfc               call 0x275d0
0x384d506: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x384d50a: 4d01e4                   add r12, r12
0x384d50d: 4c89f6                   mov rsi, r14
0x384d510: 4c89e2                   mov rdx, r12
0x384d513: e83849c203               call 0x7471e50
0x384d518: eb07                     jmp 0x384d521
0x384d51a: c745c400000000           mov dword ptr [rbp - 0x3c], 0
0x384d521: 488d9db0feffff           lea rbx, [rbp - 0x150]
0x384d528: 4c89ee                   mov rsi, r13
0x384d52b: 31d2                     xor edx, edx
0x384d52d: 4889df                   mov rdi, rbx
0x384d530: e8eb11f5ff               call 0x379e720
0x384d535: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384d53c: 4939dd                   cmp r13, rbx
0x384d53f: 7431                     je 0x384d572
0x384d541: 498b7d00                 mov rdi, qword ptr [r13]
0x384d545: 4885ff                   test rdi, rdi
0x384d548: 7405                     je 0x384d54f
0x384d54a: e841a586fc               call 0xb7a90
0x384d54f: 488b85b0feffff           mov rax, qword ptr [rbp - 0x150]
0x384d556: 49894728                 mov qword ptr [r15 + 0x28], rax
0x384d55a: 48c785b0feffff00000000   mov qword ptr [rbp - 0x150], 0
0x384d565: 488b85b8feffff           mov rax, qword ptr [rbp - 0x148]
0x384d56c: 49894730                 mov qword ptr [r15 + 0x30], rax
0x384d570: eb11                     jmp 0x384d583
0x384d572: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384d579: 4885ff                   test rdi, rdi
0x384d57c: 7405                     je 0x384d583
0x384d57e: e80da586fc               call 0xb7a90
0x384d583: e8e8eb86fc               call 0xbc170
0x384d588: 4889c7                   mov rdi, rax
0x384d58b: e800dd7bfc               call 0xb290
0x384d590: e8eb0de8fd               call 0x16ce380
0x384d595: 488bbd80feffff           mov rdi, qword ptr [rbp - 0x180]
0x384d59c: 488d35fd1fde03           lea rsi, [rip + 0x3de1ffd]
0x384d5a3: e8a86f81fc               call 0x64550
0x384d5a8: e8a3c59cfc               call 0x219b50
0x384d5ad: 4889c7                   mov rdi, rax
0x384d5b0: e8dbdc7bfc               call 0xb290
0x384d5b5: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384d5bd: 4885c0                   test rax, rax
0x384d5c0: 7407                     je 0x384d5c9
0x384d5c2: 80a08b0100007f           and byte ptr [rax + 0x18b], 0x7f
0x384d5c9: 488d1d18c9b605           lea rbx, [rip + 0x5b6c918]
0x384d5d0: 4c89bd90feffff           mov qword ptr [rbp - 0x170], r15
0x384d5d7: 8b4b14                   mov ecx, dword ptr [rbx + 0x14]
0x384d5da: 8d4101                   lea eax, [rcx + 1]
0x384d5dd: 894314                   mov dword ptr [rbx + 0x14], eax
0x384d5e0: 48634308                 movsxd rax, dword ptr [rbx + 8]
0x384d5e4: 4885c0                   test rax, rax
0x384d5e7: 0f8e94000000             jle 0x384d681
0x384d5ed: 4531f6                   xor r14d, r14d
0x384d5f0: eb20                     jmp 0x384d612
0x384d5f2: 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384d600: 498d4424ff               lea rax, [r12 - 1]
0x384d605: 41b601                   mov r14b, 1
0x384d608: 4983fc01                 cmp r12, 1
0x384d60c: 0f8657040000             jbe 0x384da69
0x384d612: 4989c7                   mov r15, rax
0x384d615: 4989c4                   mov r12, rax
0x384d618: 49c1e704                 shl r15, 4
0x384d61c: 4983c7f8                 add r15, -8
0x384d620: 488b03                   mov rax, qword ptr [rbx]
0x384d623: 42833c3800               cmp dword ptr [rax + r15], 0
0x384d628: 74d6                     je 0x384d600
0x384d62a: 4a8b7c38f8               mov rdi, qword ptr [rax + r15 - 8]
0x384d62f: 4885ff                   test rdi, rdi
0x384d632: 74cc                     je 0x384d600
0x384d634: 488b07                   mov rax, qword ptr [rdi]
0x384d637: 488bb598feffff           mov rsi, qword ptr [rbp - 0x168]
0x384d63e: 4c89ea                   mov rdx, r13
0x384d641: ff5068                   call qword ptr [rax + 0x68]
0x384d644: 84c0                     test al, al
0x384d646: 74b8                     je 0x384d600
0x384d648: 49ffcc                   dec r12
0x384d64b: 4983c7f0                 add r15, -0x10
0x384d64f: 498d442401               lea rax, [r12 + 1]
0x384d654: 4883f801                 cmp rax, 1
0x384d658: 77c6                     ja 0x384d620
0x384d65a: ff4b14                   dec dword ptr [rbx + 0x14]
0x384d65d: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384d664: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384d66b: 41f6c601                 test r14b, 1
0x384d66f: 7413                     je 0x384d684
0x384d671: 488d3d70c8b605           lea rdi, [rip + 0x5b6c870]
0x384d678: 31f6                     xor esi, esi
0x384d67a: e871377efc               call 0x30df0
0x384d67f: eb03                     jmp 0x384d684
0x384d681: 894b14                   mov dword ptr [rbx + 0x14], ecx
0x384d684: 488d1d45c8b605           lea rbx, [rip + 0x5b6c845]
0x384d68b: 8b4b14                   mov ecx, dword ptr [rbx + 0x14]
0x384d68e: 8d4101                   lea eax, [rcx + 1]
0x384d691: 894314                   mov dword ptr [rbx + 0x14], eax
0x384d694: 48634308                 movsxd rax, dword ptr [rbx + 8]
0x384d698: 4885c0                   test rax, rax
0x384d69b: 0f8e89000000             jle 0x384d72a
0x384d6a1: 4531f6                   xor r14d, r14d
0x384d6a4: eb1c                     jmp 0x384d6c2
0x384d6a6: 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x384d6b0: 498d4424ff               lea rax, [r12 - 1]
0x384d6b5: 41b601                   mov r14b, 1
0x384d6b8: 4983fc01                 cmp r12, 1
0x384d6bc: 0f86bd030000             jbe 0x384da7f
0x384d6c2: 4989c7                   mov r15, rax
0x384d6c5: 4989c4                   mov r12, rax
0x384d6c8: 49c1e704                 shl r15, 4
0x384d6cc: 4983c7f8                 add r15, -8
0x384d6d0: 488b03                   mov rax, qword ptr [rbx]
0x384d6d3: 42833c3800               cmp dword ptr [rax + r15], 0
0x384d6d8: 74d6                     je 0x384d6b0
0x384d6da: 4a8b7c38f8               mov rdi, qword ptr [rax + r15 - 8]
0x384d6df: 4885ff                   test rdi, rdi
0x384d6e2: 74cc                     je 0x384d6b0
0x384d6e4: 488b07                   mov rax, qword ptr [rdi]
0x384d6e7: 4c89ee                   mov rsi, r13
0x384d6ea: ff5068                   call qword ptr [rax + 0x68]
0x384d6ed: 84c0                     test al, al
0x384d6ef: 74bf                     je 0x384d6b0
0x384d6f1: 49ffcc                   dec r12
0x384d6f4: 4983c7f0                 add r15, -0x10
0x384d6f8: 498d442401               lea rax, [r12 + 1]
0x384d6fd: 4883f801                 cmp rax, 1
0x384d701: 77cd                     ja 0x384d6d0
0x384d703: ff4b14                   dec dword ptr [rbx + 0x14]
0x384d706: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384d70d: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384d714: 41f6c601                 test r14b, 1
0x384d718: 7413                     je 0x384d72d
0x384d71a: 488d3dafc7b605           lea rdi, [rip + 0x5b6c7af]
0x384d721: 31f6                     xor esi, esi
0x384d723: e8c8367efc               call 0x30df0
0x384d728: eb03                     jmp 0x384d72d
0x384d72a: 894b14                   mov dword ptr [rbx + 0x14], ecx
0x384d72d: e86e0ce8fd               call 0x16ce3a0
0x384d732: e8798fe9ff               call 0x36e66b0
0x384d737: 488b8588feffff           mov rax, qword ptr [rbp - 0x178]
0x384d73e: 80a004110000fe           and byte ptr [rax + 0x1104], 0xfe
0x384d745: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384d74d: 4885c0                   test rax, rax
0x384d750: 744d                     je 0x384d79f
0x384d752: 488b7830                 mov rdi, qword ptr [rax + 0x30]
0x384d756: 4885ff                   test rdi, rdi
0x384d759: 7444                     je 0x384d79f
0x384d75b: e8f089c3fc               call 0x486150
0x384d760: 488b4018                 mov rax, qword ptr [rax + 0x18]
0x384d764: 488d9db0feffff           lea rbx, [rbp - 0x150]
0x384d76b: 488db530ffffff           lea rsi, [rbp - 0xd0]
0x384d772: 4889df                   mov rdi, rbx
0x384d775: 48898530ffffff           mov qword ptr [rbp - 0xd0], rax
0x384d77c: e83f199ffc               call 0x23f0c0
0x384d781: 4c89e6                   mov rsi, r12
0x384d784: 4889d9                   mov rcx, rbx
0x384d787: 31d2                     xor edx, edx
0x384d789: e8b21a0000               call 0x384f240
0x384d78e: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384d795: 4885ff                   test rdi, rdi
0x384d798: 7405                     je 0x384d79f
0x384d79a: e8f1a286fc               call 0xb7a90
0x384d79f: 488d9db0feffff           lea rbx, [rbp - 0x150]
0x384d7a6: 488d35f31dde03           lea rsi, [rip + 0x3de1df3]
0x384d7ad: 4889df                   mov rdi, rbx
0x384d7b0: e83b6781fc               call 0x63ef0
0x384d7b5: 4c89e6                   mov rsi, r12
0x384d7b8: ba01000000               mov edx, 1
0x384d7bd: 4889d9                   mov rcx, rbx
0x384d7c0: e87b1a0000               call 0x384f240
0x384d7c5: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384d7cc: 4885ff                   test rdi, rdi
0x384d7cf: 7405                     je 0x384d7d6
0x384d7d1: e8baa286fc               call 0xb7a90
0x384d7d6: 488d35c31dde03           lea rsi, [rip + 0x3de1dc3]
0x384d7dd: 4889df                   mov rdi, rbx
0x384d7e0: e80b6781fc               call 0x63ef0
0x384d7e5: 4c89e6                   mov rsi, r12
0x384d7e8: ba02000000               mov edx, 2
0x384d7ed: 4889d9                   mov rcx, rbx
0x384d7f0: e84b1a0000               call 0x384f240
0x384d7f5: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384d7fc: 4885ff                   test rdi, rdi
0x384d7ff: 7405                     je 0x384d806
0x384d801: e88aa286fc               call 0xb7a90
0x384d806: 488d35931dde03           lea rsi, [rip + 0x3de1d93]
0x384d80d: 4889df                   mov rdi, rbx
0x384d810: e8db6681fc               call 0x63ef0
0x384d815: 4c89e6                   mov rsi, r12
0x384d818: ba04000000               mov edx, 4
0x384d81d: 4889d9                   mov rcx, rbx
0x384d820: e81b1a0000               call 0x384f240
0x384d825: 488bbdb0feffff           mov rdi, qword ptr [rbp - 0x150]
0x384d82c: 4885ff                   test rdi, rdi
0x384d82f: 7405                     je 0x384d836
0x384d831: e85aa286fc               call 0xb7a90
0x384d836: bfffffffff               mov edi, 0xffffffff
0x384d83b: e840d5aafc               call 0x2fad80
0x384d840: 4c89e6                   mov rsi, r12
0x384d843: e8a81b0000               call 0x384f3f0
0x384d848: 498d7c2408               lea rdi, [r12 + 8]
0x384d84d: e8ee11f5ff               call 0x379ea40
0x384d852: e8094bc203               call 0x7472360
0x384d857: c4e1f96ec0               vmovq xmm0, rax
0x384d85c: c5f962055cf61e04         vpunpckldq xmm0, xmm0, xmmword ptr [rip + 0x41ef65c]
0x384d864: 488d0505e8a805           lea rax, [rip + 0x5a8e805] ; ['鸰锒輭㸅Ā']
0x384d86b: c5f95c05bddb1e04         vsubpd xmm0, xmm0, xmmword ptr [rip + 0x41edbbd]
0x384d873: c5f9c6c801               vshufpd xmm1, xmm0, xmm0, 1
0x384d878: c5f358c0                 vaddsd xmm0, xmm1, xmm0
0x384d87c: c5fb5900                 vmulsd xmm0, xmm0, qword ptr [rax]
0x384d880: c5fb118558feffff         vmovsd qword ptr [rbp - 0x1a8], xmm0
0x384d888: e803da7bfc               call 0xb290
0x384d88d: 4c89ad78feffff           mov qword ptr [rbp - 0x188], r13
0x384d894: 4d8bb424c0020000         mov r14, qword ptr [r12 + 0x2c0]
0x384d89c: 4c8bad88feffff           mov r13, qword ptr [rbp - 0x178]
0x384d8a3: 4d85f6                   test r14, r14
0x384d8a6: 0f84db060000             je 0x384df87
0x384d8ac: 488d1dc53ad005           lea rbx, [rip + 0x5d03ac5]
0x384d8b3: 8b4b14                   mov ecx, dword ptr [rbx + 0x14]
0x384d8b6: 8d4101                   lea eax, [rcx + 1]
0x384d8b9: 894314                   mov dword ptr [rbx + 0x14], eax
0x384d8bc: 48634308                 movsxd rax, dword ptr [rbx + 8]
0x384d8c0: 4885c0                   test rax, rax
0x384d8c3: 0f8e89000000             jle 0x384d952
0x384d8c9: 4531ff                   xor r15d, r15d
0x384d8cc: eb13                     jmp 0x384d8e1
0x384d8ce: 6690                     nop 
0x384d8d0: 498d45ff                 lea rax, [r13 - 1]
0x384d8d4: 41b701                   mov r15b, 1
0x384d8d7: 4983fd01                 cmp r13, 1
0x384d8db: 0f86b4010000             jbe 0x384da95
0x384d8e1: 4989c4                   mov r12, rax
0x384d8e4: 4989c5                   mov r13, rax
0x384d8e7: 49c1e404                 shl r12, 4
0x384d8eb: 4983c4f8                 add r12, -8
0x384d8ef: 90                       nop 
0x384d8f0: 488b03                   mov rax, qword ptr [rbx]
0x384d8f3: 42833c2000               cmp dword ptr [rax + r12], 0
0x384d8f8: 74d6                     je 0x384d8d0
0x384d8fa: 4a8b7c20f8               mov rdi, qword ptr [rax + r12 - 8]
0x384d8ff: 4885ff                   test rdi, rdi
0x384d902: 74cc                     je 0x384d8d0
0x384d904: 488b07                   mov rax, qword ptr [rdi]
0x384d907: 4c89f2                   mov rdx, r14
0x384d90a: 31f6                     xor esi, esi
0x384d90c: ff5068                   call qword ptr [rax + 0x68]
0x384d90f: 84c0                     test al, al
0x384d911: 74bd                     je 0x384d8d0
0x384d913: 49ffcd                   dec r13
0x384d916: 4983c4f0                 add r12, -0x10
0x384d91a: 498d4501                 lea rax, [r13 + 1]
0x384d91e: 4883f801                 cmp rax, 1
0x384d922: 77cc                     ja 0x384d8f0
0x384d924: ff4b14                   dec dword ptr [rbx + 0x14]
0x384d927: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384d92e: 4c8bad88feffff           mov r13, qword ptr [rbp - 0x178]
0x384d935: 41f6c701                 test r15b, 1
0x384d939: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384d940: 7413                     je 0x384d955
0x384d942: 488d3d2f3ad005           lea rdi, [rip + 0x5d03a2f]
0x384d949: 31f6                     xor esi, esi
0x384d94b: e8a0347efc               call 0x30df0
0x384d950: eb03                     jmp 0x384d955
0x384d952: 894b14                   mov dword ptr [rbx + 0x14], ecx
0x384d955: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384d95d: e88e9ef4ff               call 0x37977f0
0x384d962: 488d35419d0e04           lea rsi, [rip + 0x40e9d41] ; ['quiet']
0x384d969: 4c89ff                   mov rdi, r15
0x384d96c: e8bfcaedff               call 0x372a430
0x384d971: 84c0                     test al, al
0x384d973: 0f8570010000             jne 0x384dae9
0x384d979: 41c685440d000002         mov byte ptr [r13 + 0xd44], 2
0x384d981: 31c0                     xor eax, eax
0x384d983: 498dbd480d0000           lea rdi, [r13 + 0xd48]
0x384d98a: 418b5730                 mov edx, dword ptr [r15 + 0x30]
0x384d98e: 498b7728                 mov rsi, qword ptr [r15 + 0x28]
0x384d992: 83ea01                   sub edx, 1
0x384d995: 0f42d0                   cmovb edx, eax
0x384d998: e8736c81fc               call 0x64610
0x384d99d: 488d3502401604           lea rsi, [rip + 0x4164002] ; ['Game=']
0x384d9a4: 4c89ff                   mov rdi, r15
0x384d9a7: e884caedff               call 0x372a430
0x384d9ac: 84c0                     test al, al
0x384d9ae: 7434                     je 0x384d9e4
0x384d9b0: 488d35ef3f1604           lea rsi, [rip + 0x4163fef] ; ['Game=']
0x384d9b7: 488d15e21bde03           lea rdx, [rip + 0x3de1be2]
0x384d9be: 4c89ff                   mov rdi, r15
0x384d9c1: e81acbedff               call 0x372a4e0
0x384d9c6: 498dbd580d0000           lea rdi, [r13 + 0xd58]
0x384d9cd: 4889c6                   mov rsi, rax
0x384d9d0: e87b6b81fc               call 0x64550
0x384d9d5: 488d1d342db005           lea rbx, [rip + 0x5b02d34]
0x384d9dc: 837b0800                 cmp dword ptr [rbx + 8], 0
0x384d9e0: 7522                     jne 0x384da04
0x384d9e2: eb44                     jmp 0x384da28
0x384d9e4: 498dbd580d0000           lea rdi, [r13 + 0xd58]
0x384d9eb: 488d35ae1bde03           lea rsi, [rip + 0x3de1bae]
0x384d9f2: e8596b81fc               call 0x64550
0x384d9f7: 488d1d122db005           lea rbx, [rip + 0x5b02d12]
0x384d9fe: 837b0800                 cmp dword ptr [rbx + 8], 0
0x384da02: 7424                     je 0x384da28
0x384da04: 488b3b                   mov rdi, qword ptr [rbx]
0x384da07: 4885ff                   test rdi, rdi
0x384da0a: 741c                     je 0x384da28
0x384da0c: 488b07                   mov rax, qword ptr [rdi]
0x384da0f: ff5038                   call qword ptr [rax + 0x38]
0x384da12: 84c0                     test al, al
0x384da14: 7412                     je 0x384da28
0x384da16: 837b0800                 cmp dword ptr [rbx + 8], 0
0x384da1a: 0f8492000000             je 0x384dab2
0x384da20: 488b3b                   mov rdi, qword ptr [rbx]
0x384da23: e98c000000               jmp 0x384dab4
0x384da28: 4531f6                   xor r14d, r14d
0x384da2b: 488b05e65ed005           mov rax, qword ptr [rip + 0x5d05ee6]
0x384da32: 4885c0                   test rax, rax
0x384da35: 0f8492000000             je 0x384dacd
0x384da3b: 488bb8c80f0000           mov rdi, qword ptr [rax + 0xfc8]
0x384da42: 4885ff                   test rdi, rdi
0x384da45: 0f8482000000             je 0x384dacd
0x384da4b: 488b07                   mov rax, qword ptr [rdi]
0x384da4e: ff9040010000             call qword ptr [rax + 0x140]
0x384da54: 4885c0                   test rax, rax
0x384da57: 7474                     je 0x384dacd
0x384da59: 488b08                   mov rcx, qword ptr [rax]
0x384da5c: 4889c7                   mov rdi, rax
0x384da5f: ff5138                   call qword ptr [rcx + 0x38]
0x384da62: 4108c6                   or r14b, al
0x384da65: 757a                     jne 0x384dae1
0x384da67: eb6b                     jmp 0x384dad4
0x384da69: ff4b14                   dec dword ptr [rbx + 0x14]
0x384da6c: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384da73: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384da7a: e9f2fbffff               jmp 0x384d671
0x384da7f: ff4b14                   dec dword ptr [rbx + 0x14]
0x384da82: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384da89: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384da90: e985fcffff               jmp 0x384d71a
0x384da95: ff4b14                   dec dword ptr [rbx + 0x14]
0x384da98: 4c8bbd90feffff           mov r15, qword ptr [rbp - 0x170]
0x384da9f: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384daa6: 4c8bad88feffff           mov r13, qword ptr [rbp - 0x178]
0x384daad: e990feffff               jmp 0x384d942
0x384dab2: 31ff                     xor edi, edi
0x384dab4: 488b07                   mov rax, qword ptr [rdi]
0x384dab7: ff5060                   call qword ptr [rax + 0x60]
0x384daba: 4189c6                   mov r14d, eax
0x384dabd: 488b05545ed005           mov rax, qword ptr [rip + 0x5d05e54]
0x384dac4: 4885c0                   test rax, rax
0x384dac7: 0f856effffff             jne 0x384da3b
0x384dacd: 31c0                     xor eax, eax
0x384dacf: 4108c6                   or r14b, al
0x384dad2: 750d                     jne 0x384dae1
0x384dad4: 498b4500                 mov rax, qword ptr [r13]
0x384dad8: 4c89ef                   mov rdi, r13
0x384dadb: ff9010050000             call qword ptr [rax + 0x510]
0x384dae1: 41c685440d000000         mov byte ptr [r13 + 0xd44], 0
0x384dae9: 498bb424c0020000         mov rsi, qword ptr [r12 + 0x2c0]
0x384daf1: 4c89ef                   mov rdi, r13
0x384daf4: e8d7a2ffff               call 0x3847dd0
0x384daf9: 498b8424c0020000         mov rax, qword ptr [r12 + 0x2c0]
0x384db01: be01000000               mov esi, 1
0x384db06: 81a08b0100007fffffff     and dword ptr [rax + 0x18b], 0xffffff7f
0x384db10: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384db18: e87309f4ff               call 0x378e490
0x384db1d: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384db25: be02000000               mov esi, 2
0x384db2a: e8a109f4ff               call 0x378e4d0
0x384db2f: 488d1d5a38d005           lea rbx, [rip + 0x5d0385a]
0x384db36: 4d8bb424c0020000         mov r14, qword ptr [r12 + 0x2c0]
0x384db3e: 8b4b14                   mov ecx, dword ptr [rbx + 0x14]
0x384db41: 8d4101                   lea eax, [rcx + 1]
0x384db44: 894314                   mov dword ptr [rbx + 0x14], eax
0x384db47: 48634308                 movsxd rax, dword ptr [rbx + 8]
0x384db4b: 4885c0                   test rax, rax
0x384db4e: 0f8e90000000             jle 0x384dbe4
0x384db54: 4531ff                   xor r15d, r15d
0x384db57: eb18                     jmp 0x384db71
0x384db59: 0f1f8000000000           nop dword ptr [rax]
0x384db60: 498d45ff                 lea rax, [r13 - 1]
0x384db64: 41b701                   mov r15b, 1
0x384db67: 4983fd01                 cmp r13, 1
0x384db6b: 0f869d150000             jbe 0x384f10e
0x384db71: 4989c4                   mov r12, rax
0x384db74: 4989c5                   mov r13, rax
0x384db77: 49c1e404                 shl r12, 4
0x384db7b: 4983c4f8                 add r12, -8
0x384db7f: 90                       nop 
0x384db80: 488b03                   mov rax, qword ptr [rbx]
0x384db83: 42833c2000               cmp dword ptr [rax + r12], 0
0x384db88: 74d6                     je 0x384db60
0x384db8a: 4a8b7c20f8               mov rdi, qword ptr [rax + r12 - 8]
0x384db8f: 4885ff                   test rdi, rdi
0x384db92: 74cc                     je 0x384db60
0x384db94: 488b07                   mov rax, qword ptr [rdi]
0x384db97: 4c89f2                   mov rdx, r14
0x384db9a: 31f6                     xor esi, esi
0x384db9c: ff5068                   call qword ptr [rax + 0x68]
0x384db9f: 84c0                     test al, al
0x384dba1: 74bd                     je 0x384db60
0x384dba3: 49ffcd                   dec r13
0x384dba6: 4983c4f0                 add r12, -0x10
0x384dbaa: 498d4501                 lea rax, [r13 + 1]
0x384dbae: 4883f801                 cmp rax, 1
0x384dbb2: 77cc                     ja 0x384db80
0x384dbb4: ff4b14                   dec dword ptr [rbx + 0x14]
0x384dbb7: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384dbbe: 41f6c701                 test r15b, 1
0x384dbc2: 740e                     je 0x384dbd2
0x384dbc4: 488d3dc537d005           lea rdi, [rip + 0x5d037c5]
0x384dbcb: 31f6                     xor esi, esi
0x384dbcd: e81e327efc               call 0x30df0
0x384dbd2: 498bbc2408020000         mov rdi, qword ptr [r12 + 0x208]
0x384dbda: 4885ff                   test rdi, rdi
0x384dbdd: 7519                     jne 0x384dbf8
0x384dbdf: e93b010000               jmp 0x384dd1f
0x384dbe4: 894b14                   mov dword ptr [rbx + 0x14], ecx
0x384dbe7: 498bbc2408020000         mov rdi, qword ptr [r12 + 0x208]
0x384dbef: 4885ff                   test rdi, rdi
0x384dbf2: 0f8427010000             je 0x384dd1f
0x384dbf8: e8339f77ff               call 0x2fc7b30
0x384dbfd: 85d2                     test edx, edx
0x384dbff: 0f881a010000             js 0x384dd1f
0x384dc05: 4989c5                   mov r13, rax
0x384dc08: 395008                   cmp dword ptr [rax + 8], edx
0x384dc0b: 0f8e0e010000             jle 0x384dd1f
0x384dc11: 4c8d3d4412b505           lea r15, [rip + 0x5b51244]
0x384dc18: 89d3                     mov ebx, edx
0x384dc1a: eb64                     jmp 0x384dc80
0x384dc1c: 4c89f6                   mov rsi, r14
0x384dc1f: ba01000000               mov edx, 1
0x384dc24: b901000000               mov ecx, 1
0x384dc29: e8d23793ff               call 0x3181400
0x384dc2e: 488b8598feffff           mov rax, qword ptr [rbp - 0x168]
0x384dc35: 498b742430               mov rsi, qword ptr [r12 + 0x30]
0x384dc3a: 488bb8c0020000           mov rdi, qword ptr [rax + 0x2c0]
0x384dc41: ba01000000               mov edx, 1
0x384dc46: b901000000               mov ecx, 1
0x384dc4b: e8b03793ff               call 0x3181400
0x384dc50: 49c744243000000000       mov qword ptr [r12 + 0x30], 0
0x384dc59: 4180a424c8000000fe       and byte ptr [r12 + 0xc8], 0xfe
0x384dc62: 4c89e7                   mov rdi, r12
0x384dc65: 31f6                     xor esi, esi
0x384dc67: 31d2                     xor edx, edx
0x384dc69: 498b0424                 mov rax, qword ptr [r12]
0x384dc6d: ff9088030000             call qword ptr [rax + 0x388]
0x384dc73: 48ffc3                   inc rbx
0x384dc76: 41395d08                 cmp dword ptr [r13 + 8], ebx
0x384dc7a: 0f8e9f000000             jle 0x384dd1f
0x384dc80: 498b4500                 mov rax, qword ptr [r13]
0x384dc84: 4c8b24d8                 mov r12, qword ptr [rax + rbx*8]
0x384dc88: 498b7c2430               mov rdi, qword ptr [r12 + 0x30]
0x384dc8d: 4885ff                   test rdi, rdi
0x384dc90: 74c7                     je 0x384dc59
0x384dc92: e809c62cff               call 0x2b1a2a0
0x384dc97: 488b8d98feffff           mov rcx, qword ptr [rbp - 0x168]
0x384dc9e: 488bb9c0020000           mov rdi, qword ptr [rcx + 0x2c0]
0x384dca5: 4839f8                   cmp rax, rdi
0x384dca8: 75af                     jne 0x384dc59
0x384dcaa: 498b742430               mov rsi, qword ptr [r12 + 0x30]
0x384dcaf: 410fb607                 movzx eax, byte ptr [r15]
0x384dcb3: 4c8bb6e8020000           mov r14, qword ptr [rsi + 0x2e8]
0x384dcba: 3c01                     cmp al, 1
0x384dcbc: 752d                     jne 0x384dceb
0x384dcbe: 4d85f6                   test r14, r14
0x384dcc1: 7428                     je 0x384dceb
0x384dcc3: 4c89f7                   mov rdi, r14
0x384dcc6: e8f5a5b1fc               call 0x3682c0
0x384dccb: 488b8598feffff           mov rax, qword ptr [rbp - 0x168]
0x384dcd2: 488bb8c0020000           mov rdi, qword ptr [rax + 0x2c0]
0x384dcd9: 498b442430               mov rax, qword ptr [r12 + 0x30]
0x384dcde: 4c8bb0e8020000           mov r14, qword ptr [rax + 0x2e8]
0x384dce5: 410fb607                 movzx eax, byte ptr [r15]
0x384dce9: eb09                     jmp 0x384dcf4
0x384dceb: 4d85f6                   test r14, r14
0x384dcee: 0f844dffffff             je 0x384dc41
0x384dcf4: a801                     test al, 1
0x384dcf6: 0f8420ffffff             je 0x384dc1c
0x384dcfc: 4d85f6                   test r14, r14
0x384dcff: 0f8417ffffff             je 0x384dc1c
0x384dd05: 4989ff                   mov r15, rdi
0x384dd08: 4c89f7                   mov rdi, r14
0x384dd0b: e8b0a5b1fc               call 0x3682c0
0x384dd10: 4c89ff                   mov rdi, r15
0x384dd13: 4c8d3d4211b505           lea r15, [rip + 0x5b51142]
0x384dd1a: e9fdfeffff               jmp 0x384dc1c
0x384dd1f: 4c8ba598feffff           mov r12, qword ptr [rbp - 0x168]
0x384dd26: be01000000               mov esi, 1
0x384dd2b: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384dd33: e8e89cf4ff               call 0x3797a20
0x384dd38: 498bbc24c0020000         mov rdi, qword ptr [r12 + 0x2c0]
0x384dd40: be01000000               mov esi, 1
0x384dd45: ba01000000               mov edx, 1
0x384dd4a: 31c9                     xor ecx, ecx
0x384dd4c: e86f09f4ff               call 0x378e6c0
0x384dd51: 488b3dc05bd005           mov rdi, qword ptr [rip + 0x5d05bc0]
0x384dd58: 4c8bad88feffff           mov r13, qword ptr [rbp - 0x178]

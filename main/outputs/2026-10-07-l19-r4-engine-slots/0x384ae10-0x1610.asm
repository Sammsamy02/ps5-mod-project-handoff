0x384ae10: 55                       push rbp
0x384ae11: 4889e5                   mov rbp, rsp
0x384ae14: 4157                     push r15
0x384ae16: 4156                     push r14
0x384ae18: 4155                     push r13
0x384ae1a: 4154                     push r12
0x384ae1c: 53                       push rbx
0x384ae1d: 4881ec88020000           sub rsp, 0x288
0x384ae24: 488b05dd708e05           mov rax, qword ptr [rip + 0x58e70dd]
0x384ae2b: 4889bd60fdffff           mov qword ptr [rbp - 0x2a0], rdi
0x384ae32: 4c8d2d6747de03           lea r13, [rip + 0x3de4767]
0x384ae39: 4889f3                   mov rbx, rsi
0x384ae3c: 4889cf                   mov rdi, rcx
0x384ae3f: 4989cf                   mov r15, rcx
0x384ae42: 4989d4                   mov r12, rdx
0x384ae45: 4c89ee                   mov rsi, r13
0x384ae48: 488b00                   mov rax, qword ptr [rax]
0x384ae4b: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384ae4f: e8fc9681fc               call 0x64550
0x384ae54: 488dbba8000000           lea rdi, [rbx + 0xa8]
0x384ae5b: 4c89ee                   mov rsi, r13
0x384ae5e: e8ed9681fc               call 0x64550
0x384ae63: 488b83c0020000           mov rax, qword ptr [rbx + 0x2c0]
0x384ae6a: 48899d68fdffff           mov qword ptr [rbp - 0x298], rbx
0x384ae71: 4885c0                   test rax, rax
0x384ae74: 7416                     je 0x384ae8c
0x384ae76: 488b4038                 mov rax, qword ptr [rax + 0x38]
0x384ae7a: 4885c0                   test rax, rax
0x384ae7d: 740d                     je 0x384ae8c
0x384ae7f: 80b80803000001           cmp byte ptr [rax + 0x308], 1
0x384ae86: 0f8462130000             je 0x384c1ee
0x384ae8c: 41837c243000             cmp dword ptr [r12 + 0x30], 0
0x384ae92: 498d5c2428               lea rbx, [r12 + 0x28]
0x384ae97: 7403                     je 0x384ae9c
0x384ae99: 4c8b2b                   mov r13, qword ptr [rbx]
0x384ae9c: 488d3503d6cd03           lea rsi, [rip + 0x3cdd603] ; ['.unreal']
0x384aea3: 4c89ef                   mov rdi, r13
0x384aea6: e8f579c203               call 0x74728a0
0x384aeab: 458b742430               mov r14d, dword ptr [r12 + 0x30]
0x384aeb0: 4989c5                   mov r13, rax
0x384aeb3: 4585f6                   test r14d, r14d
0x384aeb6: 7405                     je 0x384aebd
0x384aeb8: 488b3b                   mov rdi, qword ptr [rbx]
0x384aebb: eb07                     jmp 0x384aec4
0x384aebd: 488d3ddc46de03           lea rdi, [rip + 0x3de46dc]
0x384aec4: 4929fd                   sub r13, rdi
0x384aec7: 49d1fd                   sar r13, 1
0x384aeca: e8c171c203               call 0x7472090
0x384aecf: 83c0f9                   add eax, -7
0x384aed2: 4c89bd58fdffff           mov qword ptr [rbp - 0x2a8], r15
0x384aed9: 4898                     cdqe 
0x384aedb: 4939c5                   cmp r13, rax
0x384aede: 0f858b030000             jne 0x384b26f
0x384aee4: 488d054d48b005           lea rax, [rip + 0x5b0484d]
0x384aeeb: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384aeef: c5f8118540ffffff         vmovups xmmword ptr [rbp - 0xc0], xmm0
0x384aef7: 4c8b28                   mov r13, qword ptr [rax]
0x384aefa: 4585f6                   test r14d, r14d
0x384aefd: 7405                     je 0x384af04
0x384aeff: 488b33                   mov rsi, qword ptr [rbx]
0x384af02: eb07                     jmp 0x384af0b
0x384af04: 488d359546de03           lea rsi, [rip + 0x3de4695]
0x384af0b: 4c8db550ffffff           lea r14, [rbp - 0xb0]
0x384af12: 4c89f7                   mov rdi, r14
0x384af15: e8d68f81fc               call 0x63ef0
0x384af1a: 488d354fb8e703           lea rsi, [rip + 0x3e7b84f] ; ['Link']
0x384af21: 488d15609ee303           lea rdx, [rip + 0x3e39e60] ; ['Server']
0x384af28: 488d8d40ffffff           lea rcx, [rbp - 0xc0]
0x384af2f: 4c89ef                   mov rdi, r13
0x384af32: 4d89f0                   mov r8, r14
0x384af35: e886f694fc               call 0x19a5c0
0x384af3a: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384af41: 4189c6                   mov r14d, eax
0x384af44: 4885ff                   test rdi, rdi
0x384af47: 7405                     je 0x384af4e
0x384af49: e842cb86fc               call 0xb7a90
0x384af4e: 4584f6                   test r14b, r14b
0x384af51: 7419                     je 0x384af6c
0x384af53: 83bd48ffffff00           cmp dword ptr [rbp - 0xb8], 0
0x384af5a: 0f8462020000             je 0x384b1c2
0x384af60: 488b9540ffffff           mov rdx, qword ptr [rbp - 0xc0]
0x384af67: e95d020000               jmp 0x384b1c9
0x384af6c: 4488b577fdffff           mov byte ptr [rbp - 0x289], r14b
0x384af73: 4c8db5ccfeffff           lea r14, [rbp - 0x134]
0x384af7a: 488d35edc0fd03           lea rsi, [rip + 0x3fdc0ed] ; ['Engine']
0x384af81: 4c89f7                   mov rdi, r14
0x384af84: e887d58cfc               call 0x118510
0x384af89: 4c8dadc8feffff           lea r13, [rbp - 0x138]
0x384af90: 488d3555141904           lea rsi, [rip + 0x4191455] ; ['InvalidLink']
0x384af97: 4c89ef                   mov rdi, r13
0x384af9a: e871d58cfc               call 0x118510
0x384af9f: 4c8dbd20ffffff           lea r15, [rbp - 0xe0]
0x384afa6: 488d0dcdb7e703           lea rcx, [rip + 0x3e7b7cd] ; ['Invalid Link: {0}']
0x384afad: 4c89f6                   mov rsi, r14
0x384afb0: 4c89ea                   mov rdx, r13
0x384afb3: 4c89ff                   mov rdi, r15
0x384afb6: e885b48afc               call 0xf6440
0x384afbb: 488dbdd0feffff           lea rdi, [rbp - 0x130]
0x384afc2: 4c89fe                   mov rsi, r15
0x384afc5: ba01000000               mov edx, 1
0x384afca: e831ec8bfc               call 0x109c00
0x384afcf: 488dbd10ffffff           lea rdi, [rbp - 0xf0]
0x384afd6: 4889de                   mov rsi, rbx
0x384afd9: e8c2688bfc               call 0x1018a0
0x384afde: c5f81085d0feffff         vmovups xmm0, xmmword ptr [rbp - 0x130]
0x384afe6: 488b85d8feffff           mov rax, qword ptr [rbp - 0x128]
0x384afed: c5f81185f0feffff         vmovups xmmword ptr [rbp - 0x110], xmm0
0x384aff5: 4885c0                   test rax, rax
0x384aff8: 7404                     je 0x384affe
0x384affa: f0ff4008                 lock inc dword ptr [rax + 8]
0x384affe: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x384b005: 488db510ffffff           lea rsi, [rbp - 0xf0]
0x384b00c: c78550ffffff04000000     mov dword ptr [rbp - 0xb0], 4
0x384b016: e8851a8bfc               call 0xfcaa0
0x384b01b: 488d9de0feffff           lea rbx, [rbp - 0x120]
0x384b022: 488d85ecfeffff           lea rax, [rbp - 0x114]
0x384b029: bf20000000               mov edi, 0x20
0x384b02e: be08000000               mov esi, 8
0x384b033: ba01000000               mov edx, 1
0x384b038: 41b901000000             mov r9d, 1
0x384b03e: 48c785e0feffff00000000   mov qword ptr [rbp - 0x120], 0
0x384b049: c785e8feffff01000000     mov dword ptr [rbp - 0x118], 1
0x384b053: 31c9                     xor ecx, ecx
0x384b055: 4989d8                   mov r8, rbx
0x384b058: 48890424                 mov qword ptr [rsp], rax
0x384b05c: e86fc57dfc               call 0x275d0
0x384b061: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b068: 488db550ffffff           lea rsi, [rbp - 0xb0]
0x384b06f: ba01000000               mov edx, 1
0x384b074: e8d70083fc               call 0x7b150
0x384b079: 488dbd30ffffff           lea rdi, [rbp - 0xd0]
0x384b080: 488db5f0feffff           lea rsi, [rbp - 0x110]
0x384b087: 4889da                   mov rdx, rbx
0x384b08a: e871298bfc               call 0xfda00
0x384b08f: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b096: 8bb5e8feffff             mov esi, dword ptr [rbp - 0x118]
0x384b09c: e83f0183fc               call 0x7b1e0
0x384b0a1: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b0a8: 4c8bbd58fdffff           mov r15, qword ptr [rbp - 0x2a8]
0x384b0af: 4885ff                   test rdi, rdi
0x384b0b2: 7405                     je 0x384b0b9
0x384b0b4: e8d7c986fc               call 0xb7a90
0x384b0b9: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384b0c0: 440fb6b577fdffff         movzx r14d, byte ptr [rbp - 0x289]
0x384b0c8: 4885ff                   test rdi, rdi
0x384b0cb: 7406                     je 0x384b0d3
0x384b0cd: 488b07                   mov rax, qword ptr [rdi]
0x384b0d0: ff5018                   call qword ptr [rax + 0x18]
0x384b0d3: 488b9df8feffff           mov rbx, qword ptr [rbp - 0x108]
0x384b0da: 4885db                   test rbx, rbx
0x384b0dd: 741d                     je 0x384b0fc
0x384b0df: f0ff4b08                 lock dec dword ptr [rbx + 8]
0x384b0e3: 7517                     jne 0x384b0fc
0x384b0e5: 488b03                   mov rax, qword ptr [rbx]
0x384b0e8: 4889df                   mov rdi, rbx
0x384b0eb: ff10                     call qword ptr [rax]
0x384b0ed: f0ff4b0c                 lock dec dword ptr [rbx + 0xc]
0x384b0f1: 7509                     jne 0x384b0fc
0x384b0f3: 488b03                   mov rax, qword ptr [rbx]
0x384b0f6: 4889df                   mov rdi, rbx
0x384b0f9: ff5010                   call qword ptr [rax + 0x10]
0x384b0fc: 488dbd30ffffff           lea rdi, [rbp - 0xd0]
0x384b103: e878138bfc               call 0xfc480
0x384b108: 4c39f8                   cmp rax, r15
0x384b10b: 744d                     je 0x384b15a
0x384b10d: 418b4f0c                 mov ecx, dword ptr [r15 + 0xc]
0x384b111: 48635808                 movsxd rbx, dword ptr [rax + 8]
0x384b115: 4c8b28                   mov r13, qword ptr [rax]
0x384b118: 498d470c                 lea rax, [r15 + 0xc]
0x384b11c: 89ca                     mov edx, ecx
0x384b11e: 41895f08                 mov dword ptr [r15 + 8], ebx
0x384b122: 09da                     or edx, ebx
0x384b124: 0f849c0b0000             je 0x384bcc6
0x384b12a: bf02000000               mov edi, 2
0x384b12f: be02000000               mov esi, 2
0x384b134: 89da                     mov edx, ebx
0x384b136: 4d89f8                   mov r8, r15
0x384b139: 4189d9                   mov r9d, ebx
0x384b13c: 48890424                 mov qword ptr [rsp], rax
0x384b140: e88bc47dfc               call 0x275d0
0x384b145: 85db                     test ebx, ebx
0x384b147: 7411                     je 0x384b15a
0x384b149: 498b3f                   mov rdi, qword ptr [r15]
0x384b14c: 4801db                   add rbx, rbx
0x384b14f: 4c89ee                   mov rsi, r13
0x384b152: 4889da                   mov rdx, rbx
0x384b155: e8f66cc203               call 0x7471e50
0x384b15a: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384b161: 4885ff                   test rdi, rdi
0x384b164: 7406                     je 0x384b16c
0x384b166: 488b07                   mov rax, qword ptr [rdi]
0x384b169: ff5018                   call qword ptr [rax + 0x18]
0x384b16c: 488bbd10ffffff           mov rdi, qword ptr [rbp - 0xf0]
0x384b173: 4885ff                   test rdi, rdi
0x384b176: 7406                     je 0x384b17e
0x384b178: 488b07                   mov rax, qword ptr [rdi]
0x384b17b: ff5018                   call qword ptr [rax + 0x18]
0x384b17e: 488b9dd8feffff           mov rbx, qword ptr [rbp - 0x128]
0x384b185: 4885db                   test rbx, rbx
0x384b188: 741d                     je 0x384b1a7
0x384b18a: f0ff4b08                 lock dec dword ptr [rbx + 8]
0x384b18e: 7517                     jne 0x384b1a7
0x384b190: 488b03                   mov rax, qword ptr [rbx]
0x384b193: 4889df                   mov rdi, rbx
0x384b196: ff10                     call qword ptr [rax]
0x384b198: f0ff4b0c                 lock dec dword ptr [rbx + 0xc]
0x384b19c: 7509                     jne 0x384b1a7
0x384b19e: 488b03                   mov rax, qword ptr [rbx]
0x384b1a1: 4889df                   mov rdi, rbx
0x384b1a4: ff5010                   call qword ptr [rax + 0x10]
0x384b1a7: 488bbd20ffffff           mov rdi, qword ptr [rbp - 0xe0]
0x384b1ae: 4885ff                   test rdi, rdi
0x384b1b1: 0f8499000000             je 0x384b250
0x384b1b7: 488b07                   mov rax, qword ptr [rdi]
0x384b1ba: ff5018                   call qword ptr [rax + 0x18]
0x384b1bd: e98e000000               jmp 0x384b250
0x384b1c2: 488d15d743de03           lea rdx, [rip + 0x3de43d7]
0x384b1c9: 488d9d50ffffff           lea rbx, [rbp - 0xb0]
0x384b1d0: 31f6                     xor esi, esi
0x384b1d2: 31c9                     xor ecx, ecx
0x384b1d4: 4889df                   mov rdi, rbx
0x384b1d7: e8d4cfedff               call 0x37281b0
0x384b1dc: 4c89e7                   mov rdi, r12
0x384b1df: 4889de                   mov rsi, rbx
0x384b1e2: e809d69eff               call 0x32387f0
0x384b1e7: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x384b1eb: 4885ff                   test rdi, rdi
0x384b1ee: 7405                     je 0x384b1f5
0x384b1f0: e89bc886fc               call 0xb7a90
0x384b1f5: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384b1f9: 8b75a0                   mov esi, dword ptr [rbp - 0x60]
0x384b1fc: e83ffc7dfc               call 0x2ae40
0x384b201: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384b205: 4885ff                   test rdi, rdi
0x384b208: 7405                     je 0x384b20f
0x384b20a: e881c886fc               call 0xb7a90
0x384b20f: 488b7d88                 mov rdi, qword ptr [rbp - 0x78]
0x384b213: 4885ff                   test rdi, rdi
0x384b216: 7405                     je 0x384b21d
0x384b218: e873c886fc               call 0xb7a90
0x384b21d: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x384b224: 4885ff                   test rdi, rdi
0x384b227: 7405                     je 0x384b22e
0x384b229: e862c886fc               call 0xb7a90
0x384b22e: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384b235: 4885ff                   test rdi, rdi
0x384b238: 7405                     je 0x384b23f
0x384b23a: e851c886fc               call 0xb7a90
0x384b23f: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384b246: 4885ff                   test rdi, rdi
0x384b249: 7405                     je 0x384b250
0x384b24b: e840c886fc               call 0xb7a90
0x384b250: 488bbd40ffffff           mov rdi, qword ptr [rbp - 0xc0]
0x384b257: 4885ff                   test rdi, rdi
0x384b25a: 7405                     je 0x384b261
0x384b25c: e82fc886fc               call 0xb7a90
0x384b261: bb01000000               mov ebx, 1
0x384b266: 4584f6                   test r14b, r14b
0x384b269: 0f84270c0000             je 0x384be96
0x384b26f: 41837c242400             cmp dword ptr [r12 + 0x24], 0
0x384b275: 0f841c010000             je 0x384b397
0x384b27b: 488d357c661604           lea rsi, [rip + 0x416667c] ; ['failed']
0x384b282: 4c89e7                   mov rdi, r12
0x384b285: e8a6f1edff               call 0x372a430
0x384b28a: 84c0                     test al, al
0x384b28c: 7517                     jne 0x384b2a5
0x384b28e: 488d355f660404           lea rsi, [rip + 0x404665f] ; ['closed']
0x384b295: 4c89e7                   mov rdi, r12
0x384b298: e893f1edff               call 0x372a430
0x384b29d: 84c0                     test al, al
0x384b29f: 0f8487040000             je 0x384b72c
0x384b2a5: 4c8bad68fdffff           mov r13, qword ptr [rbp - 0x298]
0x384b2ac: 4c8bbd60fdffff           mov r15, qword ptr [rbp - 0x2a0]
0x384b2b3: 4983bd9001000000         cmp qword ptr [r13 + 0x190], 0
0x384b2bb: 740f                     je 0x384b2cc
0x384b2bd: 498b07                   mov rax, qword ptr [r15]
0x384b2c0: 4c89ff                   mov rdi, r15
0x384b2c3: 4c89ee                   mov rsi, r13
0x384b2c6: ff9060050000             call qword ptr [rax + 0x560]
0x384b2cc: 498b85c0020000           mov rax, qword ptr [r13 + 0x2c0]
0x384b2d3: 4885c0                   test rax, rax
0x384b2d6: 7409                     je 0x384b2e1
0x384b2d8: 488b7820                 mov rdi, qword ptr [rax + 0x20]
0x384b2dc: e86f85b5fc               call 0x3a3850
0x384b2e1: 41807d0006               cmp byte ptr [r13], 6
0x384b2e6: 0f85dd030000             jne 0x384b6c9
0x384b2ec: 48c78550ffffff00000000   mov qword ptr [rbp - 0xb0], 0
0x384b2f7: 498b9d10020000           mov rbx, qword ptr [r13 + 0x210]
0x384b2fe: 458bbd18020000           mov r15d, dword ptr [r13 + 0x218]
0x384b305: 4489bd58ffffff           mov dword ptr [rbp - 0xa8], r15d
0x384b30c: 4585ff                   test r15d, r15d
0x384b30f: 0f84d7070000             je 0x384baec
0x384b315: 488d855cffffff           lea rax, [rbp - 0xa4]
0x384b31c: 4c8d8550ffffff           lea r8, [rbp - 0xb0]
0x384b323: bf10000000               mov edi, 0x10
0x384b328: be08000000               mov esi, 8
0x384b32d: 4489fa                   mov edx, r15d
0x384b330: 4589f9                   mov r9d, r15d
0x384b333: 31c9                     xor ecx, ecx
0x384b335: 48890424                 mov qword ptr [rsp], rax
0x384b339: e892c27dfc               call 0x275d0
0x384b33e: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384b345: 4889de                   mov rsi, rbx
0x384b348: 4489fa                   mov edx, r15d
0x384b34b: e8905122ff               call 0x2a704e0
0x384b350: 48639d58ffffff           movsxd rbx, dword ptr [rbp - 0xa8]
0x384b357: 4885db                   test rbx, rbx
0x384b35a: 0f8496070000             je 0x384baf6
0x384b360: 4c8bb550ffffff           mov r14, qword ptr [rbp - 0xb0]
0x384b367: 48c1e304                 shl rbx, 4
0x384b36b: 4531ff                   xor r15d, r15d
0x384b36e: eb0d                     jmp 0x384b37d
0x384b370: 4983c710                 add r15, 0x10
0x384b374: 4c39fb                   cmp rbx, r15
0x384b377: 0f8479070000             je 0x384baf6
0x384b37d: 4b8b043e                 mov rax, qword ptr [r14 + r15]
0x384b381: 4885c0                   test rax, rax
0x384b384: 74ea                     je 0x384b370
0x384b386: 488bb000020000           mov rsi, qword ptr [rax + 0x200]
0x384b38d: 4c89ef                   mov rdi, r13
0x384b390: e82bd5ffff               call 0x38488c0
0x384b395: ebd9                     jmp 0x384b370
0x384b397: 488d9dccfeffff           lea rbx, [rbp - 0x134]
0x384b39e: 488d35c9bcfd03           lea rsi, [rip + 0x3fdbcc9] ; ['Engine']
0x384b3a5: 4889df                   mov rdi, rbx
0x384b3a8: e863d18cfc               call 0x118510
0x384b3ad: 4c8db5c8feffff           lea r14, [rbp - 0x138]
0x384b3b4: 488d35c3c21304           lea rsi, [rip + 0x413c2c3] ; ['InvalidUrl']
0x384b3bb: 4c89f7                   mov rdi, r14
0x384b3be: e84dd18cfc               call 0x118510
0x384b3c3: 4c8dbd30ffffff           lea r15, [rbp - 0xd0]
0x384b3ca: 488d0d49e60104           lea rcx, [rip + 0x401e649] ; ['Invalid URL: {0}']
0x384b3d1: 4889de                   mov rsi, rbx
0x384b3d4: 4c89f2                   mov rdx, r14
0x384b3d7: 4c89ff                   mov rdi, r15
0x384b3da: e861b08afc               call 0xf6440
0x384b3df: 488dbdb0feffff           lea rdi, [rbp - 0x150]
0x384b3e6: 4c89fe                   mov rsi, r15
0x384b3e9: ba01000000               mov edx, 1
0x384b3ee: e80de88bfc               call 0x109c00
0x384b3f3: 488d9d20ffffff           lea rbx, [rbp - 0xe0]
0x384b3fa: 4c89e6                   mov rsi, r12
0x384b3fd: 31d2                     xor edx, edx
0x384b3ff: 4889df                   mov rdi, rbx
0x384b402: e8d9eaedff               call 0x3729ee0
0x384b407: 488dbd00ffffff           lea rdi, [rbp - 0x100]
0x384b40e: 4889de                   mov rsi, rbx
0x384b411: e81a1d8bfc               call 0xfd130
0x384b416: c5f81085b0feffff         vmovups xmm0, xmmword ptr [rbp - 0x150]
0x384b41e: 488b85b8feffff           mov rax, qword ptr [rbp - 0x148]
0x384b425: c5f81185f0feffff         vmovups xmmword ptr [rbp - 0x110], xmm0
0x384b42d: 4885c0                   test rax, rax
0x384b430: 7404                     je 0x384b436
0x384b432: f0ff4008                 lock inc dword ptr [rax + 8]
0x384b436: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x384b43d: 488db500ffffff           lea rsi, [rbp - 0x100]
0x384b444: c78550ffffff04000000     mov dword ptr [rbp - 0xb0], 4
0x384b44e: e84d168bfc               call 0xfcaa0
0x384b453: 488d9de0feffff           lea rbx, [rbp - 0x120]
0x384b45a: 488d85ecfeffff           lea rax, [rbp - 0x114]
0x384b461: bf20000000               mov edi, 0x20
0x384b466: be08000000               mov esi, 8
0x384b46b: ba01000000               mov edx, 1
0x384b470: 41b901000000             mov r9d, 1
0x384b476: 48c785e0feffff00000000   mov qword ptr [rbp - 0x120], 0
0x384b481: c785e8feffff01000000     mov dword ptr [rbp - 0x118], 1
0x384b48b: 31c9                     xor ecx, ecx
0x384b48d: 4989d8                   mov r8, rbx
0x384b490: 48890424                 mov qword ptr [rsp], rax
0x384b494: e837c17dfc               call 0x275d0
0x384b499: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b4a0: 488db550ffffff           lea rsi, [rbp - 0xb0]
0x384b4a7: ba01000000               mov edx, 1
0x384b4ac: e89ffc82fc               call 0x7b150
0x384b4b1: 488dbd40ffffff           lea rdi, [rbp - 0xc0]
0x384b4b8: 488db5f0feffff           lea rsi, [rbp - 0x110]
0x384b4bf: 4889da                   mov rdx, rbx
0x384b4c2: e839258bfc               call 0xfda00
0x384b4c7: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b4ce: 8bb5e8feffff             mov esi, dword ptr [rbp - 0x118]
0x384b4d4: e807fd82fc               call 0x7b1e0
0x384b4d9: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b4e0: 4885ff                   test rdi, rdi
0x384b4e3: 7405                     je 0x384b4ea
0x384b4e5: e8a6c586fc               call 0xb7a90
0x384b4ea: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384b4f1: 4c8bb560fdffff           mov r14, qword ptr [rbp - 0x2a0]
0x384b4f8: 4c8bbd68fdffff           mov r15, qword ptr [rbp - 0x298]
0x384b4ff: 4885ff                   test rdi, rdi
0x384b502: 7406                     je 0x384b50a
0x384b504: 488b07                   mov rax, qword ptr [rdi]
0x384b507: ff5018                   call qword ptr [rax + 0x18]
0x384b50a: 488b9df8feffff           mov rbx, qword ptr [rbp - 0x108]
0x384b511: 4885db                   test rbx, rbx
0x384b514: 741d                     je 0x384b533
0x384b516: f0ff4b08                 lock dec dword ptr [rbx + 8]
0x384b51a: 7517                     jne 0x384b533
0x384b51c: 488b03                   mov rax, qword ptr [rbx]
0x384b51f: 4889df                   mov rdi, rbx
0x384b522: ff10                     call qword ptr [rax]
0x384b524: f0ff4b0c                 lock dec dword ptr [rbx + 0xc]
0x384b528: 7509                     jne 0x384b533
0x384b52a: 488b03                   mov rax, qword ptr [rbx]
0x384b52d: 4889df                   mov rdi, rbx
0x384b530: ff5010                   call qword ptr [rax + 0x10]
0x384b533: 488dbd40ffffff           lea rdi, [rbp - 0xc0]
0x384b53a: e8410f8bfc               call 0xfc480
0x384b53f: 4c8bad58fdffff           mov r13, qword ptr [rbp - 0x2a8]
0x384b546: 4c39e8                   cmp rax, r13
0x384b549: 7451                     je 0x384b59c
0x384b54b: 418b4d0c                 mov ecx, dword ptr [r13 + 0xc]
0x384b54f: 4c636008                 movsxd r12, dword ptr [rax + 8]
0x384b553: 488b18                   mov rbx, qword ptr [rax]
0x384b556: 498d450c                 lea rax, [r13 + 0xc]
0x384b55a: 89ca                     mov edx, ecx
0x384b55c: 45896508                 mov dword ptr [r13 + 8], r12d
0x384b560: 4409e2                   or edx, r12d
0x384b563: 0f8442070000             je 0x384bcab
0x384b569: bf02000000               mov edi, 2
0x384b56e: be02000000               mov esi, 2
0x384b573: 4489e2                   mov edx, r12d
0x384b576: 4d89e8                   mov r8, r13
0x384b579: 4589e1                   mov r9d, r12d
0x384b57c: 48890424                 mov qword ptr [rsp], rax
0x384b580: e84bc07dfc               call 0x275d0
0x384b585: 4585e4                   test r12d, r12d
0x384b588: 7412                     je 0x384b59c
0x384b58a: 498b7d00                 mov rdi, qword ptr [r13]
0x384b58e: 4d01e4                   add r12, r12
0x384b591: 4889de                   mov rsi, rbx
0x384b594: 4c89e2                   mov rdx, r12
0x384b597: e8b468c203               call 0x7471e50
0x384b59c: 488bbd40ffffff           mov rdi, qword ptr [rbp - 0xc0]
0x384b5a3: 4885ff                   test rdi, rdi
0x384b5a6: 7406                     je 0x384b5ae
0x384b5a8: 488b07                   mov rax, qword ptr [rdi]
0x384b5ab: ff5018                   call qword ptr [rax + 0x18]
0x384b5ae: 488bbd00ffffff           mov rdi, qword ptr [rbp - 0x100]
0x384b5b5: 4885ff                   test rdi, rdi
0x384b5b8: 7406                     je 0x384b5c0
0x384b5ba: 488b07                   mov rax, qword ptr [rdi]
0x384b5bd: ff5018                   call qword ptr [rax + 0x18]
0x384b5c0: 488bbd20ffffff           mov rdi, qword ptr [rbp - 0xe0]
0x384b5c7: 4885ff                   test rdi, rdi
0x384b5ca: 7405                     je 0x384b5d1
0x384b5cc: e8bfc486fc               call 0xb7a90
0x384b5d1: 488b9db8feffff           mov rbx, qword ptr [rbp - 0x148]
0x384b5d8: 4885db                   test rbx, rbx
0x384b5db: 741d                     je 0x384b5fa
0x384b5dd: f0ff4b08                 lock dec dword ptr [rbx + 8]
0x384b5e1: 7517                     jne 0x384b5fa
0x384b5e3: 488b03                   mov rax, qword ptr [rbx]
0x384b5e6: 4889df                   mov rdi, rbx
0x384b5e9: ff10                     call qword ptr [rax]
0x384b5eb: f0ff4b0c                 lock dec dword ptr [rbx + 0xc]
0x384b5ef: 7509                     jne 0x384b5fa
0x384b5f1: 488b03                   mov rax, qword ptr [rbx]
0x384b5f4: 4889df                   mov rdi, rbx
0x384b5f7: ff5010                   call qword ptr [rax + 0x10]
0x384b5fa: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384b601: 4885ff                   test rdi, rdi
0x384b604: 7406                     je 0x384b60c
0x384b606: 488b07                   mov rax, qword ptr [rdi]
0x384b609: ff5018                   call qword ptr [rax + 0x18]
0x384b60c: 418b8eac0e0000           mov ecx, dword ptr [r14 + 0xeac]
0x384b613: 498b9fc0020000           mov rbx, qword ptr [r15 + 0x2c0]
0x384b61a: 8d4101                   lea eax, [rcx + 1]
0x384b61d: 418986ac0e0000           mov dword ptr [r14 + 0xeac], eax
0x384b624: 496386a00e0000           movsxd rax, dword ptr [r14 + 0xea0]
0x384b62b: 4885c0                   test rax, rax
0x384b62e: 0f8e08020000             jle 0x384b83c
0x384b634: 4981c6980e0000           add r14, 0xe98
0x384b63b: 4531ff                   xor r15d, r15d
0x384b63e: eb11                     jmp 0x384b651
0x384b640: 498d45ff                 lea rax, [r13 - 1]
0x384b644: 41b701                   mov r15b, 1
0x384b647: 4983fd01                 cmp r13, 1
0x384b64b: 0f8622090000             jbe 0x384bf73
0x384b651: 4989c4                   mov r12, rax
0x384b654: 4989c5                   mov r13, rax
0x384b657: 49c1e404                 shl r12, 4
0x384b65b: 4983c4f8                 add r12, -8
0x384b65f: 90                       nop 
0x384b660: 498b06                   mov rax, qword ptr [r14]
0x384b663: 42833c2000               cmp dword ptr [rax + r12], 0
0x384b668: 74d6                     je 0x384b640
0x384b66a: 4a8b7c20f8               mov rdi, qword ptr [rax + r12 - 8]
0x384b66f: 4885ff                   test rdi, rdi
0x384b672: 74cc                     je 0x384b640
0x384b674: 488b07                   mov rax, qword ptr [rdi]
0x384b677: 488b8d58fdffff           mov rcx, qword ptr [rbp - 0x2a8]
0x384b67e: 4889de                   mov rsi, rbx
0x384b681: ba02000000               mov edx, 2
0x384b686: ff5068                   call qword ptr [rax + 0x68]
0x384b689: 84c0                     test al, al
0x384b68b: 74b3                     je 0x384b640
0x384b68d: 49ffcd                   dec r13
0x384b690: 4983c4f0                 add r12, -0x10
0x384b694: 498d4501                 lea rax, [r13 + 1]
0x384b698: 4883f801                 cmp rax, 1
0x384b69c: 77c2                     ja 0x384b660
0x384b69e: 488b8560fdffff           mov rax, qword ptr [rbp - 0x2a0]
0x384b6a5: bb01000000               mov ebx, 1
0x384b6aa: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384b6b0: 41f6c701                 test r15b, 1
0x384b6b4: 0f84dc070000             je 0x384be96
0x384b6ba: 4c89f7                   mov rdi, r14
0x384b6bd: 31f6                     xor esi, esi
0x384b6bf: e82c577efc               call 0x30df0
0x384b6c4: e9c8070000               jmp 0x384be91
0x384b6c9: e8d2dc22fd               call 0xa793a0
0x384b6ce: 488b9810010000           mov rbx, qword ptr [rax + 0x110]
0x384b6d5: 4885db                   test rbx, rbx
0x384b6d8: 7512                     jne 0x384b6ec
0x384b6da: 4889c7                   mov rdi, rax
0x384b6dd: 4889c3                   mov rbx, rax
0x384b6e0: e8cb7eadfc               call 0x3235b0
0x384b6e5: 488b9b10010000           mov rbx, qword ptr [rbx + 0x110]
0x384b6ec: 4c8db5e0feffff           lea r14, [rbp - 0x120]
0x384b6f3: 31f6                     xor esi, esi
0x384b6f5: 4c89f7                   mov rdi, r14
0x384b6f8: e8d3ec22fd               call 0xa7a3d0
0x384b6fd: 4883c328                 add rbx, 0x28
0x384b701: 488dbdf0feffff           lea rdi, [rbp - 0x110]
0x384b708: 4c89f6                   mov rsi, r14
0x384b70b: 4889da                   mov rdx, rbx
0x384b70e: e8ddbb81fc               call 0x672f0
0x384b713: 83bdf8feffff00           cmp dword ptr [rbp - 0x108], 0
0x384b71a: 0f8428010000             je 0x384b848
0x384b720: 488b95f0feffff           mov rdx, qword ptr [rbp - 0x110]
0x384b727: e923010000               jmp 0x384b84f
0x384b72c: 488d353157dd03           lea rsi, [rip + 0x3dd5731] ; ['restart']
0x384b733: 4c89e7                   mov rdi, r12
0x384b736: e8f5ecedff               call 0x372a430
0x384b73b: 4c8bad60fdffff           mov r13, qword ptr [rbp - 0x2a0]
0x384b742: 84c0                     test al, al
0x384b744: 7416                     je 0x384b75c
0x384b746: 488b8568fdffff           mov rax, qword ptr [rbp - 0x298]
0x384b74d: 4c89e7                   mov rdi, r12
0x384b750: 488db0c0000000           lea rsi, [rax + 0xc0]
0x384b757: e8a44922ff               call 0x2a70100
0x384b75c: 488d05853dcd05           lea rax, [rip + 0x5cd3d85]
0x384b763: 803801                   cmp byte ptr [rax], 1
0x384b766: 0f8522020000             jne 0x384b98e
0x384b76c: 488d3523e7df03           lea rsi, [rip + 0x3dfe723] ; ['listen']
0x384b773: 4c89e7                   mov rdi, r12
0x384b776: e8b5ecedff               call 0x372a430
0x384b77b: 84c0                     test al, al
0x384b77d: 0f840b020000             je 0x384b98e
0x384b783: 488d9df0feffff           lea rbx, [rbp - 0x110]
0x384b78a: 488d35ddb8fd03           lea rsi, [rip + 0x3fdb8dd] ; ['Engine']
0x384b791: 4889df                   mov rdi, rbx
0x384b794: e877cd8cfc               call 0x118510
0x384b799: 4c8db5e0feffff           lea r14, [rbp - 0x120]
0x384b7a0: 488d3589bd0104           lea rsi, [rip + 0x401bd89] ; ['UsedCheatCommands']
0x384b7a7: 4c89f7                   mov rdi, r14
0x384b7aa: e861cd8cfc               call 0x118510
0x384b7af: 4c8dbd50ffffff           lea r15, [rbp - 0xb0]
0x384b7b6: 488d0d0bda1d04           lea rcx, [rip + 0x41dda0b] ; ['Console commands were used which are disallowed in netplay.  You must restart the game to create a match.']
0x384b7bd: 4889de                   mov rsi, rbx
0x384b7c0: 4c89f2                   mov rdx, r14
0x384b7c3: 4c89ff                   mov rdi, r15
0x384b7c6: e875ac8afc               call 0xf6440
0x384b7cb: 4c8bb558fdffff           mov r14, qword ptr [rbp - 0x2a8]
0x384b7d2: 4c89ff                   mov rdi, r15
0x384b7d5: e8a60c8bfc               call 0xfc480
0x384b7da: 4c39f0                   cmp rax, r14
0x384b7dd: 0f84e1060000             je 0x384bec4
0x384b7e3: 418b4e0c                 mov ecx, dword ptr [r14 + 0xc]
0x384b7e7: 4c636008                 movsxd r12, dword ptr [rax + 8]
0x384b7eb: 488b18                   mov rbx, qword ptr [rax]
0x384b7ee: 498d460c                 lea rax, [r14 + 0xc]
0x384b7f2: 89ca                     mov edx, ecx
0x384b7f4: 45896608                 mov dword ptr [r14 + 8], r12d
0x384b7f8: 4409e2                   or edx, r12d
0x384b7fb: 0f84bd060000             je 0x384bebe
0x384b801: bf02000000               mov edi, 2
0x384b806: be02000000               mov esi, 2
0x384b80b: 4489e2                   mov edx, r12d
0x384b80e: 4d89f0                   mov r8, r14
0x384b811: 4589e1                   mov r9d, r12d
0x384b814: 48890424                 mov qword ptr [rsp], rax
0x384b818: e8b3bd7dfc               call 0x275d0
0x384b81d: 4585e4                   test r12d, r12d
0x384b820: 0f849e060000             je 0x384bec4
0x384b826: 498b3e                   mov rdi, qword ptr [r14]
0x384b829: 4d01e4                   add r12, r12
0x384b82c: 4889de                   mov rsi, rbx
0x384b82f: 4c89e2                   mov rdx, r12
0x384b832: e81966c203               call 0x7471e50
0x384b837: e988060000               jmp 0x384bec4
0x384b83c: 41898eac0e0000           mov dword ptr [r14 + 0xeac], ecx
0x384b843: e949060000               jmp 0x384be91
0x384b848: 488d15513dde03           lea rdx, [rip + 0x3de3d51]
0x384b84f: 488dbd50ffffff           lea rdi, [rbp - 0xb0]
0x384b856: 4c89e6                   mov rsi, r12
0x384b859: b901000000               mov ecx, 1
0x384b85e: e84dc9edff               call 0x37281b0
0x384b863: 488bbdf0feffff           mov rdi, qword ptr [rbp - 0x110]
0x384b86a: 4885ff                   test rdi, rdi
0x384b86d: 7405                     je 0x384b874
0x384b86f: e81cc286fc               call 0xb7a90
0x384b874: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384b87b: 4885ff                   test rdi, rdi
0x384b87e: 7405                     je 0x384b885
0x384b880: e80bc286fc               call 0xb7a90
0x384b885: 488d9d48feffff           lea rbx, [rbp - 0x1b8]
0x384b88c: 488db550ffffff           lea rsi, [rbp - 0xb0]
0x384b893: 4889df                   mov rdi, rbx
0x384b896: e8c56d78ff               call 0x2fd2660
0x384b89b: 498b07                   mov rax, qword ptr [r15]
0x384b89e: 4c8b8558fdffff           mov r8, qword ptr [rbp - 0x2a8]
0x384b8a5: 4c89ff                   mov rdi, r15
0x384b8a8: 4c89ee                   mov rsi, r13
0x384b8ab: 4889da                   mov rdx, rbx
0x384b8ae: 31c9                     xor ecx, ecx
0x384b8b0: ff90f8040000             call qword ptr [rax + 0x4f8]
0x384b8b6: 488bbda0feffff           mov rdi, qword ptr [rbp - 0x160]
0x384b8bd: 89c3                     mov ebx, eax
0x384b8bf: 4885ff                   test rdi, rdi
0x384b8c2: 7405                     je 0x384b8c9
0x384b8c4: e8c7c186fc               call 0xb7a90
0x384b8c9: 488bbd90feffff           mov rdi, qword ptr [rbp - 0x170]
0x384b8d0: 8bb598feffff             mov esi, dword ptr [rbp - 0x168]
0x384b8d6: e865f57dfc               call 0x2ae40
0x384b8db: 488bbd90feffff           mov rdi, qword ptr [rbp - 0x170]
0x384b8e2: 4885ff                   test rdi, rdi
0x384b8e5: 7405                     je 0x384b8ec
0x384b8e7: e8a4c186fc               call 0xb7a90
0x384b8ec: 488bbd80feffff           mov rdi, qword ptr [rbp - 0x180]
0x384b8f3: 4885ff                   test rdi, rdi
0x384b8f6: 7405                     je 0x384b8fd
0x384b8f8: e893c186fc               call 0xb7a90
0x384b8fd: 488bbd70feffff           mov rdi, qword ptr [rbp - 0x190]
0x384b904: 4885ff                   test rdi, rdi
0x384b907: 7405                     je 0x384b90e
0x384b909: e882c186fc               call 0xb7a90
0x384b90e: 488bbd58feffff           mov rdi, qword ptr [rbp - 0x1a8]
0x384b915: 4885ff                   test rdi, rdi
0x384b918: 7405                     je 0x384b91f
0x384b91a: e871c186fc               call 0xb7a90
0x384b91f: 488bbd48feffff           mov rdi, qword ptr [rbp - 0x1b8]
0x384b926: 4885ff                   test rdi, rdi
0x384b929: 7405                     je 0x384b930
0x384b92b: e860c186fc               call 0xb7a90
0x384b930: 84db                     test bl, bl
0x384b932: 0f84ab000000             je 0x384b9e3
0x384b938: be01000000               mov esi, 1
0x384b93d: 31ff                     xor edi, edi
0x384b93f: 31db                     xor ebx, ebx
0x384b941: e81a12b2fc               call 0x36cb60
0x384b946: 4c8d359b3eb005           lea r14, [rip + 0x5b03e9b]
0x384b94d: 4981c5c0000000           add r13, 0xc0
0x384b954: 488d35a35f1604           lea rsi, [rip + 0x4165fa3] ; ['failed']
0x384b95b: 31d2                     xor edx, edx
0x384b95d: 4c89ef                   mov rdi, r13
0x384b960: 4c89f1                   mov rcx, r14
0x384b963: e828e2edff               call 0x3729b90
0x384b968: 488d35855f0404           lea rsi, [rip + 0x4045f85] ; ['closed']
0x384b96f: 4c89ef                   mov rdi, r13
0x384b972: 4c89f1                   mov rcx, r14
0x384b975: 31d2                     xor edx, edx
0x384b977: e814e2edff               call 0x3729b90
0x384b97c: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x384b980: 4885ff                   test rdi, rdi
0x384b983: 0f85aa000000             jne 0x384ba33
0x384b989: e9aa000000               jmp 0x384ba38
0x384b98e: 4183bd3011000000         cmp dword ptr [r13 + 0x1130], 0
0x384b996: 0f8400010000             je 0x384ba9c
0x384b99c: 498bbd28110000           mov rdi, qword ptr [r13 + 0x1128]
0x384b9a3: 4885ff                   test rdi, rdi
0x384b9a6: 0f84f0000000             je 0x384ba9c
0x384b9ac: 488b07                   mov rax, qword ptr [rdi]
0x384b9af: ff5038                   call qword ptr [rax + 0x38]
0x384b9b2: 84c0                     test al, al
0x384b9b4: 0f84e2000000             je 0x384ba9c
0x384b9ba: 488dbde0fdffff           lea rdi, [rbp - 0x220]
0x384b9c1: 4c89e6                   mov rsi, r12
0x384b9c4: e8976c78ff               call 0x2fd2660
0x384b9c9: 4183bd3011000000         cmp dword ptr [r13 + 0x1130], 0
0x384b9d1: 0f84ba050000             je 0x384bf91
0x384b9d7: 498bbd28110000           mov rdi, qword ptr [r13 + 0x1128]
0x384b9de: e9b0050000               jmp 0x384bf93
0x384b9e3: 488d9df0feffff           lea rbx, [rbp - 0x110]
0x384b9ea: 488db550ffffff           lea rsi, [rbp - 0xb0]
0x384b9f1: 31d2                     xor edx, edx
0x384b9f3: 4889df                   mov rdi, rbx
0x384b9f6: e8e5e4edff               call 0x3729ee0
0x384b9fb: 498b07                   mov rax, qword ptr [r15]
0x384b9fe: 488b8d58fdffff           mov rcx, qword ptr [rbp - 0x2a8]
0x384ba05: 4c89ff                   mov rdi, r15
0x384ba08: 4c89ee                   mov rsi, r13
0x384ba0b: 4889da                   mov rdx, rbx
0x384ba0e: ff9080050000             call qword ptr [rax + 0x580]
0x384ba14: 488bbdf0feffff           mov rdi, qword ptr [rbp - 0x110]
0x384ba1b: 4885ff                   test rdi, rdi
0x384ba1e: 7405                     je 0x384ba25
0x384ba20: e86bc086fc               call 0xb7a90
0x384ba25: bb01000000               mov ebx, 1
0x384ba2a: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x384ba2e: 4885ff                   test rdi, rdi
0x384ba31: 7405                     je 0x384ba38
0x384ba33: e858c086fc               call 0xb7a90
0x384ba38: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384ba3c: 8b75a0                   mov esi, dword ptr [rbp - 0x60]
0x384ba3f: e8fcf37dfc               call 0x2ae40
0x384ba44: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384ba48: 4885ff                   test rdi, rdi
0x384ba4b: 7405                     je 0x384ba52
0x384ba4d: e83ec086fc               call 0xb7a90
0x384ba52: 488b7d88                 mov rdi, qword ptr [rbp - 0x78]
0x384ba56: 4885ff                   test rdi, rdi
0x384ba59: 7405                     je 0x384ba60
0x384ba5b: e830c086fc               call 0xb7a90
0x384ba60: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x384ba67: 4885ff                   test rdi, rdi
0x384ba6a: 7405                     je 0x384ba71
0x384ba6c: e81fc086fc               call 0xb7a90
0x384ba71: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384ba78: 4885ff                   test rdi, rdi
0x384ba7b: 7405                     je 0x384ba82
0x384ba7d: e80ec086fc               call 0xb7a90
0x384ba82: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384ba89: 4885ff                   test rdi, rdi
0x384ba8c: 0f8404040000             je 0x384be96
0x384ba92: e8f9bf86fc               call 0xb7a90
0x384ba97: e9fa030000               jmp 0x384be96
0x384ba9c: 4c89e7                   mov rdi, r12
0x384ba9f: e86ce8edff               call 0x372a310
0x384baa4: 84c0                     test al, al
0x384baa6: 0f8435020000             je 0x384bce1
0x384baac: 4c8db578fdffff           lea r14, [rbp - 0x288]
0x384bab3: 4c89e6                   mov rsi, r12
0x384bab6: 4c89f7                   mov rdi, r14
0x384bab9: e8a26b78ff               call 0x2fd2660
0x384babe: 498b4500                 mov rax, qword ptr [r13]
0x384bac2: 488bb568fdffff           mov rsi, qword ptr [rbp - 0x298]
0x384bac9: 4c89ef                   mov rdi, r13
0x384bacc: 4c89f2                   mov rdx, r14
0x384bacf: 4d89f8                   mov r8, r15
0x384bad2: 31c9                     xor ecx, ecx
0x384bad4: ff90f8040000             call qword ptr [rax + 0x4f8]
0x384bada: 3401                     xor al, 1
0x384badc: 4c89f7                   mov rdi, r14
0x384badf: 0fb6d8                   movzx ebx, al
0x384bae2: e829edf3ff               call 0x378a810
0x384bae7: e9aa030000               jmp 0x384be96
0x384baec: c7855cffffff00000000     mov dword ptr [rbp - 0xa4], 0
0x384baf6: be01000000               mov esi, 1
0x384bafb: 31ff                     xor edi, edi
0x384bafd: e85e10b2fc               call 0x36cb60
0x384bb02: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384bb09: 8bb558ffffff             mov esi, dword ptr [rbp - 0xa8]
0x384bb0f: e87cf77bfc               call 0xb290
0x384bb14: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384bb1b: 4885ff                   test rdi, rdi
0x384bb1e: 7405                     je 0x384bb25
0x384bb20: e86bbf86fc               call 0xb7a90
0x384bb25: e876aeb7fc               call 0x3c69a0
0x384bb2a: 488d3553d51404           lea rsi, [rip + 0x414d553] ; ["NewObject with empty name can't be used to create default subobjects (inside of UObject derived class constructor) as it produces inconsistent object "]
0x384bb31: 4889c7                   mov rdi, rax
0x384bb34: 4889c3                   mov rbx, rax
0x384bb37: e8447ac4fc               call 0x493580
0x384bb3c: e8df36b1ff               call 0x335f220
0x384bb41: 4c8db550ffffff           lea r14, [rbp - 0xb0]
0x384bb48: 4889c6                   mov rsi, rax
0x384bb4b: 4c89f7                   mov rdi, r14
0x384bb4e: e8dd79c4fc               call 0x493530
0x384bb53: 4c89f7                   mov rdi, r14
0x384bb56: 48899d58ffffff           mov qword ptr [rbp - 0xa8], rbx
0x384bb5d: e83e4fc4fc               call 0x490aa0
0x384bb62: 48837d9000               cmp qword ptr [rbp - 0x70], 0
0x384bb67: 4889c3                   mov rbx, rax
0x384bb6a: 7415                     je 0x384bb81
0x384bb6c: 488b45b8                 mov rax, qword ptr [rbp - 0x48]
0x384bb70: 488d7da0                 lea rdi, [rbp - 0x60]
0x384bb74: 4885c0                   test rax, rax
0x384bb77: 480f45f8                 cmovne rdi, rax
0x384bb7b: 488b07                   mov rax, qword ptr [rdi]
0x384bb7e: ff5010                   call qword ptr [rax + 0x10]
0x384bb81: 4885db                   test rbx, rbx
0x384bb84: 7414                     je 0x384bb9a
0x384bb86: 488d05cf32b505           lea rax, [rip + 0x5b532cf]
0x384bb8d: 803800                   cmp byte ptr [rax], 0
0x384bb90: 7408                     je 0x384bb9a
0x384bb92: 4889df                   mov rdi, rbx
0x384bb95: e826c7b1fc               call 0x3682c0
0x384bb9a: 498db5c0000000           lea rsi, [r13 + 0xc0]
0x384bba1: 4889df                   mov rdi, rbx
0x384bba4: 49899d90010000           mov qword ptr [r13 + 0x190], rbx
0x384bbab: e8c038b1ff               call 0x335f470
0x384bbb0: 498bbd90010000           mov rdi, qword ptr [r13 + 0x190]
0x384bbb7: 31f6                     xor esi, esi
0x384bbb9: e83239b1ff               call 0x335f4f0
0x384bbbe: 4d8bbd90010000           mov r15, qword ptr [r13 + 0x190]
0x384bbc5: bb02000000               mov ebx, 2
0x384bbca: 49837f3000               cmp qword ptr [r15 + 0x30], 0
0x384bbcf: 0f85c1020000             jne 0x384be96
0x384bbd5: 488b9560fdffff           mov rdx, qword ptr [rbp - 0x2a0]
0x384bbdc: 4c8bb568fdffff           mov r14, qword ptr [rbp - 0x298]
0x384bbe3: 8b8aac0e0000             mov ecx, dword ptr [rdx + 0xeac]
0x384bbe9: 498b9ec0020000           mov rbx, qword ptr [r14 + 0x2c0]
0x384bbf0: 8d4101                   lea eax, [rcx + 1]
0x384bbf3: 8982ac0e0000             mov dword ptr [rdx + 0xeac], eax
0x384bbf9: 486382a00e0000           movsxd rax, dword ptr [rdx + 0xea0]
0x384bc00: 4885c0                   test rax, rax
0x384bc03: 0f8e77020000             jle 0x384be80
0x384bc09: 4981c7b0000000           add r15, 0xb0
0x384bc10: 4c8da2980e0000           lea r12, [rdx + 0xe98]
0x384bc17: 31c9                     xor ecx, ecx
0x384bc19: eb10                     jmp 0x384bc2b
0x384bc1b: 498d46ff                 lea rax, [r14 - 1]
0x384bc1f: b101                     mov cl, 1
0x384bc21: 4983fe01                 cmp r14, 1
0x384bc25: 0f8613050000             jbe 0x384c13e
0x384bc2b: 4989c5                   mov r13, rax
0x384bc2e: 4989c6                   mov r14, rax
0x384bc31: 898d58fdffff             mov dword ptr [rbp - 0x2a8], ecx
0x384bc37: 49c1e504                 shl r13, 4
0x384bc3b: 4983c5f8                 add r13, -8
0x384bc3f: 90                       nop 
0x384bc40: 498b0424                 mov rax, qword ptr [r12]
0x384bc44: 42833c2800               cmp dword ptr [rax + r13], 0
0x384bc49: 74d0                     je 0x384bc1b
0x384bc4b: 4a8b7c28f8               mov rdi, qword ptr [rax + r13 - 8]
0x384bc50: 4885ff                   test rdi, rdi
0x384bc53: 74c6                     je 0x384bc1b
0x384bc55: 488b07                   mov rax, qword ptr [rdi]
0x384bc58: 4889de                   mov rsi, rbx
0x384bc5b: ba08000000               mov edx, 8
0x384bc60: 4c89f9                   mov rcx, r15
0x384bc63: ff5068                   call qword ptr [rax + 0x68]
0x384bc66: 84c0                     test al, al
0x384bc68: 74b1                     je 0x384bc1b
0x384bc6a: 49ffce                   dec r14
0x384bc6d: 4983c5f0                 add r13, -0x10
0x384bc71: 498d4601                 lea rax, [r14 + 1]
0x384bc75: 4883f801                 cmp rax, 1
0x384bc79: 77c5                     ja 0x384bc40
0x384bc7b: 488b8560fdffff           mov rax, qword ptr [rbp - 0x2a0]
0x384bc82: 4c8bb568fdffff           mov r14, qword ptr [rbp - 0x298]
0x384bc89: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384bc8f: f68558fdffff01           test byte ptr [rbp - 0x2a8], 1
0x384bc96: 0f84ea010000             je 0x384be86
0x384bc9c: 4c89e7                   mov rdi, r12
0x384bc9f: 31f6                     xor esi, esi
0x384bca1: e84a517efc               call 0x30df0
0x384bca6: e9db010000               jmp 0x384be86
0x384bcab: c70000000000             mov dword ptr [rax], 0
0x384bcb1: 488bbd40ffffff           mov rdi, qword ptr [rbp - 0xc0]
0x384bcb8: 4885ff                   test rdi, rdi
0x384bcbb: 0f85e7f8ffff             jne 0x384b5a8
0x384bcc1: e9e8f8ffff               jmp 0x384b5ae
0x384bcc6: c70000000000             mov dword ptr [rax], 0
0x384bccc: 488bbd30ffffff           mov rdi, qword ptr [rbp - 0xd0]
0x384bcd3: 4885ff                   test rdi, rdi
0x384bcd6: 0f858af4ffff             jne 0x384b166
0x384bcdc: e98bf4ffff               jmp 0x384b16c
0x384bce1: 4c89e7                   mov rdi, r12
0x384bce4: e8e7e5edff               call 0x372a2d0
0x384bce9: 84c0                     test al, al
0x384bceb: 0f8408010000             je 0x384bdf9
0x384bcf1: 488d0512cdb005           lea rax, [rip + 0x5b0cd12]
0x384bcf8: 803801                   cmp byte ptr [rax], 1
0x384bcfb: 0f85f8000000             jne 0x384bdf9
0x384bd01: 488b9d68fdffff           mov rbx, qword ptr [rbp - 0x298]
0x384bd08: 4883bb9001000000         cmp qword ptr [rbx + 0x190], 0
0x384bd10: 7410                     je 0x384bd22
0x384bd12: 498b4500                 mov rax, qword ptr [r13]
0x384bd16: 4c89ef                   mov rdi, r13
0x384bd19: 4889de                   mov rsi, rbx
0x384bd1c: ff9060050000             call qword ptr [rax + 0x560]
0x384bd22: 4883bbc002000000         cmp qword ptr [rbx + 0x2c0], 0
0x384bd2a: 7427                     je 0x384bd53
0x384bd2c: 498b4500                 mov rax, qword ptr [r13]
0x384bd30: 4c89ef                   mov rdi, r13
0x384bd33: ff9078050000             call qword ptr [rax + 0x578]
0x384bd39: 84c0                     test al, al
0x384bd3b: 7416                     je 0x384bd53
0x384bd3d: 488b8568fdffff           mov rax, qword ptr [rbp - 0x298]
0x384bd44: 4c89ef                   mov rdi, r13
0x384bd47: 488bb0c0020000           mov rsi, qword ptr [rax + 0x2c0]
0x384bd4e: e87dc0ffff               call 0x3847dd0
0x384bd53: e848acb7fc               call 0x3c69a0
0x384bd58: 4889c7                   mov rdi, rax
0x384bd5b: e8a0050000               call 0x384c300
0x384bd60: 4889c3                   mov rbx, rax
0x384bd63: 4885c0                   test rax, rax
0x384bd66: 7414                     je 0x384bd7c
0x384bd68: 488d05ed30b505           lea rax, [rip + 0x5b530ed]
0x384bd6f: 803800                   cmp byte ptr [rax], 0
0x384bd72: 7408                     je 0x384bd7c
0x384bd74: 4889df                   mov rdi, rbx
0x384bd77: e844c5b1fc               call 0x3682c0
0x384bd7c: 4c8bb568fdffff           mov r14, qword ptr [rbp - 0x298]
0x384bd83: 4889df                   mov rdi, rbx
0x384bd86: 4c89e6                   mov rsi, r12
0x384bd89: 49899e90010000           mov qword ptr [r14 + 0x190], rbx
0x384bd90: e8db36b1ff               call 0x335f470
0x384bd95: 498bbe90010000           mov rdi, qword ptr [r14 + 0x190]
0x384bd9c: 31f6                     xor esi, esi
0x384bd9e: e84d37b1ff               call 0x335f4f0
0x384bda3: 498b8e90010000           mov rcx, qword ptr [r14 + 0x190]
0x384bdaa: 4885c9                   test rcx, rcx
0x384bdad: 0f84a4030000             je 0x384c157
0x384bdb3: 4883793000               cmp qword ptr [rcx + 0x30], 0
0x384bdb8: bb02000000               mov ebx, 2
0x384bdbd: 0f85d3000000             jne 0x384be96
0x384bdc3: 488b9d68fdffff           mov rbx, qword ptr [rbp - 0x298]
0x384bdca: 488bbd60fdffff           mov rdi, qword ptr [rbp - 0x2a0]
0x384bdd1: 4881c1b0000000           add rcx, 0xb0
0x384bdd8: ba08000000               mov edx, 8
0x384bddd: 488bb3c0020000           mov rsi, qword ptr [rbx + 0x2c0]
0x384bde4: e827040000               call 0x384c210
0x384bde9: 48c7839001000000000000   mov qword ptr [rbx + 0x190], 0
0x384bdf4: e998000000               jmp 0x384be91
0x384bdf9: 4c89e7                   mov rdi, r12
0x384bdfc: e8cfe4edff               call 0x372a2d0
0x384be01: bb01000000               mov ebx, 1
0x384be06: 84c0                     test al, al
0x384be08: 0f8488000000             je 0x384be96
0x384be0e: 4c8db5f0feffff           lea r14, [rbp - 0x110]
0x384be15: 488d3552b2fd03           lea rsi, [rip + 0x3fdb252] ; ['Engine']
0x384be1c: 4c89f7                   mov rdi, r14
0x384be1f: e8ecc68cfc               call 0x118510
0x384be24: 4d89fd                   mov r13, r15
0x384be27: 4c8dbde0feffff           lea r15, [rbp - 0x120]
0x384be2e: 488d35a1c01304           lea rsi, [rip + 0x413c0a1] ; ['ServerOpen']
0x384be35: 4c89ff                   mov rdi, r15
0x384be38: e8d3c68cfc               call 0x118510
0x384be3d: 4c8da550ffffff           lea r12, [rbp - 0xb0]
0x384be44: 488d0d21aefc03           lea rcx, [rip + 0x3fcae21] ; ["Servers can't open network URLs"]
0x384be4b: 4c89f6                   mov rsi, r14
0x384be4e: 4c89fa                   mov rdx, r15
0x384be51: 4c89e7                   mov rdi, r12
0x384be54: e8e7a58afc               call 0xf6440
0x384be59: 4c89e7                   mov rdi, r12
0x384be5c: e81f068bfc               call 0xfc480
0x384be61: 4c89ef                   mov rdi, r13
0x384be64: 4889c6                   mov rsi, rax
0x384be67: e8b49b1dfd               call 0xa25a20
0x384be6c: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384be73: 4885ff                   test rdi, rdi
0x384be76: 741e                     je 0x384be96
0x384be78: 488b07                   mov rax, qword ptr [rdi]
0x384be7b: ff5018                   call qword ptr [rax + 0x18]
0x384be7e: eb16                     jmp 0x384be96
0x384be80: 898aac0e0000             mov dword ptr [rdx + 0xeac], ecx
0x384be86: 49c7869001000000000000   mov qword ptr [r14 + 0x190], 0
0x384be91: bb01000000               mov ebx, 1
0x384be96: 488b056b608e05           mov rax, qword ptr [rip + 0x58e606b]
0x384be9d: 488b00                   mov rax, qword ptr [rax]
0x384bea0: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x384bea4: 0f8558030000             jne 0x384c202
0x384beaa: 89d8                     mov eax, ebx
0x384beac: 4881c488020000           add rsp, 0x288
0x384beb3: 5b                       pop rbx
0x384beb4: 415c                     pop r12
0x384beb6: 415d                     pop r13
0x384beb8: 415e                     pop r14
0x384beba: 415f                     pop r15
0x384bebc: 5d                       pop rbp
0x384bebd: c3                       ret 
0x384bebe: c70000000000             mov dword ptr [rax], 0
0x384bec4: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384becb: 4885ff                   test rdi, rdi
0x384bece: 7406                     je 0x384bed6
0x384bed0: 488b07                   mov rax, qword ptr [rdi]
0x384bed3: ff5018                   call qword ptr [rax + 0x18]
0x384bed6: 418b8dac0e0000           mov ecx, dword ptr [r13 + 0xeac]
0x384bedd: 488b8568fdffff           mov rax, qword ptr [rbp - 0x298]
0x384bee4: 8d5101                   lea edx, [rcx + 1]
0x384bee7: 488b98c0020000           mov rbx, qword ptr [rax + 0x2c0]
0x384beee: 418995ac0e0000           mov dword ptr [r13 + 0xeac], edx
0x384bef5: 496385a00e0000           movsxd rax, dword ptr [r13 + 0xea0]
0x384befc: 4885c0                   test rax, rax
0x384beff: 0f8e80000000             jle 0x384bf85
0x384bf05: 4d8db5980e0000           lea r14, [r13 + 0xe98]
0x384bf0c: 4531ff                   xor r15d, r15d
0x384bf0f: eb0d                     jmp 0x384bf1e
0x384bf11: 498d45ff                 lea rax, [r13 - 1]
0x384bf15: 41b701                   mov r15b, 1
0x384bf18: 4983fd01                 cmp r13, 1
0x384bf1c: 7655                     jbe 0x384bf73
0x384bf1e: 4989c4                   mov r12, rax
0x384bf21: 4989c5                   mov r13, rax
0x384bf24: 49c1e404                 shl r12, 4
0x384bf28: 4983c4f8                 add r12, -8
0x384bf2c: 0f1f4000                 nop dword ptr [rax]
0x384bf30: 498b06                   mov rax, qword ptr [r14]
0x384bf33: 42833c2000               cmp dword ptr [rax + r12], 0
0x384bf38: 74d7                     je 0x384bf11
0x384bf3a: 4a8b7c20f8               mov rdi, qword ptr [rax + r12 - 8]
0x384bf3f: 4885ff                   test rdi, rdi
0x384bf42: 74cd                     je 0x384bf11
0x384bf44: 488b07                   mov rax, qword ptr [rdi]
0x384bf47: 488b8d58fdffff           mov rcx, qword ptr [rbp - 0x2a8]
0x384bf4e: 4889de                   mov rsi, rbx
0x384bf51: ba07000000               mov edx, 7
0x384bf56: ff5068                   call qword ptr [rax + 0x68]
0x384bf59: 84c0                     test al, al
0x384bf5b: 74b4                     je 0x384bf11
0x384bf5d: 49ffcd                   dec r13
0x384bf60: 4983c4f0                 add r12, -0x10
0x384bf64: 498d4501                 lea rax, [r13 + 1]
0x384bf68: 4883f801                 cmp rax, 1
0x384bf6c: 77c2                     ja 0x384bf30
0x384bf6e: e92bf7ffff               jmp 0x384b69e
0x384bf73: 488b8560fdffff           mov rax, qword ptr [rbp - 0x2a0]
0x384bf7a: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384bf80: e935f7ffff               jmp 0x384b6ba
0x384bf85: 41898dac0e0000           mov dword ptr [r13 + 0xeac], ecx
0x384bf8c: e900ffffff               jmp 0x384be91
0x384bf91: 31ff                     xor edi, edi
0x384bf93: c5f81085e0fdffff         vmovups xmm0, xmmword ptr [rbp - 0x220]
0x384bf9b: c5f8108df0fdffff         vmovups xmm1, xmmword ptr [rbp - 0x210]
0x384bfa3: c5f8109500feffff         vmovups xmm2, xmmword ptr [rbp - 0x200]
0x384bfab: c5f8109d10feffff         vmovups xmm3, xmmword ptr [rbp - 0x1f0]
0x384bfb3: 488bb568fdffff           mov rsi, qword ptr [rbp - 0x298]
0x384bfba: c5f8118550ffffff         vmovups xmmword ptr [rbp - 0xb0], xmm0
0x384bfc2: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384bfc6: c5f81185e0fdffff         vmovups xmmword ptr [rbp - 0x220], xmm0
0x384bfce: c5f81185f0fdffff         vmovups xmmword ptr [rbp - 0x210], xmm0
0x384bfd6: c5f8118508feffff         vmovups xmmword ptr [rbp - 0x1f8], xmm0
0x384bfde: c5f8118d60ffffff         vmovups xmmword ptr [rbp - 0xa0], xmm1
0x384bfe6: c5f8119570ffffff         vmovups xmmword ptr [rbp - 0x90], xmm2
0x384bfee: c5f8115d80               vmovups xmmword ptr [rbp - 0x80], xmm3
0x384bff3: c5f8108d20feffff         vmovups xmm1, xmmword ptr [rbp - 0x1e0]
0x384bffb: c5f8118518feffff         vmovups xmmword ptr [rbp - 0x1e8], xmm0
0x384c003: 488b8530feffff           mov rax, qword ptr [rbp - 0x1d0]
0x384c00a: c5f8118528feffff         vmovups xmmword ptr [rbp - 0x1d8], xmm0
0x384c012: 488b9538feffff           mov rdx, qword ptr [rbp - 0x1c8]
0x384c019: 48c78538feffff00000000   mov qword ptr [rbp - 0x1c8], 0
0x384c024: 488b8d40feffff           mov rcx, qword ptr [rbp - 0x1c0]
0x384c02b: 48c78540feffff00000000   mov qword ptr [rbp - 0x1c0], 0
0x384c036: c5f8114d90               vmovups xmmword ptr [rbp - 0x70], xmm1
0x384c03b: 488945a0                 mov qword ptr [rbp - 0x60], rax
0x384c03f: 488955a8                 mov qword ptr [rbp - 0x58], rdx
0x384c043: 48894db0                 mov qword ptr [rbp - 0x50], rcx
0x384c047: 488d9550ffffff           lea rdx, [rbp - 0xb0]
0x384c04e: 4c89f9                   mov rcx, r15
0x384c051: 488b07                   mov rax, qword ptr [rdi]
0x384c054: ff5060                   call qword ptr [rax + 0x60]
0x384c057: 488b7da8                 mov rdi, qword ptr [rbp - 0x58]
0x384c05b: 89c3                     mov ebx, eax
0x384c05d: 4885ff                   test rdi, rdi
0x384c060: 7405                     je 0x384c067
0x384c062: e829ba86fc               call 0xb7a90
0x384c067: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384c06b: 8b75a0                   mov esi, dword ptr [rbp - 0x60]
0x384c06e: e8cded7dfc               call 0x2ae40
0x384c073: 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x384c077: 4885ff                   test rdi, rdi
0x384c07a: 7405                     je 0x384c081
0x384c07c: e80fba86fc               call 0xb7a90
0x384c081: 488b7d88                 mov rdi, qword ptr [rbp - 0x78]
0x384c085: 4885ff                   test rdi, rdi
0x384c088: 7405                     je 0x384c08f
0x384c08a: e801ba86fc               call 0xb7a90
0x384c08f: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x384c096: 4885ff                   test rdi, rdi
0x384c099: 7405                     je 0x384c0a0
0x384c09b: e8f0b986fc               call 0xb7a90
0x384c0a0: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384c0a7: 4885ff                   test rdi, rdi
0x384c0aa: 7405                     je 0x384c0b1
0x384c0ac: e8dfb986fc               call 0xb7a90
0x384c0b1: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384c0b8: 4885ff                   test rdi, rdi
0x384c0bb: 7405                     je 0x384c0c2
0x384c0bd: e8ceb986fc               call 0xb7a90
0x384c0c2: 488bbd38feffff           mov rdi, qword ptr [rbp - 0x1c8]
0x384c0c9: 4885ff                   test rdi, rdi
0x384c0cc: 7405                     je 0x384c0d3
0x384c0ce: e8bdb986fc               call 0xb7a90
0x384c0d3: 488bbd28feffff           mov rdi, qword ptr [rbp - 0x1d8]
0x384c0da: 8bb530feffff             mov esi, dword ptr [rbp - 0x1d0]
0x384c0e0: e85bed7dfc               call 0x2ae40
0x384c0e5: 488bbd28feffff           mov rdi, qword ptr [rbp - 0x1d8]
0x384c0ec: 4885ff                   test rdi, rdi
0x384c0ef: 7405                     je 0x384c0f6
0x384c0f1: e89ab986fc               call 0xb7a90
0x384c0f6: 488bbd18feffff           mov rdi, qword ptr [rbp - 0x1e8]
0x384c0fd: 4885ff                   test rdi, rdi
0x384c100: 7405                     je 0x384c107
0x384c102: e889b986fc               call 0xb7a90
0x384c107: 488bbd08feffff           mov rdi, qword ptr [rbp - 0x1f8]
0x384c10e: 4885ff                   test rdi, rdi
0x384c111: 7405                     je 0x384c118
0x384c113: e878b986fc               call 0xb7a90
0x384c118: 488bbdf0fdffff           mov rdi, qword ptr [rbp - 0x210]
0x384c11f: 4885ff                   test rdi, rdi
0x384c122: 7405                     je 0x384c129
0x384c124: e867b986fc               call 0xb7a90
0x384c129: 488bbde0fdffff           mov rdi, qword ptr [rbp - 0x220]
0x384c130: 4885ff                   test rdi, rdi
0x384c133: 0f8559f9ffff             jne 0x384ba92
0x384c139: e958fdffff               jmp 0x384be96
0x384c13e: 488b8560fdffff           mov rax, qword ptr [rbp - 0x2a0]
0x384c145: 4c8bb568fdffff           mov r14, qword ptr [rbp - 0x298]
0x384c14c: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384c152: e945fbffff               jmp 0x384bc9c
0x384c157: 488d9df0feffff           lea rbx, [rbp - 0x110]
0x384c15e: 488d3509affd03           lea rsi, [rip + 0x3fdaf09] ; ['Engine']
0x384c165: 4889df                   mov rdi, rbx
0x384c168: e8a3c38cfc               call 0x118510
0x384c16d: 4c8db5e0feffff           lea r14, [rbp - 0x120]
0x384c174: 488d359fbfda03           lea rsi, [rip + 0x3dabf9f] ; ['PendingNetGameInitFailure']
0x384c17b: 4c89f7                   mov rdi, r14
0x384c17e: e88dc38cfc               call 0x118510
0x384c183: 4c8dbd50ffffff           lea r15, [rbp - 0xb0]
0x384c18a: 488d0dbd25e503           lea rcx, [rip + 0x3e525bd] ; ['Error initializing the network driver.']
0x384c191: 4889de                   mov rsi, rbx
0x384c194: 4c89f2                   mov rdx, r14
0x384c197: 4c89ff                   mov rdi, r15
0x384c19a: e8a1a28afc               call 0xf6440
0x384c19f: 488b9d58fdffff           mov rbx, qword ptr [rbp - 0x2a8]
0x384c1a6: 4c89ff                   mov rdi, r15
0x384c1a9: e8d2028bfc               call 0xfc480
0x384c1ae: 4889df                   mov rdi, rbx
0x384c1b1: 4889c6                   mov rsi, rax
0x384c1b4: e867981dfd               call 0xa25a20
0x384c1b9: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384c1c0: 4885ff                   test rdi, rdi
0x384c1c3: 7406                     je 0x384c1cb
0x384c1c5: 488b07                   mov rax, qword ptr [rdi]
0x384c1c8: ff5018                   call qword ptr [rax + 0x18]
0x384c1cb: 488b8568fdffff           mov rax, qword ptr [rbp - 0x298]
0x384c1d2: 4c89ef                   mov rdi, r13
0x384c1d5: ba08000000               mov edx, 8
0x384c1da: 4889d9                   mov rcx, rbx
0x384c1dd: 488bb0c0020000           mov rsi, qword ptr [rax + 0x2c0]
0x384c1e4: e827000000               call 0x384c210
0x384c1e9: e9a3fcffff               jmp 0x384be91
0x384c1ee: 488d35271fd803           lea rsi, [rip + 0x3d81f27] ; ['Attempting to call UEngine::Browse and destroy the net driver while the net driver is ticking. Instead try using UWorld::ServerTravel or APlayerContro']
0x384c1f5: 4c89ff                   mov rdi, r15
0x384c1f8: e8538381fc               call 0x64550
0x384c1fd: e98ffcffff               jmp 0x384be91
0x384c202: e8595cc203               call 0x7471e60
0x384c207: 0f0b                     ud2 
0x384c209: cc                       int3 
0x384c20a: cc                       int3 
0x384c20b: cc                       int3 
0x384c20c: cc                       int3 
0x384c20d: cc                       int3 
0x384c20e: cc                       int3 
0x384c20f: cc                       int3 
0x384c210: 55                       push rbp
0x384c211: 4889e5                   mov rbp, rsp
0x384c214: 4157                     push r15
0x384c216: 4156                     push r14
0x384c218: 4155                     push r13
0x384c21a: 4154                     push r12
0x384c21c: 53                       push rbx
0x384c21d: 4883ec18                 sub rsp, 0x18
0x384c221: 48894dc0                 mov qword ptr [rbp - 0x40], rcx
0x384c225: 8b8fac0e0000             mov ecx, dword ptr [rdi + 0xeac]
0x384c22b: 8d4101                   lea eax, [rcx + 1]
0x384c22e: 8987ac0e0000             mov dword ptr [rdi + 0xeac], eax
0x384c234: 486387a00e0000           movsxd rax, dword ptr [rdi + 0xea0]
0x384c23b: 4885c0                   test rax, rax
0x384c23e: 0f8e91000000             jle 0x384c2d5
0x384c244: 4189d7                   mov r15d, edx
0x384c247: 4989f4                   mov r12, rsi
0x384c24a: 4c8daf980e0000           lea r13, [rdi + 0xe98]
0x384c251: 31c9                     xor ecx, ecx
0x384c253: 48897dc8                 mov qword ptr [rbp - 0x38], rdi
0x384c257: eb13                     jmp 0x384c26c
0x384c259: 0f1f8000000000           nop dword ptr [rax]
0x384c260: 498d46ff                 lea rax, [r14 - 1]
0x384c264: b101                     mov cl, 1
0x384c266: 4983fe01                 cmp r14, 1
0x384c26a: 767e                     jbe 0x384c2ea
0x384c26c: 4889c3                   mov rbx, rax
0x384c26f: 4989c6                   mov r14, rax
0x384c272: 894dd4                   mov dword ptr [rbp - 0x2c], ecx
0x384c275: 48c1e304                 shl rbx, 4
0x384c279: 4883c3f8                 add rbx, -8
0x384c27d: 0f1f00                   nop dword ptr [rax]
0x384c280: 498b4500                 mov rax, qword ptr [r13]
0x384c284: 833c1800                 cmp dword ptr [rax + rbx], 0
0x384c288: 74d6                     je 0x384c260
0x384c28a: 488b7c18f8               mov rdi, qword ptr [rax + rbx - 8]
0x384c28f: 4885ff                   test rdi, rdi
0x384c292: 74cc                     je 0x384c260
0x384c294: 488b07                   mov rax, qword ptr [rdi]
0x384c297: 488b4dc0                 mov rcx, qword ptr [rbp - 0x40]
0x384c29b: 4c89e6                   mov rsi, r12
0x384c29e: 4489fa                   mov edx, r15d
0x384c2a1: ff5068                   call qword ptr [rax + 0x68]
0x384c2a4: 84c0                     test al, al
0x384c2a6: 74b8                     je 0x384c260
0x384c2a8: 49ffce                   dec r14
0x384c2ab: 4883c3f0                 add rbx, -0x10
0x384c2af: 498d4601                 lea rax, [r14 + 1]
0x384c2b3: 4883f801                 cmp rax, 1
0x384c2b7: 77c7                     ja 0x384c280
0x384c2b9: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x384c2bd: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384c2c3: f645d401                 test byte ptr [rbp - 0x2c], 1
0x384c2c7: 7412                     je 0x384c2db
0x384c2c9: 4c89ef                   mov rdi, r13
0x384c2cc: 31f6                     xor esi, esi
0x384c2ce: e81d4b7efc               call 0x30df0
0x384c2d3: eb06                     jmp 0x384c2db
0x384c2d5: 898fac0e0000             mov dword ptr [rdi + 0xeac], ecx
0x384c2db: 4883c418                 add rsp, 0x18
0x384c2df: 5b                       pop rbx
0x384c2e0: 415c                     pop r12
0x384c2e2: 415d                     pop r13
0x384c2e4: 415e                     pop r14
0x384c2e6: 415f                     pop r15
0x384c2e8: 5d                       pop rbp
0x384c2e9: c3                       ret 
0x384c2ea: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x384c2ee: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384c2f4: ebd3                     jmp 0x384c2c9
0x384c2f6: cc                       int3 
0x384c2f7: cc                       int3 
0x384c2f8: cc                       int3 
0x384c2f9: cc                       int3 
0x384c2fa: cc                       int3 
0x384c2fb: cc                       int3 
0x384c2fc: cc                       int3 
0x384c2fd: cc                       int3 
0x384c2fe: cc                       int3 
0x384c2ff: cc                       int3 
0x384c300: 55                       push rbp
0x384c301: 4889e5                   mov rbp, rsp
0x384c304: 4157                     push r15
0x384c306: 4156                     push r14
0x384c308: 53                       push rbx
0x384c309: 4881ec88000000           sub rsp, 0x88
0x384c310: 4c8b3df15b8e05           mov r15, qword ptr [rip + 0x58e5bf1]
0x384c317: 488d3566cd1404           lea rsi, [rip + 0x414cd66] ; ["NewObject with empty name can't be used to create default subobjects (inside of UObject derived class constructor) as it produces inconsistent object "]
0x384c31e: 4889fb                   mov rbx, rdi
0x384c321: 498b07                   mov rax, qword ptr [r15]
0x384c324: 488945e0                 mov qword ptr [rbp - 0x20], rax
0x384c328: e85372c4fc               call 0x493580
0x384c32d: e8ee2eb1ff               call 0x335f220
0x384c332: 4c8db560ffffff           lea r14, [rbp - 0xa0]
0x384c339: 4889c6                   mov rsi, rax
0x384c33c: 4c89f7                   mov rdi, r14
0x384c33f: e8ec71c4fc               call 0x493530
0x384c344: 4c89f7                   mov rdi, r14
0x384c347: 48899d68ffffff           mov qword ptr [rbp - 0x98], rbx
0x384c34e: e84d47c4fc               call 0x490aa0
0x384c353: 48837da000               cmp qword ptr [rbp - 0x60], 0
0x384c358: 741b                     je 0x384c375
0x384c35a: 488b4dc8                 mov rcx, qword ptr [rbp - 0x38]
0x384c35e: 488d7db0                 lea rdi, [rbp - 0x50]
0x384c362: 4889c3                   mov rbx, rax
0x384c365: 4885c9                   test rcx, rcx
0x384c368: 480f45f9                 cmovne rdi, rcx
0x384c36c: 488b0f                   mov rcx, qword ptr [rdi]
0x384c36f: ff5110                   call qword ptr [rcx + 0x10]
0x384c372: 4889d8                   mov rax, rbx
0x384c375: 498b0f                   mov rcx, qword ptr [r15]
0x384c378: 483b4de0                 cmp rcx, qword ptr [rbp - 0x20]
0x384c37c: 750e                     jne 0x384c38c
0x384c37e: 4881c488000000           add rsp, 0x88
0x384c385: 5b                       pop rbx
0x384c386: 415e                     pop r14
0x384c388: 415f                     pop r15
0x384c38a: 5d                       pop rbp
0x384c38b: c3                       ret 
0x384c38c: e8cf5ac203               call 0x7471e60
0x384c391: 0f0b                     ud2 
0x384c393: cc                       int3 
0x384c394: cc                       int3 
0x384c395: cc                       int3 
0x384c396: cc                       int3 
0x384c397: cc                       int3 
0x384c398: cc                       int3 
0x384c399: cc                       int3 
0x384c39a: cc                       int3 
0x384c39b: cc                       int3 
0x384c39c: cc                       int3 
0x384c39d: cc                       int3 
0x384c39e: cc                       int3 
0x384c39f: cc                       int3 
0x384c3a0: 4885f6                   test rsi, rsi
0x384c3a3: 7472                     je 0x384c417
0x384c3a5: 55                       push rbp
0x384c3a6: 4889e5                   mov rbp, rsp
0x384c3a9: 4157                     push r15
0x384c3ab: 4156                     push r14
0x384c3ad: 53                       push rbx
0x384c3ae: 50                       push rax
0x384c3af: 8b8750110000             mov eax, dword ptr [rdi + 0x1150]
0x384c3b5: 4989fe                   mov r14, rdi
0x384c3b8: 85c0                     test eax, eax
0x384c3ba: 7e51                     jle 0x384c40d
0x384c3bc: 4889f3                   mov rbx, rsi
0x384c3bf: 4531ff                   xor r15d, r15d
0x384c3c2: eb17                     jmp 0x384c3db
0x384c3c4: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384c3d0: 49ffc7                   inc r15
0x384c3d3: 4863c8                   movsxd rcx, eax
0x384c3d6: 4939cf                   cmp r15, rcx
0x384c3d9: 7d32                     jge 0x384c40d
0x384c3db: 498b8e48110000           mov rcx, qword ptr [r14 + 0x1148]
0x384c3e2: 4a8b34f9                 mov rsi, qword ptr [rcx + r15*8]
0x384c3e6: 488b8e90010000           mov rcx, qword ptr [rsi + 0x190]
0x384c3ed: 4885c9                   test rcx, rcx
0x384c3f0: 74de                     je 0x384c3d0
0x384c3f2: 48395930                 cmp qword ptr [rcx + 0x30], rbx
0x384c3f6: 75d8                     jne 0x384c3d0
0x384c3f8: 498b06                   mov rax, qword ptr [r14]
0x384c3fb: 4c89f7                   mov rdi, r14
0x384c3fe: ff9060050000             call qword ptr [rax + 0x560]
0x384c404: 418b8650110000           mov eax, dword ptr [r14 + 0x1150]
0x384c40b: ebc3                     jmp 0x384c3d0
0x384c40d: 4883c408                 add rsp, 8
0x384c411: 5b                       pop rbx
0x384c412: 415e                     pop r14
0x384c414: 415f                     pop r15
0x384c416: 5d                       pop rbp
0x384c417: c3                       ret 
0x384c418: cc                       int3 
0x384c419: cc                       int3 
0x384c41a: cc                       int3 
0x384c41b: cc                       int3 
0x384c41c: cc                       int3 
0x384c41d: cc                       int3 
0x384c41e: cc                       int3 
0x384c41f: cc                       int3 

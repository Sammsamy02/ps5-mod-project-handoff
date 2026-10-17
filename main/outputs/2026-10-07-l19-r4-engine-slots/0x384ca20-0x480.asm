0x384ca20: 55                       push rbp
0x384ca21: 4889e5                   mov rbp, rsp
0x384ca24: 4157                     push r15
0x384ca26: 4156                     push r14
0x384ca28: 4155                     push r13
0x384ca2a: 4154                     push r12
0x384ca2c: 53                       push rbx
0x384ca2d: 4881ec78010000           sub rsp, 0x178
0x384ca34: 488b1dcd548e05           mov rbx, qword ptr [rip + 0x58e54cd]
0x384ca3b: 4989f6                   mov r14, rsi
0x384ca3e: 4989fd                   mov r13, rdi
0x384ca41: 488b03                   mov rax, qword ptr [rbx]
0x384ca44: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x384ca48: 80be8800000001           cmp byte ptr [rsi + 0x88], 1
0x384ca4f: 7519                     jne 0x384ca6a
0x384ca51: 498d7e08                 lea rdi, [r14 + 8]
0x384ca55: c5fa118574feffff         vmovss dword ptr [rbp - 0x18c], xmm0
0x384ca5d: e8ae27f5ff               call 0x379f210
0x384ca62: c5fa108574feffff         vmovss xmm0, dword ptr [rbp - 0x18c]
0x384ca6a: 498b86c0020000           mov rax, qword ptr [r14 + 0x2c0]
0x384ca71: 4885c0                   test rax, rax
0x384ca74: 0f84dd000000             je 0x384cb57
0x384ca7a: 448bb820080000           mov r15d, dword ptr [rax + 0x820]
0x384ca81: 4183ff02                 cmp r15d, 2
0x384ca85: 7c22                     jl 0x384caa9
0x384ca87: c5fa1088f8070000         vmovss xmm1, dword ptr [rax + 0x7f8]
0x384ca8f: c5f25cd0                 vsubss xmm2, xmm1, xmm0
0x384ca93: c5f057c9                 vxorps xmm1, xmm1, xmm1
0x384ca97: c5f82eca                 vucomiss xmm1, xmm2
0x384ca9b: c5fa1190f8070000         vmovss dword ptr [rax + 0x7f8], xmm2
0x384caa3: 0f83f1010000             jae 0x384cc9a
0x384caa9: 458bbeb0000000           mov r15d, dword ptr [r14 + 0xb0]
0x384cab0: 4183ff02                 cmp r15d, 2
0x384cab4: 0f8d8f010000             jge 0x384cc49
0x384caba: 4183bd4011000000         cmp dword ptr [r13 + 0x1140], 0
0x384cac2: 742d                     je 0x384caf1
0x384cac4: 498bbd38110000           mov rdi, qword ptr [r13 + 0x1138]
0x384cacb: 4885ff                   test rdi, rdi
0x384cace: 7421                     je 0x384caf1
0x384cad0: 488b07                   mov rax, qword ptr [rdi]
0x384cad3: 4c89f6                   mov rsi, r14
0x384cad6: c5fa118574feffff         vmovss dword ptr [rbp - 0x18c], xmm0
0x384cade: ff5068                   call qword ptr [rax + 0x68]
0x384cae1: c5fa108574feffff         vmovss xmm0, dword ptr [rbp - 0x18c]
0x384cae9: 84c0                     test al, al
0x384caeb: 0f853b070000             jne 0x384d22c
0x384caf1: 498bbe90010000           mov rdi, qword ptr [r14 + 0x190]
0x384caf8: 4885ff                   test rdi, rdi
0x384cafb: 0f8428040000             je 0x384cf29
0x384cb01: 488b07                   mov rax, qword ptr [rdi]
0x384cb04: ff90d8020000             call qword ptr [rax + 0x2d8]
0x384cb0a: 4d8b8690010000           mov r8, qword ptr [r14 + 0x190]
0x384cb11: 4d85c0                   test r8, r8
0x384cb14: 0f8412070000             je 0x384d22c
0x384cb1a: 4183b8b800000002         cmp dword ptr [r8 + 0xb8], 2
0x384cb22: 0f8c58050000             jl 0x384d080
0x384cb28: 498b5030                 mov rdx, qword ptr [r8 + 0x30]
0x384cb2c: 4981c0b0000000           add r8, 0xb0
0x384cb33: 4c89ef                   mov rdi, r13
0x384cb36: b908000000               mov ecx, 8
0x384cb3b: 31f6                     xor esi, esi
0x384cb3d: e83eadffff               call 0x3847880
0x384cb42: 498b4500                 mov rax, qword ptr [r13]
0x384cb46: 4c89ef                   mov rdi, r13
0x384cb49: 4c89f6                   mov rsi, r14
0x384cb4c: ff9060050000             call qword ptr [rax + 0x560]
0x384cb52: e910080000               jmp 0x384d367
0x384cb57: 4c89ef                   mov rdi, r13
0x384cb5a: 4c89f6                   mov rsi, r14
0x384cb5d: e81efaffff               call 0x384c580
0x384cb62: 4d8bb6c0020000           mov r14, qword ptr [r14 + 0x2c0]
0x384cb69: 488d359c4d1604           lea rsi, [rip + 0x4164d9c] ; ['UEngine::TickWorldTravel has no world after ticking seamless travel handler.']
0x384cb70: 488d7dc0                 lea rdi, [rbp - 0x40]
0x384cb74: e8777381fc               call 0x63ef0
0x384cb79: 418b8dac0e0000           mov ecx, dword ptr [r13 + 0xeac]
0x384cb80: 8d4101                   lea eax, [rcx + 1]
0x384cb83: 418985ac0e0000           mov dword ptr [r13 + 0xeac], eax
0x384cb8a: 496385a00e0000           movsxd rax, dword ptr [r13 + 0xea0]
0x384cb91: 4885c0                   test rax, rax
0x384cb94: 0f8e5f010000             jle 0x384ccf9
0x384cb9a: 4d8dbd980e0000           lea r15, [r13 + 0xe98]
0x384cba1: 4c8d65c0                 lea r12, [rbp - 0x40]
0x384cba5: 31c9                     xor ecx, ecx
0x384cba7: 4c89ad68feffff           mov qword ptr [rbp - 0x198], r13
0x384cbae: eb10                     jmp 0x384cbc0
0x384cbb0: 498d45ff                 lea rax, [r13 - 1]
0x384cbb4: b101                     mov cl, 1
0x384cbb6: 4983fd01                 cmp r13, 1
0x384cbba: 0f868b060000             jbe 0x384d24b
0x384cbc0: 4889c3                   mov rbx, rax
0x384cbc3: 4989c5                   mov r13, rax
0x384cbc6: 898d74feffff             mov dword ptr [rbp - 0x18c], ecx
0x384cbcc: 48c1e304                 shl rbx, 4
0x384cbd0: 4883c3f8                 add rbx, -8
0x384cbd4: 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x384cbe0: 498b07                   mov rax, qword ptr [r15]
0x384cbe3: 833c1800                 cmp dword ptr [rax + rbx], 0
0x384cbe7: 74c7                     je 0x384cbb0
0x384cbe9: 488b7c18f8               mov rdi, qword ptr [rax + rbx - 8]
0x384cbee: 4885ff                   test rdi, rdi
0x384cbf1: 74bd                     je 0x384cbb0
0x384cbf3: 488b07                   mov rax, qword ptr [rdi]
0x384cbf6: 4c89f6                   mov rsi, r14
0x384cbf9: ba0a000000               mov edx, 0xa
0x384cbfe: 4c89e1                   mov rcx, r12
0x384cc01: ff5068                   call qword ptr [rax + 0x68]
0x384cc04: 84c0                     test al, al
0x384cc06: 74a8                     je 0x384cbb0
0x384cc08: 49ffcd                   dec r13
0x384cc0b: 4883c3f0                 add rbx, -0x10
0x384cc0f: 498d4501                 lea rax, [r13 + 1]
0x384cc13: 4883f801                 cmp rax, 1
0x384cc17: 77c7                     ja 0x384cbe0
0x384cc19: 488b8568feffff           mov rax, qword ptr [rbp - 0x198]
0x384cc20: 488b1de1528e05           mov rbx, qword ptr [rip + 0x58e52e1]
0x384cc27: ff88ac0e0000             dec dword ptr [rax + 0xeac]
0x384cc2d: f68574feffff01           test byte ptr [rbp - 0x18c], 1
0x384cc34: 0f84e4050000             je 0x384d21e
0x384cc3a: 4c89ff                   mov rdi, r15
0x384cc3d: 31f6                     xor esi, esi
0x384cc3f: e8ac417efc               call 0x30df0
0x384cc44: e9d5050000               jmp 0x384d21e
0x384cc49: 488bb8a8010000           mov rdi, qword ptr [rax + 0x1a8]
0x384cc50: 4885ff                   test rdi, rdi
0x384cc53: 0f84b9000000             je 0x384cd12
0x384cc59: 488b07                   mov rax, qword ptr [rdi]
0x384cc5c: ff9058080000             call qword ptr [rax + 0x858]
0x384cc62: 458bbeb0000000           mov r15d, dword ptr [r14 + 0xb0]
0x384cc69: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384cc6d: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x384cc72: 48c745b000000000         mov qword ptr [rbp - 0x50], 0
0x384cc7a: 4d8ba6a8000000           mov r12, qword ptr [r14 + 0xa8]
0x384cc81: 44897db8                 mov dword ptr [rbp - 0x48], r15d
0x384cc85: 4585ff                   test r15d, r15d
0x384cc88: 0f85a0000000             jne 0x384cd2e
0x384cc8e: c745bc00000000           mov dword ptr [rbp - 0x44], 0
0x384cc95: e9d5000000               jmp 0x384cd6f
0x384cc9a: 488bb8a8010000           mov rdi, qword ptr [rax + 0x1a8]
0x384cca1: 4885ff                   test rdi, rdi
0x384cca4: 0f849a020000             je 0x384cf44
0x384ccaa: 488b07                   mov rax, qword ptr [rdi]
0x384ccad: ff9058080000             call qword ptr [rax + 0x858]
0x384ccb3: 498b86c0020000           mov rax, qword ptr [r14 + 0x2c0]
0x384ccba: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384ccbe: 448bb820080000           mov r15d, dword ptr [rax + 0x820]
0x384ccc5: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x384ccca: 48c745b000000000         mov qword ptr [rbp - 0x50], 0
0x384ccd2: 4c8ba018080000           mov r12, qword ptr [rax + 0x818]
0x384ccd9: 44897db8                 mov dword ptr [rbp - 0x48], r15d
0x384ccdd: 4585ff                   test r15d, r15d
0x384cce0: 0f857a020000             jne 0x384cf60
0x384cce6: c745bc00000000           mov dword ptr [rbp - 0x44], 0
0x384cced: 488d15ac28de03           lea rdx, [rip + 0x3de28ac]
0x384ccf4: e9b6020000               jmp 0x384cfaf
0x384ccf9: 41898dac0e0000           mov dword ptr [r13 + 0xeac], ecx
0x384cd00: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x384cd04: 4885ff                   test rdi, rdi
0x384cd07: 0f851a050000             jne 0x384d227
0x384cd0d: e91a050000               jmp 0x384d22c
0x384cd12: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x384cd16: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x384cd1b: 48c745b000000000         mov qword ptr [rbp - 0x50], 0
0x384cd23: 4d8ba6a8000000           mov r12, qword ptr [r14 + 0xa8]
0x384cd2a: 44897db8                 mov dword ptr [rbp - 0x48], r15d
0x384cd2e: 488d45bc                 lea rax, [rbp - 0x44]
0x384cd32: 4c8d45b0                 lea r8, [rbp - 0x50]
0x384cd36: bf02000000               mov edi, 2
0x384cd3b: be02000000               mov esi, 2
0x384cd40: 4489fa                   mov edx, r15d
0x384cd43: 4589f9                   mov r9d, r15d
0x384cd46: 31c9                     xor ecx, ecx
0x384cd48: 48890424                 mov qword ptr [rsp], rax
0x384cd4c: e87fa87dfc               call 0x275d0
0x384cd51: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x384cd55: 4963d7                   movsxd rdx, r15d
0x384cd58: 4c89e6                   mov rsi, r12
0x384cd5b: 4801d2                   add rdx, rdx
0x384cd5e: e8ed50c203               call 0x7471e50
0x384cd63: 837db800                 cmp dword ptr [rbp - 0x48], 0
0x384cd67: 7406                     je 0x384cd6f
0x384cd69: 488b55b0                 mov rdx, qword ptr [rbp - 0x50]
0x384cd6d: eb07                     jmp 0x384cd76
0x384cd6f: 488d152a28de03           lea rdx, [rip + 0x3de282a]
0x384cd76: 410fb68eb8000000         movzx ecx, byte ptr [r14 + 0xb8]
0x384cd7e: 4c8dbde0feffff           lea r15, [rbp - 0x120]
0x384cd85: 498db6c0000000           lea rsi, [r14 + 0xc0]
0x384cd8c: 4c89ff                   mov rdi, r15
0x384cd8f: e81cb4edff               call 0x37281b0
0x384cd94: 498b4500                 mov rax, qword ptr [r13]
0x384cd98: 488d4dc0                 lea rcx, [rbp - 0x40]
0x384cd9c: 4c89ef                   mov rdi, r13
0x384cd9f: 4c89f6                   mov rsi, r14
0x384cda2: 4c89fa                   mov rdx, r15
0x384cda5: ff90e8040000             call qword ptr [rax + 0x4e8]
0x384cdab: 488bbd38ffffff           mov rdi, qword ptr [rbp - 0xc8]
0x384cdb2: 4189c7                   mov r15d, eax
0x384cdb5: 4885ff                   test rdi, rdi
0x384cdb8: 7405                     je 0x384cdbf
0x384cdba: e8d1ac86fc               call 0xb7a90
0x384cdbf: 488bbd28ffffff           mov rdi, qword ptr [rbp - 0xd8]
0x384cdc6: 8bb530ffffff             mov esi, dword ptr [rbp - 0xd0]
0x384cdcc: e86fe07dfc               call 0x2ae40
0x384cdd1: 488bbd28ffffff           mov rdi, qword ptr [rbp - 0xd8]
0x384cdd8: 4885ff                   test rdi, rdi
0x384cddb: 7405                     je 0x384cde2
0x384cddd: e8aeac86fc               call 0xb7a90
0x384cde2: 488bbd18ffffff           mov rdi, qword ptr [rbp - 0xe8]
0x384cde9: 4885ff                   test rdi, rdi
0x384cdec: 7405                     je 0x384cdf3
0x384cdee: e89dac86fc               call 0xb7a90
0x384cdf3: 488bbd08ffffff           mov rdi, qword ptr [rbp - 0xf8]
0x384cdfa: 4885ff                   test rdi, rdi
0x384cdfd: 7405                     je 0x384ce04
0x384cdff: e88cac86fc               call 0xb7a90
0x384ce04: 488bbdf0feffff           mov rdi, qword ptr [rbp - 0x110]
0x384ce0b: 4885ff                   test rdi, rdi
0x384ce0e: 7405                     je 0x384ce15
0x384ce10: e87bac86fc               call 0xb7a90
0x384ce15: 488bbde0feffff           mov rdi, qword ptr [rbp - 0x120]
0x384ce1c: 4885ff                   test rdi, rdi
0x384ce1f: 7405                     je 0x384ce26
0x384ce21: e86aac86fc               call 0xb7a90
0x384ce26: 4183ff01                 cmp r15d, 1
0x384ce2a: 0f85e0030000             jne 0x384d210
0x384ce30: 4d8bbec0020000           mov r15, qword ptr [r14 + 0x2c0]
0x384ce37: 4d85ff                   test r15, r15
0x384ce3a: 7512                     jne 0x384ce4e
0x384ce3c: 4c89ef                   mov rdi, r13
0x384ce3f: 4c89f6                   mov rsi, r14
0x384ce42: e839f7ffff               call 0x384c580
0x384ce47: 4d8bbec0020000           mov r15, qword ptr [r14 + 0x2c0]
0x384ce4e: 418b8dac0e0000           mov ecx, dword ptr [r13 + 0xeac]
0x384ce55: 8d4101                   lea eax, [rcx + 1]
0x384ce58: 418985ac0e0000           mov dword ptr [r13 + 0xeac], eax
0x384ce5f: 496385a00e0000           movsxd rax, dword ptr [r13 + 0xea0]
0x384ce66: 4885c0                   test rax, rax
0x384ce69: 0f8e9a030000             jle 0x384d209
0x384ce6f: 4d8db5980e0000           lea r14, [r13 + 0xe98]
0x384ce76: 4c8d65c0                 lea r12, [rbp - 0x40]
0x384ce7a: 31c9                     xor ecx, ecx
0x384ce7c: 4c89ad68feffff           mov qword ptr [rbp - 0x198], r13
0x384ce83: eb1b                     jmp 0x384cea0
0x384ce85: 66662e0f1f840000000000   nop word ptr cs:[rax + rax]
0x384ce90: 498d45ff                 lea rax, [r13 - 1]
0x384ce94: b101                     mov cl, 1
0x384ce96: 4983fd01                 cmp r13, 1
0x384ce9a: 0f86c4030000             jbe 0x384d264

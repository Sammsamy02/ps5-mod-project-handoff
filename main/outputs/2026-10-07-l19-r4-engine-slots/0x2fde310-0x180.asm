0x2fde310: 55                       push rbp
0x2fde311: 4889e5                   mov rbp, rsp
0x2fde314: 4157                     push r15
0x2fde316: 4156                     push r14
0x2fde318: 4155                     push r13
0x2fde31a: 4154                     push r12
0x2fde31c: 53                       push rbx
0x2fde31d: 4883ec28                 sub rsp, 0x28
0x2fde321: 4c8b2de03b1506           mov r13, qword ptr [rip + 0x6153be0]
0x2fde328: 4989f7                   mov r15, rsi
0x2fde32b: 4989fe                   mov r14, rdi
0x2fde32e: 498b4500                 mov rax, qword ptr [r13]
0x2fde332: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fde336: 4885f6                   test rsi, rsi
0x2fde339: 7423                     je 0x2fde35e
0x2fde33b: e880924400               call 0x34275c0
0x2fde340: 498b5710                 mov rdx, qword ptr [r15 + 0x10]
0x2fde344: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fde348: 3b4a38                   cmp ecx, dword ptr [rdx + 0x38]
0x2fde34b: 7f11                     jg 0x2fde35e
0x2fde34d: 488b5230                 mov rdx, qword ptr [rdx + 0x30]
0x2fde351: 4883c030                 add rax, 0x30
0x2fde355: 4c89fb                   mov rbx, r15
0x2fde358: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fde35c: 7402                     je 0x2fde360
0x2fde35e: 31db                     xor ebx, ebx
0x2fde360: 4c8d65c0                 lea r12, [rbp - 0x40]
0x2fde364: 488d3535126504           lea rsi, [rip + 0x4651235]
0x2fde36b: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fde36f: c5f81145b0               vmovups xmmword ptr [rbp - 0x50], xmm0
0x2fde374: 4c89e7                   mov rdi, r12
0x2fde377: e8745b08fd               call 0x63ef0
0x2fde37c: 498b06                   mov rax, qword ptr [r14]
0x2fde37f: 488d4db0                 lea rcx, [rbp - 0x50]
0x2fde383: 4c89f7                   mov rdi, r14
0x2fde386: 4c89fe                   mov rsi, r15
0x2fde389: 4c89e2                   mov rdx, r12
0x2fde38c: ff90f0080000             call qword ptr [rax + 0x8f0]
0x2fde392: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fde396: 4885ff                   test rdi, rdi
0x2fde399: 7405                     je 0x2fde3a0
0x2fde39b: e8f0960dfd               call 0xb7a90
0x2fde3a0: 4885db                   test rbx, rbx
0x2fde3a3: 0f8484000000             je 0x2fde42d
0x2fde3a9: 488b03                   mov rax, qword ptr [rbx]
0x2fde3ac: 4889df                   mov rdi, rbx
0x2fde3af: ff90e00c0000             call qword ptr [rax + 0xce0]
0x2fde3b5: 488b35e43d5406           mov rsi, qword ptr [rip + 0x6543de4]
0x2fde3bc: 4c89f7                   mov rdi, r14
0x2fde3bf: e87cb347fd               call 0x459740
0x2fde3c4: 4889c7                   mov rdi, rax
0x2fde3c7: 4989c7                   mov r15, rax
0x2fde3ca: e831be33fd               call 0x31a200
0x2fde3cf: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fde3d6: 757c                     jne 0x2fde454
0x2fde3d8: c645c800                 mov byte ptr [rbp - 0x38], 0
0x2fde3dc: 48895dc0                 mov qword ptr [rbp - 0x40], rbx
0x2fde3e0: 488d55c0                 lea rdx, [rbp - 0x40]
0x2fde3e4: 4c89f7                   mov rdi, r14
0x2fde3e7: 4c89fe                   mov rsi, r15
0x2fde3ea: 498b06                   mov rax, qword ptr [r14]
0x2fde3ed: ff9068020000             call qword ptr [rax + 0x268]
0x2fde3f3: 807dc801                 cmp byte ptr [rbp - 0x38], 1
0x2fde3f7: 746e                     je 0x2fde467
0x2fde3f9: 808bb804000010           or byte ptr [rbx + 0x4b8], 0x10
0x2fde400: bf42010000               mov edi, 0x142
0x2fde405: e8e61e26fd               call 0x2402f0
0x2fde40a: 4c8b33                   mov r14, qword ptr [rbx]
0x2fde40d: 89c6                     mov esi, eax
0x2fde40f: 4889df                   mov rdi, rbx
0x2fde412: 41ff9698070000           call qword ptr [r14 + 0x798]
0x2fde419: bf42010000               mov edi, 0x142
0x2fde41e: e8cd1e26fd               call 0x2402f0
0x2fde423: 89c6                     mov esi, eax
0x2fde425: 4889df                   mov rdi, rbx
0x2fde428: e8031d4500               call 0x3430130
0x2fde42d: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x2fde431: 4885ff                   test rdi, rdi
0x2fde434: 7405                     je 0x2fde43b
0x2fde436: e855960dfd               call 0xb7a90
0x2fde43b: 498b4500                 mov rax, qword ptr [r13]
0x2fde43f: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fde443: 7539                     jne 0x2fde47e
0x2fde445: 4883c428                 add rsp, 0x28
0x2fde449: 5b                       pop rbx
0x2fde44a: 415c                     pop r12
0x2fde44c: 415d                     pop r13
0x2fde44e: 415e                     pop r14
0x2fde450: 415f                     pop r15
0x2fde452: 5d                       pop rbp
0x2fde453: c3                       ret 
0x2fde454: 498b06                   mov rax, qword ptr [r14]
0x2fde457: 4c89f7                   mov rdi, r14
0x2fde45a: 4889de                   mov rsi, rbx
0x2fde45d: ff9090070000             call qword ptr [rax + 0x790]
0x2fde463: 84c0                     test al, al
0x2fde465: 7492                     je 0x2fde3f9
0x2fde467: 488b03                   mov rax, qword ptr [rbx]
0x2fde46a: 4889df                   mov rdi, rbx
0x2fde46d: ff90280f0000             call qword ptr [rax + 0xf28]
0x2fde473: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x2fde477: 4885ff                   test rdi, rdi
0x2fde47a: 75ba                     jne 0x2fde436
0x2fde47c: ebbd                     jmp 0x2fde43b
0x2fde47e: e8dd394904               call 0x7471e60
0x2fde483: 0f0b                     ud2 
0x2fde485: cc                       int3 
0x2fde486: cc                       int3 
0x2fde487: cc                       int3 
0x2fde488: cc                       int3 
0x2fde489: cc                       int3 
0x2fde48a: cc                       int3 
0x2fde48b: cc                       int3 
0x2fde48c: cc                       int3 
0x2fde48d: cc                       int3 
0x2fde48e: cc                       int3 
0x2fde48f: cc                       int3 

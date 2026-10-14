0x496380: 55                       push rbp
0x496381: 4889e5                   mov rbp, rsp
0x496384: 4157                     push r15
0x496386: 4156                     push r14
0x496388: 4155                     push r13
0x49638a: 4154                     push r12
0x49638c: 53                       push rbx
0x49638d: 4883ec18                 sub rsp, 0x18
0x496391: 4c8b2570bbc908           mov r12, qword ptr [rip + 0x8c9bb70]
0x496398: 4889f3                   mov rbx, rsi
0x49639b: 4989ff                   mov r15, rdi
0x49639e: 498b0424                 mov rax, qword ptr [r12]
0x4963a2: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x4963a6: 488b4628                 mov rax, qword ptr [rsi + 0x28]
0x4963aa: 480b4620                 or rax, qword ptr [rsi + 0x20]
0x4963ae: 7579                     jne 0x496429
0x4963b0: bf80000000               mov edi, 0x80
0x4963b5: 31f6                     xor esi, esi
0x4963b7: e86416c2ff               call 0xb7a20
0x4963bc: 4989c6                   mov r14, rax
0x4963bf: e87cb9f6ff               call 0x401d40
0x4963c4: 4c89f7                   mov rdi, r14
0x4963c7: 4889c2                   mov rdx, rax
0x4963ca: 31f6                     xor esi, esi
0x4963cc: e8efe1ecff               call 0x3645c0
0x4963d1: 488d05b899a507           lea rax, [rip + 0x7a599b8]
0x4963d8: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x4963dc: c4c178114668             vmovups xmmword ptr [r14 + 0x68], xmm0
0x4963e2: 4883c010                 add rax, 0x10
0x4963e6: 498906                   mov qword ptr [r14], rax
0x4963e9: 49c7467800000000         mov qword ptr [r14 + 0x78], 0
0x4963f1: 498b3f                   mov rdi, qword ptr [r15]
0x4963f4: 48897dc0                 mov qword ptr [rbp - 0x40], rdi
0x4963f8: 40f6c701                 test dil, 1
0x4963fc: 7417                     je 0x496415
0x4963fe: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x496402: 7411                     je 0x496415
0x496404: 488d05518af008           lea rax, [rip + 0x8f08a51]
0x49640b: 803801                   cmp byte ptr [rax], 1
0x49640e: 7505                     jne 0x496415
0x496410: e8ab1eedff               call 0x3682c0
0x496415: 488d75c0                 lea rsi, [rbp - 0x40]
0x496419: 4c89f7                   mov rdi, r14
0x49641c: 4889da                   mov rdx, rbx
0x49641f: e81cbcf6ff               call 0x402040
0x496424: e9ac000000               jmp 0x4964d5
0x496429: bf90000000               mov edi, 0x90
0x49642e: 31f6                     xor esi, esi
0x496430: e8eb15c2ff               call 0xb7a20
0x496435: 4989c6                   mov r14, rax
0x496438: e803b9f6ff               call 0x401d40
0x49643d: 4c89f7                   mov rdi, r14
0x496440: 4889c2                   mov rdx, rax
0x496443: 31f6                     xor esi, esi
0x496445: e876e1ecff               call 0x3645c0
0x49644a: 488d053f99a507           lea rax, [rip + 0x7a5993f]
0x496451: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x496455: c4c178114668             vmovups xmmword ptr [r14 + 0x68], xmm0
0x49645b: 4883c010                 add rax, 0x10
0x49645f: 498906                   mov qword ptr [r14], rax
0x496462: 49c7467800000000         mov qword ptr [r14 + 0x78], 0
0x49646a: 4d8b2f                   mov r13, qword ptr [r15]
0x49646d: 41f6c501                 test r13b, 1
0x496471: 7437                     je 0x4964aa
0x496473: 4d89ef                   mov r15, r13
0x496476: 4983e7fe                 and r15, 0xfffffffffffffffe
0x49647a: 742e                     je 0x4964aa
0x49647c: 488d05d989f008           lea rax, [rip + 0x8f089d9]
0x496483: 803801                   cmp byte ptr [rax], 1
0x496486: 750f                     jne 0x496497
0x496488: 4c89ff                   mov rdi, r15
0x49648b: e8301eedff               call 0x3682c0
0x496490: 488d05c589f008           lea rax, [rip + 0x8f089c5]
0x496497: 803801                   cmp byte ptr [rax], 1
0x49649a: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x49649e: 750e                     jne 0x4964ae
0x4964a0: 4c89ff                   mov rdi, r15
0x4964a3: e8181eedff               call 0x3682c0
0x4964a8: eb04                     jmp 0x4964ae
0x4964aa: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x4964ae: 488d75c8                 lea rsi, [rbp - 0x38]
0x4964b2: 4c89f7                   mov rdi, r14
0x4964b5: 4889da                   mov rdx, rbx
0x4964b8: e883bbf6ff               call 0x402040
0x4964bd: 488d0584c4a607           lea rax, [rip + 0x7a6c484]
0x4964c4: 498906                   mov qword ptr [r14], rax
0x4964c7: c5f8104320               vmovups xmm0, xmmword ptr [rbx + 0x20]
0x4964cc: c4c178118680000000       vmovups xmmword ptr [r14 + 0x80], xmm0
0x4964d5: 498b0424                 mov rax, qword ptr [r12]
0x4964d9: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x4964dd: 7512                     jne 0x4964f1
0x4964df: 4c89f0                   mov rax, r14
0x4964e2: 4883c418                 add rsp, 0x18
0x4964e6: 5b                       pop rbx
0x4964e7: 415c                     pop r12
0x4964e9: 415d                     pop r13
0x4964eb: 415e                     pop r14
0x4964ed: 415f                     pop r15
0x4964ef: 5d                       pop rbp
0x4964f0: c3                       ret 
0x4964f1: e86ab9fd06               call 0x7471e60
0x4964f6: 0f0b                     ud2 
0x4964f8: cc                       int3 
0x4964f9: cc                       int3 
0x4964fa: cc                       int3 
0x4964fb: cc                       int3 
0x4964fc: cc                       int3 
0x4964fd: cc                       int3 
0x4964fe: cc                       int3 
0x4964ff: cc                       int3 

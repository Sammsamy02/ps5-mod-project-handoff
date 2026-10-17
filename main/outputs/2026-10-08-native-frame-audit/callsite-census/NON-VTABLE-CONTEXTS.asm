CALL 0x2d5aed unresolved-member-or-global-load
2d5a49 c5fc118550ffffff vmovups ymmword ptr [rbp - 0xb0], ymm0
2d5a51 c5fc118570ffffff vmovups ymmword ptr [rbp - 0x90], ymm0
2d5a59 e8a253f4ff call 0x21ae00
2d5a5e 488d0543ffbe07 lea rax, [rip + 0x7beff43]
2d5a65 4c89ff mov rdi, r15
2d5a68 be01000000 mov esi, 1
2d5a6d 48c7459000000000 mov qword ptr [rbp - 0x70], 0
2d5a75 48898500ffffff mov qword ptr [rbp - 0x100], rax
2d5a7c 4c896598 mov qword ptr [rbp - 0x68], r12
2d5a80 48c745a000000000 mov qword ptr [rbp - 0x60], 0
2d5a88 e8d376f4ff call 0x21d160
2d5a8d 488b8500ffffff mov rax, qword ptr [rbp - 0x100]
2d5a94 4c89ff mov rdi, r15
2d5a97 31f6 xor esi, esi
2d5a99 ff9008010000 call qword ptr [rax + 0x108]
2d5a9f 488d058afcbe07 lea rax, [rip + 0x7befc8a]
2d5aa6 bf10000000 mov edi, 0x10
2d5aab 48898500ffffff mov qword ptr [rbp - 0x100], rax
2d5ab2 e8c9115c04 call 0x4896c80
2d5ab7 c5f857c0 vxorps xmm0, xmm0, xmm0
2d5abb c5f81100 vmovups xmmword ptr [rax], xmm0
2d5abf 48c70000000000 mov qword ptr [rax], 0
2d5ac6 488945b8 mov qword ptr [rbp - 0x48], rax
2d5aca 488945b0 mov qword ptr [rbp - 0x50], rax
2d5ace c5f81145c0 vmovups xmmword ptr [rbp - 0x40], xmm0
2d5ad3 488dbd60feffff lea rdi, [rbp - 0x1a0]
2d5ada be01000000 mov esi, 1
2d5adf 808d8afeffff10 or byte ptr [rbp - 0x176], 0x10
2d5ae6 488b8560feffff mov rax, qword ptr [rbp - 0x1a0]
2d5aed ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x326b25 confirmed-UFunction-Func-Invoke
326ac6 4156 push r14
326ac8 4155 push r13
326aca 4154 push r12
326acc 53 push rbx
326acd 50 push rax
326ace 4c8b6f20 mov r13, qword ptr [rdi + 0x20]
326ad2 4989ce mov r14, rcx
326ad5 4889d3 mov rbx, rdx
326ad8 4989f4 mov r12, rsi
326adb 4989ff mov r15, rdi
326ade e83d8f0700 call 0x39fa20
326ae3 4885c0 test rax, rax
326ae6 7426 je 0x326b0e
326ae8 48634838 movsxd rcx, dword ptr [rax + 0x38]
326aec 413b4d38 cmp ecx, dword ptr [r13 + 0x38]
326af0 7f1c jg 0x326b0e
326af2 498b5530 mov rdx, qword ptr [r13 + 0x30]
326af6 4883c030 add rax, 0x30
326afa 483904ca cmp qword ptr [rdx + rcx*8], rax
326afe 750e jne 0x326b0e
326b00 4c89e7 mov rdi, r12
326b03 4c89ee mov rsi, r13
326b06 e8250c1600 call 0x487730
326b0b 4989c4 mov r12, rax
326b0e 4c8bab90000000 mov r13, qword ptr [rbx + 0x90]
326b15 4c89e7 mov rdi, r12
326b18 4889de mov rsi, rbx
326b1b 4c89f2 mov rdx, r14
326b1e 4c89bb90000000 mov qword ptr [rbx + 0x90], r15
326b25 41ff97d8000000 call qword ptr [r15 + 0xd8]

CALL 0x632234 unresolved-no-aligned-local-prologue


CALL 0x8be990 unresolved-member-or-global-load
8be91b 4531c0 xor r8d, r8d
8be91e 6690 nop 
8be920 448d4801 lea r9d, [rax + 1]
8be924 f0440fb14b08 lock cmpxchg dword ptr [rbx + 8], r9d
8be92a 746f je 0x8be99b
8be92c 85c0 test eax, eax
8be92e 75f0 jne 0x8be920
8be930 31db xor ebx, ebx
8be932 eb6e jmp 0x8be9a2
8be934 c60700 mov byte ptr [rdi], 0
8be937 c5f857c0 vxorps xmm0, xmm0, xmm0
8be93b c5f8114708 vmovups xmmword ptr [rdi + 8], xmm0
8be940 c6472000 mov byte ptr [rdi + 0x20], 0
8be944 c5f857c0 vxorps xmm0, xmm0, xmm0
8be948 c5fc114728 vmovups ymmword ptr [rdi + 0x28], ymm0
8be94d c5fc114748 vmovups ymmword ptr [rdi + 0x48], ymm0
8be952 c5fc114768 vmovups ymmword ptr [rdi + 0x68], ymm0
8be957 c5fc118788000000 vmovups ymmword ptr [rdi + 0x88], ymm0
8be95f 48c787a800000000000000 mov qword ptr [rdi + 0xa8], 0
8be96a c787b000000002070200 mov dword ptr [rdi + 0xb0], 0x20702
8be974 c687b400000000 mov byte ptr [rdi + 0xb4], 0
8be97b 4889f8 mov rax, rdi
8be97e 5b pop rbx
8be97f 415e pop r14
8be981 5d pop rbp
8be982 c3 ret 
8be983 488b042500000000 mov rax, qword ptr [0]
8be98b 31f6 xor esi, esi
8be98d 4889fb mov rbx, rdi
8be990 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0xa4e9b8 unresolved-member-or-global-load
a4e90d e88e070000 call 0xa4f0a0
a4e912 4c89e7 mov rdi, r12
a4e915 e8e60b0000 call 0xa4f500
a4e91a 4c89ff mov rdi, r15
a4e91d e8de0b0000 call 0xa4f500
a4e922 4c89f7 mov rdi, r14
a4e925 e876070000 call 0xa4f0a0
a4e92a 4889df mov rdi, rbx
a4e92d e86e070000 call 0xa4f0a0
a4e932 488b7c2448 mov rdi, qword ptr [rsp + 0x48]
a4e937 e864110000 call 0xa4faa0
a4e93c 4c8db42480010000 lea r14, [rsp + 0x180]
a4e944 c5f857c0 vxorps xmm0, xmm0, xmm0
a4e948 c5fc118424f0010000 vmovups ymmword ptr [rsp + 0x1f0], ymm0
a4e951 c5fc118424e0010000 vmovups ymmword ptr [rsp + 0x1e0], ymm0
a4e95a c5fc118424c0010000 vmovups ymmword ptr [rsp + 0x1c0], ymm0
a4e963 c5fc118424a0010000 vmovups ymmword ptr [rsp + 0x1a0], ymm0
a4e96c c5fc11842480010000 vmovups ymmword ptr [rsp + 0x180], ymm0
a4e975 4c89f7 mov rdi, r14
a4e978 e883c47cff call 0x21ae00
a4e97d 488d9c2440020000 lea rbx, [rsp + 0x240]
a4e985 488d05fc595007 lea rax, [rip + 0x75059fc]
a4e98c 48c784241002000000000000 mov qword ptr [rsp + 0x210], 0
a4e998 4889df mov rdi, rbx
a4e99b 4889842480010000 mov qword ptr [rsp + 0x180], rax
a4e9a3 e8282067ff call 0xc09d0
a4e9a8 488b842480010000 mov rax, qword ptr [rsp + 0x180]
a4e9b0 4c89f7 mov rdi, r14
a4e9b3 be01000000 mov esi, 1
a4e9b8 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x222a582 unresolved-member-or-global-load
222a4fe e8dde65501 call 0x3788be0
222a503 4c89ff mov rdi, r15
222a506 eb08 jmp 0x222a510
222a508 e8d3e65501 call 0x3788be0
222a50d 4c89f7 mov rdi, r14
222a510 4889c6 mov rsi, rax
222a513 e828d025fe call 0x487540
222a518 4989c7 mov r15, rax
222a51b 4885c0 test rax, rax
222a51e 0f8414010000 je 0x222a638
222a524 4c89ff mov rdi, r15
222a527 e8f48a5601 call 0x3793020
222a52c 84c0 test al, al
222a52e 0f8404010000 je 0x222a638
222a534 4c8da5d0feffff lea r12, [rbp - 0x130]
222a53b 4c89e7 mov rdi, r12
222a53e e8bd08fffd call 0x21ae00
222a543 4c8d3576d5c905 lea r14, [rip + 0x5c9d576]
222a54a 488dbd60ffffff lea rdi, [rbp - 0xa0]
222a551 4983c610 add r14, 0x10
222a555 4c89b5d0feffff mov qword ptr [rbp - 0x130], r14
222a55c e8af7afdfd call 0x202010
222a561 488b85d0feffff mov rax, qword ptr [rbp - 0x130]
222a568 4c89e7 mov rdi, r12
222a56b 31f6 xor esi, esi
222a56d ff90c8000000 call qword ptr [rax + 0xc8]
222a573 488b85d0feffff mov rax, qword ptr [rbp - 0x130]
222a57a 4c89e7 mov rdi, r12
222a57d be01000000 mov esi, 1
222a582 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x2b3ca9c unresolved-other-base-definition
2b3ca10 488b4598 mov rax, qword ptr [rbp - 0x68]
2b3ca14 488d7d80 lea rdi, [rbp - 0x80]
2b3ca18 4885c0 test rax, rax
2b3ca1b 480f45f8 cmovne rdi, rax
2b3ca1f 488b07 mov rax, qword ptr [rdi]
2b3ca22 ff5010 call qword ptr [rax + 0x10]
2b3ca25 4c89f7 mov rdi, r14
2b3ca28 be00300000 mov esi, 0x3000
2b3ca2d e8de9478fd call 0x2c5f10
2b3ca32 4c89ff mov rdi, r15
2b3ca35 4d8d6f38 lea r13, [r15 + 0x38]
2b3ca39 e8b2c81a00 call 0x2ce92f0
2b3ca3e 4c8dbd30ffffff lea r15, [rbp - 0xd0]
2b3ca45 c5f857c0 vxorps xmm0, xmm0, xmm0
2b3ca49 c5fc114590 vmovups ymmword ptr [rbp - 0x70], ymm0
2b3ca4e 4989c4 mov r12, rax
2b3ca51 c5fc118530ffffff vmovups ymmword ptr [rbp - 0xd0], ymm0
2b3ca59 c5fc118550ffffff vmovups ymmword ptr [rbp - 0xb0], ymm0
2b3ca61 c5fc118570ffffff vmovups ymmword ptr [rbp - 0x90], ymm0
2b3ca69 c5fc1145a0 vmovups ymmword ptr [rbp - 0x60], ymm0
2b3ca6e 4c89ff mov rdi, r15
2b3ca71 e88ae36dfd call 0x21ae00
2b3ca76 488d05d3853905 lea rax, [rip + 0x53985d3]
2b3ca7d 4c89ff mov rdi, r15
2b3ca80 be01000000 mov esi, 1
2b3ca85 48c745c000000000 mov qword ptr [rbp - 0x40], 0
2b3ca8d 488d4810 lea rcx, [rax + 0x10]
2b3ca91 48898d30ffffff mov qword ptr [rbp - 0xd0], rcx
2b3ca98 4c896dc8 mov qword ptr [rbp - 0x38], r13
2b3ca9c ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x2d51e38 unresolved-other-base-definition
2d51d9c 53 push rbx
2d51d9d 4881ec98020000 sub rsp, 0x298
2d51da4 488b0d5d013e06 mov rcx, qword ptr [rip + 0x63e015d]
2d51dab ffca dec edx
2d51dad 488b01 mov rax, qword ptr [rcx]
2d51db0 488945d0 mov qword ptr [rbp - 0x30], rax
2d51db4 83fa01 cmp edx, 1
2d51db7 0f870c020000 ja 0x2d51fc9
2d51dbd 837f1000 cmp dword ptr [rdi + 0x10], 0
2d51dc1 0f8e02020000 jle 0x2d51fc9
2d51dc7 4c8db540fdffff lea r14, [rbp - 0x2c0]
2d51dce 4c8d7f08 lea r15, [rdi + 8]
2d51dd2 4989fc mov r12, rdi
2d51dd5 c5f857c0 vxorps xmm0, xmm0, xmm0
2d51dd9 c5fc1185b0fdffff vmovups ymmword ptr [rbp - 0x250], ymm0
2d51de1 c5fc1185a0fdffff vmovups ymmword ptr [rbp - 0x260], ymm0
2d51de9 c5fc118580fdffff vmovups ymmword ptr [rbp - 0x280], ymm0
2d51df1 c5fc118560fdffff vmovups ymmword ptr [rbp - 0x2a0], ymm0
2d51df9 c5fc118540fdffff vmovups ymmword ptr [rbp - 0x2c0], ymm0
2d51e01 4889f3 mov rbx, rsi
2d51e04 4c89f7 mov rdi, r14
2d51e07 e8f48f4cfd call 0x21ae00
2d51e0c 488d053d321805 lea rax, [rip + 0x518323d]
2d51e13 4c89f7 mov rdi, r14
2d51e16 be01000000 mov esi, 1
2d51e1b 48c785d0fdffff00000000 mov qword ptr [rbp - 0x230], 0
2d51e26 488d4810 lea rcx, [rax + 0x10]
2d51e2a 48898d40fdffff mov qword ptr [rbp - 0x2c0], rcx
2d51e31 4c89bdd8fdffff mov qword ptr [rbp - 0x228], r15
2d51e38 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x2d9ecc8 unresolved-other-base-definition
2d9ec50 55 push rbp
2d9ec51 4889e5 mov rbp, rsp
2d9ec54 4157 push r15
2d9ec56 4156 push r14
2d9ec58 4155 push r13
2d9ec5a 4154 push r12
2d9ec5c 53 push rbx
2d9ec5d 4881ec98010000 sub rsp, 0x198
2d9ec64 488b059d323906 mov rax, qword ptr [rip + 0x639329d]
2d9ec6b 4989d7 mov r15, rdx
2d9ec6e 4889f3 mov rbx, rsi
2d9ec71 4989fe mov r14, rdi
2d9ec74 4c8d6208 lea r12, [rdx + 8]
2d9ec78 c5f857c0 vxorps xmm0, xmm0, xmm0
2d9ec7c 488b00 mov rax, qword ptr [rax]
2d9ec7f 488945d0 mov qword ptr [rbp - 0x30], rax
2d9ec83 c5fc114770 vmovups ymmword ptr [rdi + 0x70], ymm0
2d9ec88 c5fc114760 vmovups ymmword ptr [rdi + 0x60], ymm0
2d9ec8d c5fc114740 vmovups ymmword ptr [rdi + 0x40], ymm0
2d9ec92 c5fc114720 vmovups ymmword ptr [rdi + 0x20], ymm0
2d9ec97 c5fc1107 vmovups ymmword ptr [rdi], ymm0
2d9ec9b e860c147fd call 0x21ae00
2d9eca0 488d05a9631305 lea rax, [rip + 0x51363a9]
2d9eca7 4c89f7 mov rdi, r14
2d9ecaa be01000000 mov esi, 1
2d9ecaf 49c7869000000000000000 mov qword ptr [r14 + 0x90], 0
2d9ecba 488d4810 lea rcx, [rax + 0x10]
2d9ecbe 49890e mov qword ptr [r14], rcx
2d9ecc1 4d89a698000000 mov qword ptr [r14 + 0x98], r12
2d9ecc8 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x31f7f66 unresolved-member-or-global-load
31f7f20 55 push rbp
31f7f21 4889e5 mov rbp, rsp
31f7f24 4156 push r14
31f7f26 53 push rbx
31f7f27 4883ec10 sub rsp, 0x10
31f7f2b 4c8b35d69ff305 mov r14, qword ptr [rip + 0x5f39fd6]
31f7f32 4889f3 mov rbx, rsi
31f7f35 498b06 mov rax, qword ptr [r14]
31f7f38 488945e8 mov qword ptr [rbp - 0x18], rax
31f7f3c 488b06 mov rax, qword ptr [rsi]
31f7f3f 488d75e0 lea rsi, [rbp - 0x20]
31f7f43 488945e0 mov qword ptr [rbp - 0x20], rax
31f7f47 ff809c000000 inc dword ptr [rax + 0x9c]
31f7f4d e82e191dfd call 0x3c9880
31f7f52 488b03 mov rax, qword ptr [rbx]
31f7f55 488b7810 mov rdi, qword ptr [rax + 0x10]
31f7f59 4885ff test rdi, rdi
31f7f5c 740e je 0x31f7f6c
31f7f5e 488b4708 mov rax, qword ptr [rdi + 8]
31f7f62 4883c708 add rdi, 8
31f7f66 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x37de9bd unresolved-member-or-global-load
37de91d c5fc1185a8feffff vmovups ymmword ptr [rbp - 0x158], ymm0
37de925 c5fc118590feffff vmovups ymmword ptr [rbp - 0x170], ymm0
37de92d 488b5628 mov rdx, qword ptr [rsi + 0x28]
37de931 888d3efeffff mov byte ptr [rbp - 0x1c2], cl
37de937 48898528feffff mov qword ptr [rbp - 0x1d8], rax
37de93e 4488853ffeffff mov byte ptr [rbp - 0x1c1], r8b
37de945 48899580feffff mov qword ptr [rbp - 0x180], rdx
37de94c 8b4e3c mov ecx, dword ptr [rsi + 0x3c]
37de94f 48899578feffff mov qword ptr [rbp - 0x188], rdx
37de956 898d88feffff mov dword ptr [rbp - 0x178], ecx
37de95c 488b07 mov rax, qword ptr [rdi]
37de95f 4c89ff mov rdi, r15
37de962 c5f8104040 vmovups xmm0, xmmword ptr [rax + 0x40]
37de967 c5f8118560feffff vmovups xmmword ptr [rbp - 0x1a0], xmm0
37de96f e88cc4a3fc call 0x21ae00
37de974 488d0545916e04 lea rax, [rip + 0x46e9145]
37de97b 488dbd58ffffff lea rdi, [rbp - 0xa8]
37de982 4889fb mov rbx, rdi
37de985 4883c010 add rax, 0x10
37de989 48898520feffff mov qword ptr [rbp - 0x1e0], rax
37de990 488985c8feffff mov qword ptr [rbp - 0x138], rax
37de997 e87436a2fc call 0x202010
37de99c 488b85c8feffff mov rax, qword ptr [rbp - 0x138]
37de9a3 4c89ff mov rdi, r15
37de9a6 31f6 xor esi, esi
37de9a8 ff90c8000000 call qword ptr [rax + 0xc8]
37de9ae 488b85c8feffff mov rax, qword ptr [rbp - 0x138]
37de9b5 4c89ff mov rdi, r15
37de9b8 be01000000 mov esi, 1
37de9bd ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x37ff8fa unresolved-member-or-global-load
37ff872 be02000000 mov esi, 2
37ff877 4489fa mov edx, r15d
37ff87a 4589f9 mov r9d, r15d
37ff87d 31c9 xor ecx, ecx
37ff87f 48890424 mov qword ptr [rsp], rax
37ff883 e8487d82fc call 0x275d0
37ff888 488bbda0feffff mov rdi, qword ptr [rbp - 0x160]
37ff88f 4d01ff add r15, r15
37ff892 4889de mov rsi, rbx
37ff895 4c89fa mov rdx, r15
37ff898 e8b325c703 call 0x7471e50
37ff89d eb0a jmp 0x37ff8a9
37ff89f c785acfeffff00000000 mov dword ptr [rbp - 0x154], 0
37ff8a9 488d9dc0feffff lea rbx, [rbp - 0x140]
37ff8b0 4889df mov rdi, rbx
37ff8b3 e848b5a1fc call 0x21ae00
37ff8b8 4c8d2501826c04 lea r12, [rip + 0x46c8201]
37ff8bf 4c8dbd50ffffff lea r15, [rbp - 0xb0]
37ff8c6 4c89ff mov rdi, r15
37ff8c9 4983c410 add r12, 0x10
37ff8cd 4c89a5c0feffff mov qword ptr [rbp - 0x140], r12
37ff8d4 e83727a0fc call 0x202010
37ff8d9 488b85c0feffff mov rax, qword ptr [rbp - 0x140]
37ff8e0 4889df mov rdi, rbx
37ff8e3 31f6 xor esi, esi
37ff8e5 ff90c8000000 call qword ptr [rax + 0xc8]
37ff8eb 488b85c0feffff mov rax, qword ptr [rbp - 0x140]
37ff8f2 4889df mov rdi, rbx
37ff8f5 be01000000 mov esi, 1
37ff8fa ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x380c36c unresolved-member-or-global-load
380c2df ffc8 dec eax
380c2e1 4898 cdqe 
380c2e3 4889c1 mov rcx, rax
380c2e6 48c1e107 shl rcx, 7
380c2ea 488d3cc1 lea rdi, [rcx + rax*8]
380c2ee 4803bdb0feffff add rdi, qword ptr [rbp - 0x150]
380c2f5 e816c9fdff call 0x37e8c10
380c2fa 48898550feffff mov qword ptr [rbp - 0x1b0], rax
380c301 488b03 mov rax, qword ptr [rbx]
380c304 4c8dbde0feffff lea r15, [rbp - 0x120]
380c30b 4c89ff mov rdi, r15
380c30e 48898538feffff mov qword ptr [rbp - 0x1c8], rax
380c315 498b06 mov rax, qword ptr [r14]
380c318 c5f8104040 vmovups xmm0, xmmword ptr [rax + 0x40]
380c31d c5f8118540feffff vmovups xmmword ptr [rbp - 0x1c0], xmm0
380c325 e8d6eaa0fc call 0x21ae00
380c32a 488d1d8fb76b04 lea rbx, [rip + 0x46bb78f]
380c331 4c8dad70ffffff lea r13, [rbp - 0x90]
380c338 4c89ef mov rdi, r13
380c33b 4883c310 add rbx, 0x10
380c33f 48899de0feffff mov qword ptr [rbp - 0x120], rbx
380c346 e8c55c9ffc call 0x202010
380c34b 488b85e0feffff mov rax, qword ptr [rbp - 0x120]
380c352 4c89ff mov rdi, r15
380c355 31f6 xor esi, esi
380c357 ff90c8000000 call qword ptr [rax + 0xc8]
380c35d 488b85e0feffff mov rax, qword ptr [rbp - 0x120]
380c364 4c89ff mov rdi, r15
380c367 be01000000 mov esi, 1
380c36c ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x3853a42 unresolved-other-base-definition
3853987 4885d2 test rdx, rdx
385398a 48898de0fbffff mov qword ptr [rbp - 0x420], rcx
3853991 480f44d6 cmove rdx, rsi
3853995 488995e8fbffff mov qword ptr [rbp - 0x418], rdx
385399c 83bd50fdffff00 cmp dword ptr [rbp - 0x2b0], 0
38539a3 7f28 jg 0x38539cd
38539a5 e94f010000 jmp 0x3853af9
38539aa 48c785e0fbffff00000000 mov qword ptr [rbp - 0x420], 0
38539b5 48c785e8fbffff00000000 mov qword ptr [rbp - 0x418], 0
38539c0 83bd50fdffff00 cmp dword ptr [rbp - 0x2b0], 0
38539c7 0f8e2c010000 jle 0x3853af9
38539cd 4c8bad70fcffff mov r13, qword ptr [rbp - 0x390]
38539d4 4c8db570fdffff lea r14, [rbp - 0x290]
38539db c5f857c0 vxorps xmm0, xmm0, xmm0
38539df c5fc1185d0fdffff vmovups ymmword ptr [rbp - 0x230], ymm0
38539e7 488d9d48fdffff lea rbx, [rbp - 0x2b8]
38539ee c5fc118570fdffff vmovups ymmword ptr [rbp - 0x290], ymm0
38539f6 c5fc118590fdffff vmovups ymmword ptr [rbp - 0x270], ymm0
38539fe c5fc1185b0fdffff vmovups ymmword ptr [rbp - 0x250], ymm0
3853a06 c5fc1185e0fdffff vmovups ymmword ptr [rbp - 0x220], ymm0
3853a0e 4c89f7 mov rdi, r14
3853a11 e8ea739cfc call 0x21ae00
3853a16 488d0533166804 lea rax, [rip + 0x4681633]
3853a1d 4c89f7 mov rdi, r14
3853a20 be01000000 mov esi, 1
3853a25 48c78500feffff00000000 mov qword ptr [rbp - 0x200], 0
3853a30 488d4810 lea rcx, [rax + 0x10]
3853a34 48898d70fdffff mov qword ptr [rbp - 0x290], rcx
3853a3b 48899d08feffff mov qword ptr [rbp - 0x1f8], rbx
3853a42 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x395fe78 unresolved-member-or-global-load
395fdcf e88cb88bfc call 0x21b660
395fdd4 31db xor ebx, ebx
395fdd6 e9cb010000 jmp 0x395ffa6
395fddb 4c8dbdd0feffff lea r15, [rbp - 0x130]
395fde2 c5f857c0 vxorps xmm0, xmm0, xmm0
395fde6 c5fc118540ffffff vmovups ymmword ptr [rbp - 0xc0], ymm0
395fdee 4989f6 mov r14, rsi
395fdf1 c5fc118530ffffff vmovups ymmword ptr [rbp - 0xd0], ymm0
395fdf9 c5fc118510ffffff vmovups ymmword ptr [rbp - 0xf0], ymm0
395fe01 c5fc1185f0feffff vmovups ymmword ptr [rbp - 0x110], ymm0
395fe09 c5fc1185d0feffff vmovups ymmword ptr [rbp - 0x130], ymm0
395fe11 4c89ff mov rdi, r15
395fe14 e8e7af8bfc call 0x21ae00
395fe19 c5f8100dffcb0d04 vmovups xmm1, xmmword ptr [rip + 0x40dcbff]
395fe21 488d05e0f2cb04 lea rax, [rip + 0x4cbf2e0]
395fe28 4c89ff mov rdi, r15
395fe2b 31f6 xor esi, esi
395fe2d c5f857c0 vxorps xmm0, xmm0, xmm0
395fe31 488985d0feffff mov qword ptr [rbp - 0x130], rax
395fe38 48c7459800000000 mov qword ptr [rbp - 0x68], 0
395fe40 c5fc118560ffffff vmovups ymmword ptr [rbp - 0xa0], ymm0
395fe48 48c7458000000000 mov qword ptr [rbp - 0x80], 0
395fe50 c5f8114da0 vmovups xmmword ptr [rbp - 0x60], xmm1
395fe55 48c745b800000000 mov qword ptr [rbp - 0x48], 0
395fe5d c745c000000000 mov dword ptr [rbp - 0x40], 0
395fe64 e8b7d38bfc call 0x21d220
395fe69 488b85d0feffff mov rax, qword ptr [rbp - 0x130]
395fe70 4c89ff mov rdi, r15
395fe73 be01000000 mov esi, 1
395fe78 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x4959da7 unresolved-member-or-global-load
4959d34 4157 push r15
4959d36 4156 push r14
4959d38 4155 push r13
4959d3a 4154 push r12
4959d3c 53 push rbx
4959d3d 4883ec28 sub rsp, 0x28
4959d41 4c8b25c0817d04 mov r12, qword ptr [rip + 0x47d81c0]
4959d48 4889fb mov rbx, rdi
4959d4b 498b0424 mov rax, qword ptr [r12]
4959d4f 488945d0 mov qword ptr [rbp - 0x30], rax
4959d53 807f7401 cmp byte ptr [rdi + 0x74], 1
4959d57 7509 jne 0x4959d62
4959d59 448b7370 mov r14d, dword ptr [rbx + 0x70]
4959d5d e9cf000000 jmp 0x4959e31
4959d62 488d75c0 lea rsi, [rbp - 0x40]
4959d66 4889df mov rdi, rbx
4959d69 c5f857c0 vxorps xmm0, xmm0, xmm0
4959d6d c5f81145c0 vmovups xmmword ptr [rbp - 0x40], xmm0
4959d72 e839010000 call 0x4959eb0
4959d77 448b6dc8 mov r13d, dword ptr [rbp - 0x38]
4959d7b 4585ed test r13d, r13d
4959d7e 0f8ec9000000 jle 0x4959e4d
4959d84 0fb6050559c304 movzx eax, byte ptr [rip + 0x4c35905]
4959d8b 84c0 test al, al
4959d8d 0f84d2000000 je 0x4959e65
4959d93 488b45c0 mov rax, qword ptr [rbp - 0x40]
4959d97 488d3d2a55c304 lea rdi, [rip + 0x4c3552a]
4959d9e 8b30 mov esi, dword ptr [rax]
4959da0 488b052155c304 mov rax, qword ptr [rip + 0x4c35521]
4959da7 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x4959e03 unresolved-member-or-global-load
4959d8d 0f84d2000000 je 0x4959e65
4959d93 488b45c0 mov rax, qword ptr [rbp - 0x40]
4959d97 488d3d2a55c304 lea rdi, [rip + 0x4c3552a]
4959d9e 8b30 mov esi, dword ptr [rax]
4959da0 488b052155c304 mov rax, qword ptr [rip + 0x4c35521]
4959da7 ff90d8000000 call qword ptr [rax + 0xd8]
4959dad 488d75b0 lea rsi, [rbp - 0x50]
4959db1 4889df mov rdi, rbx
4959db4 c5f857c0 vxorps xmm0, xmm0, xmm0
4959db8 4189c7 mov r15d, eax
4959dbb 440fb6f0 movzx r14d, al
4959dbf c5f81145b0 vmovups xmmword ptr [rbp - 0x50], xmm0
4959dc4 e827cbffff call 0x49568f0
4959dc9 84c0 test al, al
4959dcb 754e jne 0x4959e1b
4959dcd 4183fd01 cmp r13d, 1
4959dd1 7448 je 0x4959e1b
4959dd3 41bc01000000 mov r12d, 1
4959dd9 eb0d jmp 0x4959de8
4959ddb 0f1f440000 nop dword ptr [rax + rax]
4959de0 49ffc4 inc r12
4959de3 4d39e5 cmp r13, r12
4959de6 742c je 0x4959e14
4959de8 488b45c0 mov rax, qword ptr [rbp - 0x40]
4959dec 428b34a0 mov esi, dword ptr [rax + r12*4]
4959df0 83feff cmp esi, -1
4959df3 74eb je 0x4959de0
4959df5 488b05cc54c304 mov rax, qword ptr [rip + 0x4c354cc]
4959dfc 488d3dc554c304 lea rdi, [rip + 0x4c354c5]
4959e03 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x495c990 unresolved-member-or-global-load
495c903 4885ff test rdi, rdi
495c906 7405 je 0x495c90d
495c908 e883b175fb call 0xb7a90
495c90d 488bbdc0feffff mov rdi, qword ptr [rbp - 0x140]
495c914 4885ff test rdi, rdi
495c917 7405 je 0x495c91e
495c919 e872b175fb call 0xb7a90
495c91e 41f6c501 test r13b, 1
495c922 4c8bada8feffff mov r13, qword ptr [rbp - 0x158]
495c929 0f8521010000 jne 0x495ca50
495c92f 4c89e7 mov rdi, r12
495c932 e8f9d3ffff call 0x4959d30
495c937 83f801 cmp eax, 1
495c93a 0f845bf2ffff je 0x495bb9b
495c940 488b9d78feffff mov rbx, qword ptr [rbp - 0x188]
495c947 83f803 cmp eax, 3
495c94a 0f85ca000000 jne 0x495ca1a
495c950 488d05818a9804 lea rax, [rip + 0x4988a81]
495c957 4c89ef mov rdi, r13
495c95a 488db530ffffff lea rsi, [rbp - 0xd0]
495c961 48899d30ffffff mov qword ptr [rbp - 0xd0], rbx
495c968 31d2 xor edx, edx
495c96a 48898538ffffff mov qword ptr [rbp - 0xc8], rax
495c971 e8aa017dfb call 0x12cb20
495c976 4d85ff test r15, r15
495c979 0f8449f2ffff je 0x495bbc8
495c97f 418b37 mov esi, dword ptr [r15]
495c982 488b053f29c304 mov rax, qword ptr [rip + 0x4c3293f]
495c989 488d3d3829c304 lea rdi, [rip + 0x4c32938]
495c990 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x4dbf5cf unresolved-other-base-definition
4dbf546 c645cc00 mov byte ptr [rbp - 0x34], 0
4dbf54a 4989c6 mov r14, rax
4dbf54d 488d4dc8 lea rcx, [rbp - 0x38]
4dbf551 488b00 mov rax, qword ptr [rax]
4dbf554 4c89f7 mov rdi, r14
4dbf557 ff5040 call qword ptr [rax + 0x40]
4dbf55a 807dcc01 cmp byte ptr [rbp - 0x34], 1
4dbf55e 4989c7 mov r15, rax
4dbf561 7504 jne 0x4dbf567
4dbf563 c645cc00 mov byte ptr [rbp - 0x34], 0
4dbf567 4d85ff test r15, r15
4dbf56a 0f84d6000000 je 0x4dbf646
4dbf570 4c8da520ffffff lea r12, [rbp - 0xe0]
4dbf577 c5f857c0 vxorps xmm0, xmm0, xmm0
4dbf57b c5fc114590 vmovups ymmword ptr [rbp - 0x70], ymm0
4dbf580 4c8d6b48 lea r13, [rbx + 0x48]
4dbf584 c5fc114580 vmovups ymmword ptr [rbp - 0x80], ymm0
4dbf589 c5fc118560ffffff vmovups ymmword ptr [rbp - 0xa0], ymm0
4dbf591 c5fc118540ffffff vmovups ymmword ptr [rbp - 0xc0], ymm0
4dbf599 c5fc118520ffffff vmovups ymmword ptr [rbp - 0xe0], ymm0
4dbf5a1 4c89e7 mov rdi, r12
4dbf5a4 e857b845fb call 0x21ae00
4dbf5a9 488d05a05a1103 lea rax, [rip + 0x3115aa0]
4dbf5b0 4c89e7 mov rdi, r12
4dbf5b3 be01000000 mov esi, 1
4dbf5b8 48c745b000000000 mov qword ptr [rbp - 0x50], 0
4dbf5c0 488d4810 lea rcx, [rax + 0x10]
4dbf5c4 48898d20ffffff mov qword ptr [rbp - 0xe0], rcx
4dbf5cb 4c896db8 mov qword ptr [rbp - 0x48], r13
4dbf5cf ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x4dc3336 unresolved-other-base-definition
4dc32ac 7f0e jg 0x4dc32bc
4dc32ae 488b4930 mov rcx, qword ptr [rcx + 0x30]
4dc32b2 4883c030 add rax, 0x30
4dc32b6 483904d1 cmp qword ptr [rcx + rdx*8], rax
4dc32ba 7403 je 0x4dc32bf
4dc32bc 4531f6 xor r14d, r14d
4dc32bf 488b7318 mov rsi, qword ptr [rbx + 0x18]
4dc32c3 4c89f7 mov rdi, r14
4dc32c6 e8b5e80600 call 0x4e31b80
4dc32cb 4885c0 test rax, rax
4dc32ce 0f84d8000000 je 0x4dc33ac
4dc32d4 4c8da530ffffff lea r12, [rbp - 0xd0]
4dc32db c5f857c0 vxorps xmm0, xmm0, xmm0
4dc32df c5fc1145a0 vmovups ymmword ptr [rbp - 0x60], ymm0
4dc32e4 4989c7 mov r15, rax
4dc32e7 4c8d6b58 lea r13, [rbx + 0x58]
4dc32eb c5fc114590 vmovups ymmword ptr [rbp - 0x70], ymm0
4dc32f0 c5fc118570ffffff vmovups ymmword ptr [rbp - 0x90], ymm0
4dc32f8 c5fc118550ffffff vmovups ymmword ptr [rbp - 0xb0], ymm0
4dc3300 c5fc118530ffffff vmovups ymmword ptr [rbp - 0xd0], ymm0
4dc3308 4c89e7 mov rdi, r12
4dc330b e8f07a45fb call 0x21ae00
4dc3310 488d05391d1103 lea rax, [rip + 0x3111d39]
4dc3317 4c89e7 mov rdi, r12
4dc331a be01000000 mov esi, 1
4dc331f 48c745c000000000 mov qword ptr [rbp - 0x40], 0
4dc3327 488d4810 lea rcx, [rax + 0x10]
4dc332b 48898d30ffffff mov qword ptr [rbp - 0xd0], rcx
4dc3332 4c896dc8 mov qword ptr [rbp - 0x38], r13
4dc3336 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x4dc3fe6 unresolved-other-base-definition
4dc3f5c 7f0e jg 0x4dc3f6c
4dc3f5e 488b4930 mov rcx, qword ptr [rcx + 0x30]
4dc3f62 4883c030 add rax, 0x30
4dc3f66 483904d1 cmp qword ptr [rcx + rdx*8], rax
4dc3f6a 7403 je 0x4dc3f6f
4dc3f6c 4531f6 xor r14d, r14d
4dc3f6f 488b7318 mov rsi, qword ptr [rbx + 0x18]
4dc3f73 4c89f7 mov rdi, r14
4dc3f76 e8d5d20600 call 0x4e31250
4dc3f7b 4885c0 test rax, rax
4dc3f7e 0f84d8000000 je 0x4dc405c
4dc3f84 4c8da530ffffff lea r12, [rbp - 0xd0]
4dc3f8b c5f857c0 vxorps xmm0, xmm0, xmm0
4dc3f8f c5fc1145a0 vmovups ymmword ptr [rbp - 0x60], ymm0
4dc3f94 4989c7 mov r15, rax
4dc3f97 4c8d6b58 lea r13, [rbx + 0x58]
4dc3f9b c5fc114590 vmovups ymmword ptr [rbp - 0x70], ymm0
4dc3fa0 c5fc118570ffffff vmovups ymmword ptr [rbp - 0x90], ymm0
4dc3fa8 c5fc118550ffffff vmovups ymmword ptr [rbp - 0xb0], ymm0
4dc3fb0 c5fc118530ffffff vmovups ymmword ptr [rbp - 0xd0], ymm0
4dc3fb8 4c89e7 mov rdi, r12
4dc3fbb e8406e45fb call 0x21ae00
4dc3fc0 488d0589101103 lea rax, [rip + 0x3111089]
4dc3fc7 4c89e7 mov rdi, r12
4dc3fca be01000000 mov esi, 1
4dc3fcf 48c745c000000000 mov qword ptr [rbp - 0x40], 0
4dc3fd7 488d4810 lea rcx, [rax + 0x10]
4dc3fdb 48898d30ffffff mov qword ptr [rbp - 0xd0], rcx
4dc3fe2 4c896dc8 mov qword ptr [rbp - 0x38], r13
4dc3fe6 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x5454fad unresolved-member-or-global-load
5454f25 ff9088000000 call qword ptr [rax + 0x88]
5454f2b 488b0b mov rcx, qword ptr [rbx]
5454f2e 4885c0 test rax, rax
5454f31 4889df mov rdi, rbx
5454f34 490f44c5 cmove rax, r13
5454f38 4889c6 mov rsi, rax
5454f3b ff9180000000 call qword ptr [rcx + 0x80]
5454f41 f685d4fcffff02 test byte ptr [rbp - 0x32c], 2
5454f48 4c8bbdc8fcffff mov r15, qword ptr [rbp - 0x338]
5454f4f 0f84500c0000 je 0x5455ba5
5454f55 488b9db8fcffff mov rbx, qword ptr [rbp - 0x348]
5454f5c 498b07 mov rax, qword ptr [r15]
5454f5f 488b0b mov rcx, qword ptr [rbx]
5454f62 488b8060feffff mov rax, qword ptr [rax - 0x1a0]
5454f69 4c8bb160feffff mov r14, qword ptr [rcx - 0x1a0]
5454f70 488985d8fcffff mov qword ptr [rbp - 0x328], rax
5454f77 4d8d2c07 lea r13, [r15 + rax]
5454f7b 4a8b0433 mov rax, qword ptr [rbx + r14]
5454f7f 4e8d2433 lea r12, [rbx + r14]
5454f83 4c89e7 mov rdi, r12
5454f86 ff9088000000 call qword ptr [rax + 0x88]
5454f8c 4a8b0433 mov rax, qword ptr [rbx + r14]
5454f90 4c89e7 mov rdi, r12
5454f93 ff9098000000 call qword ptr [rax + 0x98]
5454f99 4a8b0433 mov rax, qword ptr [rbx + r14]
5454f9d 4c89e7 mov rdi, r12
5454fa0 ff90a8000000 call qword ptr [rax + 0xa8]
5454fa6 4a8b0433 mov rax, qword ptr [rbx + r14]
5454faa 4c89e7 mov rdi, r12
5454fad ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x5458dca unresolved-member-or-global-load
5458d80 55 push rbp
5458d81 4889e5 mov rbp, rsp
5458d84 4157 push r15
5458d86 4156 push r14
5458d88 4155 push r13
5458d8a 4154 push r12
5458d8c 53 push rbx
5458d8d 4881ece8000000 sub rsp, 0xe8
5458d94 4c8b356d91cd03 mov r14, qword ptr [rip + 0x3cd916d]
5458d9b 4c89c3 mov rbx, r8
5458d9e 4989fd mov r13, rdi
5458da1 498b06 mov rax, qword ptr [r14]
5458da4 488945d0 mov qword ptr [rbp - 0x30], rax
5458da8 807e0201 cmp byte ptr [rsi + 2], 1
5458dac 0f85a4020000 jne 0x5459056
5458db2 488b02 mov rax, qword ptr [rdx]
5458db5 4989cc mov r12, rcx
5458db8 4989d7 mov r15, rdx
5458dbb 488b8060feffff mov rax, qword ptr [rax - 0x1a0]
5458dc2 488d3c02 lea rdi, [rdx + rax]
5458dc6 488b0402 mov rax, qword ptr [rdx + rax]
5458dca ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x5458f63 unresolved-member-or-global-load
5458eeb 4983c502 add r13, 2
5458eef 4983c6fe add r14, -2
5458ef3 75eb jne 0x5458ee0
5458ef5 8b4d80 mov ecx, dword ptr [rbp - 0x80]
5458ef8 488b8578ffffff mov rax, qword ptr [rbp - 0x88]
5458eff 29c1 sub ecx, eax
5458f01 f7c1feff0100 test ecx, 0x1fffe
5458f07 7448 je 0x5458f51
5458f09 4981c448010000 add r12, 0x148
5458f10 41be01100000 mov r14d, 0x1001
5458f16 31db xor ebx, ebx
5458f18 0f1f840000000000 nop dword ptr [rax + rax]
5458f20 488b9548ffffff mov rdx, qword ptr [rbp - 0xb8]
5458f27 0fb70c58 movzx ecx, word ptr [rax + rbx*2]
5458f2b 0fb7f3 movzx esi, bx
5458f2e 4c89e7 mov rdi, r12
5458f31 e82a130000 call 0x545a260
5458f36 8b5580 mov edx, dword ptr [rbp - 0x80]
5458f39 488b8578ffffff mov rax, qword ptr [rbp - 0x88]
5458f40 48ffc3 inc rbx
5458f43 0fb7cb movzx ecx, bx
5458f46 29c2 sub edx, eax
5458f48 c4e208f7d2 bextr edx, edx, r14d
5458f4d 39ca cmp edx, ecx
5458f4f 77cf ja 0x5458f20
5458f51 498b07 mov rax, qword ptr [r15]
5458f54 488b8060feffff mov rax, qword ptr [rax - 0x1a0]
5458f5b 498d3c07 lea rdi, [r15 + rax]
5458f5f 498b0407 mov rax, qword ptr [r15 + rax]
5458f63 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x5467af7 unresolved-member-or-global-load
5467a77 488d3c03 lea rdi, [rbx + rax]
5467a7b 488b0403 mov rax, qword ptr [rbx + rax]
5467a7f ff90a0000000 call qword ptr [rax + 0xa0]
5467a85 488b4d80 mov rcx, qword ptr [rbp - 0x80]
5467a89 66894104 mov word ptr [rcx + 4], ax
5467a8d 488b03 mov rax, qword ptr [rbx]
5467a90 488b8070fdffff mov rax, qword ptr [rax - 0x290]
5467a97 488d3c03 lea rdi, [rbx + rax]
5467a9b 488b0403 mov rax, qword ptr [rbx + rax]
5467a9f ff90c0010000 call qword ptr [rax + 0x1c0]
5467aa5 488b4d80 mov rcx, qword ptr [rbp - 0x80]
5467aa9 66894106 mov word ptr [rcx + 6], ax
5467aad 410fb64607 movzx eax, byte ptr [r14 + 7]
5467ab2 410fb64e08 movzx ecx, byte ptr [r14 + 8]
5467ab7 450fb67e0a movzx r15d, byte ptr [r14 + 0xa]
5467abc 01c1 add ecx, eax
5467abe 4101cf add r15d, ecx
5467ac1 488b0b mov rcx, qword ptr [rbx]
5467ac4 488b8160feffff mov rax, qword ptr [rcx - 0x1a0]
5467acb 488d3c03 lea rdi, [rbx + rax]
5467acf 488b0403 mov rax, qword ptr [rbx + rax]
5467ad3 ff90b0000000 call qword ptr [rax + 0xb0]
5467ad9 488b4d80 mov rcx, qword ptr [rbp - 0x80]
5467add 410fafc7 imul eax, r15d
5467ae1 6689410c mov word ptr [rcx + 0xc], ax
5467ae5 488b03 mov rax, qword ptr [rbx]
5467ae8 488b8060feffff mov rax, qword ptr [rax - 0x1a0]
5467aef 488d3c03 lea rdi, [rbx + rax]
5467af3 488b0403 mov rax, qword ptr [rbx + rax]
5467af7 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x55080aa unresolved-member-or-global-load
5508090 55 push rbp
5508091 4889e5 mov rbp, rsp
5508094 488b4728 mov rax, qword ptr [rdi + 0x28]
5508098 488b08 mov rcx, qword ptr [rax]
550809b 488b8960feffff mov rcx, qword ptr [rcx - 0x1a0]
55080a2 488d3c08 lea rdi, [rax + rcx]
55080a6 488b0408 mov rax, qword ptr [rax + rcx]
55080aa ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x55150ea unresolved-member-or-global-load
55150d0 55 push rbp
55150d1 4889e5 mov rbp, rsp
55150d4 488b4738 mov rax, qword ptr [rdi + 0x38]
55150d8 488b08 mov rcx, qword ptr [rax]
55150db 488b8960feffff mov rcx, qword ptr [rcx - 0x1a0]
55150e2 488d3c08 lea rdi, [rax + rcx]
55150e6 488b0408 mov rax, qword ptr [rax + rcx]
55150ea ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x6193b29 unresolved-no-aligned-local-prologue


CALL 0x6194a17 unresolved-no-aligned-local-prologue


CALL 0x6195bcb unresolved-no-aligned-local-prologue


CALL 0x6195e40 unresolved-no-aligned-local-prologue


CALL 0x61962c5 unresolved-no-aligned-local-prologue


CALL 0x6197860 unresolved-no-aligned-local-prologue


CALL 0x6198340 unresolved-no-aligned-local-prologue


CALL 0x61989b9 unresolved-no-aligned-local-prologue


CALL 0x61991d0 unresolved-no-aligned-local-prologue


CALL 0x6199530 unresolved-no-aligned-local-prologue


CALL 0x619a74a unresolved-no-aligned-local-prologue


CALL 0x619b6c9 unresolved-no-aligned-local-prologue


CALL 0x619be26 unresolved-no-aligned-local-prologue


CALL 0x619c9a0 unresolved-no-aligned-local-prologue


CALL 0x619ccf9 unresolved-no-aligned-local-prologue


CALL 0x652d6f9 unresolved-member-or-global-load
652d62f 4c89ff mov rdi, r15
652d632 c5f857c0 vxorps xmm0, xmm0, xmm0
652d636 c5e857d2 vxorps xmm2, xmm2, xmm2
652d63a 48898530feffff mov qword ptr [rbp - 0x1d0], rax
652d641 48c785f0feffff00000000 mov qword ptr [rbp - 0x110], 0
652d64c c5f81185d0feffff vmovups xmmword ptr [rbp - 0x130], xmm0
652d654 c5f8118df8feffff vmovups xmmword ptr [rbp - 0x108], xmm1
652d65c 48c78510ffffff00000000 mov qword ptr [rbp - 0xf0], 0
652d667 c78518ffffff00000000 mov dword ptr [rbp - 0xe8], 0
652d671 48c78550ffffff00000000 mov qword ptr [rbp - 0xb0], 0
652d67c c5fc119520ffffff vmovups ymmword ptr [rbp - 0xe0], ymm2
652d684 c5f8118d58ffffff vmovups xmmword ptr [rbp - 0xa8], xmm1
652d68c 48c78570ffffff00000000 mov qword ptr [rbp - 0x90], 0
652d697 c78578ffffff00000000 mov dword ptr [rbp - 0x88], 0
652d6a1 48c745a000000000 mov qword ptr [rbp - 0x60], 0
652d6a9 c5f8114580 vmovups xmmword ptr [rbp - 0x80], xmm0
652d6ae c5f8114da8 vmovups xmmword ptr [rbp - 0x58], xmm1
652d6b3 48c745c000000000 mov qword ptr [rbp - 0x40], 0
652d6bb c745c800000000 mov dword ptr [rbp - 0x38], 0
652d6c2 4c89bdc0feffff mov qword ptr [rbp - 0x140], r15
652d6c9 e8828af5f9 call 0x486150
652d6ce 488985c8feffff mov qword ptr [rbp - 0x138], rax
652d6d5 4c89f7 mov rdi, r14
652d6d8 be01000000 mov esi, 1
652d6dd 488b8530feffff mov rax, qword ptr [rbp - 0x1d0]
652d6e4 ff9008010000 call qword ptr [rax + 0x108]
652d6ea 488b8530feffff mov rax, qword ptr [rbp - 0x1d0]
652d6f1 4c89f7 mov rdi, r14
652d6f4 be01000000 mov esi, 1
652d6f9 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x7250ce5 unresolved-member-or-global-load
7250c73 4c8d4dcc lea r9, [rbp - 0x34]
7250c77 be04000000 mov esi, 4
7250c7c 4c89e7 mov rdi, r12
7250c7f 4c89f1 mov rcx, r14
7250c82 4d89ef mov r15, r13
7250c85 4531ed xor r13d, r13d
7250c88 31d2 xor edx, edx
7250c8a 4531c0 xor r8d, r8d
7250c8d e87edafaff call 0x71fe710
7250c92 837dcc00 cmp dword ptr [rbp - 0x34], 0
7250c96 0f8556020000 jne 0x7250ef2
7250c9c 0fbf5588 movsx edx, word ptr [rbp - 0x78]
7250ca0 0fb7cb movzx ecx, bx
7250ca3 4c89a568ffffff mov qword ptr [rbp - 0x98], r12
7250caa 4531e4 xor r12d, r12d
7250cad 48898570ffffff mov qword ptr [rbp - 0x90], rax
7250cb4 898d78ffffff mov dword ptr [rbp - 0x88], ecx
7250cba d1e9 shr ecx, 1
7250cbc 898d7cffffff mov dword ptr [rbp - 0x84], ecx
7250cc2 895588 mov dword ptr [rbp - 0x78], edx
7250cc5 eb0c jmp 0x7250cd3
7250cc7 49ffc4 inc r12
7250cca 4d39e6 cmp r14, r12
7250ccd 0f848c000000 je 0x7250d5f
7250cd3 488b4580 mov rax, qword ptr [rbp - 0x80]
7250cd7 498b5f40 mov rbx, qword ptr [r15 + 0x40]
7250cdb 4c89ff mov rdi, r15
7250cde 4c89e6 mov rsi, r12
7250ce1 488d5590 lea rdx, [rbp - 0x70]
7250ce5 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x725d9dd unresolved-member-or-global-load
725d966 31f6 xor esi, esi
725d968 e893aa0000 call 0x7268400
725d96d 4189c7 mov r15d, eax
725d970 85c0 test eax, eax
725d972 0f8592000000 jne 0x725da0a
725d978 488b4358 mov rax, qword ptr [rbx + 0x58]
725d97c 31d2 xor edx, edx
725d97e 0fb7bba8000000 movzx edi, word ptr [rbx + 0xa8]
725d985 be00120000 mov esi, 0x1200
725d98a 0fb708 movzx ecx, word ptr [rax]
725d98d 663b4802 cmp cx, word ptr [rax + 2]
725d991 b848000000 mov eax, 0x48
725d996 0f96c2 setbe dl
725d999 418b549618 mov edx, dword ptr [r14 + rdx*4 + 0x18]
725d99e 4885d2 test rdx, rdx
725d9a1 480f44d0 cmove rdx, rax
725d9a5 41833e04 cmp dword ptr [r14], 4
725d9a9 480f44d0 cmove rdx, rax
725d9ad e8eefef9ff call 0x71fd8a0
725d9b2 488983f0000000 mov qword ptr [rbx + 0xf0], rax
725d9b9 4531ff xor r15d, r15d
725d9bc eb4c jmp 0x725da0a
725d9be 488b75d0 mov rsi, qword ptr [rbp - 0x30]
725d9c2 488b3b mov rdi, qword ptr [rbx]
725d9c5 4889b3e8000000 mov qword ptr [rbx + 0xe8], rsi
725d9cc f6471001 test byte ptr [rdi + 0x10], 1
725d9d0 7526 jne 0x725d9f8
725d9d2 488b8770030000 mov rax, qword ptr [rdi + 0x370]
725d9d9 488d5318 lea rdx, [rbx + 0x18]
725d9dd ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x725da54 unresolved-member-or-global-load
725da30 55 push rbp
725da31 4889e5 mov rbp, rsp
725da34 53 push rbx
725da35 50 push rax
725da36 4889fb mov rbx, rdi
725da39 488b3f mov rdi, qword ptr [rdi]
725da3c 4889b3e8000000 mov qword ptr [rbx + 0xe8], rsi
725da43 f6471001 test byte ptr [rdi + 0x10], 1
725da47 7528 jne 0x725da71
725da49 488b8770030000 mov rax, qword ptr [rdi + 0x370]
725da50 488d5318 lea rdx, [rbx + 0x18]
725da54 ff90d8000000 call qword ptr [rax + 0xd8]

CALL 0x7359dc6 unresolved-member-or-global-load
7359d63 4489f8 mov eax, r15d
7359d66 4883c478 add rsp, 0x78
7359d6a 5b pop rbx
7359d6b 415c pop r12
7359d6d 415d pop r13
7359d6f 415e pop r14
7359d71 415f pop r15
7359d73 5d pop rbp
7359d74 c3 ret 
7359d75 4c89f7 mov rdi, r14
7359d78 e84323feff call 0x733c0c0
7359d7d 85c0 test eax, eax
7359d7f 0f846cffffff je 0x7359cf1
7359d85 6a01 push 1
7359d87 5e pop rsi
7359d88 4c89f7 mov rdi, r14
7359d8b e84622feff call 0x733bfd6
7359d90 85c0 test eax, eax
7359d92 0f8459ffffff je 0x7359cf1
7359d98 488d55b8 lea rdx, [rbp - 0x48]
7359d9c be00100000 mov esi, 0x1000
7359da1 4c89f7 mov rdi, r14
7359da4 e83721feff call 0x733bee0
7359da9 85c0 test eax, eax
7359dab 0f8440ffffff je 0x7359cf1
7359db1 488b83a0050000 mov rax, qword ptr [rbx + 0x5a0]
7359db8 488b75b8 mov rsi, qword ptr [rbp - 0x48]
7359dbc 488d9568ffffff lea rdx, [rbp - 0x98]
7359dc3 4889df mov rdi, rbx
7359dc6 ff90d8000000 call qword ptr [rax + 0xd8]

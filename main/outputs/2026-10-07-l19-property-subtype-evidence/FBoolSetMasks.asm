0x3fe720 (file 0x402720) 55 push rbp
0x3fe721 (file 0x402721) 4889e5 mov rbp, rsp
0x3fe724 (file 0x402724) 4157 push r15
0x3fe726 (file 0x402726) 4156 push r14
0x3fe728 (file 0x402728) 4154 push r12
0x3fe72a (file 0x40272a) 53 push rbx
0x3fe72b (file 0x40272b) 4883ec10 sub rsp, 0x10
0x3fe72f (file 0x40272f) 4c8b25d237d308 mov r12, qword ptr [rip + 0x8d337d2]
0x3fe736 (file 0x402736) 4189d7 mov r15d, edx
0x3fe739 (file 0x402739) 4189f6 mov r14d, esi
0x3fe73c (file 0x40273c) 48bafffdffbfefffffff movabs rdx, 0xffffffefbffffdff
0x3fe746 (file 0x402746) 48be0000000010000000 movabs rsi, 0x1000000000
0x3fe750 (file 0x402750) 4889fb mov rbx, rdi
0x3fe753 (file 0x402753) 498b0424 mov rax, qword ptr [r12]
0x3fe757 (file 0x402757) 488945d8 mov qword ptr [rbp - 0x28], rax
0x3fe75b (file 0x40275b) 488b4738 mov rax, qword ptr [rdi + 0x38]
0x3fe75f (file 0x40275f) 4821c2 and rdx, rax
0x3fe762 (file 0x402762) 4809f0 or rax, rsi
0x3fe765 (file 0x402765) 4809f2 or rdx, rsi
0x3fe768 (file 0x402768) 480d00020040 or rax, 0x40000200
0x3fe76e (file 0x40276e) 4585ff test r15d, r15d
0x3fe771 (file 0x402771) 4489f6 mov esi, r14d
0x3fe774 (file 0x402774) 480f44c2 cmove rax, rdx
0x3fe778 (file 0x402778) 83f901 cmp ecx, 1
0x3fe77b (file 0x40277b) 83d100 adc ecx, 0
0x3fe77e (file 0x40277e) 48894738 mov qword ptr [rdi + 0x38], rax
0x3fe782 (file 0x402782) 894dd4 mov dword ptr [rbp - 0x2c], ecx
0x3fe785 (file 0x402785) e8d635feff call 0x3e1d60
0x3fe78a (file 0x40278a) 0fb64330 movzx eax, byte ptr [rbx + 0x30]
0x3fe78e (file 0x40278e) 884370 mov byte ptr [rbx + 0x70], al
0x3fe791 (file 0x402791) c6437100 mov byte ptr [rbx + 0x71], 0
0x3fe795 (file 0x402795) 4585ff test r15d, r15d
0x3fe798 (file 0x402798) 7408 je 0x3fe7a2
0x3fe79a (file 0x40279a) c6437201 mov byte ptr [rbx + 0x72], 1
0x3fe79e (file 0x40279e) b1ff mov cl, 0xff
0x3fe7a0 (file 0x4027a0) eb3a jmp 0x3fe7dc
0x3fe7a2 (file 0x4027a2) 4585f6 test r14d, r14d
0x3fe7a5 (file 0x4027a5) 7427 je 0x3fe7ce
0x3fe7a7 (file 0x4027a7) 31c0 xor eax, eax
0x3fe7a9 (file 0x4027a9) 0f1f8000000000 nop dword ptr [rax]
0x3fe7b0 (file 0x4027b0) 0fb6c8 movzx ecx, al
0x3fe7b3 (file 0x4027b3) 0fb64c0dd4 movzx ecx, byte ptr [rbp + rcx - 0x2c]
0x3fe7b8 (file 0x4027b8) 84c9 test cl, cl
0x3fe7ba (file 0x4027ba) 751a jne 0x3fe7d6
0x3fe7bc (file 0x4027bc) fec0 inc al
0x3fe7be (file 0x4027be) 0fb6c8 movzx ecx, al
0x3fe7c1 (file 0x4027c1) 4439f1 cmp ecx, r14d
0x3fe7c4 (file 0x4027c4) 72ea jb 0x3fe7b0
0x3fe7c6 (file 0x4027c6) c6437200 mov byte ptr [rbx + 0x72], 0
0x3fe7ca (file 0x4027ca) 31c9 xor ecx, ecx
0x3fe7cc (file 0x4027cc) eb0b jmp 0x3fe7d9
0x3fe7ce (file 0x4027ce) 0fb64b72 movzx ecx, byte ptr [rbx + 0x72]
0x3fe7d2 (file 0x4027d2) 31c0 xor eax, eax
0x3fe7d4 (file 0x4027d4) eb03 jmp 0x3fe7d9
0x3fe7d6 (file 0x4027d6) 884b72 mov byte ptr [rbx + 0x72], cl
0x3fe7d9 (file 0x4027d9) 884371 mov byte ptr [rbx + 0x71], al
0x3fe7dc (file 0x4027dc) 884b73 mov byte ptr [rbx + 0x73], cl
0x3fe7df (file 0x4027df) 498b0424 mov rax, qword ptr [r12]

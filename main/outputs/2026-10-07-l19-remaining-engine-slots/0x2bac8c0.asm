0x2bac8c0 98 cwde 
0x2bac8c1 050000440f add eax, 0xf440000
0x2bac8c6 bf88a80500 mov edi, 0x5a888
0x2bac8cb 00440fbf add byte ptr [rdi + rcx - 0x41], al
0x2bac8cf 90 nop 
0x2bac8d0 880500004c8d mov byte ptr [rip - 0x72b40000], al
0x2bac8d6 6d insd dword ptr [rdi], dx
0x2bac8d7 c0488d35 ror byte ptr [rax - 0x73], 0x35
0x2bac8db 71c3 jno 0x2bac8a0
0x2bac8dd c8044489 enter 0x4404, -0x77
0x2bac8e1 e1b0 loope 0x2bac893
0x2bac8e3 014c89ef add dword ptr [rcx + rcx*4 - 0x11], ecx
0x2bac8e7 c5fa5ac0 vcvtss2sd xmm0, xmm0, xmm0
0x2bac8eb 4153 push r11
0x2bac8ed 4152 push r10
0x2bac8ef e88cf14bfd call 0x6ba80
0x2bac8f4 4883c410 add rsp, 0x10
0x2bac8f8 4c89f7 mov rdi, r14
0x2bac8fb 4c89ee mov rsi, r13
0x2bac8fe 4889da mov rdx, rbx
0x2bac901 e86aa2b800 call 0x3736b70
0x2bac906 4c89ff mov rdi, r15
0x2bac909 e8d2612b00 call 0x2e62ae0
0x2bac90e 84c0 test al, al
0x2bac910 7465 je 0x2bac977
0x2bac912 498b87f0080000 mov rax, qword ptr [r15 + 0x8f0]
0x2bac919 4885c0 test rax, rax
0x2bac91c 7459 je 0x2bac977
0x2bac91e 440fb64802 movzx r9d, byte ptr [rax + 2]
0x2bac923 8b5004 mov edx, dword ptr [rax + 4]
0x2bac926 b803010000 mov eax, 0x103
0x2bac92b 488d3502bbae04 lea rsi, [rip + 0x4aebb02]
0x2bac932 488d7db0 lea rdi, [rbp - 0x50]
0x2bac936 c4c278f7c9 bextr ecx, r9d, eax
0x2bac93b b804010000 mov eax, 0x104
0x2bac940 c44278f7c1 bextr r8d, r9d, eax
0x2bac945 4183e101 and r9d, 1
0x2bac949 31c0 xor eax, eax
0x2bac94b e830f14bfd call 0x6ba80
0x2bac950 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0x2bac954 4885ff test rdi, rdi
0x2bac957 7405 je 0x2bac95e
0x2bac959 e832b150fd call 0xb7a90

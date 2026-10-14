0x3ea490 (file 0x3ee490) 55 push rbp
0x3ea491 (file 0x3ee491) 4889e5 mov rbp, rsp
0x3ea494 (file 0x3ee494) 4157 push r15
0x3ea496 (file 0x3ee496) 4156 push r14
0x3ea498 (file 0x3ee498) 4155 push r13
0x3ea49a (file 0x3ee49a) 4154 push r12
0x3ea49c (file 0x3ee49c) 53 push rbx
0x3ea49d (file 0x3ee49d) 4883ec38 sub rsp, 0x38
0x3ea4a1 (file 0x3ee4a1) 488b05607ad408 mov rax, qword ptr [rip + 0x8d47a60]
0x3ea4a8 (file 0x3ee4a8) 4889fb mov rbx, rdi
0x3ea4ab (file 0x3ee4ab) 4989d4 mov r12, rdx
0x3ea4ae (file 0x3ee4ae) 4989f6 mov r14, rsi
0x3ea4b1 (file 0x3ee4b1) 488b00 mov rax, qword ptr [rax]
0x3ea4b4 (file 0x3ee4b4) 488945d0 mov qword ptr [rbp - 0x30], rax
0x3ea4b8 (file 0x3ee4b8) 488b7f78 mov rdi, qword ptr [rdi + 0x78]
0x3ea4bc (file 0x3ee4bc) 488b07 mov rax, qword ptr [rdi]
0x3ea4bf (file 0x3ee4bf) ff9070010000 call qword ptr [rax + 0x170]
0x3ea4c5 (file 0x3ee4c5) 4c8b7b78 mov r15, qword ptr [rbx + 0x78]
0x3ea4c9 (file 0x3ee4c9) 0fb64370 movzx eax, byte ptr [rbx + 0x70]
0x3ea4cd (file 0x3ee4cd) 8845af mov byte ptr [rbp - 0x51], al
0x3ea4d0 (file 0x3ee4d0) 498b07 mov rax, qword ptr [r15]
0x3ea4d3 (file 0x3ee4d3) 458b6f30 mov r13d, dword ptr [r15 + 0x30]
0x3ea4d7 (file 0x3ee4d7) 4c89ff mov rdi, r15
0x3ea4da (file 0x3ee4da) ff9070010000 call qword ptr [rax + 0x170]
0x3ea4e0 (file 0x3ee4e0) 0fb64b70 movzx ecx, byte ptr [rbx + 0x70]
0x3ea4e4 (file 0x3ee4e4) 4c897db0 mov qword ptr [rbp - 0x50], r15
0x3ea4e8 (file 0x3ee4e8) 44896dc0 mov dword ptr [rbp - 0x40], r13d
0x3ea4ec (file 0x3ee4ec) 8945c4 mov dword ptr [rbp - 0x3c], eax
0x3ea4ef (file 0x3ee4ef) 488d7db0 lea rdi, [rbp - 0x50]
0x3ea4f3 (file 0x3ee4f3) 884dc8 mov byte ptr [rbp - 0x38], cl
0x3ea4f6 (file 0x3ee4f6) 4c8975b8 mov qword ptr [rbp - 0x48], r14
0x3ea4fa (file 0x3ee4fa) 488b4378 mov rax, qword ptr [rbx + 0x78]
0x3ea4fe (file 0x3ee4fe) 458b742408 mov r14d, dword ptr [r12 + 8]
0x3ea503 (file 0x3ee503) 4c8b7838 mov r15, qword ptr [rax + 0x38]
0x3ea507 (file 0x3ee507) 4489f6 mov esi, r14d
0x3ea50a (file 0x3ee50a) e8518bfeff call 0x3d3060
0x3ea50f (file 0x3ee50f) 41f7c700000040 test r15d, 0x40000000
0x3ea516 (file 0x3ee516) 753a jne 0x3ea552
0x3ea518 (file 0x3ee518) 488d7db0 lea rdi, [rbp - 0x50]
0x3ea51c (file 0x3ee51c) 4489f6 mov esi, r14d
0x3ea51f (file 0x3ee51f) e80c8cfeff call 0x3d3130
0x3ea524 (file 0x3ee524) 4d85f6 test r14, r14
0x3ea527 (file 0x3ee527) 0f8446010000 je 0x3ea673
0x3ea52d (file 0x3ee52d) 41837c240800 cmp dword ptr [r12 + 8], 0
0x3ea533 (file 0x3ee533) 7453 je 0x3ea588
0x3ea535 (file 0x3ee535) f645af01 test byte ptr [rbp - 0x51], 1
0x3ea539 (file 0x3ee539) 757a jne 0x3ea5b5
0x3ea53b (file 0x3ee53b) 4d8b3c24 mov r15, qword ptr [r12]
0x3ea53f (file 0x3ee53f) 488b4db8 mov rcx, qword ptr [rbp - 0x48]
0x3ea543 (file 0x3ee543) 488b4378 mov rax, qword ptr [rbx + 0x78]
0x3ea547 (file 0x3ee547) 83790800 cmp dword ptr [rcx + 8], 0
0x3ea54b (file 0x3ee54b) 754c jne 0x3ea599
0x3ea54d (file 0x3ee54d) e985000000 jmp 0x3ea5d7
0x3ea552 (file 0x3ee552) 488b7db8 mov rdi, qword ptr [rbp - 0x48]
0x3ea556 (file 0x3ee556) f645c801 test byte ptr [rbp - 0x38], 1
0x3ea55a (file 0x3ee55a) 8b55c0 mov edx, dword ptr [rbp - 0x40]
0x3ea55d (file 0x3ee55d) 8b4dc4 mov ecx, dword ptr [rbp - 0x3c]
0x3ea560 (file 0x3ee560) 8b7708 mov esi, dword ptr [rdi + 8]
0x3ea563 (file 0x3ee563) 8b470c mov eax, dword ptr [rdi + 0xc]
0x3ea566 (file 0x3ee566) 468d0436 lea r8d, [rsi + r14]
0x3ea56a (file 0x3ee56a) 44894708 mov dword ptr [rdi + 8], r8d
0x3ea56e (file 0x3ee56e) 750c jne 0x3ea57c
0x3ea570 (file 0x3ee570) 4139c0 cmp r8d, eax
0x3ea573 (file 0x3ee573) 7eaf jle 0x3ea524
0x3ea575 (file 0x3ee575) e8864df4ff call 0x32f300
0x3ea57a (file 0x3ee57a) eba8 jmp 0x3ea524
0x3ea57c (file 0x3ee57c) 4139c0 cmp r8d, eax
0x3ea57f (file 0x3ee57f) 7ea3 jle 0x3ea524
0x3ea581 (file 0x3ee581) e8da8dfeff call 0x3d3360
0x3ea586 (file 0x3ee586) eb9c jmp 0x3ea524
0x3ea588 (file 0x3ee588) 4531ff xor r15d, r15d
0x3ea58b (file 0x3ee58b) 488b4db8 mov rcx, qword ptr [rbp - 0x48]

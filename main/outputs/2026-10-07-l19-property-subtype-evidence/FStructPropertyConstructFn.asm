0x4366e0 (file 0x43a6e0) 55 push rbp
0x4366e1 (file 0x43a6e1) 4889e5 mov rbp, rsp
0x4366e4 (file 0x43a6e4) 4157 push r15
0x4366e6 (file 0x43a6e6) 4156 push r14
0x4366e8 (file 0x43a6e8) 4155 push r13
0x4366ea (file 0x43a6ea) 4154 push r12
0x4366ec (file 0x43a6ec) 53 push rbx
0x4366ed (file 0x43a6ed) 4883ec18 sub rsp, 0x18
0x4366f1 (file 0x43a6f1) 488b0510b8cf08 mov rax, qword ptr [rip + 0x8cfb810]
0x4366f8 (file 0x43a6f8) 8955c4 mov dword ptr [rbp - 0x3c], edx
0x4366fb (file 0x43a6fb) 4989fc mov r12, rdi
0x4366fe (file 0x43a6fe) bf78000000 mov edi, 0x78
0x436703 (file 0x43a703) 4989f7 mov r15, rsi
0x436706 (file 0x43a706) 31f6 xor esi, esi
0x436708 (file 0x43a708) 488b00 mov rax, qword ptr [rax]
0x43670b (file 0x43a70b) 488945d0 mov qword ptr [rbp - 0x30], rax
0x43670f (file 0x43a70f) e80c13c8ff call 0xb7a20
0x436714 (file 0x43a714) 0fb60d8d1cf708 movzx ecx, byte ptr [rip + 0x8f71c8d]
0x43671b (file 0x43a71b) 4889c3 mov rbx, rax
0x43671e (file 0x43a71e) 84c9 test cl, cl
0x436720 (file 0x43a720) 0f84b2000000 je 0x4367d8
0x436726 (file 0x43a726) 488d153b1cf708 lea rdx, [rip + 0x8f71c3b]
0x43672d (file 0x43a72d) 4889df mov rdi, rbx
0x436730 (file 0x43a730) 31f6 xor esi, esi
0x436732 (file 0x43a732) e889def2ff call 0x3645c0
0x436737 (file 0x43a737) 4c8d2d6a66ac07 lea r13, [rip + 0x7ac666a]
0x43673e (file 0x43a73e) c5f857c0 vxorps xmm0, xmm0, xmm0
0x436742 (file 0x43a742) c5f8114368 vmovups xmmword ptr [rbx + 0x68], xmm0
0x436747 (file 0x43a747) 4c892b mov qword ptr [rbx], r13
0x43674a (file 0x43a74a) 4d8b2424 mov r12, qword ptr [r12]
0x43674e (file 0x43a74e) 41f6c401 test r12b, 1
0x436752 (file 0x43a752) 7437 je 0x43678b
0x436754 (file 0x43a754) 4d89e6 mov r14, r12
0x436757 (file 0x43a757) 4983e6fe and r14, 0xfffffffffffffffe
0x43675b (file 0x43a75b) 742e je 0x43678b
0x43675d (file 0x43a75d) 488d05f886f608 lea rax, [rip + 0x8f686f8]
0x436764 (file 0x43a764) 803801 cmp byte ptr [rax], 1
0x436767 (file 0x43a767) 750f jne 0x436778
0x436769 (file 0x43a769) 4c89f7 mov rdi, r14
0x43676c (file 0x43a76c) e84f1bf3ff call 0x3682c0
0x436771 (file 0x43a771) 488d05e486f608 lea rax, [rip + 0x8f686e4]
0x436778 (file 0x43a778) 803801 cmp byte ptr [rax], 1
0x43677b (file 0x43a77b) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x43677f (file 0x43a77f) 750e jne 0x43678f
0x436781 (file 0x43a781) 4c89f7 mov rdi, r14
0x436784 (file 0x43a784) e8371bf3ff call 0x3682c0
0x436789 (file 0x43a789) eb04 jmp 0x43678f
0x43678b (file 0x43a78b) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x43678f (file 0x43a78f) 8b4dc4 mov ecx, dword ptr [rbp - 0x3c]
0x436792 (file 0x43a792) 488d75c8 lea rsi, [rbp - 0x38]
0x436796 (file 0x43a796) 4889df mov rdi, rbx
0x436799 (file 0x43a799) 4c89fa mov rdx, r15
0x43679c (file 0x43a79c) e8cfb5faff call 0x3e1d70
0x4367a1 (file 0x43a7a1) 4889df mov rdi, rbx
0x4367a4 (file 0x43a7a4) 4c892b mov qword ptr [rbx], r13
0x4367a7 (file 0x43a7a7) 31f6 xor esi, esi
0x4367a9 (file 0x43a7a9) 48c7437000000000 mov qword ptr [rbx + 0x70], 0
0x4367b1 (file 0x43a7b1) e8aab5faff call 0x3e1d60
0x4367b6 (file 0x43a7b6) 488b054bb7cf08 mov rax, qword ptr [rip + 0x8cfb74b]
0x4367bd (file 0x43a7bd) 488b00 mov rax, qword ptr [rax]
0x4367c0 (file 0x43a7c0) 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x4367c4 (file 0x43a7c4) 757c jne 0x436842
0x4367c6 (file 0x43a7c6) 4889d8 mov rax, rbx
0x4367c9 (file 0x43a7c9) 4883c418 add rsp, 0x18
0x4367cd (file 0x43a7cd) 5b pop rbx
0x4367ce (file 0x43a7ce) 415c pop r12
0x4367d0 (file 0x43a7d0) 415d pop r13
0x4367d2 (file 0x43a7d2) 415e pop r14
0x4367d4 (file 0x43a7d4) 415f pop r15
0x4367d6 (file 0x43a7d6) 5d pop rbp
0x4367d7 (file 0x43a7d7) c3 ret 

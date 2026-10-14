0x3ffdf0 (file 0x403df0) 55 push rbp
0x3ffdf1 (file 0x403df1) 4889e5 mov rbp, rsp
0x3ffdf4 (file 0x403df4) 4157 push r15
0x3ffdf6 (file 0x403df6) 4156 push r14
0x3ffdf8 (file 0x403df8) 4154 push r12
0x3ffdfa (file 0x403dfa) 53 push rbx
0x3ffdfb (file 0x403dfb) 4883ec10 sub rsp, 0x10
0x3ffdff (file 0x403dff) 4c8b3d0221d308 mov r15, qword ptr [rip + 0x8d32102]
0x3ffe06 (file 0x403e06) 4889cb mov rbx, rcx
0x3ffe09 (file 0x403e09) 4889f0 mov rax, rsi
0x3ffe0c (file 0x403e0c) 4989fe mov r14, rdi
0x3ffe0f (file 0x403e0f) ba01000000 mov edx, 1
0x3ffe14 (file 0x403e14) 498b0f mov rcx, qword ptr [r15]
0x3ffe17 (file 0x403e17) 48894dd8 mov qword ptr [rbp - 0x28], rcx
0x3ffe1b (file 0x403e1b) 440fb66771 movzx r12d, byte ptr [rdi + 0x71]
0x3ffe20 (file 0x403e20) 0fb64f73 movzx ecx, byte ptr [rdi + 0x73]
0x3ffe24 (file 0x403e24) 4889c7 mov rdi, rax
0x3ffe27 (file 0x403e27) 42840c23 test byte ptr [rbx + r12], cl
0x3ffe2b (file 0x403e2b) 0f9545d7 setne byte ptr [rbp - 0x29]
0x3ffe2f (file 0x403e2f) 488b0e mov rcx, qword ptr [rsi]
0x3ffe32 (file 0x403e32) 488d75d7 lea rsi, [rbp - 0x29]
0x3ffe36 (file 0x403e36) ff9190010000 call qword ptr [rcx + 0x190]
0x3ffe3c (file 0x403e3c) 807dd700 cmp byte ptr [rbp - 0x29], 0
0x3ffe40 (file 0x403e40) 420fb60423 movzx eax, byte ptr [rbx + r12]
0x3ffe45 (file 0x403e45) 410fb64e73 movzx ecx, byte ptr [r14 + 0x73]
0x3ffe4a (file 0x403e4a) 7407 je 0x3ffe53
0x3ffe4c (file 0x403e4c) 410fb65672 movzx edx, byte ptr [r14 + 0x72]
0x3ffe51 (file 0x403e51) eb02 jmp 0x3ffe55
0x3ffe53 (file 0x403e53) 31d2 xor edx, edx
0x3ffe55 (file 0x403e55) f6d1 not cl
0x3ffe57 (file 0x403e57) 20c8 and al, cl
0x3ffe59 (file 0x403e59) 08d0 or al, dl
0x3ffe5b (file 0x403e5b) 42880423 mov byte ptr [rbx + r12], al
0x3ffe5f (file 0x403e5f) 498b07 mov rax, qword ptr [r15]
0x3ffe62 (file 0x403e62) 483b45d8 cmp rax, qword ptr [rbp - 0x28]
0x3ffe66 (file 0x403e66) 750f jne 0x3ffe77
0x3ffe68 (file 0x403e68) b001 mov al, 1
0x3ffe6a (file 0x403e6a) 4883c410 add rsp, 0x10
0x3ffe6e (file 0x403e6e) 5b pop rbx
0x3ffe6f (file 0x403e6f) 415c pop r12

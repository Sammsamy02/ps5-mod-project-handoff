0x361170 (file 0x365170) 55 push rbp
0x361171 (file 0x365171) 4889e5 mov rbp, rsp
0x361174 (file 0x365174) 4157 push r15
0x361176 (file 0x365176) 4156 push r14
0x361178 (file 0x365178) 4154 push r12
0x36117a (file 0x36517a) 53 push rbx
0x36117b (file 0x36517b) 4883ec20 sub rsp, 0x20
0x36117f (file 0x36517f) 4c8b25820ddd08 mov r12, qword ptr [rip + 0x8dd0d82]
0x361186 (file 0x365186) 4889f3 mov rbx, rsi
0x361189 (file 0x365189) 4989fe mov r14, rdi
0x36118c (file 0x36518c) 498b0424 mov rax, qword ptr [r12]
0x361190 (file 0x365190) 488945d8 mov qword ptr [rbp - 0x28], rax
0x361194 (file 0x365194) e897100800 call 0x3e2230
0x361199 (file 0x365199) 488b03 mov rax, qword ptr [rbx]
0x36119c (file 0x36519c) 498d7678 lea rsi, [r14 + 0x78]
0x3611a0 (file 0x3651a0) 4889df mov rdi, rbx
0x3611a3 (file 0x3651a3) ff9060010000 call qword ptr [rax + 0x160]
0x3611a9 (file 0x3651a9) 498b7678 mov rsi, qword ptr [r14 + 0x78]
0x3611ad (file 0x3651ad) 4885f6 test rsi, rsi
0x3611b0 (file 0x3651b0) 740c je 0x3611be
0x3611b2 (file 0x3651b2) 488b03 mov rax, qword ptr [rbx]
0x3611b5 (file 0x3651b5) 4889df mov rdi, rbx
0x3611b8 (file 0x3651b8) ff90b0010000 call qword ptr [rax + 0x1b0]
0x3611be (file 0x3651be) 4c8975c0 mov qword ptr [rbp - 0x40], r14
0x3611c2 (file 0x3651c2) f6432801 test byte ptr [rbx + 0x28], 1
0x3611c6 (file 0x3651c6) 7512 jne 0x3611da
0x3611c8 (file 0x3651c8) 498b4670 mov rax, qword ptr [r14 + 0x70]
0x3611cc (file 0x3651cc) 4885c0 test rax, rax
0x3611cf (file 0x3651cf) 746e je 0x36123f
0x3611d1 (file 0x3651d1) 488b4008 mov rax, qword ptr [rax + 8]
0x3611d5 (file 0x3651d5) 488b00 mov rax, qword ptr [rax]
0x3611d8 (file 0x3651d8) eb67 jmp 0x361241

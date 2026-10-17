2fac8c0 55 push rbp
2fac8c1 4889e5 mov rbp, rsp
2fac8c4 4157 push r15
2fac8c6 4156 push r14
2fac8c8 4155 push r13
2fac8ca 4154 push r12
2fac8cc 53 push rbx
2fac8cd 4881ec88000000 sub rsp, 0x88
2fac8d4 4c8b2d2d561806 mov r13, qword ptr [rip + 0x618562d]
2fac8db c5f057c9 vxorps xmm1, xmm1, xmm1
2fac8df c5f82ec8 vucomiss xmm1, xmm0
2fac8e3 4189f7 mov r15d, esi
2fac8e6 4989fe mov r14, rdi
2fac8e9 498b4500 mov rax, qword ptr [r13]
2fac8ed 488945d0 mov qword ptr [rbp - 0x30], rax
2fac8f1 c5fa114584 vmovss dword ptr [rbp - 0x7c], xmm0
2fac8f6 0f870f080000 ja 0x2fad10b
2fac8fc 4c8d25f6c03a06 lea r12, [rip + 0x63ac0f6]
2fac903 41803c2400 cmp byte ptr [r12], 0
2fac908 7543 jne 0x2fac94d
2fac90a 0fb605b70d4106 movzx eax, byte ptr [rip + 0x6410db7]
2fac911 84c0 test al, al
2fac913 0f843f090000 je 0x2fad258
2fac919 41803c2401 cmp byte ptr [r12], 1
2fac91e 7517 jne 0x2fac937
2fac920 488d05e1c03a06 lea rax, [rip + 0x63ac0e1]
2fac927 803801 cmp byte ptr [rax], 1
2fac92a 7521 jne 0x2fac94d
2fac92c f6058d0d410601 test byte ptr [rip + 0x6410d8d], 1
2fac933 740b je 0x2fac940
2fac935 eb16 jmp 0x2fac94d
2fac937 803d820d410600 cmp byte ptr [rip + 0x6410d82], 0
2fac93e 750d jne 0x2fac94d
2fac940 4c89f7 mov rdi, r14
2fac943 e8a8708900 call 0x38439f0
2fac948 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
2fac94d 488d05b6c03a06 lea rax, [rip + 0x63ac0b6]
2fac954 803801 cmp byte ptr [rax], 1
2fac957 750e jne 0x2fac967

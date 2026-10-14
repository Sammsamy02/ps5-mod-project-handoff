0x764310  (f 0x364310)  55                     push rbp
0x764311  (f 0x364311)  4889e5                 mov rbp, rsp
0x764314  (f 0x364314)  4157                   push r15
0x764316  (f 0x364316)  4156                   push r14
0x764318  (f 0x364318)  53                     push rbx
0x764319  (f 0x364319)  4883ec18               sub rsp, 0x18
0x76431d  (f 0x36431d)  4c8b3de4dbdc08         mov r15, qword ptr [rip + 0x8dcdbe4]  ; rip->0x9531f08 (file 0x9131f08)
0x764324  (f 0x364324)  4889fb                 mov rbx, rdi
0x764327  (f 0x364327)  498b07                 mov rax, qword ptr [r15]
0x76432a  (f 0x36432a)  488945e0               mov qword ptr [rbp - 0x20], rax
0x76432e  (f 0x36432e)  4c8b36                 mov r14, qword ptr [rsi]
0x764331  (f 0x364331)  41f6c601               test r14b, 1
0x764335  (f 0x364335)  752d                   jne 0x764364
0x764337  (f 0x364337)  488d354daf8707         lea rsi, [rip + 0x787af4d]  ; rip->0x7fdf28b (file 0x7bdf28b)
0x76433e  (f 0x36433e)  4c89f7                 mov rdi, r14
0x764341  (f 0x364341)  e8ca96e4ff             call 0x5ada10
0x764346  (f 0x364346)  84c0                   test al, al
0x764348  (f 0x364348)  7449                   je 0x764393
0x76434a  (f 0x36434a)  4983c620               add r14, 0x20
0x76434e  (f 0x36434e)  4889df                 mov rdi, rbx
0x764351  (f 0x364351)  4c89f6                 mov rsi, r14
0x764354  (f 0x364354)  e867adedff             call 0x63f0c0
0x764359  (f 0x364359)  498b07                 mov rax, qword ptr [r15]
0x76435c  (f 0x36435c)  483b45e0               cmp rax, qword ptr [rbp - 0x20]
0x764360  (f 0x364360)  7423                   je 0x764385
0x764362  (f 0x364362)  eb47                   jmp 0x7643ab
0x764364  (f 0x364364)  4983e6fe               and r14, 0xfffffffffffffffe
0x764368  (f 0x364368)  488d75d8               lea rsi, [rbp - 0x28]
0x76436c  (f 0x36436c)  4889df                 mov rdi, rbx
0x76436f  (f 0x36436f)  498b4618               mov rax, qword ptr [r14 + 0x18]
0x764373  (f 0x364373)  488945d8               mov qword ptr [rbp - 0x28], rax
0x764377  (f 0x364377)  e844adedff             call 0x63f0c0
0x76437c  (f 0x36437c)  498b07                 mov rax, qword ptr [r15]
0x76437f  (f 0x36437f)  483b45e0               cmp rax, qword ptr [rbp - 0x20]
0x764383  (f 0x364383)  7526                   jne 0x7643ab
0x764385  (f 0x364385)  4889d8                 mov rax, rbx
0x764388  (f 0x364388)  4883c418               add rsp, 0x18
0x76438c  (f 0x36438c)  5b                     pop rbx
0x76438d  (f 0x36438d)  415e                   pop r14
0x76438f  (f 0x36438f)  415f                   pop r15
0x764391  (f 0x364391)  5d                     pop rbp
0x764392  (f 0x364392)  c3                     ret 
0x764393  (f 0x364393)  488d35843f5107         lea rsi, [rip + 0x7513f84]  ; rip->0x7c7831e (file 0x787831e)
0x76439a  (f 0x36439a)  4889df                 mov rdi, rbx
0x76439d  (f 0x36439d)  e84efbcfff             call 0x463ef0
0x7643a2  (f 0x3643a2)  498b07                 mov rax, qword ptr [r15]
0x7643a5  (f 0x3643a5)  483b45e0               cmp rax, qword ptr [rbp - 0x20]
0x7643a9  (f 0x3643a9)  74da                   je 0x764385
0x7643ab  (f 0x3643ab)  e8b0da1007             call 0x7871e60
0x7643b0  (f 0x3643b0)  0f0b                   ud2 
--- calls: [('0xb64341', '0x5ada10'), ('0xb64354', '0x63f0c0'), ('0xb643ab', '0x7871e60'), ('0xb6439d', '0x463ef0'), ('0xb64377', '0x63f0c0')]

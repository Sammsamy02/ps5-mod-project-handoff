0x764010  (f 0x364010)  55                     push rbp
0x764011  (f 0x364011)  4889e5                 mov rbp, rsp
0x764014  (f 0x364014)  4157                   push r15
0x764016  (f 0x364016)  4156                   push r14
0x764018  (f 0x364018)  4155                   push r13
0x76401a  (f 0x36401a)  4154                   push r12
0x76401c  (f 0x36401c)  53                     push rbx
0x76401d  (f 0x36401d)  4881ec38020000         sub rsp, 0x238
0x764024  (f 0x364024)  488b05dddedc08         mov rax, qword ptr [rip + 0x8dcdedd]  ; rip->0x9531f08 (file 0x9131f08)
0x76402b  (f 0x36402b)  4989f6                 mov r14, rsi
0x76402e  (f 0x36402e)  4889fb                 mov rbx, rdi
0x764031  (f 0x364031)  488b00                 mov rax, qword ptr [rax]
0x764034  (f 0x364034)  488945d0               mov qword ptr [rbp - 0x30], rax
0x764038  (f 0x364038)  488b7608               mov rsi, qword ptr [rsi + 8]
0x76403c  (f 0x36403c)  e87fb0edff             call 0x63f0c0
0x764041  (f 0x364041)  488d35d2a16007         lea rsi, [rip + 0x760a1d2]  ; rip->0x7d6e21a (file 0x796e21a)
0x764048  (f 0x364048)  4889df                 mov rdi, rbx
0x76404b  (f 0x36404b)  ba01000000             mov edx, 1
0x764050  (f 0x364050)  e82b0bd0ff             call 0x464b80
0x764055  (f 0x364055)  4c8dbdb0fdffff         lea r15, [rbp - 0x250]
0x76405c  (f 0x36405c)  488d85cafdffff         lea rax, [rbp - 0x236]
0x764063  (f 0x364063)  488d4dca               lea rcx, [rbp - 0x36]
0x764067  (f 0x364067)  4c89f7                 mov rdi, r14
0x76406a  (f 0x36406a)  c685c8fdffff00         mov byte ptr [rbp - 0x238], 0
0x764071  (f 0x364071)  4531ed                 xor r13d, r13d
0x764074  (f 0x364074)  31f6                   xor esi, esi
0x764076  (f 0x364076)  4c89fa                 mov rdx, r15
0x764079  (f 0x364079)  488985b0fdffff         mov qword ptr [rbp - 0x250], rax
0x764080  (f 0x364080)  488985b8fdffff         mov qword ptr [rbp - 0x248], rax
0x764087  (f 0x364087)  48898dc0fdffff         mov qword ptr [rbp - 0x240], rcx
0x76408e  (f 0x36408e)  e85d080000             call 0x7648f0
0x764093  (f 0x364093)  488bb5b0fdffff         mov rsi, qword ptr [rbp - 0x250]
0x76409a  (f 0x36409a)  488b95b8fdffff         mov rdx, qword ptr [rbp - 0x248]
0x7640a1  (f 0x3640a1)  488dbda0fdffff         lea rdi, [rbp - 0x260]
0x7640a8  (f 0x3640a8)  4829f2                 sub rdx, rsi
0x7640ab  (f 0x3640ab)  48d1ea                 shr rdx, 1
0x7640ae  (f 0x3640ae)  e85d03d0ff             call 0x464410
0x7640b3  (f 0x3640b3)  448ba5a8fdffff         mov r12d, dword ptr [rbp - 0x258]
0x7640ba  (f 0x3640ba)  4c8bb5a0fdffff         mov r14, qword ptr [rbp - 0x260]
0x7640c1  (f 0x3640c1)  4c89ff                 mov rdi, r15
0x7640c4  (f 0x3640c4)  e81700eaff             call 0x6040e0
0x7640c9  (f 0x3640c9)  4183ec01               sub r12d, 1
0x7640cd  (f 0x3640cd)  4889df                 mov rdi, rbx
0x7640d0  (f 0x3640d0)  4c89f6                 mov rsi, r14
0x7640d3  (f 0x3640d3)  450f42e5               cmovb r12d, r13d
0x7640d7  (f 0x3640d7)  4489e2                 mov edx, r12d
0x7640da  (f 0x3640da)  e8a10ad0ff             call 0x464b80
0x7640df  (f 0x3640df)  4d85f6                 test r14, r14
0x7640e2  (f 0x3640e2)  7408                   je 0x7640ec
0x7640e4  (f 0x3640e4)  4c89f7                 mov rdi, r14
0x7640e7  (f 0x3640e7)  e8a439d5ff             call 0x4b7a90
0x7640ec  (f 0x3640ec)  488b0515dedc08         mov rax, qword ptr [rip + 0x8dcde15]  ; rip->0x9531f08 (file 0x9131f08)
0x7640f3  (f 0x3640f3)  488b00                 mov rax, qword ptr [rax]
0x7640f6  (f 0x3640f6)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x7640fa  (f 0x3640fa)  7515                   jne 0x764111
0x7640fc  (f 0x3640fc)  4889d8                 mov rax, rbx
0x7640ff  (f 0x3640ff)  4881c438020000         add rsp, 0x238
0x764106  (f 0x364106)  5b                     pop rbx
0x764107  (f 0x364107)  415c                   pop r12
0x764109  (f 0x364109)  415d                   pop r13
0x76410b  (f 0x36410b)  415e                   pop r14
0x76410d  (f 0x36410d)  415f                   pop r15
0x76410f  (f 0x36410f)  5d                     pop rbp
0x764110  (f 0x364110)  c3                     ret 
0x764111  (f 0x364111)  e84add1007             call 0x7871e60
0x764116  (f 0x364116)  0f0b                   ud2 
--- calls: [('0xb6403c', '0x63f0c0'), ('0xb64050', '0x464b80'), ('0xb6408e', '0x7648f0'), ('0xb640ae', '0x464410'), ('0xb640c4', '0x6040e0'), ('0xb640da', '0x464b80'), ('0xb640e7', '0x4b7a90'), ('0xb64111', '0x7871e60')]

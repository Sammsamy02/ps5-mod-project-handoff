0x764120  (f 0x364120)  55                     push rbp
0x764121  (f 0x364121)  4889e5                 mov rbp, rsp
0x764124  (f 0x364124)  4157                   push r15
0x764126  (f 0x364126)  4156                   push r14
0x764128  (f 0x364128)  53                     push rbx
0x764129  (f 0x364129)  4881ec38020000         sub rsp, 0x238
0x764130  (f 0x364130)  4c8b3dd1dddc08         mov r15, qword ptr [rip + 0x8dcddd1]  ; rip->0x9531f08 (file 0x9131f08)
0x764137  (f 0x364137)  4889fb                 mov rbx, rdi
0x76413a  (f 0x36413a)  498b07                 mov rax, qword ptr [r15]
0x76413d  (f 0x36413d)  488945e0               mov qword ptr [rbp - 0x20], rax
0x764141  (f 0x364141)  488b36                 mov rsi, qword ptr [rsi]
0x764144  (f 0x364144)  40f6c601               test sil, 1
0x764148  (f 0x364148)  757f                   jne 0x7641c9
0x76414a  (f 0x36414a)  4889f7                 mov rdi, rsi
0x76414d  (f 0x36414d)  488d85dafdffff         lea rax, [rbp - 0x226]
0x764154  (f 0x364154)  488d4dda               lea rcx, [rbp - 0x26]
0x764158  (f 0x364158)  488d95c0fdffff         lea rdx, [rbp - 0x240]
0x76415f  (f 0x36415f)  c685d8fdffff00         mov byte ptr [rbp - 0x228], 0
0x764166  (f 0x364166)  31f6                   xor esi, esi
0x764168  (f 0x364168)  488985c0fdffff         mov qword ptr [rbp - 0x240], rax
0x76416f  (f 0x36416f)  488985c8fdffff         mov qword ptr [rbp - 0x238], rax
0x764176  (f 0x364176)  48898dd0fdffff         mov qword ptr [rbp - 0x230], rcx
0x76417d  (f 0x36417d)  e86e070000             call 0x7648f0
0x764182  (f 0x364182)  488bb5c0fdffff         mov rsi, qword ptr [rbp - 0x240]
0x764189  (f 0x364189)  488b95c8fdffff         mov rdx, qword ptr [rbp - 0x238]
0x764190  (f 0x364190)  4c8db5b0fdffff         lea r14, [rbp - 0x250]
0x764197  (f 0x364197)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x76419b  (f 0x36419b)  c5f81103               vmovups xmmword ptr [rbx], xmm0
0x76419f  (f 0x36419f)  4c89f7                 mov rdi, r14
0x7641a2  (f 0x3641a2)  4829f2                 sub rdx, rsi
0x7641a5  (f 0x3641a5)  48d1ea                 shr rdx, 1
0x7641a8  (f 0x3641a8)  e86302d0ff             call 0x464410
0x7641ad  (f 0x3641ad)  4939de                 cmp r14, rbx
0x7641b0  (f 0x3641b0)  7427                   je 0x7641d9
0x7641b2  (f 0x3641b2)  488b85b0fdffff         mov rax, qword ptr [rbp - 0x250]
0x7641b9  (f 0x3641b9)  488b8db8fdffff         mov rcx, qword ptr [rbp - 0x248]
0x7641c0  (f 0x3641c0)  488903                 mov qword ptr [rbx], rax
0x7641c3  (f 0x3641c3)  48894b08               mov qword ptr [rbx + 8], rcx
0x7641c7  (f 0x3641c7)  eb21                   jmp 0x7641ea
0x7641c9  (f 0x3641c9)  4883e6fe               and rsi, 0xfffffffffffffffe
0x7641cd  (f 0x3641cd)  4889df                 mov rdi, rbx
0x7641d0  (f 0x3641d0)  31d2                   xor edx, edx
0x7641d2  (f 0x3641d2)  e8092b1200             call 0x886ce0
0x7641d7  (f 0x3641d7)  eb1d                   jmp 0x7641f6
0x7641d9  (f 0x3641d9)  488bbdb0fdffff         mov rdi, qword ptr [rbp - 0x250]
0x7641e0  (f 0x3641e0)  4885ff                 test rdi, rdi
0x7641e3  (f 0x3641e3)  7405                   je 0x7641ea
0x7641e5  (f 0x3641e5)  e8a638d5ff             call 0x4b7a90
0x7641ea  (f 0x3641ea)  488dbdc0fdffff         lea rdi, [rbp - 0x240]
0x7641f1  (f 0x3641f1)  e8eafee9ff             call 0x6040e0
0x7641f6  (f 0x3641f6)  498b07                 mov rax, qword ptr [r15]
0x7641f9  (f 0x3641f9)  483b45e0               cmp rax, qword ptr [rbp - 0x20]
0x7641fd  (f 0x3641fd)  7511                   jne 0x764210
0x7641ff  (f 0x3641ff)  4889d8                 mov rax, rbx
0x764202  (f 0x364202)  4881c438020000         add rsp, 0x238
0x764209  (f 0x364209)  5b                     pop rbx
0x76420a  (f 0x36420a)  415e                   pop r14
0x76420c  (f 0x36420c)  415f                   pop r15
0x76420e  (f 0x36420e)  5d                     pop rbp
0x76420f  (f 0x36420f)  c3                     ret 
0x764210  (f 0x364210)  e84bdc1007             call 0x7871e60
0x764215  (f 0x364215)  0f0b                   ud2 
--- calls: [('0xb6417d', '0x7648f0'), ('0xb641a8', '0x464410'), ('0xb641f1', '0x6040e0'), ('0xb64210', '0x7871e60'), ('0xb641e5', '0x4b7a90'), ('0xb641d2', '0x886ce0')]

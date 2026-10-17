0x722840  (f 0x322840)  55                     push rbp
0x722841  (f 0x322841)  4889e5                 mov rbp, rsp
0x722844  (f 0x322844)  4157                   push r15
0x722846  (f 0x322846)  4156                   push r14
0x722848  (f 0x322848)  4155                   push r13
0x72284a  (f 0x32284a)  4154                   push r12
0x72284c  (f 0x32284c)  53                     push rbx
0x72284d  (f 0x32284d)  4881ec48080000         sub rsp, 0x848
0x722854  (f 0x322854)  4c8b2dadf6e008         mov r13, qword ptr [rip + 0x8e0f6ad]  ; rip->0x9531f08 (file 0x9131f08)
0x72285b  (f 0x32285b)  89cb                   mov ebx, ecx
0x72285d  (f 0x32285d)  4989d6                 mov r14, rdx
0x722860  (f 0x322860)  4989fc                 mov r12, rdi
0x722863  (f 0x322863)  498b4500               mov rax, qword ptr [r13]
0x722867  (f 0x322867)  488945d0               mov qword ptr [rbp - 0x30], rax
0x72286b  (f 0x32286b)  e8d03c0a00             call 0x7c6540
0x722870  (f 0x322870)  84c0                   test al, al
0x722872  (f 0x322872)  0f840b010000           je 0x722983
0x722878  (f 0x322878)  4d8bbc2410010000       mov r15, qword ptr [r12 + 0x110]
0x722880  (f 0x322880)  4d85ff                 test r15, r15
0x722883  (f 0x322883)  0f84fa000000           je 0x722983
0x722889  (f 0x322889)  888597f7ffff           mov byte ptr [rbp - 0x869], al
0x72288f  (f 0x32288f)  488dbdbaf7ffff         lea rdi, [rbp - 0x846]
0x722896  (f 0x322896)  488d45d0               lea rax, [rbp - 0x30]
0x72289a  (f 0x32289a)  488d3551535007         lea rsi, [rip + 0x7505351]  ; rip->0x7c27bf2 (file 0x7827bf2)
0x7228a1  (f 0x3228a1)  ba12000000             mov edx, 0x12
0x7228a6  (f 0x3228a6)  c685b8f7ffff00         mov byte ptr [rbp - 0x848], 0
0x7228ad  (f 0x3228ad)  4889bda0f7ffff         mov qword ptr [rbp - 0x860], rdi
0x7228b4  (f 0x3228b4)  4889bda8f7ffff         mov qword ptr [rbp - 0x858], rdi
0x7228bb  (f 0x3228bb)  488985b0f7ffff         mov qword ptr [rbp - 0x850], rax
0x7228c2  (f 0x3228c2)  e869a7d6ff             call 0x48d030
0x7228c7  (f 0x3228c7)  4883c012               add rax, 0x12
0x7228cb  (f 0x3228cb)  488dbd98f7ffff         lea rdi, [rbp - 0x868]
0x7228d2  (f 0x3228d2)  488985a8f7ffff         mov qword ptr [rbp - 0x858], rax
0x7228d9  (f 0x3228d9)  498b442418             mov rax, qword ptr [r12 + 0x18]
0x7228de  (f 0x3228de)  4c8da5a0f7ffff         lea r12, [rbp - 0x860]
0x7228e5  (f 0x3228e5)  4c89e6                 mov rsi, r12
0x7228e8  (f 0x3228e8)  48898598f7ffff         mov qword ptr [rbp - 0x868], rax
0x7228ef  (f 0x3228ef)  e84ccdf1ff             call 0x63f640
0x7228f4  (f 0x3228f4)  488b85a8f7ffff         mov rax, qword ptr [rbp - 0x858]
0x7228fb  (f 0x3228fb)  488dbd98f7ffff         lea rdi, [rbp - 0x868]
0x722902  (f 0x322902)  ba01000000             mov edx, 1
0x722907  (f 0x322907)  66c7000000             mov word ptr [rax], 0
0x72290c  (f 0x32290c)  488bb5a0f7ffff         mov rsi, qword ptr [rbp - 0x860]
0x722913  (f 0x322913)  e878acf1ff             call 0x63d590
0x722918  (f 0x322918)  4c89e7                 mov rdi, r12
0x72291b  (f 0x32291b)  e8c017eeff             call 0x6040e0
0x722920  (f 0x322920)  488b8598f7ffff         mov rax, qword ptr [rbp - 0x868]
0x722927  (f 0x322927)  488dbda0f7ffff         lea rdi, [rbp - 0x860]
0x72292e  (f 0x32292e)  488db598f7ffff         lea rsi, [rbp - 0x868]
0x722935  (f 0x322935)  48898598f7ffff         mov qword ptr [rbp - 0x868], rax
0x72293c  (f 0x32293c)  e87fc7f1ff             call 0x63f0c0
--- calls: [('0xb2286b', '0x7c6540'), ('0xb228c2', '0x48d030'), ('0xb228ef', '0x63f640'), ('0xb22913', '0x63d590'), ('0xb2291b', '0x6040e0'), ('0xb2293c', '0x63f0c0')]

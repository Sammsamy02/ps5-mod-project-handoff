0x726ff0  (f 0x326ff0)  89de                   mov esi, ebx
0x726ff2  (f 0x326ff2)  eb15                   jmp 0x727009
0x727000  (f 0x327000)  4d8b7620               mov r14, qword ptr [r14 + 0x20]
0x727004  (f 0x327004)  4d85f6                 test r14, r14
0x727007  (f 0x327007)  743c                   je 0x727045
0x727009  (f 0x327009)  e8b2f4ffff             call 0x7264c0
0x72700e  (f 0x32700e)  498b4e10               mov rcx, qword ptr [r14 + 0x10]
0x727012  (f 0x327012)  48635038               movsxd rdx, dword ptr [rax + 0x38]
0x727016  (f 0x327016)  3b5138                 cmp edx, dword ptr [rcx + 0x38]
0x727019  (f 0x327019)  7fe5                   jg 0x727000
0x72701b  (f 0x32701b)  488b4930               mov rcx, qword ptr [rcx + 0x30]
0x72701f  (f 0x32701f)  4883c030               add rax, 0x30
0x727023  (f 0x327023)  483904d1               cmp qword ptr [rcx + rdx*8], rax
0x727027  (f 0x327027)  75d7                   jne 0x727000
0x727029  (f 0x327029)  f683b100000004         test byte ptr [rbx + 0xb1], 4
0x727030  (f 0x327030)  751f                   jne 0x727051
0x727032  (f 0x327032)  488d05871a1300         lea rax, [rip + 0x131a87]  ; rip->0x858ac0 (file 0x458ac0)
0x727039  (f 0x327039)  488983d8000000         mov qword ptr [rbx + 0xd8], rax
0x727040  (f 0x327040)  5b                     pop rbx
0x727041  (f 0x327041)  415e                   pop r14
0x727043  (f 0x327043)  5d                     pop rbp
0x727044  (f 0x327044)  c3                     ret 
0x727045  (f 0x327045)  4531f6                 xor r14d, r14d
0x727048  (f 0x327048)  f683b100000004         test byte ptr [rbx + 0xb1], 4
0x72704f  (f 0x32704f)  74e1                   je 0x727032
0x727051  (f 0x327051)  496386f8010000         movsxd rax, dword ptr [r14 + 0x1f8]
0x727058  (f 0x327058)  4885c0                 test rax, rax
0x72705b  (f 0x32705b)  7422                   je 0x72707f
0x72705d  (f 0x32705d)  488b5318               mov rdx, qword ptr [rbx + 0x18]
0x727061  (f 0x327061)  498b8ef0010000         mov rcx, qword ptr [r14 + 0x1f0]
0x727068  (f 0x327068)  48c1e004               shl rax, 4
0x72706c  (f 0x32706c)  0f1f4000               nop dword ptr [rax]
0x727070  (f 0x327070)  483911                 cmp qword ptr [rcx], rdx
0x727073  (f 0x327073)  740f                   je 0x727084
0x727075  (f 0x327075)  4883c110               add rcx, 0x10
0x727079  (f 0x327079)  4883c0f0               add rax, -0x10
0x72707d  (f 0x32707d)  75f1                   jne 0x727070
0x72707f  (f 0x32707f)  5b                     pop rbx
0x727080  (f 0x327080)  415e                   pop r14
0x727082  (f 0x327082)  5d                     pop rbp
0x727083  (f 0x327083)  c3                     ret 
0x727084  (f 0x327084)  4885c9                 test rcx, rcx
0x727087  (f 0x327087)  74f6                   je 0x72707f
0x727089  (f 0x327089)  488b4108               mov rax, qword ptr [rcx + 8]
0x72708d  (f 0x32708d)  ebaa                   jmp 0x727039
--- calls: [('0xb27009', '0x7264c0')]

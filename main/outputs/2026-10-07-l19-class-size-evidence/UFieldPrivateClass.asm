0x71a3f0  (f 0x31a3f0)  55                     push rbp
0x71a3f1  (f 0x31a3f1)  4889e5                 mov rbp, rsp
0x71a3f4  (f 0x31a3f4)  4157                   push r15
0x71a3f6  (f 0x31a3f6)  4156                   push r14
0x71a3f8  (f 0x31a3f8)  53                     push rbx
0x71a3f9  (f 0x31a3f9)  4881ecc8010000         sub rsp, 0x1c8
0x71a400  (f 0x31a400)  4c8b3d017be108         mov r15, qword ptr [rip + 0x8e17b01]  ; rip->0x9531f08 (file 0x9131f08)
0x71a407  (f 0x31a407)  498b07                 mov rax, qword ptr [r15]
0x71a40a  (f 0x31a40a)  488945e0               mov qword ptr [rbp - 0x20], rax
0x71a40e  (f 0x31a40e)  e84d7c0a00             call 0x7c2060
0x71a413  (f 0x31a413)  e8d8000000             call 0x71a4f0
0x71a418  (f 0x31a418)  4889c7                 mov rdi, rax
0x71a41b  (f 0x31a41b)  be01000000             mov esi, 1
0x71a420  (f 0x31a420)  4889c3                 mov rbx, rax
0x71a423  (f 0x31a423)  e8c8b91600             call 0x885df0
0x71a428  (f 0x31a428)  bf02000000             mov edi, 2
0x71a42d  (f 0x31a42d)  e8ee330800             call 0x79d820
0x71a432  (f 0x31a432)  4c8db538feffff         lea r14, [rbp - 0x1c8]
0x71a439  (f 0x31a439)  488d8d38feffff         lea rcx, [rbp - 0x1c8]
0x71a440  (f 0x31a440)  488d9520feffff         lea rdx, [rbp - 0x1e0]
0x71a447  (f 0x31a447)  be30000000             mov esi, 0x30
0x71a44c  (f 0x31a44c)  48898520feffff         mov qword ptr [rbp - 0x1e0], rax
0x71a453  (f 0x31a453)  c78528feffff28000000   mov dword ptr [rbp - 0x1d8], 0x28
0x71a45d  (f 0x31a45d)  c6852cfeffff02         mov byte ptr [rbp - 0x1d4], 2
0x71a464  (f 0x31a464)  48c78530feffff00000000 mov qword ptr [rbp - 0x1d0], 0
0x71a46f  (f 0x31a46f)  4c89f7                 mov rdi, r14
0x71a472  (f 0x31a472)  e829380800             call 0x79dca0
0x71a477  (f 0x31a477)  4c89f7                 mov rdi, r14
0x71a47a  (f 0x31a47a)  31f6                   xor esi, esi
0x71a47c  (f 0x31a47c)  e89f3f0800             call 0x79e420
0x71a481  (f 0x31a481)  4889df                 mov rdi, rbx
0x71a484  (f 0x31a484)  4889c6                 mov rsi, rax
0x71a487  (f 0x31a487)  e824450800             call 0x79e9b0
0x71a48c  (f 0x31a48c)  4c89f7                 mov rdi, r14
0x71a48f  (f 0x31a48f)  e82c390800             call 0x79ddc0
0x71a494  (f 0x31a494)  4c89f7                 mov rdi, r14
0x71a497  (f 0x31a497)  e86409f0ff             call 0x61ae00
0x71a49c  (f 0x31a49c)  488d052db2bb07         lea rax, [rip + 0x7bbb22d]  ; rip->0x82d56d0 (file 0x7ed56d0)
0x71a4a3  (f 0x31a4a3)  4889df                 mov rdi, rbx
0x71a4a6  (f 0x31a4a6)  4c89f6                 mov rsi, r14
0x71a4a9  (f 0x31a4a9)  31d2                   xor edx, edx
0x71a4ab  (f 0x31a4ab)  48898538feffff         mov qword ptr [rbp - 0x1c8], rax
0x71a4b2  (f 0x31a4b2)  488b03                 mov rax, qword ptr [rbx]
0x71a4b5  (f 0x31a4b5)  ff90d0020000           call qword ptr [rax + 0x2d0]
0x71a4bb  (f 0x31a4bb)  4c89f7                 mov rdi, r14
0x71a4be  (f 0x31a4be)  e84d10f0ff             call 0x61b510
0x71a4c3  (f 0x31a4c3)  498b07                 mov rax, qword ptr [r15]
0x71a4c6  (f 0x31a4c6)  483b45e0               cmp rax, qword ptr [rbp - 0x20]
0x71a4ca  (f 0x31a4ca)  7511                   jne 0x71a4dd
0x71a4cc  (f 0x31a4cc)  4889d8                 mov rax, rbx
0x71a4cf  (f 0x31a4cf)  4881c4c8010000         add rsp, 0x1c8
0x71a4d6  (f 0x31a4d6)  5b                     pop rbx
0x71a4d7  (f 0x31a4d7)  415e                   pop r14
0x71a4d9  (f 0x31a4d9)  415f                   pop r15
0x71a4db  (f 0x31a4db)  5d                     pop rbp
0x71a4dc  (f 0x31a4dc)  c3                     ret 
0x71a4dd  (f 0x31a4dd)  e87e791507             call 0x7871e60
0x71a4e2  (f 0x31a4e2)  0f0b                   ud2 
--- calls: [('0xb1a40e', '0x7c2060'), ('0xb1a413', '0x71a4f0'), ('0xb1a423', '0x885df0'), ('0xb1a42d', '0x79d820'), ('0xb1a472', '0x79dca0'), ('0xb1a47c', '0x79e420'), ('0xb1a487', '0x79e9b0'), ('0xb1a48f', '0x79ddc0'), ('0xb1a497', '0x61ae00'), ('0xb1a4be', '0x61b510'), ('0xb1a4dd', '0x7871e60')]

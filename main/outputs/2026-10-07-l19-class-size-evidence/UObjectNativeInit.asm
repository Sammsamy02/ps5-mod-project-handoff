0x891c20  (f 0x491c20)  55                     push rbp
0x891c21  (f 0x491c21)  4889e5                 mov rbp, rsp
0x891c24  (f 0x491c24)  4156                   push r14
0x891c26  (f 0x491c26)  53                     push rbx
0x891c27  (f 0x491c27)  488d052adaa607         lea rax, [rip + 0x7a6da2a]  ; rip->0x82ff658 (file 0x7eff658)
0x891c2e  (f 0x491c2e)  4889f3                 mov rbx, rsi
0x891c31  (f 0x491c31)  4989fe                 mov r14, rdi
0x891c34  (f 0x491c34)  488907                 mov qword ptr [rdi], rax
0x891c37  (f 0x491c37)  e8d444f3ff             call 0x7c6110
0x891c3c  (f 0x491c3c)  488b03                 mov rax, qword ptr [rbx]
0x891c3f  (f 0x491c3f)  4885c0                 test rax, rax
0x891c42  (f 0x491c42)  0f94c1                 sete cl
0x891c45  (f 0x491c45)  4c39f0                 cmp rax, r14
0x891c48  (f 0x491c48)  0f94c0                 sete al
0x891c4b  (f 0x491c4b)  08c8                   or al, cl
0x891c4d  (f 0x491c4d)  740c                   je 0x891c5b
0x891c4f  (f 0x491c4f)  4c8933                 mov qword ptr [rbx], r14
0x891c52  (f 0x491c52)  c6431300               mov byte ptr [rbx + 0x13], 0
0x891c56  (f 0x491c56)  5b                     pop rbx
0x891c57  (f 0x491c57)  415e                   pop r14
0x891c59  (f 0x491c59)  5d                     pop rbp
0x891c5a  (f 0x491c5a)  c3                     ret 
0x891c5b  (f 0x491c5b)  488d3de0976307         lea rdi, [rip + 0x76397e0]  ; rip->0x7ecb442 (file 0x7acb442)
0x891c62  (f 0x491c62)  488d15a9eb4d07         lea rdx, [rip + 0x74deba9]  ; rip->0x7d70812 (file 0x7970812)
0x891c69  (f 0x491c69)  be910f0000             mov esi, 0xf91
0x891c6e  (f 0x491c6e)  31c0                   xor eax, eax
0x891c70  (f 0x491c70)  e83b34ceff             call 0x5750b0
0x891c75  (f 0x491c75)  ebd8                   jmp 0x891c4f
--- calls: [('0xc91c37', '0x7c6110'), ('0xc91c70', '0x5750b0')]

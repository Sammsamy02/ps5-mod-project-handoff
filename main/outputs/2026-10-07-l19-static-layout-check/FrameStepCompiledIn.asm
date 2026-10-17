0x8576e0  (f 0x4576e0)  80bd7cffffff00         cmp byte ptr [rbp - 0x84], 0
0x8576e7  (f 0x4576e7)  75c9                   jne 0x8576b2
0x8576e9  (f 0x4576e9)  4c8b75c0               mov r14, qword ptr [rbp - 0x40]
0x8576ed  (f 0x4576ed)  4c637dc8               movsxd r15, dword ptr [rbp - 0x38]
0x8576f1  (f 0x4576f1)  488d430c               lea rax, [rbx + 0xc]
0x8576f5  (f 0x4576f5)  48c70300000000         mov qword ptr [rbx], 0
0x8576fc  (f 0x4576fc)  44897b08               mov dword ptr [rbx + 8], r15d
0x857700  (f 0x457700)  4d85ff                 test r15, r15
0x857703  (f 0x457703)  747e                   je 0x857783
0x857705  (f 0x457705)  bf02000000             mov edi, 2
0x85770a  (f 0x45770a)  be02000000             mov esi, 2
0x85770f  (f 0x45770f)  4489fa                 mov edx, r15d
0x857712  (f 0x457712)  4989d8                 mov r8, rbx
0x857715  (f 0x457715)  4589f9                 mov r9d, r15d
0x857718  (f 0x457718)  31c9                   xor ecx, ecx
0x85771a  (f 0x45771a)  48890424               mov qword ptr [rsp], rax
0x85771e  (f 0x45771e)  e8adfebcff             call 0x4275d0
0x857723  (f 0x457723)  488b3b                 mov rdi, qword ptr [rbx]
0x857726  (f 0x457726)  4d01ff                 add r15, r15
0x857729  (f 0x457729)  4c89f6                 mov rsi, r14
0x85772c  (f 0x45772c)  4c89fa                 mov rdx, r15
0x85772f  (f 0x45772f)  e81ca70107             call 0x7871e50
0x857734  (f 0x457734)  488b7da0               mov rdi, qword ptr [rbp - 0x60]
0x857738  (f 0x457738)  4885ff                 test rdi, rdi
0x85773b  (f 0x45773b)  7405                   je 0x857742
0x85773d  (f 0x45773d)  e84e03c6ff             call 0x4b7a90
0x857742  (f 0x457742)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x857746  (f 0x457746)  4885ff                 test rdi, rdi
0x857749  (f 0x457749)  7405                   je 0x857750
0x85774b  (f 0x45774b)  e84003c6ff             call 0x4b7a90
0x857750  (f 0x457750)  488b7dc0               mov rdi, qword ptr [rbp - 0x40]
0x857754  (f 0x457754)  4885ff                 test rdi, rdi
0x857757  (f 0x457757)  7405                   je 0x85775e
0x857759  (f 0x457759)  e83203c6ff             call 0x4b7a90
0x85775e  (f 0x45775e)  488b05a3a7cd08         mov rax, qword ptr [rip + 0x8cda7a3]  ; rip->0x9531f08 (file 0x9131f08)
0x857765  (f 0x457765)  488b00                 mov rax, qword ptr [rax]
0x857768  (f 0x457768)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x85776c  (f 0x45776c)  7526                   jne 0x857794
0x85776e  (f 0x45776e)  4889d8                 mov rax, rbx
0x857771  (f 0x457771)  4881c488000000         add rsp, 0x88
0x857778  (f 0x457778)  5b                     pop rbx
0x857779  (f 0x457779)  415c                   pop r12
0x85777b  (f 0x45777b)  415d                   pop r13
0x85777d  (f 0x45777d)  415e                   pop r14
0x85777f  (f 0x45777f)  415f                   pop r15
0x857781  (f 0x457781)  5d                     pop rbp
0x857782  (f 0x457782)  c3                     ret 
0x857783  (f 0x457783)  c70000000000           mov dword ptr [rax], 0
0x857789  (f 0x457789)  488b7da0               mov rdi, qword ptr [rbp - 0x60]
0x85778d  (f 0x45778d)  4885ff                 test rdi, rdi
0x857790  (f 0x457790)  75ab                   jne 0x85773d
0x857792  (f 0x457792)  ebae                   jmp 0x857742
0x857794  (f 0x457794)  e8c7a60107             call 0x7871e60
0x857799  (f 0x457799)  0f0b                   ud2 
--- calls: [('0xc5771e', '0x4275d0'), ('0xc5772f', '0x7871e50'), ('0xc5773d', '0x4b7a90'), ('0xc5774b', '0x4b7a90'), ('0xc57759', '0x4b7a90'), ('0xc57794', '0x7871e60')]

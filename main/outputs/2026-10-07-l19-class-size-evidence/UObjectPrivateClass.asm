0x7c5bb0  (f 0x3c5bb0)  55                     push rbp
0x7c5bb1  (f 0x3c5bb1)  4889e5                 mov rbp, rsp
0x7c5bb4  (f 0x3c5bb4)  4157                   push r15
0x7c5bb6  (f 0x3c5bb6)  4156                   push r14
0x7c5bb8  (f 0x3c5bb8)  53                     push rbx
0x7c5bb9  (f 0x3c5bb9)  4883ec18               sub rsp, 0x18
0x7c5bbd  (f 0x3c5bbd)  488b1d44c3d608         mov rbx, qword ptr [rip + 0x8d6c344]  ; rip->0x9531f08 (file 0x9131f08)
0x7c5bc4  (f 0x3c5bc4)  488b03                 mov rax, qword ptr [rbx]
0x7c5bc7  (f 0x3c5bc7)  488945e0               mov qword ptr [rbp - 0x20], rax
0x7c5bcb  (f 0x3c5bcb)  488b050e11fe08         mov rax, qword ptr [rip + 0x8fe110e]  ; rip->0x97a6ce0 (file 0x93a6ce0)
0x7c5bd2  (f 0x3c5bd2)  4885c0                 test rax, rax
0x7c5bd5  (f 0x3c5bd5)  7574                   jne 0x7c5c4b
0x7c5bd7  (f 0x3c5bd7)  488d05b256c4ff         lea rax, [rip - 0x3ba94e]  ; rip->0x40b290 (file 0xb290)
0x7c5bde  (f 0x3c5bde)  4c8d55d8               lea r10, [rbp - 0x28]
0x7c5be2  (f 0x3c5be2)  4c8d1d37010000         lea r11, [rip + 0x137]  ; rip->0x7c5d20 (file 0x3c5d20)
0x7c5be9  (f 0x3c5be9)  4c8d3510010000         lea r14, [rip + 0x110]  ; rip->0x7c5d00 (file 0x3c5d00)
0x7c5bf0  (f 0x3c5bf0)  4c8d3d77144607         lea r15, [rip + 0x7461477]  ; rip->0x7c2706e (file 0x782706e)
0x7c5bf7  (f 0x3c5bf7)  488d3dc6144607         lea rdi, [rip + 0x74614c6]  ; rip->0x7c270c4 (file 0x78270c4)
0x7c5bfe  (f 0x3c5bfe)  488d35fd6b6207         lea rsi, [rip + 0x7626bfd]  ; rip->0x7dec802 (file 0x79ec802)
0x7c5c05  (f 0x3c5c05)  488d15d410fe08         lea rdx, [rip + 0x8fe10d4]  ; rip->0x97a6ce0 (file 0x93a6ce0)
0x7c5c0c  (f 0x3c5c0c)  488d0d7d56c4ff         lea rcx, [rip - 0x3ba983]  ; rip->0x40b290 (file 0xb290)
0x7c5c13  (f 0x3c5c13)  41b828000000           mov r8d, 0x28
0x7c5c19  (f 0x3c5c19)  41b908000000           mov r9d, 8
0x7c5c1f  (f 0x3c5c1f)  488945d8               mov qword ptr [rbp - 0x28], rax
0x7c5c23  (f 0x3c5c23)  488d05c668edff         lea rax, [rip - 0x12973a]  ; rip->0x69c4f0 (file 0x29c4f0)
0x7c5c2a  (f 0x3c5c2a)  50                     push rax
0x7c5c2b  (f 0x3c5c2b)  50                     push rax
0x7c5c2c  (f 0x3c5c2c)  4152                   push r10
0x7c5c2e  (f 0x3c5c2e)  4153                   push r11
0x7c5c30  (f 0x3c5c30)  4156                   push r14
0x7c5c32  (f 0x3c5c32)  4157                   push r15
0x7c5c34  (f 0x3c5c34)  6a00                   push 0
0x7c5c36  (f 0x3c5c36)  6821000010             push 0x10000021
0x7c5c3b  (f 0x3c5c3b)  e8207cf4ff             call 0x70d860
0x7c5c40  (f 0x3c5c40)  4883c440               add rsp, 0x40
0x7c5c44  (f 0x3c5c44)  488b059510fe08         mov rax, qword ptr [rip + 0x8fe1095]  ; rip->0x97a6ce0 (file 0x93a6ce0)
0x7c5c4b  (f 0x3c5c4b)  488b0b                 mov rcx, qword ptr [rbx]
0x7c5c4e  (f 0x3c5c4e)  483b4de0               cmp rcx, qword ptr [rbp - 0x20]
0x7c5c52  (f 0x3c5c52)  750b                   jne 0x7c5c5f
0x7c5c54  (f 0x3c5c54)  4883c418               add rsp, 0x18
0x7c5c58  (f 0x3c5c58)  5b                     pop rbx
0x7c5c59  (f 0x3c5c59)  415e                   pop r14
0x7c5c5b  (f 0x3c5c5b)  415f                   pop r15
0x7c5c5d  (f 0x3c5c5d)  5d                     pop rbp
0x7c5c5e  (f 0x3c5c5e)  c3                     ret 
0x7c5c5f  (f 0x3c5c5f)  e8fcc10a07             call 0x7871e60
0x7c5c64  (f 0x3c5c64)  0f0b                   ud2 
--- calls: [('0xbc5c3b', '0x70d860'), ('0xbc5c5f', '0x7871e60')]

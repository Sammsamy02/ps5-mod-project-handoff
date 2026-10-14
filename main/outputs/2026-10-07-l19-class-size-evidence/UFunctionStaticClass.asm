0x728400  (f 0x328400)  55                     push rbp
0x728401  (f 0x328401)  4889e5                 mov rbp, rsp
0x728404  (f 0x328404)  4157                   push r15
0x728406  (f 0x328406)  4156                   push r14
0x728408  (f 0x328408)  53                     push rbx
0x728409  (f 0x328409)  4881ec98000000         sub rsp, 0x98
0x728410  (f 0x328410)  4c8b3df19ae008         mov r15, qword ptr [rip + 0x8e09af1]  ; rip->0x9531f08 (file 0x9131f08)
0x728417  (f 0x328417)  498b07                 mov rax, qword ptr [r15]
0x72841a  (f 0x32841a)  488945e0               mov qword ptr [rbp - 0x20], rax
0x72841e  (f 0x32841e)  488b058b630709         mov rax, qword ptr [rip + 0x907638b]  ; rip->0x979e7b0 (file 0x939e7b0)
0x728425  (f 0x328425)  4885c0                 test rax, rax
0x728428  (f 0x328428)  755a                   jne 0x728484
0x72842a  (f 0x32842a)  e83185ffff             call 0x720960
0x72842f  (f 0x32842f)  e87c000000             call 0x7284b0
0x728434  (f 0x328434)  4889c7                 mov rdi, rax
0x728437  (f 0x328437)  be01000000             mov esi, 1
0x72843c  (f 0x32843c)  4889c3                 mov rbx, rax
0x72843f  (f 0x32843f)  e8acd91500             call 0x885df0
0x728444  (f 0x328444)  4c8db550ffffff         lea r14, [rbp - 0xb0]
0x72844b  (f 0x32844b)  4c89f7                 mov rdi, r14
0x72844e  (f 0x32844e)  e8ad29efff             call 0x61ae00
0x728453  (f 0x328453)  488d0576d2ba07         lea rax, [rip + 0x7bad276]  ; rip->0x82d56d0 (file 0x7ed56d0)
0x72845a  (f 0x32845a)  4889df                 mov rdi, rbx
0x72845d  (f 0x32845d)  4c89f6                 mov rsi, r14
0x728460  (f 0x328460)  31d2                   xor edx, edx
0x728462  (f 0x328462)  48898550ffffff         mov qword ptr [rbp - 0xb0], rax
0x728469  (f 0x328469)  488b03                 mov rax, qword ptr [rbx]
0x72846c  (f 0x32846c)  ff90d0020000           call qword ptr [rax + 0x2d0]
0x728472  (f 0x328472)  4c89f7                 mov rdi, r14
0x728475  (f 0x328475)  e89630efff             call 0x61b510
0x72847a  (f 0x32847a)  4889d8                 mov rax, rbx
0x72847d  (f 0x32847d)  48891d2c630709         mov qword ptr [rip + 0x907632c], rbx  ; rip->0x979e7b0 (file 0x939e7b0)
0x728484  (f 0x328484)  498b0f                 mov rcx, qword ptr [r15]
0x728487  (f 0x328487)  483b4de0               cmp rcx, qword ptr [rbp - 0x20]
0x72848b  (f 0x32848b)  750e                   jne 0x72849b
0x72848d  (f 0x32848d)  4881c498000000         add rsp, 0x98
0x728494  (f 0x328494)  5b                     pop rbx
0x728495  (f 0x328495)  415e                   pop r14
0x728497  (f 0x328497)  415f                   pop r15
0x728499  (f 0x328499)  5d                     pop rbp
0x72849a  (f 0x32849a)  c3                     ret 
0x72849b  (f 0x32849b)  e8c0991407             call 0x7871e60
0x7284a0  (f 0x3284a0)  0f0b                   ud2 
--- calls: [('0xb2842a', '0x720960'), ('0xb2842f', '0x7284b0'), ('0xb2843f', '0x885df0'), ('0xb2844e', '0x61ae00'), ('0xb28475', '0x61b510'), ('0xb2849b', '0x7871e60')]

0x885430  (f 0x485430)  55                     push rbp
0x885431  (f 0x485431)  4889e5                 mov rbp, rsp
0x885434  (f 0x485434)  4157                   push r15
0x885436  (f 0x485436)  4156                   push r14
0x885438  (f 0x485438)  4155                   push r13
0x88543a  (f 0x48543a)  4154                   push r12
0x88543c  (f 0x48543c)  53                     push rbx
0x88543d  (f 0x48543d)  50                     push rax
0x88543e  (f 0x48543e)  4c8d2d179af108         lea r13, [rip + 0x8f19a17]  ; rip->0x979ee5c (file 0x939ee5c)
0x885445  (f 0x485445)  488d05d4a1a707         lea rax, [rip + 0x7a7a1d4]  ; rip->0x82ff620 (file 0x7eff620)
0x88544c  (f 0x48544c)  4189ce                 mov r14d, ecx
0x88544f  (f 0x48544f)  4d89cf                 mov r15, r9
0x885452  (f 0x485452)  4d89c4                 mov r12, r8
0x885455  (f 0x485455)  4889fb                 mov rbx, rdi
0x885458  (f 0x485458)  488907                 mov qword ptr [rdi], rax
0x88545b  (f 0x48545b)  895708                 mov dword ptr [rdi + 8], edx
0x88545e  (f 0x48545e)  c7470cffffffff         mov dword ptr [rdi + 0xc], 0xffffffff
0x885465  (f 0x485465)  48897710               mov qword ptr [rdi + 0x10], rsi
0x885469  (f 0x485469)  410fb64500             movzx eax, byte ptr [r13]
0x88546e  (f 0x48546e)  89c1                   mov ecx, eax
0x885470  (f 0x485470)  80f101                 xor cl, 1
0x885473  (f 0x485473)  4885f6                 test rsi, rsi
0x885476  (f 0x485476)  0f94c2                 sete dl
0x885479  (f 0x485479)  08ca                   or dl, cl
0x88547b  (f 0x48547b)  750d                   jne 0x88548a
0x88547d  (f 0x48547d)  4889f7                 mov rdi, rsi
0x885480  (f 0x485480)  e83b2eeeff             call 0x7682c0
0x885485  (f 0x485485)  410fb64500             movzx eax, byte ptr [r13]
0x88548a  (f 0x48548a)  48c7431800000000       mov qword ptr [rbx + 0x18], 0
0x885492  (f 0x485492)  4c896320               mov qword ptr [rbx + 0x20], r12
0x885496  (f 0x485496)  4d85e4                 test r12, r12
0x885499  (f 0x485499)  740c                   je 0x8854a7
0x88549b  (f 0x48549b)  a801                   test al, 1
0x88549d  (f 0x48549d)  7408                   je 0x8854a7
0x88549f  (f 0x48549f)  4c89e7                 mov rdi, r12
0x8854a2  (f 0x4854a2)  e8192eeeff             call 0x7682c0
0x8854a7  (f 0x4854a7)  448b6518               mov r12d, dword ptr [rbp + 0x18]
0x8854ab  (f 0x4854ab)  448b6d10               mov r13d, dword ptr [rbp + 0x10]
0x8854af  (f 0x4854af)  4c897b18               mov qword ptr [rbx + 0x18], r15
0x8854b3  (f 0x4854b3)  e8d83bc3ff             call 0x4b9090
0x8854b8  (f 0x4854b8)  8b5308                 mov edx, dword ptr [rbx + 8]
0x8854bb  (f 0x4854bb)  f6c284                 test dl, 0x84
0x8854be  (f 0x4854be)  740b                   je 0x8854cb
0x8854c0  (f 0x4854c0)  89d1                   mov ecx, edx
0x8854c2  (f 0x4854c2)  81e17bffffff           and ecx, 0xffffff7b
0x8854c8  (f 0x4854c8)  894b08                 mov dword ptr [rbx + 8], ecx
0x8854cb  (f 0x4854cb)  4489f1                 mov ecx, r14d
0x8854ce  (f 0x4854ce)  488d3d3b4cf308         lea rdi, [rip + 0x8f34c3b]  ; rip->0x97ba110 (file 0x93ba110)
0x8854d5  (f 0x4854d5)  4889de                 mov rsi, rbx
0x8854d8  (f 0x4854d8)  4589e0                 mov r8d, r12d
0x8854db  (f 0x4854db)  81c900000004           or ecx, 0x4000000
0x8854e1  (f 0x4854e1)  84c0                   test al, al
0x8854e3  (f 0x4854e3)  410f45ce               cmovne ecx, r14d
0x8854e7  (f 0x4854e7)  c1e217                 shl edx, 0x17
0x8854ea  (f 0x4854ea)  81e200000042           and edx, 0x42000000
0x8854f0  (f 0x4854f0)  09ca                   or edx, ecx
0x8854f2  (f 0x4854f2)  4489e9                 mov ecx, r13d
0x8854f5  (f 0x4854f5)  e846e6ffff             call 0x883b40
0x8854fa  (f 0x4854fa)  4889df                 mov rdi, rbx
0x8854fd  (f 0x4854fd)  e8decb0200             call 0x8b20e0
0x885502  (f 0x485502)  4883c408               add rsp, 8
0x885506  (f 0x485506)  5b                     pop rbx
0x885507  (f 0x485507)  415c                   pop r12
0x885509  (f 0x485509)  415d                   pop r13
0x88550b  (f 0x48550b)  415e                   pop r14
0x88550d  (f 0x48550d)  415f                   pop r15
0x88550f  (f 0x48550f)  5d                     pop rbp
0x885510  (f 0x485510)  c3                     ret 
--- calls: [('0xc85480', '0x7682c0'), ('0xc854a2', '0x7682c0'), ('0xc854b3', '0x4b9090'), ('0xc854f5', '0x883b40'), ('0xc854fd', '0x8b20e0')]

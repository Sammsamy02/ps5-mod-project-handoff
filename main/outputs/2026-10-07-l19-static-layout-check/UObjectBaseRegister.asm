0x885590  (f 0x485590)  896320                 mov dword ptr [rbx + 0x20], esp
0x885593  (f 0x485593)  4d85ff                 test r15, r15
0x885596  (f 0x485596)  740c                   je 0x8855a4
0x885598  (f 0x485598)  a801                   test al, 1
0x88559a  (f 0x48559a)  7408                   je 0x8855a4
0x88559c  (f 0x48559c)  4c89ff                 mov rdi, r15
0x88559f  (f 0x48559f)  e81c2deeff             call 0x7682c0
0x8855a4  (f 0x4855a4)  488d7dc8               lea rdi, [rbp - 0x38]
0x8855a8  (f 0x4855a8)  4c89f6                 mov rsi, r14
0x8855ab  (f 0x4855ab)  ba01000000             mov edx, 1
0x8855b0  (f 0x4855b0)  4c897b10               mov qword ptr [rbx + 0x10], r15
0x8855b4  (f 0x4855b4)  e8d77fdbff             call 0x63d590
0x8855b9  (f 0x4855b9)  488b45c8               mov rax, qword ptr [rbp - 0x38]
0x8855bd  (f 0x4855bd)  48894318               mov qword ptr [rbx + 0x18], rax
0x8855c1  (f 0x4855c1)  e8ca3ac3ff             call 0x4b9090
0x8855c6  (f 0x4855c6)  8b5308                 mov edx, dword ptr [rbx + 8]
0x8855c9  (f 0x4855c9)  f6c284                 test dl, 0x84
0x8855cc  (f 0x4855cc)  740b                   je 0x8855d9
0x8855ce  (f 0x4855ce)  89d1                   mov ecx, edx
0x8855d0  (f 0x4855d0)  81e17bffffff           and ecx, 0xffffff7b
0x8855d6  (f 0x4855d6)  894b08                 mov dword ptr [rbx + 8], ecx
0x8855d9  (f 0x4855d9)  4c8d35304bf308         lea r14, [rip + 0x8f34b30]  ; rip->0x97ba110 (file 0x93ba110)
0x8855e0  (f 0x4855e0)  3401                   xor al, 1
0x8855e2  (f 0x4855e2)  c1e217                 shl edx, 0x17
0x8855e5  (f 0x4855e5)  4889de                 mov rsi, rbx
0x8855e8  (f 0x4855e8)  b9ffffffff             mov ecx, 0xffffffff
0x8855ed  (f 0x4855ed)  4531ff                 xor r15d, r15d
0x8855f0  (f 0x4855f0)  4531c0                 xor r8d, r8d
0x8855f3  (f 0x4855f3)  0fb6c0                 movzx eax, al
0x8855f6  (f 0x4855f6)  81e200000042           and edx, 0x42000000
0x8855fc  (f 0x4855fc)  c1e01a                 shl eax, 0x1a
0x8855ff  (f 0x4855ff)  09c2                   or edx, eax
0x885601  (f 0x485601)  4c89f7                 mov rdi, r14
0x885604  (f 0x485604)  e837e5ffff             call 0x883b40
0x885609  (f 0x485609)  4889df                 mov rdi, rbx
0x88560c  (f 0x48560c)  e8cfca0200             call 0x8b20e0
0x885611  (f 0x485611)  8b430c                 mov eax, dword ptr [rbx + 0xc]
0x885614  (f 0x485614)  41394624               cmp dword ptr [r14 + 0x24], eax
0x885618  (f 0x485618)  7e26                   jle 0x885640
0x88561a  (f 0x48561a)  498b5610               mov rdx, qword ptr [r14 + 0x10]
0x88561e  (f 0x48561e)  89c1                   mov ecx, eax
0x885620  (f 0x485620)  0fb7c0                 movzx eax, ax
0x885623  (f 0x485623)  4c8d3c40               lea r15, [rax + rax*2]
0x885627  (f 0x485627)  c1e910                 shr ecx, 0x10
0x88562a  (f 0x48562a)  41c1e703               shl r15d, 3
0x88562e  (f 0x48562e)  4c033cca               add r15, qword ptr [rdx + rcx*8]
0x885632  (f 0x485632)  66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x885640  (f 0x485640)  418b4708               mov eax, dword ptr [r15 + 8]
0x885644  (f 0x485644)  85c0                   test eax, eax
0x885646  (f 0x485646)  7910                   jns 0x885658
0x885648  (f 0x485648)  89c1                   mov ecx, eax
0x88564a  (f 0x48564a)  81e1ffffff7f           and ecx, 0x7fffffff
0x885650  (f 0x485650)  f0410fb14f08           lock cmpxchg dword ptr [r15 + 8], ecx
0x885656  (f 0x485656)  75e8                   jne 0x885640
0x885658  (f 0x485658)  488b05a9c8ca08         mov rax, qword ptr [rip + 0x8cac8a9]  ; rip->0x9531f08 (file 0x9131f08)
0x88565f  (f 0x48565f)  488b00                 mov rax, qword ptr [rax]
0x885662  (f 0x485662)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x885666  (f 0x485666)  750f                   jne 0x885677
0x885668  (f 0x485668)  4883c418               add rsp, 0x18
0x88566c  (f 0x48566c)  5b                     pop rbx
0x88566d  (f 0x48566d)  415c                   pop r12
0x88566f  (f 0x48566f)  415d                   pop r13
0x885671  (f 0x485671)  415e                   pop r14
0x885673  (f 0x485673)  415f                   pop r15
0x885675  (f 0x485675)  5d                     pop rbp
0x885676  (f 0x485676)  c3                     ret 
0x885677  (f 0x485677)  e8e4c7fe06             call 0x7871e60
0x88567c  (f 0x48567c)  0f0b                   ud2 
--- calls: [('0xc8559f', '0x7682c0'), ('0xc855b4', '0x63d590'), ('0xc855c1', '0x4b9090'), ('0xc85604', '0x883b40'), ('0xc8560c', '0x8b20e0'), ('0xc85677', '0x7871e60')]

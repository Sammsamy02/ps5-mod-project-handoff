0x8840b0  (f 0x4840b0)  55                     push rbp
0x8840b1  (f 0x4840b1)  4889e5                 mov rbp, rsp
0x8840b4  (f 0x4840b4)  4157                   push r15
0x8840b6  (f 0x4840b6)  4156                   push r14
0x8840b8  (f 0x4840b8)  4154                   push r12
0x8840ba  (f 0x4840ba)  53                     push rbx
0x8840bb  (f 0x4840bb)  8b5e0c                 mov ebx, dword ptr [rsi + 0xc]
0x8840be  (f 0x4840be)  4989f7                 mov r15, rsi
0x8840c1  (f 0x4840c1)  4989fe                 mov r14, rdi
0x8840c4  (f 0x4840c4)  395f24                 cmp dword ptr [rdi + 0x24], ebx
0x8840c7  (f 0x4840c7)  7e43                   jle 0x88410c
0x8840c9  (f 0x4840c9)  498b5610               mov rdx, qword ptr [r14 + 0x10]
0x8840cd  (f 0x4840cd)  0fb7cb                 movzx ecx, bx
0x8840d0  (f 0x4840d0)  89d8                   mov eax, ebx
0x8840d2  (f 0x4840d2)  4c8d2449               lea r12, [rcx + rcx*2]
0x8840d6  (f 0x4840d6)  c1e810                 shr eax, 0x10
0x8840d9  (f 0x4840d9)  41c1e403               shl r12d, 3
0x8840dd  (f 0x4840dd)  4c0324c2               add r12, qword ptr [rdx + rax*8]
0x8840e1  (f 0x4840e1)  4d8b0c24               mov r9, qword ptr [r12]
0x8840e5  (f 0x4840e5)  4d39f9                 cmp r9, r15
0x8840e8  (f 0x4840e8)  742e                   je 0x884118
0x8840ea  (f 0x4840ea)  488d3d18416607         lea rdi, [rip + 0x7664118]  ; rip->0x7ee8209 (file 0x7ae8209)
0x8840f1  (f 0x4840f1)  488d1514fc3407         lea rdx, [rip + 0x734fc14]  ; rip->0x7bd3d0c (file 0x77d3d0c)
0x8840f8  (f 0x4840f8)  be7d010000             mov esi, 0x17d
0x8840fd  (f 0x4840fd)  4c89f9                 mov rcx, r15
0x884100  (f 0x484100)  4189d8                 mov r8d, ebx
0x884103  (f 0x484103)  31c0                   xor eax, eax
0x884105  (f 0x484105)  e8a60fcfff             call 0x5750b0
0x88410a  (f 0x48410a)  eb0c                   jmp 0x884118
0x88410c  (f 0x48410c)  4531e4                 xor r12d, r12d
0x88410f  (f 0x48410f)  4d8b0c24               mov r9, qword ptr [r12]
0x884113  (f 0x484113)  4d39f9                 cmp r9, r15
0x884116  (f 0x484116)  75d2                   jne 0x8840ea
0x884118  (f 0x484118)  410fb744240a           movzx eax, word ptr [r12 + 0xa]
0x88411e  (f 0x48411e)  a9506e0000             test eax, 0x6e50
0x884123  (f 0x484123)  740d                   je 0x884132
0x884125  (f 0x484125)  4c89e7                 mov rdi, r12
0x884128  (f 0x484128)  be0000504e             mov esi, 0x4e500000
0x88412d  (f 0x48412d)  e85e44eeff             call 0x768590
0x884132  (f 0x484132)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x884136  (f 0x484136)  c4c178110424           vmovups xmmword ptr [r12], xmm0
0x88413c  (f 0x48413c)  49c744241000000000     mov qword ptr [r12 + 0x10], 0
0x884145  (f 0x484145)  41c7470cffffffff       mov dword ptr [r15 + 0xc], 0xffffffff
0x88414d  (f 0x48414d)  413b5e04               cmp ebx, dword ptr [r14 + 4]
0x884151  (f 0x484151)  7e42                   jle 0x884195
0x884153  (f 0x484153)  488d059eb5ec08         lea rax, [rip + 0x8ecb59e]  ; rip->0x974f6f8 (file 0x934f6f8)
0x88415a  (f 0x48415a)  803800                 cmp byte ptr [rax], 0
0x88415d  (f 0x48415d)  7536                   jne 0x884195
0x88415f  (f 0x48415f)  4180be9400000001       cmp byte ptr [r14 + 0x94], 1
0x884167  (f 0x484167)  752c                   jne 0x884195
0x884169  (f 0x484169)  418b4650               mov eax, dword ptr [r14 + 0x50]
0x88416d  (f 0x48416d)  413b4654               cmp eax, dword ptr [r14 + 0x54]
0x884171  (f 0x484171)  7512                   jne 0x884185
0x884173  (f 0x484173)  498d7648               lea rsi, [r14 + 0x48]
0x884177  (f 0x484177)  498d5654               lea rdx, [r14 + 0x54]
0x88417b  (f 0x48417b)  bf04040000             mov edi, 0x404
0x884180  (f 0x484180)  e81b57baff             call 0x4298a0
0x884185  (f 0x484185)  8d4801                 lea ecx, [rax + 1]
0x884188  (f 0x484188)  4898                   cdqe 
0x88418a  (f 0x48418a)  41894e50               mov dword ptr [r14 + 0x50], ecx
0x88418e  (f 0x48418e)  498b4e48               mov rcx, qword ptr [r14 + 0x48]
0x884192  (f 0x484192)  891c81                 mov dword ptr [rcx + rax*4], ebx
0x884195  (f 0x484195)  5b                     pop rbx
0x884196  (f 0x484196)  415c                   pop r12
0x884198  (f 0x484198)  415e                   pop r14
0x88419a  (f 0x48419a)  415f                   pop r15
0x88419c  (f 0x48419c)  5d                     pop rbp
0x88419d  (f 0x48419d)  c3                     ret 
--- calls: [('0xc84105', '0x5750b0'), ('0xc8412d', '0x768590'), ('0xc84180', '0x4298a0')]

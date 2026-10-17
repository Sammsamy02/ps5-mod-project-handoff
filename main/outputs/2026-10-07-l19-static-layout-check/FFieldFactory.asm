0x764c50  (f 0x364c50)  55                     push rbp
0x764c51  (f 0x364c51)  4889e5                 mov rbp, rsp
0x764c54  (f 0x364c54)  4157                   push r15
0x764c56  (f 0x364c56)  4156                   push r14
0x764c58  (f 0x364c58)  4154                   push r12
0x764c5a  (f 0x364c5a)  53                     push rbx
0x764c5b  (f 0x364c5b)  0fb6052ea00309         movzx eax, byte ptr [rip + 0x903a02e]  ; rip->0x979ec90 (file 0x939ec90)
0x764c62  (f 0x364c62)  89cb                   mov ebx, ecx
0x764c64  (f 0x364c64)  4989d6                 mov r14, rdx
0x764c67  (f 0x364c67)  4989f7                 mov r15, rsi
0x764c6a  (f 0x364c6a)  84c0                   test al, al
0x764c6c  (f 0x364c6c)  0f849a000000           je 0x764d0c
0x764c72  (f 0x364c72)  4c8b27                 mov r12, qword ptr [rdi]
0x764c75  (f 0x364c75)  4489e7                 mov edi, r12d
0x764c78  (f 0x364c78)  e8534eedff             call 0x639ad0
0x764c7d  (f 0x364c7d)  8b0dc59f0309           mov ecx, dword ptr [rip + 0x9039fc5]  ; rip->0x979ec48 (file 0x939ec48)
0x764c83  (f 0x364c83)  3b0deb9f0309           cmp ecx, dword ptr [rip + 0x9039feb]  ; rip->0x979ec74 (file 0x939ec74)
0x764c89  (f 0x364c89)  745d                   je 0x764ce8
0x764c8b  (f 0x364c8b)  4c89e1                 mov rcx, r12
0x764c8e  (f 0x364c8e)  488d15e39f0309         lea rdx, [rip + 0x9039fe3]  ; rip->0x979ec78 (file 0x939ec78)
0x764c95  (f 0x364c95)  48c1e920               shr rcx, 0x20
0x764c99  (f 0x364c99)  01c8                   add eax, ecx
0x764c9b  (f 0x364c9b)  488b0dde9f0309         mov rcx, qword ptr [rip + 0x9039fde]  ; rip->0x979ec80 (file 0x939ec80)
0x764ca2  (f 0x364ca2)  4885c9                 test rcx, rcx
0x764ca5  (f 0x364ca5)  480f45d1               cmovne rdx, rcx
0x764ca9  (f 0x364ca9)  8b0dd99f0309           mov ecx, dword ptr [rip + 0x9039fd9]  ; rip->0x979ec88 (file 0x939ec88)
0x764caf  (f 0x364caf)  ffc9                   dec ecx
0x764cb1  (f 0x364cb1)  21c1                   and ecx, eax
0x764cb3  (f 0x364cb3)  8b048a                 mov eax, dword ptr [rdx + rcx*4]
0x764cb6  (f 0x364cb6)  83f8ff                 cmp eax, -1
0x764cb9  (f 0x364cb9)  742d                   je 0x764ce8
0x764cbb  (f 0x364cbb)  488b0d7e9f0309         mov rcx, qword ptr [rip + 0x9039f7e]  ; rip->0x979ec40 (file 0x939ec40)
0x764cc2  (f 0x364cc2)  66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x764cd0  (f 0x364cd0)  4898                   cdqe 
0x764cd2  (f 0x364cd2)  488d1440               lea rdx, [rax + rax*2]
0x764cd6  (f 0x364cd6)  488d04d1               lea rax, [rcx + rdx*8]
0x764cda  (f 0x364cda)  4c3924d1               cmp qword ptr [rcx + rdx*8], r12
0x764cde  (f 0x364cde)  740a                   je 0x764cea
0x764ce0  (f 0x364ce0)  8b4010                 mov eax, dword ptr [rax + 0x10]
0x764ce3  (f 0x364ce3)  83f8ff                 cmp eax, -1
0x764ce6  (f 0x364ce6)  75e8                   jne 0x764cd0
0x764ce8  (f 0x364ce8)  31c0                   xor eax, eax
0x764cea  (f 0x364cea)  488d4808               lea rcx, [rax + 8]
0x764cee  (f 0x364cee)  4885c0                 test rax, rax
0x764cf1  (f 0x364cf1)  4c89ff                 mov rdi, r15
0x764cf4  (f 0x364cf4)  4c89f6                 mov rsi, r14
0x764cf7  (f 0x364cf7)  89da                   mov edx, ebx
0x764cf9  (f 0x364cf9)  480f44c8               cmove rcx, rax
0x764cfd  (f 0x364cfd)  488b01                 mov rax, qword ptr [rcx]
0x764d00  (f 0x364d00)  ff5030                 call qword ptr [rax + 0x30]
0x764d03  (f 0x364d03)  5b                     pop rbx
0x764d04  (f 0x364d04)  415c                   pop r12
0x764d06  (f 0x364d06)  415e                   pop r14
0x764d08  (f 0x364d08)  415f                   pop r15
0x764d0a  (f 0x364d0a)  5d                     pop rbp
0x764d0b  (f 0x364d0b)  c3                     ret 
0x764d0c  (f 0x364d0c)  488d057d9f0309         lea rax, [rip + 0x9039f7d]  ; rip->0x979ec90 (file 0x939ec90)
0x764d13  (f 0x364d13)  4989fc                 mov r12, rdi
0x764d16  (f 0x364d16)  4889c7                 mov rdi, rax
0x764d19  (f 0x364d19)  e892d11007             call 0x7871eb0
0x764d1e  (f 0x364d1e)  4c89e7                 mov rdi, r12
0x764d21  (f 0x364d21)  85c0                   test eax, eax
0x764d23  (f 0x364d23)  0f8449ffffff           je 0x764c72
0x764d29  (f 0x364d29)  c5f8100def7c6d07       vmovups xmm1, xmmword ptr [rip + 0x76d7cef]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x764d31  (f 0x364d31)  488d35089f0309         lea rsi, [rip + 0x9039f08]  ; rip->0x979ec40 (file 0x939ec40)
0x764d38  (f 0x364d38)  488d3dc130cbff         lea rdi, [rip - 0x34cf3f]  ; rip->0x417e00 (file 0x17e00)
0x764d3f  (f 0x364d3f)  488d15ba72f708         lea rdx, [rip + 0x8f772ba]  ; rip->0x96dc000 (file 0x92dc000)
0x764d46  (f 0x364d46)  48c7050f9f030900000000 mov qword ptr [rip + 0x9039f0f], 0  ; rip->0x979ec60 (file 0x939ec60)
0x764d51  (f 0x364d51)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x764d55  (f 0x364d55)  c5f81105e39e0309       vmovups xmmword ptr [rip + 0x9039ee3], xmm0  ; rip->0x979ec40 (file 0x939ec40)
0x764d5d  (f 0x364d5d)  c5f8110d039f0309       vmovups xmmword ptr [rip + 0x9039f03], xmm1  ; rip->0x979ec68 (file 0x939ec68)
0x764d65  (f 0x364d65)  48c705109f030900000000 mov qword ptr [rip + 0x9039f10], 0  ; rip->0x979ec80 (file 0x939ec80)
0x764d70  (f 0x364d70)  c7050e9f030900000000   mov dword ptr [rip + 0x9039f0e], 0  ; rip->0x979ec88 (file 0x939ec88)
0x764d7a  (f 0x364d7a)  e851d11007             call 0x7871ed0
0x764d7f  (f 0x364d7f)  488d3d0a9f0309         lea rdi, [rip + 0x9039f0a]  ; rip->0x979ec90 (file 0x939ec90)
0x764d86  (f 0x364d86)  e835d11007             call 0x7871ec0
0x764d8b  (f 0x364d8b)  4c89e7                 mov rdi, r12
0x764d8e  (f 0x364d8e)  e9dffeffff             jmp 0x764c72
--- calls: [('0xb64c78', '0x639ad0'), ('0xb64d19', '0x7871eb0'), ('0xb64d7a', '0x7871ed0'), ('0xb64d86', '0x7871ec0')]

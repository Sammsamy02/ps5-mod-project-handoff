0x891db0  (f 0x491db0)  55                     push rbp
0x891db1  (f 0x491db1)  4889e5                 mov rbp, rsp
0x891db4  (f 0x491db4)  4157                   push r15
0x891db6  (f 0x491db6)  4156                   push r14
0x891db8  (f 0x491db8)  4155                   push r13
0x891dba  (f 0x491dba)  4154                   push r12
0x891dbc  (f 0x491dbc)  53                     push rbx
0x891dbd  (f 0x491dbd)  50                     push rax
0x891dbe  (f 0x491dbe)  488937                 mov qword ptr [rdi], rsi
0x891dc1  (f 0x491dc1)  48b90000000008000000   movabs rcx, 0x800000000
0x891dcb  (f 0x491dcb)  4989d7                 mov r15, rdx
0x891dce  (f 0x491dce)  4889fb                 mov rbx, rdi
0x891dd1  (f 0x491dd1)  4c8d7720               lea r14, [rdi + 0x20]
0x891dd5  (f 0x491dd5)  488b4228               mov rax, qword ptr [rdx + 0x28]
0x891dd9  (f 0x491dd9)  48894708               mov qword ptr [rdi + 8], rax
0x891ddd  (f 0x491ddd)  0fb64220               movzx eax, byte ptr [rdx + 0x20]
0x891de1  (f 0x491de1)  884710                 mov byte ptr [rdi + 0x10], al
0x891de4  (f 0x491de4)  c7471101000100         mov dword ptr [rdi + 0x11], 0x10001
0x891deb  (f 0x491deb)  488b4230               mov rax, qword ptr [rdx + 0x30]
0x891def  (f 0x491def)  48894718               mov qword ptr [rdi + 0x18], rax
0x891df3  (f 0x491df3)  48c7872001000000000000 mov qword ptr [rdi + 0x120], 0
0x891dfe  (f 0x491dfe)  48898f28010000         mov qword ptr [rdi + 0x128], rcx
0x891e05  (f 0x491e05)  48c787b001000000000000 mov qword ptr [rdi + 0x1b0], 0
0x891e10  (f 0x491e10)  48898fb8010000         mov qword ptr [rdi + 0x1b8], rcx
0x891e17  (f 0x491e17)  48c787c001000000000000 mov qword ptr [rdi + 0x1c0], 0
0x891e22  (f 0x491e22)  488b4240               mov rax, qword ptr [rdx + 0x40]
0x891e26  (f 0x491e26)  488987d0010000         mov qword ptr [rdi + 0x1d0], rax
0x891e2d  (f 0x491e2d)  48c787f801000000000000 mov qword ptr [rdi + 0x1f8], 0
0x891e38  (f 0x491e38)  4885c0                 test rax, rax
0x891e3b  (f 0x491e3b)  741b                   je 0x891e58
0x891e3d  (f 0x491e3d)  498b4768               mov rax, qword ptr [r15 + 0x68]
0x891e41  (f 0x491e41)  498d7f50               lea rdi, [r15 + 0x50]
0x891e45  (f 0x491e45)  488db3e0010000         lea rsi, [rbx + 0x1e0]
0x891e4c  (f 0x491e4c)  4885c0                 test rax, rax
0x891e4f  (f 0x491e4f)  480f45f8               cmovne rdi, rax
0x891e53  (f 0x491e53)  488b07                 mov rax, qword ptr [rdi]
0x891e56  (f 0x491e56)  ff10                   call qword ptr [rax]
0x891e58  (f 0x491e58)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x891e5c  (f 0x491e5c)  c5f8118300020000       vmovups xmmword ptr [rbx + 0x200], xmm0
0x891e64  (f 0x491e64)  4d8b7f78               mov r15, qword ptr [r15 + 0x78]
0x891e68  (f 0x491e68)  4d85ff                 test r15, r15
0x891e6b  (f 0x491e6b)  0f94c0                 sete al
0x891e6e  (f 0x491e6e)  4d39fe                 cmp r14, r15
0x891e71  (f 0x491e71)  0f94c1                 sete cl
0x891e74  (f 0x491e74)  08c1                   or cl, al
0x891e76  (f 0x491e76)  0f8588000000           jne 0x891f04
0x891e7c  (f 0x491e7c)  488bbb20010000         mov rdi, qword ptr [rbx + 0x120]
0x891e83  (f 0x491e83)  8bb328010000           mov esi, dword ptr [rbx + 0x128]
0x891e89  (f 0x491e89)  4c8dab2c010000         lea r13, [rbx + 0x12c]
0x891e90  (f 0x491e90)  4885ff                 test rdi, rdi
0x891e93  (f 0x491e93)  490f44fe               cmove rdi, r14
0x891e97  (f 0x491e97)  e8c4f8e3ff             call 0x6d1760
0x891e9c  (f 0x491e9c)  8b8b2c010000           mov ecx, dword ptr [rbx + 0x12c]
0x891ea2  (f 0x491ea2)  458ba708010000         mov r12d, dword ptr [r15 + 0x108]
0x891ea9  (f 0x491ea9)  498b8700010000         mov rax, qword ptr [r15 + 0x100]
0x891eb0  (f 0x491eb0)  89ca                   mov edx, ecx
0x891eb2  (f 0x491eb2)  4489a328010000         mov dword ptr [rbx + 0x128], r12d
0x891eb9  (f 0x491eb9)  4409e2                 or edx, r12d
0x891ebc  (f 0x491ebc)  743e                   je 0x891efc
0x891ebe  (f 0x491ebe)  4885c0                 test rax, rax
0x891ec1  (f 0x491ec1)  bf20000000             mov edi, 0x20
0x891ec6  (f 0x491ec6)  be08000000             mov esi, 8
0x891ecb  (f 0x491ecb)  4489e2                 mov edx, r12d
0x891ece  (f 0x491ece)  4d89f0                 mov r8, r14
0x891ed1  (f 0x491ed1)  4589e1                 mov r9d, r12d
0x891ed4  (f 0x491ed4)  4c892c24               mov qword ptr [rsp], r13
0x891ed8  (f 0x491ed8)  4c0f45f8               cmovne r15, rax
0x891edc  (f 0x491edc)  e81ff6e3ff             call 0x6d1500
0x891ee1  (f 0x491ee1)  488bbb20010000         mov rdi, qword ptr [rbx + 0x120]
0x891ee8  (f 0x491ee8)  4c89fe                 mov rsi, r15
0x891eeb  (f 0x491eeb)  4489e2                 mov edx, r12d
0x891eee  (f 0x491eee)  4885ff                 test rdi, rdi
0x891ef1  (f 0x491ef1)  490f44fe               cmove rdi, r14
0x891ef5  (f 0x491ef5)  e8e6f6e3ff             call 0x6d15e0
0x891efa  (f 0x491efa)  eb08                   jmp 0x891f04
0x891efc  (f 0x491efc)  41c7450008000000       mov dword ptr [r13], 8
0x891f04  (f 0x491f04)  4889df                 mov rdi, rbx
0x891f07  (f 0x491f07)  e874fdffff             call 0x891c80
0x891f0c  (f 0x491f0c)  4883c408               add rsp, 8
0x891f10  (f 0x491f10)  5b                     pop rbx
0x891f11  (f 0x491f11)  415c                   pop r12
0x891f13  (f 0x491f13)  415d                   pop r13
0x891f15  (f 0x491f15)  415e                   pop r14
0x891f17  (f 0x491f17)  415f                   pop r15
0x891f19  (f 0x491f19)  5d                     pop rbp
0x891f1a  (f 0x491f1a)  c3                     ret 
--- calls: [('0xc91e97', '0x6d1760'), ('0xc91edc', '0x6d1500'), ('0xc91ef5', '0x6d15e0'), ('0xc91f07', '0x891c80')]

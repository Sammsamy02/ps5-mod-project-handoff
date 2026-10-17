0x726b50  (f 0x326b50)  55                     push rbp
0x726b51  (f 0x326b51)  4889e5                 mov rbp, rsp
0x726b54  (f 0x326b54)  4157                   push r15
0x726b56  (f 0x326b56)  4156                   push r14
0x726b58  (f 0x326b58)  4154                   push r12
0x726b5a  (f 0x326b5a)  53                     push rbx
0x726b5b  (f 0x326b5b)  4883ec10               sub rsp, 0x10
0x726b5f  (f 0x326b5f)  4c8b25a2b3e008         mov r12, qword ptr [rip + 0x8e0b3a2]  ; rip->0x9531f08 (file 0x9131f08)
0x726b66  (f 0x326b66)  4989f6                 mov r14, rsi
0x726b69  (f 0x326b69)  4889fb                 mov rbx, rdi
0x726b6c  (f 0x326b6c)  498b0424               mov rax, qword ptr [r12]
0x726b70  (f 0x326b70)  488945d8               mov qword ptr [rbp - 0x28], rax
0x726b74  (f 0x326b74)  e87760ffff             call 0x71cbf0
0x726b79  (f 0x326b79)  498b4e28               mov rcx, qword ptr [r14 + 0x28]
0x726b7d  (f 0x326b7d)  498b4608               mov rax, qword ptr [r14 + 8]
0x726b81  (f 0x326b81)  4c8dbbb0000000         lea r15, [rbx + 0xb0]
0x726b88  (f 0x326b88)  4889ca                 mov rdx, rcx
0x726b8b  (f 0x326b8b)  4881ca00200000         or rdx, 0x2000
0x726b92  (f 0x326b92)  49895628               mov qword ptr [r14 + 0x28], rdx
0x726b96  (f 0x326b96)  488b30                 mov rsi, qword ptr [rax]
0x726b99  (f 0x326b99)  488d5604               lea rdx, [rsi + 4]
0x726b9d  (f 0x326b9d)  483b5008               cmp rdx, qword ptr [rax + 8]
0x726ba1  (f 0x326ba1)  7624                   jbe 0x726bc7
0x726ba3  (f 0x326ba3)  f7c100000100           test ecx, 0x10000
0x726ba9  (f 0x326ba9)  752c                   jne 0x726bd7
0x726bab  (f 0x326bab)  498b06                 mov rax, qword ptr [r14]
0x726bae  (f 0x326bae)  ba04000000             mov edx, 4
0x726bb3  (f 0x326bb3)  4c89f7                 mov rdi, r14
0x726bb6  (f 0x326bb6)  4c89fe                 mov rsi, r15
0x726bb9  (f 0x326bb9)  ff9088010000           call qword ptr [rax + 0x188]
0x726bbf  (f 0x326bbf)  41f60740               test byte ptr [r15], 0x40
0x726bc3  (f 0x326bc3)  7523                   jne 0x726be8
0x726bc5  (f 0x326bc5)  eb6a                   jmp 0x726c31
0x726bc7  (f 0x326bc7)  8b0e                   mov ecx, dword ptr [rsi]
0x726bc9  (f 0x326bc9)  41890f                 mov dword ptr [r15], ecx
0x726bcc  (f 0x326bcc)  488910                 mov qword ptr [rax], rdx
0x726bcf  (f 0x326bcf)  41f60740               test byte ptr [r15], 0x40
0x726bd3  (f 0x326bd3)  7513                   jne 0x726be8
0x726bd5  (f 0x326bd5)  eb5a                   jmp 0x726c31
0x726bd7  (f 0x326bd7)  4c89f7                 mov rdi, r14
0x726bda  (f 0x326bda)  4c89fe                 mov rsi, r15
0x726bdd  (f 0x326bdd)  e8ce5eefff             call 0x61cab0
0x726be2  (f 0x326be2)  41f60740               test byte ptr [r15], 0x40
0x726be6  (f 0x326be6)  7449                   je 0x726c31
0x726be8  (f 0x326be8)  66c745d60000           mov word ptr [rbp - 0x2a], 0
0x726bee  (f 0x326bee)  498b4608               mov rax, qword ptr [r14 + 8]
0x726bf2  (f 0x326bf2)  488b08                 mov rcx, qword ptr [rax]
0x726bf5  (f 0x326bf5)  4883c102               add rcx, 2
0x726bf9  (f 0x326bf9)  483b4808               cmp rcx, qword ptr [rax + 8]
0x726bfd  (f 0x326bfd)  762f                   jbe 0x726c2e
0x726bff  (f 0x326bff)  41f6462a01             test byte ptr [r14 + 0x2a], 1
0x726c04  (f 0x326c04)  0f8514010000           jne 0x726d1e
0x726c0a  (f 0x326c0a)  498b06                 mov rax, qword ptr [r14]
0x726c0d  (f 0x326c0d)  488d75d6               lea rsi, [rbp - 0x2a]
0x726c11  (f 0x326c11)  ba02000000             mov edx, 2
0x726c16  (f 0x326c16)  4c89f7                 mov rdi, r14
0x726c19  (f 0x326c19)  ff9088010000           call qword ptr [rax + 0x188]
0x726c1f  (f 0x326c1f)  41817e40c3010000       cmp dword ptr [r14 + 0x40], 0x1c3
0x726c27  (f 0x326c27)  7c16                   jl 0x726c3f
0x726c29  (f 0x326c29)  e90a010000             jmp 0x726d38
0x726c2e  (f 0x326c2e)  488908                 mov qword ptr [rax], rcx
0x726c31  (f 0x326c31)  41817e40c3010000       cmp dword ptr [r14 + 0x40], 0x1c3
0x726c39  (f 0x326c39)  0f8df9000000           jge 0x726d38
0x726c3f  (f 0x326c3f)  41f6463510             test byte ptr [r14 + 0x35], 0x10
0x726c44  (f 0x326c44)  0f8554010000           jne 0x726d9e
0x726c4a  (f 0x326c4a)  41f6462801             test byte ptr [r14 + 0x28], 1
0x726c4f  (f 0x326c4f)  0f844d020000           je 0x726ea2
0x726c55  (f 0x326c55)  c683b400000000         mov byte ptr [rbx + 0xb4], 0
0x726c5c  (f 0x326c5c)  c783b60000000000ffff   mov dword ptr [rbx + 0xb6], 0xffff0000
0x726c66  (f 0x326c66)  488b4350               mov rax, qword ptr [rbx + 0x50]
0x726c6a  (f 0x326c6a)  4885c0                 test rax, rax
0x726c6d  (f 0x326c6d)  0f842f020000           je 0x726ea2
0x726c73  (f 0x326c73)  488b4808               mov rcx, qword ptr [rax + 8]
0x726c77  (f 0x326c77)  f6411180               test byte ptr [rcx + 0x11], 0x80
0x726c7b  (f 0x326c7b)  0f8421020000           je 0x726ea2
0x726c81  (f 0x326c81)  488d93c0000000         lea rdx, [rbx + 0xc0]
0x726c88  (f 0x326c88)  31c9                   xor ecx, ecx
0x726c8a  (f 0x326c8a)  660f1f440000           nop word ptr [rax + rax]
0x726c90  (f 0x326c90)  488b7038               mov rsi, qword ptr [rax + 0x38]
0x726c94  (f 0x326c94)  4084f6                 test sil, sil
0x726c97  (f 0x326c97)  7837                   js 0x726cd0
0x726c99  (f 0x326c99)  41f6470280             test byte ptr [r15 + 2], 0x80
0x726c9e  (f 0x326c9e)  0f84fe010000           je 0x726ea2
0x726ca4  (f 0x326ca4)  f7c600020000           test esi, 0x200
0x726caa  (f 0x326caa)  7552                   jne 0x726cfe
0x726cac  (f 0x326cac)  483902                 cmp qword ptr [rdx], rax
0x726caf  (f 0x326caf)  7403                   je 0x726cb4
0x726cb1  (f 0x326cb1)  488902                 mov qword ptr [rdx], rax
0x726cb4  (f 0x326cb4)  4883786000             cmp qword ptr [rax + 0x60], 0
0x726cb9  (f 0x326cb9)  488d5060               lea rdx, [rax + 0x60]
0x726cbd  (f 0x326cbd)  743f                   je 0x726cfe
0x726cbf  (f 0x326cbf)  48c70200000000         mov qword ptr [rdx], 0
0x726cc6  (f 0x326cc6)  eb36                   jmp 0x726cfe
0x726cd0  (f 0x326cd0)  fec1                   inc cl
0x726cd2  (f 0x326cd2)  888bb4000000           mov byte ptr [rbx + 0xb4], cl
0x726cd8  (f 0x326cd8)  448b4030               mov r8d, dword ptr [rax + 0x30]
0x726cdc  (f 0x326cdc)  440faf402c             imul r8d, dword ptr [rax + 0x2c]
0x726ce1  (f 0x326ce1)  8b7844                 mov edi, dword ptr [rax + 0x44]
0x726ce4  (f 0x326ce4)  4101f8                 add r8d, edi
0x726ce7  (f 0x326ce7)  66448983b6000000       mov word ptr [rbx + 0xb6], r8w
0x726cef  (f 0x326cef)  f7c600040000           test esi, 0x400
0x726cf5  (f 0x326cf5)  7407                   je 0x726cfe
0x726cf7  (f 0x326cf7)  6689bbb8000000         mov word ptr [rbx + 0xb8], di
0x726cfe  (f 0x326cfe)  488b4018               mov rax, qword ptr [rax + 0x18]
0x726d02  (f 0x326d02)  4885c0                 test rax, rax
0x726d05  (f 0x326d05)  0f8497010000           je 0x726ea2
0x726d0b  (f 0x326d0b)  488b7008               mov rsi, qword ptr [rax + 8]
0x726d0f  (f 0x326d0f)  f6461180               test byte ptr [rsi + 0x11], 0x80
0x726d13  (f 0x326d13)  0f8577ffffff           jne 0x726c90
0x726d19  (f 0x326d19)  e984010000             jmp 0x726ea2
0x726d1e  (f 0x326d1e)  488d75d6               lea rsi, [rbp - 0x2a]
0x726d22  (f 0x326d22)  4c89f7                 mov rdi, r14
0x726d25  (f 0x326d25)  e8065defff             call 0x61ca30
0x726d2a  (f 0x326d2a)  41817e40c3010000       cmp dword ptr [r14 + 0x40], 0x1c3
0x726d32  (f 0x326d32)  0f8c07ffffff           jl 0x726c3f
0x726d38  (f 0x326d38)  498b06                 mov rax, qword ptr [r14]
0x726d3b  (f 0x326d3b)  488db3c8000000         lea rsi, [rbx + 0xc8]
0x726d42  (f 0x326d42)  4c89f7                 mov rdi, r14
0x726d45  (f 0x326d45)  ff9048010000           call qword ptr [rax + 0x148]
0x726d4b  (f 0x326d4b)  498b4608               mov rax, qword ptr [r14 + 8]
0x726d4f  (f 0x326d4f)  488db3d0000000         lea rsi, [rbx + 0xd0]
0x726d56  (f 0x326d56)  488b10                 mov rdx, qword ptr [rax]
0x726d59  (f 0x326d59)  488d4a04               lea rcx, [rdx + 4]
0x726d5d  (f 0x326d5d)  483b4808               cmp rcx, qword ptr [rax + 8]
0x726d61  (f 0x326d61)  7629                   jbe 0x726d8c
0x726d63  (f 0x326d63)  41f6462a01             test byte ptr [r14 + 0x2a], 1
0x726d68  (f 0x326d68)  0f85b9000000           jne 0x726e27
0x726d6e  (f 0x326d6e)  498b06                 mov rax, qword ptr [r14]
0x726d71  (f 0x326d71)  ba04000000             mov edx, 4
0x726d76  (f 0x326d76)  4c89f7                 mov rdi, r14
0x726d79  (f 0x326d79)  ff9088010000           call qword ptr [rax + 0x188]
0x726d7f  (f 0x326d7f)  41f6463510             test byte ptr [r14 + 0x35], 0x10
0x726d84  (f 0x326d84)  0f84c0feffff           je 0x726c4a
0x726d8a  (f 0x326d8a)  eb12                   jmp 0x726d9e
0x726d8c  (f 0x326d8c)  8b12                   mov edx, dword ptr [rdx]
0x726d8e  (f 0x326d8e)  8916                   mov dword ptr [rsi], edx
0x726d90  (f 0x326d90)  488908                 mov qword ptr [rax], rcx
0x726d93  (f 0x326d93)  41f6463510             test byte ptr [r14 + 0x35], 0x10
0x726d98  (f 0x326d98)  0f84acfeffff           je 0x726c4a
0x726d9e  (f 0x326d9e)  498b4e08               mov rcx, qword ptr [r14 + 8]
0x726da2  (f 0x326da2)  488db3b4000000         lea rsi, [rbx + 0xb4]
0x726da9  (f 0x326da9)  488b01                 mov rax, qword ptr [rcx]
0x726dac  (f 0x326dac)  488d5001               lea rdx, [rax + 1]
0x726db0  (f 0x326db0)  483b5108               cmp rdx, qword ptr [rcx + 8]
0x726db4  (f 0x326db4)  7634                   jbe 0x726dea
0x726db6  (f 0x326db6)  498b06                 mov rax, qword ptr [r14]
0x726db9  (f 0x326db9)  ba01000000             mov edx, 1
0x726dbe  (f 0x326dbe)  4c89f7                 mov rdi, r14
0x726dc1  (f 0x326dc1)  ff9088010000           call qword ptr [rax + 0x188]
0x726dc7  (f 0x326dc7)  498b4608               mov rax, qword ptr [r14 + 8]
0x726dcb  (f 0x326dcb)  488b08                 mov rcx, qword ptr [rax]
0x726dce  (f 0x326dce)  488db3b6000000         lea rsi, [rbx + 0xb6]
0x726dd5  (f 0x326dd5)  488d5102               lea rdx, [rcx + 2]
0x726dd9  (f 0x326dd9)  483b5008               cmp rdx, qword ptr [rax + 8]
0x726ddd  (f 0x326ddd)  772e                   ja 0x726e0d
0x726ddf  (f 0x326ddf)  0fb709                 movzx ecx, word ptr [rcx]
0x726de2  (f 0x326de2)  66890e                 mov word ptr [rsi], cx
0x726de5  (f 0x326de5)  488910                 mov qword ptr [rax], rdx
0x726de8  (f 0x326de8)  eb5d                   jmp 0x726e47
0x726dea  (f 0x326dea)  0fb600                 movzx eax, byte ptr [rax]
0x726ded  (f 0x326ded)  8806                   mov byte ptr [rsi], al
0x726def  (f 0x326def)  498b4608               mov rax, qword ptr [r14 + 8]
0x726df3  (f 0x326df3)  488b08                 mov rcx, qword ptr [rax]
0x726df6  (f 0x326df6)  48ffc1                 inc rcx
0x726df9  (f 0x326df9)  488908                 mov qword ptr [rax], rcx
0x726dfc  (f 0x326dfc)  488db3b6000000         lea rsi, [rbx + 0xb6]
0x726e03  (f 0x326e03)  488d5102               lea rdx, [rcx + 2]
0x726e07  (f 0x326e07)  483b5008               cmp rdx, qword ptr [rax + 8]
0x726e0b  (f 0x326e0b)  76d2                   jbe 0x726ddf
0x726e0d  (f 0x326e0d)  41f6462a01             test byte ptr [r14 + 0x2a], 1
0x726e12  (f 0x326e12)  752b                   jne 0x726e3f
0x726e14  (f 0x326e14)  498b06                 mov rax, qword ptr [r14]
0x726e17  (f 0x326e17)  ba02000000             mov edx, 2
0x726e1c  (f 0x326e1c)  4c89f7                 mov rdi, r14
0x726e1f  (f 0x326e1f)  ff9088010000           call qword ptr [rax + 0x188]
0x726e25  (f 0x326e25)  eb20                   jmp 0x726e47
0x726e27  (f 0x326e27)  4c89f7                 mov rdi, r14
0x726e2a  (f 0x326e2a)  e8815cefff             call 0x61cab0
0x726e2f  (f 0x326e2f)  41f6463510             test byte ptr [r14 + 0x35], 0x10
0x726e34  (f 0x326e34)  0f8410feffff           je 0x726c4a
0x726e3a  (f 0x326e3a)  e95fffffff             jmp 0x726d9e
0x726e3f  (f 0x326e3f)  4c89f7                 mov rdi, r14
0x726e42  (f 0x326e42)  e8e95befff             call 0x61ca30
0x726e47  (f 0x326e47)  498b4608               mov rax, qword ptr [r14 + 8]
0x726e4b  (f 0x326e4b)  488db3b8000000         lea rsi, [rbx + 0xb8]
0x726e52  (f 0x326e52)  488b10                 mov rdx, qword ptr [rax]
0x726e55  (f 0x326e55)  488d4a02               lea rcx, [rdx + 2]
0x726e59  (f 0x326e59)  483b4808               cmp rcx, qword ptr [rax + 8]
0x726e5d  (f 0x326e5d)  761a                   jbe 0x726e79
0x726e5f  (f 0x326e5f)  41f6462a01             test byte ptr [r14 + 0x2a], 1
0x726e64  (f 0x326e64)  751e                   jne 0x726e84
0x726e66  (f 0x326e66)  498b06                 mov rax, qword ptr [r14]
0x726e69  (f 0x326e69)  ba02000000             mov edx, 2
0x726e6e  (f 0x326e6e)  4c89f7                 mov rdi, r14
0x726e71  (f 0x326e71)  ff9088010000           call qword ptr [rax + 0x188]
0x726e77  (f 0x326e77)  eb13                   jmp 0x726e8c
0x726e79  (f 0x326e79)  0fb712                 movzx edx, word ptr [rdx]
0x726e7c  (f 0x326e7c)  668916                 mov word ptr [rsi], dx
0x726e7f  (f 0x326e7f)  488908                 mov qword ptr [rax], rcx
0x726e82  (f 0x326e82)  eb08                   jmp 0x726e8c
0x726e84  (f 0x326e84)  4c89f7                 mov rdi, r14
0x726e87  (f 0x326e87)  e8a45befff             call 0x61ca30
0x726e8c  (f 0x326e8c)  498b06                 mov rax, qword ptr [r14]
0x726e8f  (f 0x326e8f)  4881c3c0000000         add rbx, 0xc0
0x726e96  (f 0x326e96)  4c89f7                 mov rdi, r14
0x726e99  (f 0x326e99)  4889de                 mov rsi, rbx
0x726e9c  (f 0x326e9c)  ff9050010000           call qword ptr [rax + 0x150]
0x726ea2  (f 0x326ea2)  498b0424               mov rax, qword ptr [r12]
0x726ea6  (f 0x326ea6)  483b45d8               cmp rax, qword ptr [rbp - 0x28]
0x726eaa  (f 0x326eaa)  750d                   jne 0x726eb9
0x726eac  (f 0x326eac)  4883c410               add rsp, 0x10
0x726eb0  (f 0x326eb0)  5b                     pop rbx
0x726eb1  (f 0x326eb1)  415c                   pop r12
0x726eb3  (f 0x326eb3)  415e                   pop r14
0x726eb5  (f 0x326eb5)  415f                   pop r15
0x726eb7  (f 0x326eb7)  5d                     pop rbp
0x726eb8  (f 0x326eb8)  c3                     ret 
0x726eb9  (f 0x326eb9)  e8a2af1407             call 0x7871e60
0x726ebe  (f 0x326ebe)  0f0b                   ud2 
--- calls: [('0xb26b74', '0x71cbf0'), ('0xb26eb9', '0x7871e60'), ('0xb26e87', '0x61ca30'), ('0xb26e42', '0x61ca30'), ('0xb26e2a', '0x61cab0'), ('0xb26d25', '0x61ca30'), ('0xb26bdd', '0x61cab0')]

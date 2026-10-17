0x71cbf0  (f 0x31cbf0)  55                     push rbp
0x71cbf1  (f 0x31cbf1)  4889e5                 mov rbp, rsp
0x71cbf4  (f 0x31cbf4)  4157                   push r15
0x71cbf6  (f 0x31cbf6)  4156                   push r14
0x71cbf8  (f 0x31cbf8)  4155                   push r13
0x71cbfa  (f 0x31cbfa)  4154                   push r12
0x71cbfc  (f 0x31cbfc)  53                     push rbx
0x71cbfd  (f 0x31cbfd)  4881ecb8010000         sub rsp, 0x1b8
0x71cc04  (f 0x31cc04)  4c8b2dfd52e108         mov r13, qword ptr [rip + 0x8e152fd]  ; rip->0x9531f08 (file 0x9131f08)
0x71cc0b  (f 0x31cc0b)  4889f3                 mov rbx, rsi
0x71cc0e  (f 0x31cc0e)  4989fe                 mov r14, rdi
0x71cc11  (f 0x31cc11)  498b4500               mov rax, qword ptr [r13]
0x71cc15  (f 0x31cc15)  488945d0               mov qword ptr [rbp - 0x30], rax
0x71cc19  (f 0x31cc19)  e8b26a0a00             call 0x7c36d0
0x71cc1e  (f 0x31cc1e)  4c8d3d9f359107         lea r15, [rip + 0x791359f]  ; rip->0x80301c4 (file 0x7c301c4)
0x71cc25  (f 0x31cc25)  488b03                 mov rax, qword ptr [rbx]
0x71cc28  (f 0x31cc28)  4889df                 mov rdi, rbx
0x71cc2b  (f 0x31cc2b)  4c89fe                 mov rsi, r15
0x71cc2e  (f 0x31cc2e)  ff9030020000           call qword ptr [rax + 0x230]
0x71cc34  (f 0x31cc34)  4889df                 mov rdi, rbx
0x71cc37  (f 0x31cc37)  4c89fe                 mov rsi, r15
0x71cc3a  (f 0x31cc3a)  e851f0efff             call 0x61bc90
0x71cc3f  (f 0x31cc3f)  83f81c                 cmp eax, 0x1c
0x71cc42  (f 0x31cc42)  7f10                   jg 0x71cc54
0x71cc44  (f 0x31cc44)  488b03                 mov rax, qword ptr [rbx]
0x71cc47  (f 0x31cc47)  498d7628               lea rsi, [r14 + 0x28]
0x71cc4b  (f 0x31cc4b)  4889df                 mov rdi, rbx
0x71cc4e  (f 0x31cc4e)  ff9048010000           call qword ptr [rax + 0x148]
0x71cc54  (f 0x31cc54)  488b03                 mov rax, qword ptr [rbx]
0x71cc57  (f 0x31cc57)  4d8b6640               mov r12, qword ptr [r14 + 0x40]
0x71cc5b  (f 0x31cc5b)  4d8d7e40               lea r15, [r14 + 0x40]
0x71cc5f  (f 0x31cc5f)  4889df                 mov rdi, rbx
0x71cc62  (f 0x31cc62)  4c89fe                 mov rsi, r15
0x71cc65  (f 0x31cc65)  ff9060010000           call qword ptr [rax + 0x160]
0x71cc6b  (f 0x31cc6b)  f6432801               test byte ptr [rbx + 0x28], 1
0x71cc6f  (f 0x31cc6f)  0f8581000000           jne 0x71ccf6
0x71cc75  (f 0x31cc75)  4d85e4                 test r12, r12
0x71cc78  (f 0x31cc78)  0f84fd000000           je 0x71cd7b
0x71cc7e  (f 0x31cc7e)  4d3b27                 cmp r12, qword ptr [r15]
0x71cc81  (f 0x31cc81)  0f84f4000000           je 0x71cd7b
0x71cc87  (f 0x31cc87)  498b7e30               mov rdi, qword ptr [r14 + 0x30]
0x71cc8b  (f 0x31cc8b)  4885ff                 test rdi, rdi
0x71cc8e  (f 0x31cc8e)  7405                   je 0x71cc95
0x71cc90  (f 0x31cc90)  e82b0cd3ff             call 0x44d8c0
0x71cc95  (f 0x31cc95)  4d85f6                 test r14, r14
0x71cc98  (f 0x31cc98)  0f84c8000000           je 0x71cd66
0x71cc9e  (f 0x31cc9e)  49c7c4f8ffffff         mov r12, 0xfffffffffffffff8
0x71cca5  (f 0x31cca5)  41bfffffffff           mov r15d, 0xffffffff
0x71ccab  (f 0x31ccab)  4c89f0                 mov rax, r14
0x71ccae  (f 0x31ccae)  6690                   nop 
0x71ccb0  (f 0x31ccb0)  488b4040               mov rax, qword ptr [rax + 0x40]
0x71ccb4  (f 0x31ccb4)  4983c408               add r12, 8
0x71ccb8  (f 0x31ccb8)  41ffc7                 inc r15d
0x71ccbb  (f 0x31ccbb)  4885c0                 test rax, rax
0x71ccbe  (f 0x31ccbe)  75f0                   jne 0x71ccb0
0x71ccc0  (f 0x31ccc0)  498d7c2408             lea rdi, [r12 + 8]
0x71ccc5  (f 0x31ccc5)  e8b69f5704             call 0x4c96c80
0x71ccca  (f 0x31ccca)  4889c1                 mov rcx, rax
0x71cccd  (f 0x31cccd)  4c89f2                 mov rdx, r14
0x71ccd0  (f 0x31ccd0)  4c01e1                 add rcx, r12
0x71ccd3  (f 0x31ccd3)  666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x71cce0  (f 0x31cce0)  488d7230               lea rsi, [rdx + 0x30]
0x71cce4  (f 0x31cce4)  488931                 mov qword ptr [rcx], rsi
0x71cce7  (f 0x31cce7)  4883c1f8               add rcx, -8
0x71cceb  (f 0x31cceb)  488b5240               mov rdx, qword ptr [rdx + 0x40]
0x71ccef  (f 0x31ccef)  4885d2                 test rdx, rdx
0x71ccf2  (f 0x31ccf2)  75ec                   jne 0x71cce0
0x71ccf4  (f 0x31ccf4)  eb7d                   jmp 0x71cd73
0x71ccf6  (f 0x31ccf6)  498b7e30               mov rdi, qword ptr [r14 + 0x30]
0x71ccfa  (f 0x31ccfa)  4885ff                 test rdi, rdi
0x71ccfd  (f 0x31ccfd)  7405                   je 0x71cd04
0x71ccff  (f 0x31ccff)  e8bc0bd3ff             call 0x44d8c0
0x71cd04  (f 0x31cd04)  4d85f6                 test r14, r14
0x71cd07  (f 0x31cd07)  745d                   je 0x71cd66
0x71cd09  (f 0x31cd09)  49c7c4f8ffffff         mov r12, 0xfffffffffffffff8
0x71cd10  (f 0x31cd10)  41bfffffffff           mov r15d, 0xffffffff
0x71cd16  (f 0x31cd16)  4c89f0                 mov rax, r14
0x71cd19  (f 0x31cd19)  0f1f8000000000         nop dword ptr [rax]
0x71cd20  (f 0x31cd20)  488b4040               mov rax, qword ptr [rax + 0x40]
0x71cd24  (f 0x31cd24)  4983c408               add r12, 8
0x71cd28  (f 0x31cd28)  41ffc7                 inc r15d
0x71cd2b  (f 0x31cd2b)  4885c0                 test rax, rax
0x71cd2e  (f 0x31cd2e)  75f0                   jne 0x71cd20
0x71cd30  (f 0x31cd30)  498d7c2408             lea rdi, [r12 + 8]
0x71cd35  (f 0x31cd35)  e8469f5704             call 0x4c96c80
0x71cd3a  (f 0x31cd3a)  4889c1                 mov rcx, rax
0x71cd3d  (f 0x31cd3d)  4c89f2                 mov rdx, r14
0x71cd40  (f 0x31cd40)  4c01e1                 add rcx, r12
0x71cd43  (f 0x31cd43)  666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x71cd50  (f 0x31cd50)  488d7230               lea rsi, [rdx + 0x30]
0x71cd54  (f 0x31cd54)  488931                 mov qword ptr [rcx], rsi
0x71cd57  (f 0x31cd57)  4883c1f8               add rcx, -8
0x71cd5b  (f 0x31cd5b)  488b5240               mov rdx, qword ptr [rdx + 0x40]
0x71cd5f  (f 0x31cd5f)  4885d2                 test rdx, rdx
0x71cd62  (f 0x31cd62)  75ec                   jne 0x71cd50
0x71cd64  (f 0x31cd64)  eb0d                   jmp 0x71cd73
0x71cd66  (f 0x31cd66)  31ff                   xor edi, edi
0x71cd68  (f 0x31cd68)  e8139f5704             call 0x4c96c80
0x71cd6d  (f 0x31cd6d)  41bfffffffff           mov r15d, 0xffffffff
0x71cd73  (f 0x31cd73)  49894630               mov qword ptr [r14 + 0x30], rax
0x71cd77  (f 0x31cd77)  45897e38               mov dword ptr [r14 + 0x38], r15d
0x71cd7b  (f 0x31cd7b)  4c8d3d42349107         lea r15, [rip + 0x7913442]  ; rip->0x80301c4 (file 0x7c301c4)
0x71cd82  (f 0x31cd82)  488b03                 mov rax, qword ptr [rbx]
0x71cd85  (f 0x31cd85)  4889df                 mov rdi, rbx
0x71cd88  (f 0x31cd88)  4c89fe                 mov rsi, r15
0x71cd8b  (f 0x31cd8b)  ff9030020000           call qword ptr [rax + 0x230]
0x71cd91  (f 0x31cd91)  488b03                 mov rax, qword ptr [rbx]
0x71cd94  (f 0x31cd94)  488d3509349107         lea rsi, [rip + 0x7913409]  ; rip->0x80301a4 (file 0x7c301a4)
0x71cd9b  (f 0x31cd9b)  4889df                 mov rdi, rbx
0x71cd9e  (f 0x31cd9e)  ff9030020000           call qword ptr [rax + 0x230]
0x71cda4  (f 0x31cda4)  4889df                 mov rdi, rbx
0x71cda7  (f 0x31cda7)  4c89fe                 mov rsi, r15
0x71cdaa  (f 0x31cdaa)  e8e1eeefff             call 0x61bc90
0x71cdaf  (f 0x31cdaf)  83f81c                 cmp eax, 0x1c
0x71cdb2  (f 0x31cdb2)  7f15                   jg 0x71cdc9
0x71cdb4  (f 0x31cdb4)  488b03                 mov rax, qword ptr [rbx]
0x71cdb7  (f 0x31cdb7)  498d7648               lea rsi, [r14 + 0x48]
0x71cdbb  (f 0x31cdbb)  4889df                 mov rdi, rbx
0x71cdbe  (f 0x31cdbe)  ff9060010000           call qword ptr [rax + 0x160]
0x71cdc4  (f 0x31cdc4)  e9ec000000             jmp 0x71ceb5
0x71cdc9  (f 0x31cdc9)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71cdcd  (f 0x31cdcd)  c5f8118540feffff       vmovups xmmword ptr [rbp - 0x1c0], xmm0
0x71cdd5  (f 0x31cdd5)  f6432801               test byte ptr [rbx + 0x28], 1
0x71cdd9  (f 0x31cdd9)  755e                   jne 0x71ce39
0x71cddb  (f 0x31cddb)  4d8b6e48               mov r13, qword ptr [r14 + 0x48]
0x71cddf  (f 0x31cddf)  4d85ed                 test r13, r13
0x71cde2  (f 0x31cde2)  0f84a6000000           je 0x71ce8e
0x71cde8  (f 0x31cde8)  4c8dbd4cfeffff         lea r15, [rbp - 0x1b4]
0x71cdef  (f 0x31cdef)  4c8da540feffff         lea r12, [rbp - 0x1c0]
0x71cdf6  (f 0x31cdf6)  31c0                   xor eax, eax
0x71cdf8  (f 0x31cdf8)  eb25                   jmp 0x71ce1f
0x71ce00  (f 0x31ce00)  4863c8                 movsxd rcx, eax
0x71ce03  (f 0x31ce03)  ffc0                   inc eax
0x71ce05  (f 0x31ce05)  898548feffff           mov dword ptr [rbp - 0x1b8], eax
0x71ce0b  (f 0x31ce0b)  488b9540feffff         mov rdx, qword ptr [rbp - 0x1c0]
0x71ce12  (f 0x31ce12)  4c892cca               mov qword ptr [rdx + rcx*8], r13
0x71ce16  (f 0x31ce16)  4d8b6d28               mov r13, qword ptr [r13 + 0x28]
0x71ce1a  (f 0x31ce1a)  4d85ed                 test r13, r13
0x71ce1d  (f 0x31ce1d)  746f                   je 0x71ce8e
0x71ce1f  (f 0x31ce1f)  3b854cfeffff           cmp eax, dword ptr [rbp - 0x1b4]
0x71ce25  (f 0x31ce25)  75d9                   jne 0x71ce00
0x71ce27  (f 0x31ce27)  bf08080000             mov edi, 0x808
0x71ce2c  (f 0x31ce2c)  4c89e6                 mov rsi, r12
0x71ce2f  (f 0x31ce2f)  4c89fa                 mov rdx, r15
0x71ce32  (f 0x31ce32)  e869cad0ff             call 0x4298a0
0x71ce37  (f 0x31ce37)  ebc7                   jmp 0x71ce00
0x71ce39  (f 0x31ce39)  488db540feffff         lea rsi, [rbp - 0x1c0]
0x71ce40  (f 0x31ce40)  4889df                 mov rdi, rbx
0x71ce43  (f 0x31ce43)  e858300100             call 0x72fea0
0x71ce48  (f 0x31ce48)  49c7464800000000       mov qword ptr [r14 + 0x48], 0
0x71ce50  (f 0x31ce50)  48638548feffff         movsxd rax, dword ptr [rbp - 0x1b8]
0x71ce57  (f 0x31ce57)  4885c0                 test rax, rax
0x71ce5a  (f 0x31ce5a)  7e48                   jle 0x71cea4
0x71ce5c  (f 0x31ce5c)  488b8d40feffff         mov rcx, qword ptr [rbp - 0x1c0]
0x71ce63  (f 0x31ce63)  4889c2                 mov rdx, rax
0x71ce66  (f 0x31ce66)  41bc01000000           mov r12d, 1
0x71ce6c  (f 0x31ce6c)  48f7da                 neg rdx
0x71ce6f  (f 0x31ce6f)  90                     nop 
0x71ce70  (f 0x31ce70)  4e8b7ce1f8             mov r15, qword ptr [rcx + r12*8 - 8]
0x71ce75  (f 0x31ce75)  4d85ff                 test r15, r15
0x71ce78  (f 0x31ce78)  0f852c010000           jne 0x71cfaa
0x71ce7e  (f 0x31ce7e)  4a8d742201             lea rsi, [rdx + r12 + 1]
0x71ce83  (f 0x31ce83)  49ffc4                 inc r12
0x71ce86  (f 0x31ce86)  4883fe01               cmp rsi, 1
0x71ce8a  (f 0x31ce8a)  75e4                   jne 0x71ce70
0x71ce8c  (f 0x31ce8c)  eb16                   jmp 0x71cea4
0x71ce8e  (f 0x31ce8e)  488db540feffff         lea rsi, [rbp - 0x1c0]
0x71ce95  (f 0x31ce95)  4889df                 mov rdi, rbx
0x71ce98  (f 0x31ce98)  e803300100             call 0x72fea0
0x71ce9d  (f 0x31ce9d)  4c8b2d6450e108         mov r13, qword ptr [rip + 0x8e15064]  ; rip->0x9531f08 (file 0x9131f08)
0x71cea4  (f 0x31cea4)  488bbd40feffff         mov rdi, qword ptr [rbp - 0x1c0]
0x71ceab  (f 0x31ceab)  4885ff                 test rdi, rdi
0x71ceae  (f 0x31ceae)  7405                   je 0x71ceb5
0x71ceb0  (f 0x31ceb0)  e8dbabd9ff             call 0x4b7a90
0x71ceb5  (f 0x31ceb5)  488d35e8329107         lea rsi, [rip + 0x79132e8]  ; rip->0x80301a4 (file 0x7c301a4)
0x71cebc  (f 0x31cebc)  4889df                 mov rdi, rbx
0x71cebf  (f 0x31cebf)  e8ccedefff             call 0x61bc90
0x71cec4  (f 0x31cec4)  83f804                 cmp eax, 4
0x71cec7  (f 0x31cec7)  7c0b                   jl 0x71ced4
0x71cec9  (f 0x31cec9)  4c89f7                 mov rdi, r14
0x71cecc  (f 0x31cecc)  4889de                 mov rsi, rbx
0x71cecf  (f 0x31cecf)  e81c060000             call 0x71d4f0
0x71ced4  (f 0x31ced4)  488b4328               mov rax, qword ptr [rbx + 0x28]
0x71ced8  (f 0x31ced8)  a801                   test al, 1
0x71ceda  (f 0x31ceda)  7550                   jne 0x71cf2c
0x71cedc  (f 0x31cedc)  418b4e68               mov ecx, dword ptr [r14 + 0x68]
0x71cee0  (f 0x31cee0)  49c7c7ffffffff         mov r15, 0xffffffffffffffff
0x71cee7  (f 0x31cee7)  894dcc                 mov dword ptr [rbp - 0x34], ecx
0x71ceea  (f 0x31ceea)  a804                   test al, 4
0x71ceec  (f 0x31ceec)  0f8493010000           je 0x71d085
0x71cef2  (f 0x31cef2)  488b4b08               mov rcx, qword ptr [rbx + 8]
0x71cef6  (f 0x31cef6)  488b31                 mov rsi, qword ptr [rcx]
0x71cef9  (f 0x31cef9)  488d5604               lea rdx, [rsi + 4]
0x71cefd  (f 0x31cefd)  483b5108               cmp rdx, qword ptr [rcx + 8]
0x71cf01  (f 0x31cf01)  0f8699000000           jbe 0x71cfa0
0x71cf07  (f 0x31cf07)  a900000100             test eax, 0x10000
0x71cf0c  (f 0x31cf0c)  0f8506010000           jne 0x71d018
0x71cf12  (f 0x31cf12)  488b03                 mov rax, qword ptr [rbx]
0x71cf15  (f 0x31cf15)  488d75cc               lea rsi, [rbp - 0x34]
0x71cf19  (f 0x31cf19)  ba04000000             mov edx, 4
0x71cf1e  (f 0x31cf1e)  4889df                 mov rdi, rbx
0x71cf21  (f 0x31cf21)  ff9088010000           call qword ptr [rax + 0x188]
0x71cf27  (f 0x31cf27)  e9f8000000             jmp 0x71d024
0x71cf2c  (f 0x31cf2c)  4c8dbd40feffff         lea r15, [rbp - 0x1c0]
0x71cf33  (f 0x31cf33)  4c89f6                 mov rsi, r14
0x71cf36  (f 0x31cf36)  4889da                 mov rdx, rbx
0x71cf39  (f 0x31cf39)  4c89ff                 mov rdi, r15
0x71cf3c  (f 0x31cf3c)  e83fc31400             call 0x869280
0x71cf41  (f 0x31cf41)  4c89ff                 mov rdi, r15
0x71cf44  (f 0x31cf44)  4c89f6                 mov rsi, r14
0x71cf47  (f 0x31cf47)  4889da                 mov rdx, rbx
0x71cf4a  (f 0x31cf4a)  b901000000             mov ecx, 1
0x71cf4f  (f 0x31cf4f)  e8ecbd1400             call 0x868d40
0x71cf54  (f 0x31cf54)  4d85f6                 test r14, r14
0x71cf57  (f 0x31cf57)  7424                   je 0x71cf7d
0x71cf59  (f 0x31cf59)  e862950000             call 0x7264c0
0x71cf5e  (f 0x31cf5e)  498b4e10               mov rcx, qword ptr [r14 + 0x10]
0x71cf62  (f 0x31cf62)  48635038               movsxd rdx, dword ptr [rax + 0x38]
0x71cf66  (f 0x31cf66)  3b5138                 cmp edx, dword ptr [rcx + 0x38]
0x71cf69  (f 0x31cf69)  7f12                   jg 0x71cf7d
0x71cf6b  (f 0x31cf6b)  488b4930               mov rcx, qword ptr [rcx + 0x30]
0x71cf6f  (f 0x31cf6f)  4883c030               add rax, 0x30
0x71cf73  (f 0x31cf73)  483904d1               cmp qword ptr [rcx + rdx*8], rax
0x71cf77  (f 0x31cf77)  0f8406050000           je 0x71d483
0x71cf7d  (f 0x31cf7d)  f6433510               test byte ptr [rbx + 0x35], 0x10
0x71cf81  (f 0x31cf81)  0f85fc040000           jne 0x71d483
0x71cf87  (f 0x31cf87)  498b06                 mov rax, qword ptr [r14]
0x71cf8a  (f 0x31cf8a)  4c89f7                 mov rdi, r14
0x71cf8d  (f 0x31cf8d)  4889de                 mov rsi, rbx
0x71cf90  (f 0x31cf90)  ba01000000             mov edx, 1
0x71cf95  (f 0x31cf95)  ff90d0020000           call qword ptr [rax + 0x2d0]
0x71cf9b  (f 0x31cf9b)  e9e3040000             jmp 0x71d483
0x71cfa0  (f 0x31cfa0)  8b06                   mov eax, dword ptr [rsi]
0x71cfa2  (f 0x31cfa2)  8945cc                 mov dword ptr [rbp - 0x34], eax
0x71cfa5  (f 0x31cfa5)  488911                 mov qword ptr [rcx], rdx
0x71cfa8  (f 0x31cfa8)  eb7a                   jmp 0x71d024
0x71cfaa  (f 0x31cfaa)  488d0dab1e0809         lea rcx, [rip + 0x9081eab]  ; rip->0x979ee5c (file 0x939ee5c)
0x71cfb1  (f 0x31cfb1)  803901                 cmp byte ptr [rcx], 1
0x71cfb4  (f 0x31cfb4)  750e                   jne 0x71cfc4
0x71cfb6  (f 0x31cfb6)  4c89ff                 mov rdi, r15
0x71cfb9  (f 0x31cfb9)  e802b30400             call 0x7682c0
0x71cfbe  (f 0x31cfbe)  8b8548feffff           mov eax, dword ptr [rbp - 0x1b8]
0x71cfc4  (f 0x31cfc4)  4d897e48               mov qword ptr [r14 + 0x48], r15
0x71cfc8  (f 0x31cfc8)  4139c4                 cmp r12d, eax
0x71cfcb  (f 0x31cfcb)  7d2e                   jge 0x71cffb
0x71cfcd  (f 0x31cfcd)  488b8d40feffff         mov rcx, qword ptr [rbp - 0x1c0]
0x71cfd4  (f 0x31cfd4)  89c0                   mov eax, eax
0x71cfd6  (f 0x31cfd6)  eb10                   jmp 0x71cfe8
0x71cfe0  (f 0x31cfe0)  49ffc4                 inc r12
0x71cfe3  (f 0x31cfe3)  4c39e0                 cmp rax, r12
0x71cfe6  (f 0x31cfe6)  7413                   je 0x71cffb
0x71cfe8  (f 0x31cfe8)  4a8b14e1               mov rdx, qword ptr [rcx + r12*8]
0x71cfec  (f 0x31cfec)  4885d2                 test rdx, rdx
0x71cfef  (f 0x31cfef)  74ef                   je 0x71cfe0
0x71cff1  (f 0x31cff1)  49895728               mov qword ptr [r15 + 0x28], rdx
0x71cff5  (f 0x31cff5)  4e8b3ce1               mov r15, qword ptr [rcx + r12*8]
0x71cff9  (f 0x31cff9)  ebe5                   jmp 0x71cfe0
0x71cffb  (f 0x31cffb)  49c7472800000000       mov qword ptr [r15 + 0x28], 0
0x71d003  (f 0x31d003)  488bbd40feffff         mov rdi, qword ptr [rbp - 0x1c0]
0x71d00a  (f 0x31d00a)  4885ff                 test rdi, rdi
0x71d00d  (f 0x31d00d)  0f859dfeffff           jne 0x71ceb0
0x71d013  (f 0x31d013)  e99dfeffff             jmp 0x71ceb5
0x71d018  (f 0x31d018)  488d75cc               lea rsi, [rbp - 0x34]
0x71d01c  (f 0x31d01c)  4889df                 mov rdi, rbx
0x71d01f  (f 0x31d01f)  e88cfaefff             call 0x61cab0
0x71d024  (f 0x31d024)  c78540feffff00000000   mov dword ptr [rbp - 0x1c0], 0
0x71d02e  (f 0x31d02e)  4889df                 mov rdi, rbx
0x71d031  (f 0x31d031)  488b03                 mov rax, qword ptr [rbx]
0x71d034  (f 0x31d034)  ff5030                 call qword ptr [rax + 0x30]
0x71d037  (f 0x31d037)  4989c7                 mov r15, rax
0x71d03a  (f 0x31d03a)  488b4308               mov rax, qword ptr [rbx + 8]
0x71d03e  (f 0x31d03e)  488b08                 mov rcx, qword ptr [rax]
0x71d041  (f 0x31d041)  4883c104               add rcx, 4
0x71d045  (f 0x31d045)  483b4808               cmp rcx, qword ptr [rax + 8]
0x71d049  (f 0x31d049)  7637                   jbe 0x71d082
0x71d04b  (f 0x31d04b)  f6432a01               test byte ptr [rbx + 0x2a], 1
0x71d04f  (f 0x31d04f)  0f8580010000           jne 0x71d1d5
0x71d055  (f 0x31d055)  488b03                 mov rax, qword ptr [rbx]
0x71d058  (f 0x31d058)  488db540feffff         lea rsi, [rbp - 0x1c0]
0x71d05f  (f 0x31d05f)  ba04000000             mov edx, 4
0x71d064  (f 0x31d064)  4889df                 mov rdi, rbx
0x71d067  (f 0x31d067)  ff9088010000           call qword ptr [rax + 0x188]
0x71d06d  (f 0x31d06d)  488d3d9bb90309         lea rdi, [rip + 0x903b99b]  ; rip->0x9758a0f (file 0x9358a0f)
0x71d074  (f 0x31d074)  e86707e9ff             call 0x5ad7e0
0x71d079  (f 0x31d079)  84c0                   test al, al
0x71d07b  (f 0x31d07b)  741c                   je 0x71d099
0x71d07d  (f 0x31d07d)  e901040000             jmp 0x71d483
0x71d082  (f 0x31d082)  488908                 mov qword ptr [rax], rcx
0x71d085  (f 0x31d085)  488d3d83b90309         lea rdi, [rip + 0x903b983]  ; rip->0x9758a0f (file 0x9358a0f)
0x71d08c  (f 0x31d08c)  e84f07e9ff             call 0x5ad7e0
0x71d091  (f 0x31d091)  84c0                   test al, al
0x71d093  (f 0x31d093)  0f85ea030000           jne 0x71d483
0x71d099  (f 0x31d099)  c745c800000000         mov dword ptr [rbp - 0x38], 0
0x71d0a0  (f 0x31d0a0)  4889df                 mov rdi, rbx
0x71d0a3  (f 0x31d0a3)  488b03                 mov rax, qword ptr [rbx]
0x71d0a6  (f 0x31d0a6)  ff5030                 call qword ptr [rax + 0x30]
0x71d0a9  (f 0x31d0a9)  f6432901               test byte ptr [rbx + 0x29], 1
0x71d0ad  (f 0x31d0ad)  48898528feffff         mov qword ptr [rbp - 0x1d8], rax
0x71d0b4  (f 0x31d0b4)  743a                   je 0x71d0f0
0x71d0b6  (f 0x31d0b6)  488b03                 mov rax, qword ptr [rbx]
0x71d0b9  (f 0x31d0b9)  4889df                 mov rdi, rbx
0x71d0bc  (f 0x31d0bc)  ff5028                 call qword ptr [rax + 0x28]
0x71d0bf  (f 0x31d0bf)  4885c0                 test rax, rax
0x71d0c2  (f 0x31d0c2)  742c                   je 0x71d0f0
0x71d0c4  (f 0x31d0c4)  488b03                 mov rax, qword ptr [rbx]
0x71d0c7  (f 0x31d0c7)  4889df                 mov rdi, rbx
0x71d0ca  (f 0x31d0ca)  ff5028                 call qword ptr [rax + 0x28]
0x71d0cd  (f 0x31d0cd)  4c89bd30feffff         mov qword ptr [rbp - 0x1d0], r15
0x71d0d4  (f 0x31d0d4)  4885c0                 test rax, rax
0x71d0d7  (f 0x31d0d7)  0f8420010000           je 0x71d1fd
0x71d0dd  (f 0x31d0dd)  4531ed                 xor r13d, r13d
0x71d0e0  (f 0x31d0e0)  83b8b800000002         cmp dword ptr [rax + 0xb8], 2
0x71d0e7  (f 0x31d0e7)  4c0f44e8               cmove r13, rax
0x71d0eb  (f 0x31d0eb)  e910010000             jmp 0x71d200
0x71d0f0  (f 0x31d0f0)  488dbd40feffff         lea rdi, [rbp - 0x1c0]
0x71d0f7  (f 0x31d0f7)  4889de                 mov rsi, rbx
0x71d0fa  (f 0x31d0fa)  e84101f0ff             call 0x61d240
0x71d0ff  (f 0x31d0ff)  488d1502d0bb07         lea rdx, [rip + 0x7bbd002]  ; rip->0x82da108 (file 0x7eda108)
0x71d106  (f 0x31d106)  488d45c8               lea rax, [rbp - 0x38]
0x71d10a  (f 0x31d10a)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71d10e  (f 0x31d10e)  48c7c1ffffffef         mov rcx, 0xffffffffefffffff
0x71d115  (f 0x31d115)  48899540feffff         mov qword ptr [rbp - 0x1c0], rdx
0x71d11c  (f 0x31d11c)  c5f81185d8feffff       vmovups xmmword ptr [rbp - 0x128], xmm0
0x71d124  (f 0x31d124)  488985e8feffff         mov qword ptr [rbp - 0x118], rax
0x71d12b  (f 0x31d12b)  4c89b5f0feffff         mov qword ptr [rbp - 0x110], r14
0x71d132  (f 0x31d132)  488b85d0feffff         mov rax, qword ptr [rbp - 0x130]
0x71d139  (f 0x31d139)  48238d68feffff         and rcx, qword ptr [rbp - 0x198]
0x71d140  (f 0x31d140)  488b4028               mov rax, qword ptr [rax + 0x28]
0x71d144  (f 0x31d144)  2500000010             and eax, 0x10000000
0x71d149  (f 0x31d149)  4809c1                 or rcx, rax
0x71d14c  (f 0x31d14c)  48898d68feffff         mov qword ptr [rbp - 0x198], rcx
0x71d153  (f 0x31d153)  8b4dc8                 mov ecx, dword ptr [rbp - 0x38]
0x71d156  (f 0x31d156)  3b4dcc                 cmp ecx, dword ptr [rbp - 0x34]
0x71d159  (f 0x31d159)  7d48                   jge 0x71d1a3
0x71d15b  (f 0x31d15b)  4d89fd                 mov r13, r15
0x71d15e  (f 0x31d15e)  4c8d65c8               lea r12, [rbp - 0x38]
0x71d162  (f 0x31d162)  4c8dbd40feffff         lea r15, [rbp - 0x1c0]
0x71d169  (f 0x31d169)  0f1f8000000000         nop dword ptr [rax]
0x71d170  (f 0x31d170)  498b06                 mov rax, qword ptr [r14]
0x71d173  (f 0x31d173)  4c89f7                 mov rdi, r14
0x71d176  (f 0x31d176)  4c89e6                 mov rsi, r12
0x71d179  (f 0x31d179)  4c89fa                 mov rdx, r15
0x71d17c  (f 0x31d17c)  ff9018030000           call qword ptr [rax + 0x318]
0x71d182  (f 0x31d182)  8b45c8                 mov eax, dword ptr [rbp - 0x38]
0x71d185  (f 0x31d185)  3b45cc                 cmp eax, dword ptr [rbp - 0x34]
0x71d188  (f 0x31d188)  7ce6                   jl 0x71d170
0x71d18a  (f 0x31d18a)  488bbdd8feffff         mov rdi, qword ptr [rbp - 0x128]
0x71d191  (f 0x31d191)  8bb5e0feffff           mov esi, dword ptr [rbp - 0x120]
0x71d197  (f 0x31d197)  4d89ef                 mov r15, r13
0x71d19a  (f 0x31d19a)  488d1567cfbb07         lea rdx, [rip + 0x7bbcf67]  ; rip->0x82da108 (file 0x7eda108)
0x71d1a1  (f 0x31d1a1)  eb04                   jmp 0x71d1a7
0x71d1a3  (f 0x31d1a3)  31f6                   xor esi, esi
0x71d1a5  (f 0x31d1a5)  31ff                   xor edi, edi
0x71d1a7  (f 0x31d1a7)  48899540feffff         mov qword ptr [rbp - 0x1c0], rdx
0x71d1ae  (f 0x31d1ae)  e83d0a0100             call 0x72dbf0
0x71d1b3  (f 0x31d1b3)  488bbdd8feffff         mov rdi, qword ptr [rbp - 0x128]
0x71d1ba  (f 0x31d1ba)  4885ff                 test rdi, rdi
0x71d1bd  (f 0x31d1bd)  7405                   je 0x71d1c4
0x71d1bf  (f 0x31d1bf)  e8cca8d9ff             call 0x4b7a90
0x71d1c4  (f 0x31d1c4)  488dbd40feffff         lea rdi, [rbp - 0x1c0]
0x71d1cb  (f 0x31d1cb)  e860ebefff             call 0x61bd30
0x71d1d0  (f 0x31d1d0)  e907020000             jmp 0x71d3dc
0x71d1d5  (f 0x31d1d5)  488db540feffff         lea rsi, [rbp - 0x1c0]
0x71d1dc  (f 0x31d1dc)  4889df                 mov rdi, rbx
0x71d1df  (f 0x31d1df)  e8ccf8efff             call 0x61cab0
0x71d1e4  (f 0x31d1e4)  488d3d24b80309         lea rdi, [rip + 0x903b824]  ; rip->0x9758a0f (file 0x9358a0f)
0x71d1eb  (f 0x31d1eb)  e8f005e9ff             call 0x5ad7e0
0x71d1f0  (f 0x31d1f0)  84c0                   test al, al
0x71d1f2  (f 0x31d1f2)  0f84a1feffff           je 0x71d099
0x71d1f8  (f 0x31d1f8)  e986020000             jmp 0x71d483
0x71d1fd  (f 0x31d1fd)  4531ed                 xor r13d, r13d
0x71d200  (f 0x31d200)  498b85f8020000         mov rax, qword ptr [r13 + 0x2f8]
0x71d207  (f 0x31d207)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71d20b  (f 0x31d20b)  c5f81145b0             vmovups xmmword ptr [rbp - 0x50], xmm0
0x71d210  (f 0x31d210)  b908010000             mov ecx, 0x108
0x71d215  (f 0x31d215)  4c8da500ffffff         lea r12, [rbp - 0x100]
0x71d21c  (f 0x31d21c)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71d220  (f 0x31d220)  c46270f77b28           bextr r15d, dword ptr [rbx + 0x28], ecx
0x71d226  (f 0x31d226)  4c89e7                 mov rdi, r12
0x71d229  (f 0x31d229)  c5fc118570ffffff       vmovups ymmword ptr [rbp - 0x90], ymm0
0x71d231  (f 0x31d231)  c5fc118560ffffff       vmovups ymmword ptr [rbp - 0xa0], ymm0
0x71d239  (f 0x31d239)  c5fc118540ffffff       vmovups ymmword ptr [rbp - 0xc0], ymm0
0x71d241  (f 0x31d241)  c5fc118520ffffff       vmovups ymmword ptr [rbp - 0xe0], ymm0
0x71d249  (f 0x31d249)  c5fc118500ffffff       vmovups ymmword ptr [rbp - 0x100], ymm0
0x71d251  (f 0x31d251)  48898538feffff         mov qword ptr [rbp - 0x1c8], rax
0x71d258  (f 0x31d258)  e8a3dbefff             call 0x61ae00
0x71d25d  (f 0x31d25d)  488d054487ba07         lea rax, [rip + 0x7ba8744]  ; rip->0x82c59a8 (file 0x7ec59a8)
0x71d264  (f 0x31d264)  488d4db0               lea rcx, [rbp - 0x50]
0x71d268  (f 0x31d268)  4c89e7                 mov rdi, r12
0x71d26b  (f 0x31d26b)  be01000000             mov esi, 1
0x71d270  (f 0x31d270)  48c7459000000000       mov qword ptr [rbp - 0x70], 0
0x71d278  (f 0x31d278)  48898500ffffff         mov qword ptr [rbp - 0x100], rax
0x71d27f  (f 0x31d27f)  48894d98               mov qword ptr [rbp - 0x68], rcx
0x71d283  (f 0x31d283)  48c745a000000000       mov qword ptr [rbp - 0x60], 0
0x71d28b  (f 0x31d28b)  e8d0feefff             call 0x61d160
0x71d290  (f 0x31d290)  488b8500ffffff         mov rax, qword ptr [rbp - 0x100]
0x71d297  (f 0x31d297)  4c89e7                 mov rdi, r12
0x71d29a  (f 0x31d29a)  4489fe                 mov esi, r15d
0x71d29d  (f 0x31d29d)  ff9008010000           call qword ptr [rax + 0x108]
0x71d2a3  (f 0x31d2a3)  488d058684ba07         lea rax, [rip + 0x7ba8486]  ; rip->0x82c5730 (file 0x7ec5730)
0x71d2aa  (f 0x31d2aa)  488dbd40feffff         lea rdi, [rbp - 0x1c0]
0x71d2b1  (f 0x31d2b1)  4889de                 mov rsi, rbx
0x71d2b4  (f 0x31d2b4)  48898500ffffff         mov qword ptr [rbp - 0x100], rax
0x71d2bb  (f 0x31d2bb)  4d89a5f8020000         mov qword ptr [r13 + 0x2f8], r12
0x71d2c2  (f 0x31d2c2)  e879ffefff             call 0x61d240
0x71d2c7  (f 0x31d2c7)  488d153acebb07         lea rdx, [rip + 0x7bbce3a]  ; rip->0x82da108 (file 0x7eda108)
0x71d2ce  (f 0x31d2ce)  488d45c8               lea rax, [rbp - 0x38]
0x71d2d2  (f 0x31d2d2)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x71d2d6  (f 0x31d2d6)  48c7c1ffffffef         mov rcx, 0xffffffffefffffff
0x71d2dd  (f 0x31d2dd)  48899540feffff         mov qword ptr [rbp - 0x1c0], rdx
0x71d2e4  (f 0x31d2e4)  c5f81185d8feffff       vmovups xmmword ptr [rbp - 0x128], xmm0
0x71d2ec  (f 0x31d2ec)  488985e8feffff         mov qword ptr [rbp - 0x118], rax
0x71d2f3  (f 0x31d2f3)  4c89b5f0feffff         mov qword ptr [rbp - 0x110], r14
0x71d2fa  (f 0x31d2fa)  488b85d0feffff         mov rax, qword ptr [rbp - 0x130]
0x71d301  (f 0x31d301)  48238d68feffff         and rcx, qword ptr [rbp - 0x198]
0x71d308  (f 0x31d308)  488b4028               mov rax, qword ptr [rax + 0x28]
0x71d30c  (f 0x31d30c)  2500000010             and eax, 0x10000000
0x71d311  (f 0x31d311)  4809c1                 or rcx, rax
0x71d314  (f 0x31d314)  48898d68feffff         mov qword ptr [rbp - 0x198], rcx
0x71d31b  (f 0x31d31b)  8b4dc8                 mov ecx, dword ptr [rbp - 0x38]
0x71d31e  (f 0x31d31e)  3b4dcc                 cmp ecx, dword ptr [rbp - 0x34]
0x71d321  (f 0x31d321)  7d3d                   jge 0x71d360
0x71d323  (f 0x31d323)  4c8d65c8               lea r12, [rbp - 0x38]
0x71d327  (f 0x31d327)  4c8dbd40feffff         lea r15, [rbp - 0x1c0]
0x71d32e  (f 0x31d32e)  6690                   nop 
0x71d330  (f 0x31d330)  498b06                 mov rax, qword ptr [r14]
0x71d333  (f 0x31d333)  4c89f7                 mov rdi, r14
0x71d336  (f 0x31d336)  4c89e6                 mov rsi, r12
0x71d339  (f 0x31d339)  4c89fa                 mov rdx, r15
0x71d33c  (f 0x31d33c)  ff9018030000           call qword ptr [rax + 0x318]
0x71d342  (f 0x31d342)  8b45c8                 mov eax, dword ptr [rbp - 0x38]
0x71d345  (f 0x31d345)  3b45cc                 cmp eax, dword ptr [rbp - 0x34]
0x71d348  (f 0x31d348)  7ce6                   jl 0x71d330
0x71d34a  (f 0x31d34a)  488bbdd8feffff         mov rdi, qword ptr [rbp - 0x128]
0x71d351  (f 0x31d351)  8bb5e0feffff           mov esi, dword ptr [rbp - 0x120]
0x71d357  (f 0x31d357)  488d15aacdbb07         lea rdx, [rip + 0x7bbcdaa]  ; rip->0x82da108 (file 0x7eda108)
0x71d35e  (f 0x31d35e)  eb04                   jmp 0x71d364
0x71d360  (f 0x31d360)  31ff                   xor edi, edi
0x71d362  (f 0x31d362)  31f6                   xor esi, esi
0x71d364  (f 0x31d364)  48899540feffff         mov qword ptr [rbp - 0x1c0], rdx
0x71d36b  (f 0x31d36b)  e880080100             call 0x72dbf0
0x71d370  (f 0x31d370)  488bbdd8feffff         mov rdi, qword ptr [rbp - 0x128]
0x71d377  (f 0x31d377)  4c8bbd30feffff         mov r15, qword ptr [rbp - 0x1d0]
0x71d37e  (f 0x31d37e)  4885ff                 test rdi, rdi
0x71d381  (f 0x31d381)  7405                   je 0x71d388
0x71d383  (f 0x31d383)  e808a7d9ff             call 0x4b7a90
0x71d388  (f 0x31d388)  488dbd40feffff         lea rdi, [rbp - 0x1c0]
0x71d38f  (f 0x31d38f)  e89ce9efff             call 0x61bd30
0x71d394  (f 0x31d394)  488b8538feffff         mov rax, qword ptr [rbp - 0x1c8]
0x71d39b  (f 0x31d39b)  4889df                 mov rdi, rbx
0x71d39e  (f 0x31d39e)  498985f8020000         mov qword ptr [r13 + 0x2f8], rax
0x71d3a5  (f 0x31d3a5)  488b75b0               mov rsi, qword ptr [rbp - 0x50]
0x71d3a9  (f 0x31d3a9)  486355b8               movsxd rdx, dword ptr [rbp - 0x48]
0x71d3ad  (f 0x31d3ad)  488b03                 mov rax, qword ptr [rbx]
0x71d3b0  (f 0x31d3b0)  ff9088010000           call qword ptr [rax + 0x188]
0x71d3b6  (f 0x31d3b6)  488d75b0               lea rsi, [rbp - 0x50]
0x71d3ba  (f 0x31d3ba)  4c89ef                 mov rdi, r13
0x71d3bd  (f 0x31d3bd)  e86e4a0800             call 0x7a1e30
0x71d3c2  (f 0x31d3c2)  488dbd00ffffff         lea rdi, [rbp - 0x100]
0x71d3c9  (f 0x31d3c9)  e842e1efff             call 0x61b510
0x71d3ce  (f 0x31d3ce)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x71d3d2  (f 0x31d3d2)  4885ff                 test rdi, rdi
0x71d3d5  (f 0x31d3d5)  7405                   je 0x71d3dc
0x71d3d7  (f 0x31d3d7)  e8b4a6d9ff             call 0x4b7a90
0x71d3dc  (f 0x31d3dc)  4c8b2d254be108         mov r13, qword ptr [rip + 0x8e14b25]  ; rip->0x9531f08 (file 0x9131f08)
0x71d3e3  (f 0x31d3e3)  8b4dc8                 mov ecx, dword ptr [rbp - 0x38]
0x71d3e6  (f 0x31d3e6)  448b45cc               mov r8d, dword ptr [rbp - 0x34]
0x71d3ea  (f 0x31d3ea)  4439c1                 cmp ecx, r8d
0x71d3ed  (f 0x31d3ed)  0f85cd000000           jne 0x71d4c0
0x71d3f3  (f 0x31d3f3)  f6432804               test byte ptr [rbx + 0x28], 4
0x71d3f7  (f 0x31d3f7)  0f8486000000           je 0x71d483
0x71d3fd  (f 0x31d3fd)  488b03                 mov rax, qword ptr [rbx]
0x71d400  (f 0x31d400)  4889df                 mov rdi, rbx
0x71d403  (f 0x31d403)  ff5030                 call qword ptr [rax + 0x30]
0x71d406  (f 0x31d406)  4989c4                 mov r12, rax
0x71d409  (f 0x31d409)  488b03                 mov rax, qword ptr [rbx]
0x71d40c  (f 0x31d40c)  4889df                 mov rdi, rbx
0x71d40f  (f 0x31d40f)  4c89fe                 mov rsi, r15
0x71d412  (f 0x31d412)  ff90b8010000           call qword ptr [rax + 0x1b8]
0x71d418  (f 0x31d418)  4489e0                 mov eax, r12d
0x71d41b  (f 0x31d41b)  2b8528feffff           sub eax, dword ptr [rbp - 0x1d8]
0x71d421  (f 0x31d421)  898540feffff           mov dword ptr [rbp - 0x1c0], eax
0x71d427  (f 0x31d427)  488b4308               mov rax, qword ptr [rbx + 8]
0x71d42b  (f 0x31d42b)  488b10                 mov rdx, qword ptr [rax]
0x71d42e  (f 0x31d42e)  488d4a04               lea rcx, [rdx + 4]
0x71d432  (f 0x31d432)  483b4808               cmp rcx, qword ptr [rax + 8]
0x71d436  (f 0x31d436)  7620                   jbe 0x71d458
0x71d438  (f 0x31d438)  f6432a01               test byte ptr [rbx + 0x2a], 1
0x71d43c  (f 0x31d43c)  7527                   jne 0x71d465
0x71d43e  (f 0x31d43e)  488b03                 mov rax, qword ptr [rbx]
0x71d441  (f 0x31d441)  488db540feffff         lea rsi, [rbp - 0x1c0]
0x71d448  (f 0x31d448)  ba04000000             mov edx, 4
0x71d44d  (f 0x31d44d)  4889df                 mov rdi, rbx
0x71d450  (f 0x31d450)  ff9088010000           call qword ptr [rax + 0x188]
0x71d456  (f 0x31d456)  eb1c                   jmp 0x71d474
0x71d458  (f 0x31d458)  8b12                   mov edx, dword ptr [rdx]
0x71d45a  (f 0x31d45a)  899540feffff           mov dword ptr [rbp - 0x1c0], edx
0x71d460  (f 0x31d460)  488908                 mov qword ptr [rax], rcx
0x71d463  (f 0x31d463)  eb0f                   jmp 0x71d474
0x71d465  (f 0x31d465)  488db540feffff         lea rsi, [rbp - 0x1c0]
0x71d46c  (f 0x31d46c)  4889df                 mov rdi, rbx
0x71d46f  (f 0x31d46f)  e83cf6efff             call 0x61cab0
0x71d474  (f 0x31d474)  488b03                 mov rax, qword ptr [rbx]
0x71d477  (f 0x31d477)  4889df                 mov rdi, rbx
0x71d47a  (f 0x31d47a)  4c89e6                 mov rsi, r12
0x71d47d  (f 0x31d47d)  ff90b8010000           call qword ptr [rax + 0x1b8]
0x71d483  (f 0x31d483)  b800010001             mov eax, 0x1000100
0x71d488  (f 0x31d488)  234328                 and eax, dword ptr [rbx + 0x28]
0x71d48b  (f 0x31d48b)  3d00000001             cmp eax, 0x1000000
0x71d490  (f 0x31d490)  7512                   jne 0x71d4a4
0x71d492  (f 0x31d492)  4981c690000000         add r14, 0x90
0x71d499  (f 0x31d499)  4889df                 mov rdi, rbx
0x71d49c  (f 0x31d49c)  4c89f6                 mov rsi, r14
0x71d49f  (f 0x31d49f)  e81c2c0100             call 0x7300c0
0x71d4a4  (f 0x31d4a4)  498b4500               mov rax, qword ptr [r13]
0x71d4a8  (f 0x31d4a8)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x71d4ac  (f 0x31d4ac)  7538                   jne 0x71d4e6
0x71d4ae  (f 0x31d4ae)  4881c4b8010000         add rsp, 0x1b8
0x71d4b5  (f 0x31d4b5)  5b                     pop rbx
0x71d4b6  (f 0x31d4b6)  415c                   pop r12
0x71d4b8  (f 0x31d4b8)  415d                   pop r13
0x71d4ba  (f 0x31d4ba)  415e                   pop r14
0x71d4bc  (f 0x31d4bc)  415f                   pop r15
0x71d4be  (f 0x31d4be)  5d                     pop rbp
0x71d4bf  (f 0x31d4bf)  c3                     ret 
0x71d4c0  (f 0x31d4c0)  488d3d6be18407         lea rdi, [rip + 0x784e16b]  ; rip->0x7f6b632 (file 0x7b6b632)
0x71d4c7  (f 0x31d4c7)  488d15de574b07         lea rdx, [rip + 0x74b57de]  ; rip->0x7bd2cac (file 0x77d2cac)
0x71d4ce  (f 0x31d4ce)  bea0090000             mov esi, 0x9a0
0x71d4d3  (f 0x31d4d3)  31c0                   xor eax, eax
0x71d4d5  (f 0x31d4d5)  e8d67be5ff             call 0x5750b0
0x71d4da  (f 0x31d4da)  f6432804               test byte ptr [rbx + 0x28], 4
0x71d4de  (f 0x31d4de)  0f8519ffffff           jne 0x71d3fd
0x71d4e4  (f 0x31d4e4)  eb9d                   jmp 0x71d483
0x71d4e6  (f 0x31d4e6)  e875491507             call 0x7871e60
0x71d4eb  (f 0x31d4eb)  0f0b                   ud2 
--- calls: [('0xb1cc19', '0x7c36d0'), ('0xb1cc3a', '0x61bc90'), ('0xb1cc90', '0x44d8c0'), ('0xb1ccc5', '0x4c96c80'), ('0xb1cdaa', '0x61bc90'), ('0xb1cebf', '0x61bc90'), ('0xb1cecf', '0x71d4f0'), ('0xb1d074', '0x5ad7e0'), ('0xb1d49f', '0x7300c0'), ('0xb1d4e6', '0x7871e60'), ('0xb1d258', '0x61ae00'), ('0xb1d28b', '0x61d160'), ('0xb1d2c2', '0x61d240'), ('0xb1d36b', '0x72dbf0'), ('0xb1d383', '0x4b7a90'), ('0xb1d38f', '0x61bd30'), ('0xb1d3bd', '0x7a1e30'), ('0xb1d3c9', '0x61b510'), ('0xb1d3d7', '0x4b7a90'), ('0xb1d46f', '0x61cab0'), ('0xb1d4d5', '0x5750b0'), ('0xb1d0fa', '0x61d240'), ('0xb1d1ae', '0x72dbf0'), ('0xb1d1bf', '0x4b7a90'), ('0xb1d1cb', '0x61bd30'), ('0xb1d1df', '0x61cab0'), ('0xb1d1eb', '0x5ad7e0'), ('0xb1d08c', '0x5ad7e0'), ('0xb1d01f', '0x61cab0'), ('0xb1cf3c', '0x869280'), ('0xb1cf4f', '0x868d40'), ('0xb1cf59', '0x7264c0'), ('0xb1ce32', '0x4298a0'), ('0xb1ce98', '0x72fea0'), ('0xb1ceb0', '0x4b7a90'), ('0xb1ce43', '0x72fea0'), ('0xb1cfb9', '0x7682c0'), ('0xb1cd68', '0x4c96c80'), ('0xb1ccff', '0x44d8c0'), ('0xb1cd35', '0x4c96c80')]

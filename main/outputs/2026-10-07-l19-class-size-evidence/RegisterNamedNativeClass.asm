0x885ca0  (f 0x485ca0)  55                     push rbp
0x885ca1  (f 0x485ca1)  4889e5                 mov rbp, rsp
0x885ca4  (f 0x485ca4)  4157                   push r15
0x885ca6  (f 0x485ca6)  4156                   push r14
0x885ca8  (f 0x485ca8)  4155                   push r13
0x885caa  (f 0x485caa)  4154                   push r12
0x885cac  (f 0x485cac)  53                     push rbx
0x885cad  (f 0x485cad)  4883ec38               sub rsp, 0x38
0x885cb1  (f 0x485cb1)  488b0550c2ca08         mov rax, qword ptr [rip + 0x8cac250]  ; rip->0x9531f08 (file 0x9131f08)
0x885cb8  (f 0x485cb8)  4989cf                 mov r15, rcx
0x885cbb  (f 0x485cbb)  4989d6                 mov r14, rdx
0x885cbe  (f 0x485cbe)  4889f3                 mov rbx, rsi
0x885cc1  (f 0x485cc1)  4989fc                 mov r12, rdi
0x885cc4  (f 0x485cc4)  488b00                 mov rax, qword ptr [rax]
0x885cc7  (f 0x485cc7)  488945d0               mov qword ptr [rbp - 0x30], rax
0x885ccb  (f 0x485ccb)  0fb605a643f308         movzx eax, byte ptr [rip + 0x8f343a6]  ; rip->0x97ba078 (file 0x93ba078)
0x885cd2  (f 0x485cd2)  84c0                   test al, al
0x885cd4  (f 0x485cd4)  0f8490000000           je 0x885d6a
0x885cda  (f 0x485cda)  bf10000000             mov edi, 0x10
0x885cdf  (f 0x485cdf)  e89c0f4104             call 0x4c96c80
0x885ce4  (f 0x485ce4)  4c8920                 mov qword ptr [rax], r12
0x885ce7  (f 0x485ce7)  488d4db8               lea rcx, [rbp - 0x48]
0x885ceb  (f 0x485ceb)  4989c5                 mov r13, rax
0x885cee  (f 0x485cee)  48c7400800000000       mov qword ptr [rax + 8], 0
0x885cf6  (f 0x485cf6)  488d45a0               lea rax, [rbp - 0x60]
0x885cfa  (f 0x485cfa)  488d3d2743f308         lea rdi, [rip + 0x8f34327]  ; rip->0x97ba028 (file 0x93ba028)
0x885d01  (f 0x485d01)  488d75c0               lea rsi, [rbp - 0x40]
0x885d05  (f 0x485d05)  4c897da0               mov qword ptr [rbp - 0x60], r15
0x885d09  (f 0x485d09)  4c8975a8               mov qword ptr [rbp - 0x58], r14
0x885d0d  (f 0x485d0d)  31d2                   xor edx, edx
0x885d0f  (f 0x485d0f)  4c8965b8               mov qword ptr [rbp - 0x48], r12
0x885d13  (f 0x485d13)  48895db0               mov qword ptr [rbp - 0x50], rbx
0x885d17  (f 0x485d17)  48894dc0               mov qword ptr [rbp - 0x40], rcx
0x885d1b  (f 0x485d1b)  488945c8               mov qword ptr [rbp - 0x38], rax
0x885d1f  (f 0x485d1f)  e81c5e0100             call 0x89bb40
0x885d24  (f 0x485d24)  488b05cd3ef308         mov rax, qword ptr [rip + 0x8f33ecd]  ; rip->0x97b9bf8 (file 0x93b9bf8)
0x885d2b  (f 0x485d2b)  488d15ce3ef308         lea rdx, [rip + 0x8f33ece]  ; rip->0x97b9c00 (file 0x93b9c00)
0x885d32  (f 0x485d32)  488d4808               lea rcx, [rax + 8]
0x885d36  (f 0x485d36)  4885c0                 test rax, rax
0x885d39  (f 0x485d39)  480f45d1               cmovne rdx, rcx
0x885d3d  (f 0x485d3d)  488b0dc4c1ca08         mov rcx, qword ptr [rip + 0x8cac1c4]  ; rip->0x9531f08 (file 0x9131f08)
0x885d44  (f 0x485d44)  4c892a                 mov qword ptr [rdx], r13
0x885d47  (f 0x485d47)  4c892daa3ef308         mov qword ptr [rip + 0x8f33eaa], r13  ; rip->0x97b9bf8 (file 0x93b9bf8)
0x885d4e  (f 0x485d4e)  488b01                 mov rax, qword ptr [rcx]
0x885d51  (f 0x485d51)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x885d55  (f 0x485d55)  0f858a000000           jne 0x885de5
0x885d5b  (f 0x485d5b)  4883c438               add rsp, 0x38
0x885d5f  (f 0x485d5f)  5b                     pop rbx
0x885d60  (f 0x485d60)  415c                   pop r12
0x885d62  (f 0x485d62)  415d                   pop r13
0x885d64  (f 0x485d64)  415e                   pop r14
0x885d66  (f 0x485d66)  415f                   pop r15
0x885d68  (f 0x485d68)  5d                     pop rbp
0x885d69  (f 0x485d69)  c3                     ret 
0x885d6a  (f 0x485d6a)  488d3d0743f308         lea rdi, [rip + 0x8f34307]  ; rip->0x97ba078 (file 0x93ba078)
0x885d71  (f 0x485d71)  e83ac1fe06             call 0x7871eb0
0x885d76  (f 0x485d76)  85c0                   test eax, eax
0x885d78  (f 0x485d78)  0f845cffffff           je 0x885cda
0x885d7e  (f 0x485d7e)  c5f8100d9a6c5b07       vmovups xmm1, xmmword ptr [rip + 0x75b6c9a]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x885d86  (f 0x485d86)  488d359b42f308         lea rsi, [rip + 0x8f3429b]  ; rip->0x97ba028 (file 0x93ba028)
0x885d8d  (f 0x485d8d)  488d3dbc3f0100         lea rdi, [rip + 0x13fbc]  ; rip->0x899d50 (file 0x499d50)
0x885d94  (f 0x485d94)  488d156562e508         lea rdx, [rip + 0x8e56265]  ; rip->0x96dc000 (file 0x92dc000)
0x885d9b  (f 0x485d9b)  48c705a242f30800000000 mov qword ptr [rip + 0x8f342a2], 0  ; rip->0x97ba048 (file 0x93ba048)
0x885da6  (f 0x485da6)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x885daa  (f 0x485daa)  c5f811057642f308       vmovups xmmword ptr [rip + 0x8f34276], xmm0  ; rip->0x97ba028 (file 0x93ba028)
0x885db2  (f 0x485db2)  c5f8110d9642f308       vmovups xmmword ptr [rip + 0x8f34296], xmm1  ; rip->0x97ba050 (file 0x93ba050)
0x885dba  (f 0x485dba)  48c705a342f30800000000 mov qword ptr [rip + 0x8f342a3], 0  ; rip->0x97ba068 (file 0x93ba068)
0x885dc5  (f 0x485dc5)  c705a142f30800000000   mov dword ptr [rip + 0x8f342a1], 0  ; rip->0x97ba070 (file 0x93ba070)
0x885dcf  (f 0x485dcf)  e8fcc0fe06             call 0x7871ed0
0x885dd4  (f 0x485dd4)  488d3d9d42f308         lea rdi, [rip + 0x8f3429d]  ; rip->0x97ba078 (file 0x93ba078)
0x885ddb  (f 0x485ddb)  e8e0c0fe06             call 0x7871ec0
0x885de0  (f 0x485de0)  e9f5feffff             jmp 0x885cda
0x885de5  (f 0x485de5)  e876c0fe06             call 0x7871e60
0x885dea  (f 0x485dea)  0f0b                   ud2 
--- calls: [('0xc85cdf', '0x4c96c80'), ('0xc85d1f', '0x89bb40'), ('0xc85de5', '0x7871e60'), ('0xc85d71', '0x7871eb0'), ('0xc85dcf', '0x7871ed0'), ('0xc85ddb', '0x7871ec0')]

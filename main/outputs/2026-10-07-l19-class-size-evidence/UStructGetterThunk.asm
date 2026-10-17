0x720e40  (f 0x320e40)  55                     push rbp
0x720e41  (f 0x320e41)  4889e5                 mov rbp, rsp
0x720e44  (f 0x320e44)  4156                   push r14
0x720e46  (f 0x320e46)  53                     push rbx
0x720e47  (f 0x320e47)  4883ec30               sub rsp, 0x30
0x720e4b  (f 0x320e4b)  4889fb                 mov rbx, rdi
0x720e4e  (f 0x320e4e)  e84d5b0a00             call 0x7c69a0
0x720e53  (f 0x320e53)  4989c6                 mov r14, rax
0x720e56  (f 0x320e56)  e895fcffff             call 0x720af0
0x720e5b  (f 0x320e5b)  4889c7                 mov rdi, rax
0x720e5e  (f 0x320e5e)  4c89f6                 mov rsi, r14
0x720e61  (f 0x320e61)  b910050000             mov ecx, 0x510
0x720e66  (f 0x320e66)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x720e6a  (f 0x320e6a)  c5f811442418           vmovups xmmword ptr [rsp + 0x18], xmm0
0x720e70  (f 0x320e70)  c5f8110424             vmovups xmmword ptr [rsp], xmm0
0x720e75  (f 0x320e75)  c744241000000000       mov dword ptr [rsp + 0x10], 0
0x720e7d  (f 0x320e7d)  31d2                   xor edx, edx
0x720e7f  (f 0x320e7f)  4531c0                 xor r8d, r8d
0x720e82  (f 0x320e82)  4531c9                 xor r9d, r9d
0x720e85  (f 0x320e85)  e826011700             call 0x890fb0
0x720e8a  (f 0x320e8a)  4889c7                 mov rdi, rax
0x720e8d  (f 0x320e8d)  4889de                 mov rsi, rbx
0x720e90  (f 0x320e90)  4989c6                 mov r14, rax
0x720e93  (f 0x320e93)  e8e8120a00             call 0x7c2180
0x720e98  (f 0x320e98)  488d055164bb07         lea rax, [rip + 0x7bb6451]  ; rip->0x82d72f0 (file 0x7ed72f0)
0x720e9f  (f 0x320e9f)  49c7463000000000       mov qword ptr [r14 + 0x30], 0
0x720ea7  (f 0x320ea7)  41c74638ffffffff       mov dword ptr [r14 + 0x38], 0xffffffff
0x720eaf  (f 0x320eaf)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x720eb3  (f 0x320eb3)  498906                 mov qword ptr [r14], rax
0x720eb6  (f 0x320eb6)  49c786a800000000000000 mov qword ptr [r14 + 0xa8], 0
0x720ec1  (f 0x320ec1)  c4c178114640           vmovups xmmword ptr [r14 + 0x40], xmm0
0x720ec7  (f 0x320ec7)  c4c17811465e           vmovups xmmword ptr [r14 + 0x5e], xmm0
0x720ecd  (f 0x320ecd)  6641c7466e0000         mov word ptr [r14 + 0x6e], 0
0x720ed4  (f 0x320ed4)  c4c178118690000000     vmovups xmmword ptr [r14 + 0x90], xmm0
0x720edd  (f 0x320edd)  4c89f0                 mov rax, r14
0x720ee0  (f 0x320ee0)  4883c430               add rsp, 0x30
0x720ee4  (f 0x320ee4)  5b                     pop rbx
0x720ee5  (f 0x320ee5)  415e                   pop r14
0x720ee7  (f 0x320ee7)  5d                     pop rbp
0x720ee8  (f 0x320ee8)  c3                     ret 
--- calls: [('0xb20e4e', '0x7c69a0'), ('0xb20e56', '0x720af0'), ('0xb20e85', '0x890fb0'), ('0xb20e93', '0x7c2180')]

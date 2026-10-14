0x720de0  (f 0x320de0)  55                     push rbp
0x720de1  (f 0x320de1)  4889e5                 mov rbp, rsp
0x720de4  (f 0x320de4)  53                     push rbx
0x720de5  (f 0x320de5)  50                     push rax
0x720de6  (f 0x320de6)  488b1f                 mov rbx, qword ptr [rdi]
0x720de9  (f 0x320de9)  4889fe                 mov rsi, rdi
0x720dec  (f 0x320dec)  4889df                 mov rdi, rbx
0x720def  (f 0x320def)  e82c0e1700             call 0x891c20
0x720df4  (f 0x320df4)  488d05f564bb07         lea rax, [rip + 0x7bb64f5]  ; rip->0x82d72f0 (file 0x7ed72f0)
0x720dfb  (f 0x320dfb)  48c7433000000000       mov qword ptr [rbx + 0x30], 0
0x720e03  (f 0x320e03)  c74338ffffffff         mov dword ptr [rbx + 0x38], 0xffffffff
0x720e0a  (f 0x320e0a)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x720e0e  (f 0x320e0e)  488903                 mov qword ptr [rbx], rax
0x720e11  (f 0x320e11)  48c783a800000000000000 mov qword ptr [rbx + 0xa8], 0
0x720e1c  (f 0x320e1c)  c5f8114340             vmovups xmmword ptr [rbx + 0x40], xmm0
0x720e21  (f 0x320e21)  c5f811435e             vmovups xmmword ptr [rbx + 0x5e], xmm0
0x720e26  (f 0x320e26)  66c7436e0000           mov word ptr [rbx + 0x6e], 0
0x720e2c  (f 0x320e2c)  c5f8118390000000       vmovups xmmword ptr [rbx + 0x90], xmm0
0x720e34  (f 0x320e34)  4883c408               add rsp, 8
0x720e38  (f 0x320e38)  5b                     pop rbx
0x720e39  (f 0x320e39)  5d                     pop rbp
0x720e3a  (f 0x320e3a)  c3                     ret 
--- calls: [('0xb20def', '0x891c20')]

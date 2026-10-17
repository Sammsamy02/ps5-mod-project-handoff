0x4bf050  (f 0xbf050)  55                     push rbp
0x4bf051  (f 0xbf051)  4889e5                 mov rbp, rsp
0x4bf054  (f 0xbf054)  4157                   push r15
0x4bf056  (f 0xbf056)  4156                   push r14
0x4bf058  (f 0xbf058)  53                     push rbx
0x4bf059  (f 0xbf059)  50                     push rax
0x4bf05a  (f 0xbf05a)  4c8d35ef182909         lea r14, [rip + 0x92918ef]  ; rip->0x9750950 (file 0x9350950)
0x4bf061  (f 0xbf061)  4889fb                 mov rbx, rdi
0x4bf064  (f 0xbf064)  89f2                   mov edx, esi
0x4bf066  (f 0xbf066)  498b3e                 mov rdi, qword ptr [r14]
0x4bf069  (f 0xbf069)  4885ff                 test rdi, rdi
0x4bf06c  (f 0xbf06c)  750e                   jne 0x4bf07c
0x4bf06e  (f 0xbf06e)  0fb605d3cf2609         movzx eax, byte ptr [rip + 0x926cfd3]  ; rip->0x972c048 (file 0x932c048)
0x4bf075  (f 0xbf075)  84c0                   test al, al
0x4bf077  (f 0xbf077)  7417                   je 0x4bf090
0x4bf079  (f 0xbf079)  498b3e                 mov rdi, qword ptr [r14]
0x4bf07c  (f 0xbf07c)  488b07                 mov rax, qword ptr [rdi]
0x4bf07f  (f 0xbf07f)  4889de                 mov rsi, rbx
0x4bf082  (f 0xbf082)  ff5058                 call qword ptr [rax + 0x58]
0x4bf085  (f 0xbf085)  4883c408               add rsp, 8
0x4bf089  (f 0xbf089)  5b                     pop rbx
0x4bf08a  (f 0xbf08a)  415e                   pop r14
0x4bf08c  (f 0xbf08c)  415f                   pop r15
0x4bf08e  (f 0xbf08e)  5d                     pop rbp
0x4bf08f  (f 0xbf08f)  c3                     ret 
0x4bf090  (f 0xbf090)  488d3db1cf2609         lea rdi, [rip + 0x926cfb1]  ; rip->0x972c048 (file 0x932c048)
0x4bf097  (f 0xbf097)  4189d7                 mov r15d, edx
0x4bf09a  (f 0xbf09a)  e8112e3b07             call 0x7871eb0
0x4bf09f  (f 0xbf09f)  4489fa                 mov edx, r15d
0x4bf0a2  (f 0xbf0a2)  85c0                   test eax, eax
0x4bf0a4  (f 0xbf0a4)  74d3                   je 0x4bf079
0x4bf0a6  (f 0xbf0a6)  e8f5faffff             call 0x4beba0
0x4bf0ab  (f 0xbf0ab)  488d3d96cf2609         lea rdi, [rip + 0x926cf96]  ; rip->0x972c048 (file 0x932c048)
0x4bf0b2  (f 0xbf0b2)  e8092e3b07             call 0x7871ec0
0x4bf0b7  (f 0xbf0b7)  4489fa                 mov edx, r15d
0x4bf0ba  (f 0xbf0ba)  ebbd                   jmp 0x4bf079
--- calls: [('0x8bf09a', '0x7871eb0'), ('0x8bf0a6', '0x4beba0'), ('0x8bf0b2', '0x7871ec0')]

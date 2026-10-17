0x7c2180  (f 0x3c2180)  55                     push rbp
0x7c2181  (f 0x3c2181)  4889e5                 mov rbp, rsp
0x7c2184  (f 0x3c2184)  488d05bdd4b307         lea rax, [rip + 0x7b3d4bd]  ; rip->0x82ff648 (file 0x7eff648)
0x7c218b  (f 0x3c218b)  4883c010               add rax, 0x10
0x7c218f  (f 0x3c218f)  488907                 mov qword ptr [rdi], rax
0x7c2192  (f 0x3c2192)  488d3da7bf1c07         lea rdi, [rip + 0x71cbfa7]  ; rip->0x798e140 (file 0x758e140)
0x7c2199  (f 0x3c2199)  e8f261ddff             call 0x598390
0x7c219e  (f 0x3c219e)  5d                     pop rbp
0x7c219f  (f 0x3c219f)  c3                     ret 
--- calls: [('0xbc2199', '0x598390')]

0x7c2060  (f 0x3c2060)  488b05814cfe08         mov rax, qword ptr [rip + 0x8fe4c81]  ; rip->0x97a6ce8 (file 0x93a6ce8)
0x7c2067  (f 0x3c2067)  4885c0                 test rax, rax
0x7c206a  (f 0x3c206a)  7401                   je 0x7c206d
0x7c206c  (f 0x3c206c)  c3                     ret 
0x7c206d  (f 0x3c206d)  55                     push rbp
0x7c206e  (f 0x3c206e)  4889e5                 mov rbp, rsp
0x7c2071  (f 0x3c2071)  488d3d704cfe08         lea rdi, [rip + 0x8fe4c70]  ; rip->0x97a6ce8 (file 0x93a6ce8)
0x7c2078  (f 0x3c2078)  488d35e168b207         lea rsi, [rip + 0x7b268e1]  ; rip->0x82e8960 (file 0x7ee8960)
0x7c207f  (f 0x3c207f)  e88c750d00             call 0x899610
0x7c2084  (f 0x3c2084)  488b055d4cfe08         mov rax, qword ptr [rip + 0x8fe4c5d]  ; rip->0x97a6ce8 (file 0x93a6ce8)
0x7c208b  (f 0x3c208b)  5d                     pop rbp
0x7c208c  (f 0x3c208c)  c3                     ret 
--- calls: [('0xbc207f', '0x899610')]

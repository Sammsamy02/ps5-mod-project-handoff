0x7e1cd0  (f 0x3e1cd0)  0fb6059958fc08         movzx eax, byte ptr [rip + 0x8fc5899]  ; rip->0x97a7570 (file 0x93a7570)
0x7e1cd7  (f 0x3e1cd7)  84c0                   test al, al
0x7e1cd9  (f 0x3e1cd9)  7408                   je 0x7e1ce3
0x7e1cdb  (f 0x3e1cdb)  488d054e58fc08         lea rax, [rip + 0x8fc584e]  ; rip->0x97a7530 (file 0x93a7530)
0x7e1ce2  (f 0x3e1ce2)  c3                     ret 
0x7e1ce3  (f 0x3e1ce3)  55                     push rbp
0x7e1ce4  (f 0x3e1ce4)  4889e5                 mov rbp, rsp
0x7e1ce7  (f 0x3e1ce7)  53                     push rbx
0x7e1ce8  (f 0x3e1ce8)  50                     push rax
0x7e1ce9  (f 0x3e1ce9)  488d3d8058fc08         lea rdi, [rip + 0x8fc5880]  ; rip->0x97a7570 (file 0x93a7570)
0x7e1cf0  (f 0x3e1cf0)  e8bb010907             call 0x7871eb0
0x7e1cf5  (f 0x3e1cf5)  85c0                   test eax, eax
0x7e1cf7  (f 0x3e1cf7)  7451                   je 0x7e1d4a
0x7e1cf9  (f 0x3e1cf9)  e83228f8ff             call 0x764530
0x7e1cfe  (f 0x3e1cfe)  488d1d2b58fc08         lea rbx, [rip + 0x8fc582b]  ; rip->0x97a7530 (file 0x93a7530)
0x7e1d05  (f 0x3e1d05)  488d3572c41a07         lea rsi, [rip + 0x71ac472]  ; rip->0x798e17e (file 0x758e17e)
0x7e1d0c  (f 0x3e1d0c)  4c8d0d3dfeffff         lea r9, [rip - 0x1c3]  ; rip->0x7e1b50 (file 0x3e1b50)
0x7e1d13  (f 0x3e1d13)  ba00800000             mov edx, 0x8000
0x7e1d18  (f 0x3e1d18)  b901800000             mov ecx, 0x8001
0x7e1d1d  (f 0x3e1d1d)  4989c0                 mov r8, rax
0x7e1d20  (f 0x3e1d20)  4889df                 mov rdi, rbx
0x7e1d23  (f 0x3e1d23)  e8681cf8ff             call 0x763990
0x7e1d28  (f 0x3e1d28)  488d3dd11ef8ff         lea rdi, [rip - 0x7e12f]  ; rip->0x763c00 (file 0x363c00)
0x7e1d2f  (f 0x3e1d2f)  488d15caa2ef08         lea rdx, [rip + 0x8efa2ca]  ; rip->0x96dc000 (file 0x92dc000)
0x7e1d36  (f 0x3e1d36)  4889de                 mov rsi, rbx
0x7e1d39  (f 0x3e1d39)  e892010907             call 0x7871ed0
0x7e1d3e  (f 0x3e1d3e)  488d3d2b58fc08         lea rdi, [rip + 0x8fc582b]  ; rip->0x97a7570 (file 0x93a7570)
0x7e1d45  (f 0x3e1d45)  e876010907             call 0x7871ec0
0x7e1d4a  (f 0x3e1d4a)  4883c408               add rsp, 8
0x7e1d4e  (f 0x3e1d4e)  5b                     pop rbx
0x7e1d4f  (f 0x3e1d4f)  5d                     pop rbp
0x7e1d50  (f 0x3e1d50)  488d05d957fc08         lea rax, [rip + 0x8fc57d9]  ; rip->0x97a7530 (file 0x93a7530)
0x7e1d57  (f 0x3e1d57)  c3                     ret 
--- calls: [('0xbe1cf0', '0x7871eb0'), ('0xbe1cf9', '0x764530'), ('0xbe1d23', '0x763990'), ('0xbe1d39', '0x7871ed0'), ('0xbe1d45', '0x7871ec0')]

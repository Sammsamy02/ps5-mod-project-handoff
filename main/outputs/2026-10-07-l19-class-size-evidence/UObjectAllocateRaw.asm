0x880d70  (f 0x480d70)  55                     push rbp
0x880d71  (f 0x480d71)  4889e5                 mov rbp, rsp
0x880d74  (f 0x480d74)  4156                   push r14
0x880d76  (f 0x480d76)  53                     push rbx
0x880d77  (f 0x480d77)  85c9                   test ecx, ecx
0x880d79  (f 0x480d79)  7409                   je 0x880d84
0x880d7b  (f 0x480d7b)  803df08df30800         cmp byte ptr [rip + 0x8f38df0], 0  ; rip->0x97b9b72 (file 0x93b9b72)
0x880d82  (f 0x480d82)  740f                   je 0x880d93
0x880d84  (f 0x480d84)  4863fe                 movsxd rdi, esi
0x880d87  (f 0x480d87)  89d6                   mov esi, edx
0x880d89  (f 0x480d89)  e8926cc3ff             call 0x4b7a20
0x880d8e  (f 0x480d8e)  5b                     pop rbx
0x880d8f  (f 0x480d8f)  415e                   pop r14
0x880d91  (f 0x480d91)  5d                     pop rbp
0x880d92  (f 0x480d92)  c3                     ret 
0x880d93  (f 0x480d93)  89d3                   mov ebx, edx
0x880d95  (f 0x480d95)  4189f6                 mov r14d, esi
0x880d98  (f 0x480d98)  e8932ecfff             call 0x573c30
0x880d9d  (f 0x480d9d)  4963f6                 movsxd rsi, r14d
0x880da0  (f 0x480da0)  4889c7                 mov rdi, rax
0x880da3  (f 0x480da3)  89da                   mov edx, ebx
0x880da5  (f 0x480da5)  e8962dcfff             call 0x573b40
0x880daa  (f 0x480daa)  5b                     pop rbx
0x880dab  (f 0x480dab)  415e                   pop r14
0x880dad  (f 0x480dad)  5d                     pop rbp
0x880dae  (f 0x480dae)  c3                     ret 
--- calls: [('0xc80d89', '0x4b7a20'), ('0xc80d98', '0x573c30'), ('0xc80da5', '0x573b40')]

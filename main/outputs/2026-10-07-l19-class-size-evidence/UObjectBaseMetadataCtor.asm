0x7c60b0  (f 0x3c60b0)  55                     push rbp
0x7c60b1  (f 0x3c60b1)  4889e5                 mov rbp, rsp
0x7c60b4  (f 0x3c60b4)  53                     push rbx
0x7c60b5  (f 0x3c60b5)  50                     push rax
0x7c60b6  (f 0x3c60b6)  81ca84000000           or edx, 0x84
0x7c60bc  (f 0x3c60bc)  4889fb                 mov rbx, rdi
0x7c60bf  (f 0x3c60bf)  89d6                   mov esi, edx
0x7c60c1  (f 0x3c60c1)  e83af30b00             call 0x885400
0x7c60c6  (f 0x3c60c6)  488d057b95b307         lea rax, [rip + 0x7b3957b]  ; rip->0x82ff648 (file 0x7eff648)
0x7c60cd  (f 0x3c60cd)  488d0dcc2af908         lea rcx, [rip + 0x8f92acc]  ; rip->0x9758ba0 (file 0x9358ba0)
0x7c60d4  (f 0x3c60d4)  4883c010               add rax, 0x10
0x7c60d8  (f 0x3c60d8)  488903                 mov qword ptr [rbx], rax
0x7c60db  (f 0x3c60db)  803901                 cmp byte ptr [rcx], 1
0x7c60de  (f 0x3c60de)  7407                   je 0x7c60e7
0x7c60e0  (f 0x3c60e0)  4883c408               add rsp, 8
0x7c60e4  (f 0x3c60e4)  5b                     pop rbx
0x7c60e5  (f 0x3c60e5)  5d                     pop rbp
0x7c60e6  (f 0x3c60e6)  c3                     ret 
0x7c60e7  (f 0x3c60e7)  488d3d62de7e07         lea rdi, [rip + 0x77ede62]  ; rip->0x7fb3f50 (file 0x7bb3f50)
0x7c60ee  (f 0x3c60ee)  488d150b3f4b07         lea rdx, [rip + 0x74b3f0b]  ; rip->0x7c7a000 (file 0x787a000)
0x7c60f5  (f 0x3c60f5)  beb5000000             mov esi, 0xb5
0x7c60fa  (f 0x3c60fa)  31c0                   xor eax, eax
0x7c60fc  (f 0x3c60fc)  e8afefdaff             call 0x5750b0
0x7c6101  (f 0x3c6101)  4883c408               add rsp, 8
0x7c6105  (f 0x3c6105)  5b                     pop rbx
0x7c6106  (f 0x3c6106)  5d                     pop rbp
0x7c6107  (f 0x3c6107)  c3                     ret 
--- calls: [('0xbc60c1', '0x885400'), ('0xbc60fc', '0x5750b0')]

0x764530  (f 0x364530)  0fb605a1a70309         movzx eax, byte ptr [rip + 0x903a7a1]  ; rip->0x979ecd8 (file 0x939ecd8)
0x764537  (f 0x364537)  84c0                   test al, al
0x764539  (f 0x364539)  7408                   je 0x764543
0x76453b  (f 0x36453b)  488d0556a70309         lea rax, [rip + 0x903a756]  ; rip->0x979ec98 (file 0x939ec98)
0x764542  (f 0x364542)  c3                     ret 
0x764543  (f 0x364543)  55                     push rbp
0x764544  (f 0x364544)  4889e5                 mov rbp, rsp
0x764547  (f 0x364547)  53                     push rbx
0x764548  (f 0x364548)  50                     push rax
0x764549  (f 0x364549)  488d3d88a70309         lea rdi, [rip + 0x903a788]  ; rip->0x979ecd8 (file 0x939ecd8)
0x764550  (f 0x364550)  e85bd91007             call 0x7871eb0
0x764555  (f 0x364555)  85c0                   test eax, eax
0x764557  (f 0x364557)  744c                   je 0x7645a5
0x764559  (f 0x364559)  488d1d38a70309         lea rbx, [rip + 0x903a738]  ; rip->0x979ec98 (file 0x939ec98)
0x764560  (f 0x364560)  488d355b924107         lea rsi, [rip + 0x741925b]  ; rip->0x7b7d7c2 (file 0x777d7c2)
0x764567  (f 0x364567)  4c8d0d72bbc9ff         lea r9, [rip - 0x36448e]  ; rip->0x4000e0 (file 0xe0)
0x76456e  (f 0x36456e)  ba01000000             mov edx, 1
0x764573  (f 0x364573)  b901000000             mov ecx, 1
0x764578  (f 0x364578)  4531c0                 xor r8d, r8d
0x76457b  (f 0x36457b)  4889df                 mov rdi, rbx
0x76457e  (f 0x36457e)  e80df4ffff             call 0x763990
0x764583  (f 0x364583)  488d3d76f6ffff         lea rdi, [rip - 0x98a]  ; rip->0x763c00 (file 0x363c00)
0x76458a  (f 0x36458a)  488d156f7af708         lea rdx, [rip + 0x8f77a6f]  ; rip->0x96dc000 (file 0x92dc000)
0x764591  (f 0x364591)  4889de                 mov rsi, rbx
0x764594  (f 0x364594)  e837d91007             call 0x7871ed0
0x764599  (f 0x364599)  488d3d38a70309         lea rdi, [rip + 0x903a738]  ; rip->0x979ecd8 (file 0x939ecd8)
0x7645a0  (f 0x3645a0)  e81bd91007             call 0x7871ec0
0x7645a5  (f 0x3645a5)  4883c408               add rsp, 8
0x7645a9  (f 0x3645a9)  5b                     pop rbx
0x7645aa  (f 0x3645aa)  5d                     pop rbp
0x7645ab  (f 0x3645ab)  488d05e6a60309         lea rax, [rip + 0x903a6e6]  ; rip->0x979ec98 (file 0x939ec98)
0x7645b2  (f 0x3645b2)  c3                     ret 
--- calls: [('0xb64550', '0x7871eb0'), ('0xb6457e', '0x763990'), ('0xb64594', '0x7871ed0'), ('0xb645a0', '0x7871ec0')]

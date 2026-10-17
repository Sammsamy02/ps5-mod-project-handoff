0x2fdc2e0: f6872803000004           test byte ptr [rdi + 0x328], 4
0x2fdc2e7: b001                     mov al, 1
0x2fdc2e9: 750f                     jne 0x2fdc2fa
0x2fdc2eb: 55                       push rbp
0x2fdc2ec: 4889e5                   mov rbp, rsp
0x2fdc2ef: e80cafb5ff               call 0x2b37200
0x2fdc2f4: 85c0                     test eax, eax
0x2fdc2f6: 0f94c0                   sete al
0x2fdc2f9: 5d                       pop rbp
0x2fdc2fa: c3                       ret 
0x2fdc2fb: cc                       int3 
0x2fdc2fc: cc                       int3 
0x2fdc2fd: cc                       int3 
0x2fdc2fe: cc                       int3 
0x2fdc2ff: cc                       int3 

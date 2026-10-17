0x360e70 (file 0x364e70) 55 push rbp
0x360e71 (file 0x364e71) 4889e5 mov rbp, rsp
0x360e74 (file 0x364e74) 488b4778 mov rax, qword ptr [rdi + 0x78]
0x360e78 (file 0x364e78) 48634f30 movsxd rcx, dword ptr [rdi + 0x30]
0x360e7c (file 0x364e7c) 48635048 movsxd rdx, dword ptr [rax + 0x48]
0x360e80 (file 0x364e80) 4885d2 test rdx, rdx
0x360e83 (file 0x364e83) 0f845f010000 je 0x360fe8
0x360e89 (file 0x364e89) f6405101 test byte ptr [rax + 0x51], 1
0x360e8d (file 0x364e8d) 488b4040 mov rax, qword ptr [rax + 0x40]
0x360e91 (file 0x364e91) 0f8558010000 jne 0x360fef
0x360e97 (file 0x364e97) 488b7808 mov rdi, qword ptr [rax + 8]
0x360e9b (file 0x364e9b) 83fa02 cmp edx, 2
0x360e9e (file 0x364e9e) 0f8ca5020000 jl 0x361149
0x360ea4 (file 0x364ea4) be01000000 mov esi, 1
0x360ea9 (file 0x364ea9) 83fa12 cmp edx, 0x12

0x401dd0: 0fb6055960fa08           movzx eax, byte ptr [rip + 0x8fa6059]
0x401dd7: 84c0                     test al, al
0x401dd9: 7408                     je 0x401de3
0x401ddb: 488d050e60fa08           lea rax, [rip + 0x8fa600e]
0x401de2: c3                       ret 
0x401de3: 55                       push rbp
0x401de4: 4889e5                   mov rbp, rsp
0x401de7: 53                       push rbx
0x401de8: 50                       push rax
0x401de9: 488d3d4060fa08           lea rdi, [rip + 0x8fa6040]
0x401df0: e8bb000707               call 0x7471eb0
0x401df5: 85c0                     test eax, eax
0x401df7: 7455                     je 0x401e4e
0x401df9: e89299ffff               call 0x3fb790
0x401dfe: 488d1deb5ffa08           lea rbx, [rip + 0x8fa5feb]
0x401e05: 488d35b4174a07           lea rsi, [rip + 0x74a17b4] ; ['FObjectProperty']
0x401e0c: 4c8d056d59fa08           lea r8, [rip + 0x8fa596d]
0x401e13: 4c8d0d066e0100           lea r9, [rip + 0x16e06]
0x401e1a: ba00000100               mov edx, 0x10000
0x401e1f: b901800104               mov ecx, 0x4018001
0x401e24: 4889df                   mov rdi, rbx
0x401e27: e8641bf6ff               call 0x363990
0x401e2c: 488d3dcd1df6ff           lea rdi, [rip - 0x9e233]
0x401e33: 488d15c6a1ed08           lea rdx, [rip + 0x8eda1c6]
0x401e3a: 4889de                   mov rsi, rbx
0x401e3d: e88e000707               call 0x7471ed0
0x401e42: 488d3de75ffa08           lea rdi, [rip + 0x8fa5fe7]
0x401e49: e872000707               call 0x7471ec0
0x401e4e: 4883c408                 add rsp, 8
0x401e52: 5b                       pop rbx
0x401e53: 5d                       pop rbp
0x401e54: 488d05955ffa08           lea rax, [rip + 0x8fa5f95]
0x401e5b: c3                       ret 
0x401e5c: cc                       int3 
0x401e5d: cc                       int3 
0x401e5e: cc                       int3 
0x401e5f: cc                       int3 

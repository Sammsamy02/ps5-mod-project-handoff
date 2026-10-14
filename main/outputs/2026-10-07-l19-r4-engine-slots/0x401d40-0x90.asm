0x401d40: 0fb605515bfa08           movzx eax, byte ptr [rip + 0x8fa5b51]
0x401d47: 84c0                     test al, al
0x401d49: 7408                     je 0x401d53
0x401d4b: 488d05065bfa08           lea rax, [rip + 0x8fa5b06]
0x401d52: c3                       ret 
0x401d53: 55                       push rbp
0x401d54: 4889e5                   mov rbp, rsp
0x401d57: 53                       push rbx
0x401d58: 50                       push rax
0x401d59: 488d3d385bfa08           lea rdi, [rip + 0x8fa5b38]
0x401d60: e84b010707               call 0x7471eb0
0x401d65: 85c0                     test eax, eax
0x401d67: 7455                     je 0x401dbe
0x401d69: e862000000               call 0x401dd0
0x401d6e: 488d1de35afa08           lea rbx, [rip + 0x8fa5ae3]
0x401d75: 488d351a0c5c07           lea rsi, [rip + 0x75c0c1a] ; ['FClassProperty']
0x401d7c: 4c8d056d60fa08           lea r8, [rip + 0x8fa606d]
0x401d83: 4c8d0d46feffff           lea r9, [rip - 0x1ba]
0x401d8a: ba00040000               mov edx, 0x400
0x401d8f: b901840104               mov ecx, 0x4018401
0x401d94: 4889df                   mov rdi, rbx
0x401d97: e8f41bf6ff               call 0x363990
0x401d9c: 488d3d5d1ef6ff           lea rdi, [rip - 0x9e1a3]
0x401da3: 488d1556a2ed08           lea rdx, [rip + 0x8eda256]
0x401daa: 4889de                   mov rsi, rbx
0x401dad: e81e010707               call 0x7471ed0
0x401db2: 488d3ddf5afa08           lea rdi, [rip + 0x8fa5adf]
0x401db9: e802010707               call 0x7471ec0
0x401dbe: 4883c408                 add rsp, 8
0x401dc2: 5b                       pop rbx
0x401dc3: 5d                       pop rbp
0x401dc4: 488d058d5afa08           lea rax, [rip + 0x8fa5a8d]
0x401dcb: c3                       ret 
0x401dcc: cc                       int3 
0x401dcd: cc                       int3 
0x401dce: cc                       int3 
0x401dcf: cc                       int3 

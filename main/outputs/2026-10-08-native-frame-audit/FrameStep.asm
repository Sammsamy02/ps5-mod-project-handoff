0x4577a0 file0x45b7a0 55 push rbp
0x4577a1 file0x45b7a1 4889e5 mov rbp, rsp
0x4577a4 file0x45b7a4 488b4f20 mov rcx, qword ptr [rdi + 0x20]
0x4577a8 file0x45b7a8 4889f8 mov rax, rdi
0x4577ab file0x45b7ab 4c8d054e11f608 lea r8, [rip + 0x8f6114e] rip->0x93b8900
0x4577b2 file0x45b7b2 488d7901 lea rdi, [rcx + 1]
0x4577b6 file0x45b7b6 48897820 mov qword ptr [rax + 0x20], rdi
0x4577ba file0x45b7ba 4889f7 mov rdi, rsi
0x4577bd file0x45b7bd 4889c6 mov rsi, rax
0x4577c0 file0x45b7c0 0fb609 movzx ecx, byte ptr [rcx]
0x4577c3 file0x45b7c3 41ff14c8 call qword ptr [r8 + rcx*8]
0x4577c7 file0x45b7c7 5d pop rbp
0x4577c8 file0x45b7c8 c3 ret 
0x4577c9 file0x45b7c9 cc int3 

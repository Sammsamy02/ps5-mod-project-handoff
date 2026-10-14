0xa0a783 file0xa0e783 4c89ef mov rdi, r13
0xa0a786 file0xa0e786 e815ee6aff call 0xb95a0
0xa0a78b file0xa0e78b 488d050ae39408 lea rax, [rip + 0x894e30a] rip->0x9358a9c
0xa0a792 file0xa0e792 c5fb118578ffffff vmovsd qword ptr [rbp - 0x88], xmm0
0xa0a79a file0xa0e79a 8b38 mov edi, dword ptr [rax]
0xa0a79c file0xa0e79c 89bd40ffffff mov dword ptr [rbp - 0xc0], edi
0xa0a7a2 file0xa0e7a2 e839bb0100 call 0xa262e0
0xa0a7a7 file0xa0e7a7 48c745c000000000 mov qword ptr [rbp - 0x40], 0
0xa0a7af file0xa0e7af 488b08 mov rcx, qword ptr [rax]
0xa0a7b2 file0xa0e7b2 4c637008 movsxd r14, dword ptr [rax + 8]
0xa0a7b6 file0xa0e7b6 448975c8 mov dword ptr [rbp - 0x38], r14d
0xa0a7ba file0xa0e7ba 4d85f6 test r14, r14
0xa0a7bd file0xa0e7bd 7565 jne 0xa0a824
0xa0a7bf file0xa0e7bf c745cc00000000 mov dword ptr [rbp - 0x34], 0

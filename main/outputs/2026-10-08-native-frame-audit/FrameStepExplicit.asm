0x4577d0 file0x45b7d0 55 push rbp
0x4577d1 file0x45b7d1 4889e5 mov rbp, rsp
0x4577d4 file0x45b7d4 f6423901 test byte ptr [rdx + 0x39], 1
0x4577d8 file0x45b7d8 7524 jne 0x4577fe
0x4577da file0x45b7da 488b4728 mov rax, qword ptr [rdi + 0x28]
0x4577de file0x45b7de 48634a44 movsxd rcx, dword ptr [rdx + 0x44]
0x4577e2 file0x45b7e2 4801c1 add rcx, rax
0x4577e5 file0x45b7e5 48894f38 mov qword ptr [rdi + 0x38], rcx
0x4577e9 file0x45b7e9 48894740 mov qword ptr [rdi + 0x40], rax
0x4577ed file0x45b7ed 4889d7 mov rdi, rdx
0x4577f0 file0x45b7f0 488b0a mov rcx, qword ptr [rdx]
0x4577f3 file0x45b7f3 4889c2 mov rdx, rax
0x4577f6 file0x45b7f6 ff9118010000 call qword ptr [rcx + 0x118]
0x4577fc file0x45b7fc 5d pop rbp
0x4577fd file0x45b7fd c3 ret 
0x4577fe file0x45b7fe 488d8780000000 lea rax, [rdi + 0x80]
0x457805 file0x45b805 66662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x457810 file0x45b810 488b08 mov rcx, qword ptr [rax]
0x457813 file0x45b813 488d4110 lea rax, [rcx + 0x10]
0x457817 file0x45b817 483911 cmp qword ptr [rcx], rdx
0x45781a file0x45b81a 75f4 jne 0x457810
0x45781c file0x45b81c 488b4108 mov rax, qword ptr [rcx + 8]
0x457820 file0x45b820 48894738 mov qword ptr [rdi + 0x38], rax
0x457824 file0x45b824 48c7474000000000 mov qword ptr [rdi + 0x40], 0
0x45782c file0x45b82c 5d pop rbp
0x45782d file0x45b82d c3 ret 

0x3a14e40 file0x3a18e40 85db test ebx, ebx
0x3a14e42 file0x3a18e42 741d je 0x3a14e61
0x3a14e44 file0x3a18e44 f0ff4b08 lock dec dword ptr [rbx + 8]
0x3a14e48 file0x3a18e48 7517 jne 0x3a14e61
0x3a14e4a file0x3a18e4a 488b03 mov rax, qword ptr [rbx]
0x3a14e4d file0x3a18e4d 4889df mov rdi, rbx
0x3a14e50 file0x3a18e50 ff10 call qword ptr [rax]
0x3a14e52 file0x3a18e52 f0ff4b0c lock dec dword ptr [rbx + 0xc]
0x3a14e56 file0x3a18e56 7509 jne 0x3a14e61
0x3a14e58 file0x3a18e58 488b03 mov rax, qword ptr [rbx]
0x3a14e5b file0x3a18e5b 4889df mov rdi, rbx
0x3a14e5e file0x3a18e5e ff5010 call qword ptr [rax + 0x10]
0x3a14e61 file0x3a18e61 488bbd20feffff mov rdi, qword ptr [rbp - 0x1e0]
0x3a14e68 file0x3a18e68 4885ff test rdi, rdi
0x3a14e6b file0x3a18e6b 7406 je 0x3a14e73
0x3a14e6d file0x3a18e6d 488b07 mov rax, qword ptr [rdi]
0x3a14e70 file0x3a18e70 ff5018 call qword ptr [rax + 0x18]
0x3a14e73 file0x3a18e73 488bbdf0fdffff mov rdi, qword ptr [rbp - 0x210]
0x3a14e7a file0x3a18e7a 4885ff test rdi, rdi
0x3a14e7d file0x3a18e7d 0f840f280000 je 0x3a17692
0x3a14e83 file0x3a18e83 488b07 mov rax, qword ptr [rdi]
0x3a14e86 file0x3a18e86 ff5018 call qword ptr [rax + 0x18]
0x3a14e89 file0x3a18e89 e904280000 jmp 0x3a17692

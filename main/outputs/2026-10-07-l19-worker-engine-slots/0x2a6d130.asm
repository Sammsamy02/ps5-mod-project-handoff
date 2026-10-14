2a6d130 55 push rbp
2a6d131 4889e5 mov rbp, rsp
2a6d134 4156 push r14
2a6d136 53 push rbx
2a6d137 4883ec10 sub rsp, 0x10
2a6d13b 488b1dc64d6c06 mov rbx, qword ptr [rip + 0x66c4dc6]
2a6d142 488b03 mov rax, qword ptr [rbx]
2a6d145 488945e8 mov qword ptr [rbp - 0x18], rax
2a6d149 488b05a8a7a906 mov rax, qword ptr [rip + 0x6a9a7a8]
2a6d150 4885c0 test rax, rax
2a6d153 756c jne 0x2a6d1c1
2a6d155 488d05f467dd00 lea rax, [rip + 0xdd67f4]
2a6d15c 4c8d55e0 lea r10, [rbp - 0x20]
2a6d160 4c8d1d792f59fd lea r11, [rip - 0x2a6d087]
2a6d167 4c8d35a2ffffff lea r14, [rip - 0x5e]
2a6d16e 488d35f99edb04 lea rsi, [rip + 0x4db9ef9]
2a6d175 488d3d064af504 lea rdi, [rip + 0x4f54a06]
2a6d17c 488d1575a7a906 lea rdx, [rip + 0x6a9a775]
2a6d183 488d0d06e159fd lea rcx, [rip - 0x2a61efa]
2a6d18a 41b840120000 mov r8d, 0x1240
2a6d190 41b908000000 mov r9d, 8
2a6d196 488945e0 mov qword ptr [rbp - 0x20], rax
2a6d19a 488d054ff382fd lea rax, [rip - 0x27d0cb1]
2a6d1a1 50 push rax
2a6d1a2 50 push rax
2a6d1a3 4152 push r10
2a6d1a5 4153 push r11
2a6d1a7 4156 push r14
2a6d1a9 56 push rsi
2a6d1aa 6a00 push 0
2a6d1ac 680f000010 push 0x1000000f
2a6d1b1 e8aa068afd call 0x30d860
2a6d1b6 4883c440 add rsp, 0x40

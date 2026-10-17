0x363990 (file 0x367990) 55 push rbp
0x363991 (file 0x367991) 4889e5 mov rbp, rsp
0x363994 (file 0x367994) 4156 push r14
0x363996 (file 0x367996) 53 push rbx
0x363997 (file 0x367997) 4883ec20 sub rsp, 0x20
0x36399b (file 0x36799b) 4c8b3566e5dc08 mov r14, qword ptr [rip + 0x8dce566]
0x3639a2 (file 0x3679a2) 4889fb mov rbx, rdi
0x3639a5 (file 0x3679a5) 4883c602 add rsi, 2
0x3639a9 (file 0x3679a9) 498b06 mov rax, qword ptr [r14]
0x3639ac (file 0x3679ac) 488945e8 mov qword ptr [rbp - 0x18], rax
0x3639b0 (file 0x3679b0) 48c70700000000 mov qword ptr [rdi], 0
0x3639b7 (file 0x3679b7) 48895708 mov qword ptr [rdi + 8], rdx
0x3639bb (file 0x3679bb) 48894f10 mov qword ptr [rdi + 0x10], rcx
0x3639bf (file 0x3679bf) c7471800000000 mov dword ptr [rdi + 0x18], 0
0x3639c6 (file 0x3679c6) 4c894720 mov qword ptr [rdi + 0x20], r8
0x3639ca (file 0x3679ca) 48c7472800000000 mov qword ptr [rdi + 0x28], 0
0x3639d2 (file 0x3679d2) 4c894f30 mov qword ptr [rdi + 0x30], r9
0x3639d6 (file 0x3679d6) c7473800000000 mov dword ptr [rdi + 0x38], 0
0x3639dd (file 0x3679dd) 488d7dd8 lea rdi, [rbp - 0x28]
0x3639e1 (file 0x3679e1) ba01000000 mov edx, 1
0x3639e6 (file 0x3679e6) e8a59bedff call 0x23d590
0x3639eb (file 0x3679eb) 488b45d8 mov rax, qword ptr [rbp - 0x28]
0x3639ef (file 0x3679ef) 488903 mov qword ptr [rbx], rax

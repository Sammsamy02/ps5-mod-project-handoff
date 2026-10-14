0x2fdda70: 55                       push rbp
0x2fdda71: 4889e5                   mov rbp, rsp
0x2fdda74: 4157                     push r15
0x2fdda76: 4156                     push r14
0x2fdda78: 4155                     push r13
0x2fdda7a: 4154                     push r12
0x2fdda7c: 53                       push rbx
0x2fdda7d: 4883ec18                 sub rsp, 0x18
0x2fdda81: 488b1d80441506           mov rbx, qword ptr [rip + 0x6154480]
0x2fdda88: 4c8d05cd133c06           lea r8, [rip + 0x63c13cd]
0x2fdda8f: 488b03                   mov rax, qword ptr [rbx]
0x2fdda92: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdda96: 488b87f0020000           mov rax, qword ptr [rdi + 0x2f0]
0x2fdda9d: 4885c0                   test rax, rax
0x2fddaa0: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x2fddaa4: 410f95c1                 setne r9b
0x2fddaa8: 452208                   and r9b, byte ptr [r8]
0x2fddaab: 4180f901                 cmp r9b, 1
0x2fddaaf: 7520                     jne 0x2fddad1
0x2fddab1: 4989fe                   mov r14, rdi
0x2fddab4: 4889c7                   mov rdi, rax
0x2fddab7: 4989cf                   mov r15, rcx
0x2fddaba: 4989d5                   mov r13, rdx
0x2fddabd: 4189f4                   mov r12d, esi
0x2fddac0: e8fba738fd               call 0x3682c0
0x2fddac5: 4c89f7                   mov rdi, r14
0x2fddac8: 4489e6                   mov esi, r12d
0x2fddacb: 4c89ea                   mov rdx, r13
0x2fddace: 4c89f9                   mov rcx, r15
0x2fddad1: 488b07                   mov rax, qword ptr [rdi]
0x2fddad4: 4c8d45c8                 lea r8, [rbp - 0x38]
0x2fddad8: ff9048090000             call qword ptr [rax + 0x948]
0x2fddade: 488b0b                   mov rcx, qword ptr [rbx]
0x2fddae1: 483b4dd0                 cmp rcx, qword ptr [rbp - 0x30]
0x2fddae5: 750f                     jne 0x2fddaf6
0x2fddae7: 4883c418                 add rsp, 0x18
0x2fddaeb: 5b                       pop rbx
0x2fddaec: 415c                     pop r12
0x2fddaee: 415d                     pop r13
0x2fddaf0: 415e                     pop r14
0x2fddaf2: 415f                     pop r15
0x2fddaf4: 5d                       pop rbp
0x2fddaf5: c3                       ret 
0x2fddaf6: e865434904               call 0x7471e60
0x2fddafb: 0f0b                     ud2 
0x2fddafd: cc                       int3 
0x2fddafe: cc                       int3 
0x2fddaff: cc                       int3 

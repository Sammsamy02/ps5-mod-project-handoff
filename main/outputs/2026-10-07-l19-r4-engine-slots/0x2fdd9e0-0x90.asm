0x2fdd9e0: 55                       push rbp
0x2fdd9e1: 4889e5                   mov rbp, rsp
0x2fdd9e4: 4157                     push r15
0x2fdd9e6: 4156                     push r14
0x2fdd9e8: 4155                     push r13
0x2fdd9ea: 4154                     push r12
0x2fdd9ec: 53                       push rbx
0x2fdd9ed: 4883ec18                 sub rsp, 0x18
0x2fdd9f1: 488b1d10451506           mov rbx, qword ptr [rip + 0x6154510]
0x2fdd9f8: 4c8d055d143c06           lea r8, [rip + 0x63c145d]
0x2fdd9ff: 488b03                   mov rax, qword ptr [rbx]
0x2fdda02: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdda06: 488b87c8020000           mov rax, qword ptr [rdi + 0x2c8]
0x2fdda0d: 4885c0                   test rax, rax
0x2fdda10: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x2fdda14: 410f95c1                 setne r9b
0x2fdda18: 452208                   and r9b, byte ptr [r8]
0x2fdda1b: 4180f901                 cmp r9b, 1
0x2fdda1f: 7520                     jne 0x2fdda41
0x2fdda21: 4989fe                   mov r14, rdi
0x2fdda24: 4889c7                   mov rdi, rax
0x2fdda27: 4989cf                   mov r15, rcx
0x2fdda2a: 4989d5                   mov r13, rdx
0x2fdda2d: 4189f4                   mov r12d, esi
0x2fdda30: e88ba838fd               call 0x3682c0
0x2fdda35: 4c89f7                   mov rdi, r14
0x2fdda38: 4489e6                   mov esi, r12d
0x2fdda3b: 4c89ea                   mov rdx, r13
0x2fdda3e: 4c89f9                   mov rcx, r15
0x2fdda41: 488b07                   mov rax, qword ptr [rdi]
0x2fdda44: 4c8d45c8                 lea r8, [rbp - 0x38]
0x2fdda48: ff9048090000             call qword ptr [rax + 0x948]
0x2fdda4e: 488b0b                   mov rcx, qword ptr [rbx]
0x2fdda51: 483b4dd0                 cmp rcx, qword ptr [rbp - 0x30]
0x2fdda55: 750f                     jne 0x2fdda66
0x2fdda57: 4883c418                 add rsp, 0x18
0x2fdda5b: 5b                       pop rbx
0x2fdda5c: 415c                     pop r12
0x2fdda5e: 415d                     pop r13
0x2fdda60: 415e                     pop r14
0x2fdda62: 415f                     pop r15
0x2fdda64: 5d                       pop rbp
0x2fdda65: c3                       ret 
0x2fdda66: e8f5434904               call 0x7471e60
0x2fdda6b: 0f0b                     ud2 
0x2fdda6d: cc                       int3 
0x2fdda6e: cc                       int3 
0x2fdda6f: cc                       int3 

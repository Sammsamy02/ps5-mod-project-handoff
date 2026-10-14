0x3645c0: 55                       push rbp
0x3645c1: 4889e5                   mov rbp, rsp
0x3645c4: 488d058580b707           lea rax, [rip + 0x7b78085]
0x3645cb: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x3645cf: 488907                   mov qword ptr [rdi], rax
0x3645d2: 48895708                 mov qword ptr [rdi + 8], rdx
0x3645d6: c5f8114710               vmovups xmmword ptr [rdi + 0x10], xmm0
0x3645db: c5f811471c               vmovups xmmword ptr [rdi + 0x1c], xmm0
0x3645e0: 5d                       pop rbp
0x3645e1: c3                       ret 
0x3645e2: cc                       int3 
0x3645e3: cc                       int3 
0x3645e4: cc                       int3 
0x3645e5: cc                       int3 
0x3645e6: cc                       int3 
0x3645e7: cc                       int3 
0x3645e8: cc                       int3 
0x3645e9: cc                       int3 
0x3645ea: cc                       int3 
0x3645eb: cc                       int3 
0x3645ec: cc                       int3 
0x3645ed: cc                       int3 
0x3645ee: cc                       int3 
0x3645ef: cc                       int3 
0x3645f0: 488d055980b707           lea rax, [rip + 0x7b78059]
0x3645f7: 488907                   mov qword ptr [rdi], rax
0x3645fa: 488b06                   mov rax, qword ptr [rsi]
0x3645fd: 48894710                 mov qword ptr [rdi + 0x10], rax
0x364601: a801                     test al, 1
0x364603: 743d                     je 0x364642
0x364605: 4883e0fe                 and rax, 0xfffffffffffffffe
0x364609: 7437                     je 0x364642
0x36460b: 803d4aa8030901           cmp byte ptr [rip + 0x903a84a], 1
0x364612: 752e                     jne 0x364642
0x364614: 55                       push rbp
0x364615: 4889e5                   mov rbp, rsp
0x364618: 4157                     push r15
0x36461a: 4156                     push r14
0x36461c: 53                       push rbx
0x36461d: 50                       push rax
0x36461e: 4889fb                   mov rbx, rdi
0x364621: 4889c7                   mov rdi, rax
0x364624: 4189ce                   mov r14d, ecx
0x364627: 4989d7                   mov r15, rdx
0x36462a: e8a13c0000               call 0x3682d0
0x36462f: 4c89fa                   mov rdx, r15
0x364632: 4889df                   mov rdi, rbx
0x364635: 4489f1                   mov ecx, r14d
0x364638: 4883c408                 add rsp, 8
0x36463c: 5b                       pop rbx
0x36463d: 415e                     pop r14
0x36463f: 415f                     pop r15
0x364641: 5d                       pop rbp
0x364642: 48c7471800000000         mov qword ptr [rdi + 0x18], 0
0x36464a: 488b02                   mov rax, qword ptr [rdx]
0x36464d: 48894720                 mov qword ptr [rdi + 0x20], rax
0x364651: 894f28                   mov dword ptr [rdi + 0x28], ecx
0x364654: c3                       ret 
0x364655: cc                       int3 
0x364656: cc                       int3 
0x364657: cc                       int3 
0x364658: cc                       int3 
0x364659: cc                       int3 
0x36465a: cc                       int3 
0x36465b: cc                       int3 
0x36465c: cc                       int3 
0x36465d: cc                       int3 
0x36465e: cc                       int3 
0x36465f: cc                       int3 
0x364660: 55                       push rbp
0x364661: 4889e5                   mov rbp, rsp
0x364664: 53                       push rbx
0x364665: 50                       push rax
0x364666: 488b5f10                 mov rbx, qword ptr [rdi + 0x10]
0x36466a: f6c301                   test bl, 1
0x36466d: 7524                     jne 0x364693
0x36466f: 90                       nop 
0x364670: 4883fb02                 cmp rbx, 2
0x364674: 7241                     jb 0x3646b7
0x364676: 488b5b10                 mov rbx, qword ptr [rbx + 0x10]
0x36467a: f6c301                   test bl, 1
0x36467d: 74f1                     je 0x364670
0x36467f: 4889df                   mov rdi, rbx
0x364682: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x364686: 7424                     je 0x3646ac
0x364688: 803dcda7030901           cmp byte ptr [rip + 0x903a7cd], 1
0x36468f: 7414                     je 0x3646a5
0x364691: eb1e                     jmp 0x3646b1
0x364693: 4889df                   mov rdi, rbx
0x364696: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x36469a: 7410                     je 0x3646ac
0x36469c: 803db9a7030900           cmp byte ptr [rip + 0x903a7b9], 0
0x3646a3: 740c                     je 0x3646b1
0x3646a5: e8263c0000               call 0x3682d0
0x3646aa: eb05                     jmp 0x3646b1
0x3646ac: bb01000000               mov ebx, 1
0x3646b1: 4883e3fe                 and rbx, 0xfffffffffffffffe
0x3646b5: eb12                     jmp 0x3646c9
0x3646b7: 31db                     xor ebx, ebx
0x3646b9: eb0e                     jmp 0x3646c9
0x3646bb: 0f1f440000               nop dword ptr [rax + rax]
0x3646c0: 488b5b20                 mov rbx, qword ptr [rbx + 0x20]
0x3646c4: 4885db                   test rbx, rbx
0x3646c7: 7427                     je 0x3646f0
0x3646c9: 4885db                   test rbx, rbx
0x3646cc: 74f2                     je 0x3646c0
0x3646ce: e81dc4fbff               call 0x320af0
0x3646d3: 488b4b10                 mov rcx, qword ptr [rbx + 0x10]
0x3646d7: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x3646db: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x3646de: 7fe0                     jg 0x3646c0
0x3646e0: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x3646e4: 4883c030                 add rax, 0x30
0x3646e8: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x3646ec: 75d2                     jne 0x3646c0
0x3646ee: eb02                     jmp 0x3646f2
0x3646f0: 31db                     xor ebx, ebx
0x3646f2: 4889d8                   mov rax, rbx
0x3646f5: 4883c408                 add rsp, 8
0x3646f9: 5b                       pop rbx
0x3646fa: 5d                       pop rbp
0x3646fb: c3                       ret 
0x3646fc: cc                       int3 
0x3646fd: cc                       int3 
0x3646fe: cc                       int3 
0x3646ff: cc                       int3 
0x364700: 55                       push rbp
0x364701: 4889e5                   mov rbp, rsp
0x364704: 488b07                   mov rax, qword ptr [rdi]
0x364707: ff5048                   call qword ptr [rax + 0x48]
0x36470a: 5d                       pop rbp
0x36470b: c3                       ret 
0x36470c: cc                       int3 
0x36470d: cc                       int3 
0x36470e: cc                       int3 
0x36470f: cc                       int3 

0x2fde710: 55                       push rbp
0x2fde711: 4889e5                   mov rbp, rsp
0x2fde714: 4156                     push r14
0x2fde716: 53                       push rbx
0x2fde717: 4883ec10                 sub rsp, 0x10
0x2fde71b: 4c8b35e6371506           mov r14, qword ptr [rip + 0x61537e6]
0x2fde722: 4889f3                   mov rbx, rsi
0x2fde725: 498b06                   mov rax, qword ptr [r14]
0x2fde728: 488945e8                 mov qword ptr [rbp - 0x18], rax
0x2fde72c: 488d0529073c06           lea rax, [rip + 0x63c0729]
0x2fde733: 488bbfd8020000           mov rdi, qword ptr [rdi + 0x2d8]
0x2fde73a: 4885ff                   test rdi, rdi
0x2fde73d: 48897de0                 mov qword ptr [rbp - 0x20], rdi
0x2fde741: 0f95c1                   setne cl
0x2fde744: 2208                     and cl, byte ptr [rax]
0x2fde746: 80f901                   cmp cl, 1
0x2fde749: 7505                     jne 0x2fde750
0x2fde74b: e8709b38fd               call 0x3682c0
0x2fde750: 488d75e0                 lea rsi, [rbp - 0x20]
0x2fde754: 4889df                   mov rdi, rbx
0x2fde757: e8a43b4500               call 0x3432300
0x2fde75c: 498b06                   mov rax, qword ptr [r14]
0x2fde75f: 483b45e8                 cmp rax, qword ptr [rbp - 0x18]
0x2fde763: 7509                     jne 0x2fde76e
0x2fde765: 4883c410                 add rsp, 0x10
0x2fde769: 5b                       pop rbx
0x2fde76a: 415e                     pop r14
0x2fde76c: 5d                       pop rbp
0x2fde76d: c3                       ret 
0x2fde76e: e8ed364904               call 0x7471e60
0x2fde773: 0f0b                     ud2 
0x2fde775: cc                       int3 
0x2fde776: cc                       int3 
0x2fde777: cc                       int3 
0x2fde778: cc                       int3 
0x2fde779: cc                       int3 
0x2fde77a: cc                       int3 
0x2fde77b: cc                       int3 
0x2fde77c: cc                       int3 
0x2fde77d: cc                       int3 
0x2fde77e: cc                       int3 
0x2fde77f: cc                       int3 

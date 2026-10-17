0x8916b0  (f 0x4916b0)  4889df                 mov rdi, rbx
0x8916b3  (f 0x4916b3)  ff10                   call qword ptr [rax]
0x8916b5  (f 0x4916b5)  448bad40fdffff         mov r13d, dword ptr [rbp - 0x2c0]
0x8916bc  (f 0x4916bc)  b101                   mov cl, 1
0x8916be  (f 0x4916be)  4588be94000000         mov byte ptr [r14 + 0x94], r15b
0x8916c5  (f 0x4916c5)  83bd6cfdffff10         cmp dword ptr [rbp - 0x294], 0x10
0x8916cc  (f 0x4916cc)  448b7d20               mov r15d, dword ptr [rbp + 0x20]
0x8916d0  (f 0x4916d0)  4989dc                 mov r12, rbx
0x8916d3  (f 0x4916d3)  48898d40fdffff         mov qword ptr [rbp - 0x2c0], rcx
0x8916da  (f 0x4916da)  7509                   jne 0x8916e5
0x8916dc  (f 0x4916dc)  4c8bb520fdffff         mov r14, qword ptr [rbp - 0x2e0]
0x8916e3  (f 0x4916e3)  eb24                   jmp 0x891709
0x8916e5  (f 0x4916e5)  4c8bb520fdffff         mov r14, qword ptr [rbp - 0x2e0]
0x8916ec  (f 0x4916ec)  4489e8                 mov eax, r13d
0x8916ef  (f 0x4916ef)  83c840                 or eax, 0x40
0x8916f2  (f 0x4916f2)  41f686d400000008       test byte ptr [r14 + 0xd4], 8
0x8916fa  (f 0x4916fa)  410f44c5               cmove eax, r13d
0x8916fe  (f 0x4916fe)  f68534fdffff20         test byte ptr [rbp - 0x2cc], 0x20
0x891705  (f 0x491705)  440f44e8               cmove r13d, eax
0x891709  (f 0x491709)  48639528fdffff         movsxd rdx, dword ptr [rbp - 0x2d8]
0x891710  (f 0x491710)  4c89e7                 mov rdi, r12
0x891713  (f 0x491713)  31f6                   xor esi, esi
0x891715  (f 0x491715)  c7856cfdffff00000000   mov dword ptr [rbp - 0x294], 0
0x89171f  (f 0x49171f)  e81c07fe06             call 0x7871e40
0x891724  (f 0x491724)  8b8554fdffff           mov eax, dword ptr [rbp - 0x2ac]
0x89172a  (f 0x49172a)  4c8b8da8fdffff         mov r9, qword ptr [rbp - 0x258]
0x891731  (f 0x491731)  4181cd00020000         or r13d, 0x200
0x891738  (f 0x491738)  85c0                   test eax, eax
0x89173a  (f 0x49173a)  440f45f8               cmovne r15d, eax
0x89173e  (f 0x49173e)  4883ec10               sub rsp, 0x10
0x891742  (f 0x491742)  8b8d30fdffff           mov ecx, dword ptr [rbp - 0x2d0]
0x891748  (f 0x491748)  4c8b8560fdffff         mov r8, qword ptr [rbp - 0x2a0]
0x89174f  (f 0x49174f)  4c89e7                 mov rdi, r12
0x891752  (f 0x491752)  4c89f6                 mov rsi, r14
0x891755  (f 0x491755)  4489ea                 mov edx, r13d
0x891758  (f 0x491758)  4157                   push r15
0x89175a  (f 0x49175a)  ffb538fdffff           push qword ptr [rbp - 0x2c8]
0x891760  (f 0x491760)  e8cb3cffff             call 0x885430
--- calls: [('0xc9171f', '0x7871e40'), ('0xc91760', '0x885430')]

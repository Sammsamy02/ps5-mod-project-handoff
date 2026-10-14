0x437450 (file 0x43b450) 55 push rbp
0x437451 (file 0x43b451) 4889e5 mov rbp, rsp
0x437454 (file 0x43b454) 488b4770 mov rax, qword ptr [rdi + 0x70]
0x437458 (file 0x43b458) 8b572c mov edx, dword ptr [rdi + 0x2c]
0x43745b (file 0x43b45b) 488b08 mov rcx, qword ptr [rax]
0x43745e (file 0x43b45e) 4889c7 mov rdi, rax
0x437461 (file 0x43b461) ff9108030000 call qword ptr [rcx + 0x308]
0x437467 (file 0x43b467) 5d pop rbp
0x437468 (file 0x43b468) c3 ret 
0x437469 (file 0x43b469) cc int3 
0x43746a (file 0x43b46a) cc int3 
0x43746b (file 0x43b46b) cc int3 
0x43746c (file 0x43b46c) cc int3 
0x43746d (file 0x43b46d) cc int3 
0x43746e (file 0x43b46e) cc int3 
0x43746f (file 0x43b46f) cc int3 
0x437470 (file 0x43b470) 55 push rbp
0x437471 (file 0x43b471) 4889e5 mov rbp, rsp
0x437474 (file 0x43b474) 4157 push r15
0x437476 (file 0x43b476) 4156 push r14
0x437478 (file 0x43b478) 53 push rbx
0x437479 (file 0x43b479) 50 push rax
0x43747a (file 0x43b47a) 48634608 movsxd rax, dword ptr [rsi + 8]
0x43747e (file 0x43b47e) 4889f3 mov rbx, rsi
0x437481 (file 0x43b481) 4989fe mov r14, rdi
0x437484 (file 0x43b484) 4885c0 test rax, rax
0x437487 (file 0x43b487) 742a je 0x4374b3
0x437489 (file 0x43b489) 488b0b mov rcx, qword ptr [rbx]

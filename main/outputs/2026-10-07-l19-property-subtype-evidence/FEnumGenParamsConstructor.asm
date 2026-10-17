0x3602f0 (file 0x3642f0) 55 push rbp
0x3602f1 (file 0x3642f1) 4889e5 mov rbp, rsp
0x3602f4 (file 0x3642f4) 4157 push r15
0x3602f6 (file 0x3642f6) 4156 push r14
0x3602f8 (file 0x3642f8) 53 push rbx
0x3602f9 (file 0x3642f9) 4883ec18 sub rsp, 0x18
0x3602fd (file 0x3642fd) 4c8b3d041cdd08 mov r15, qword ptr [rip + 0x8dd1c04]
0x360304 (file 0x364304) 4889fb mov rbx, rdi
0x360307 (file 0x364307) 4989d6 mov r14, rdx
0x36030a (file 0x36430a) 498b07 mov rax, qword ptr [r15]
0x36030d (file 0x36430d) 488945e0 mov qword ptr [rbp - 0x20], rax
0x360311 (file 0x364311) 488b3e mov rdi, qword ptr [rsi]
0x360314 (file 0x364314) 48897dd8 mov qword ptr [rbp - 0x28], rdi
0x360318 (file 0x364318) 40f6c701 test dil, 1
0x36031c (file 0x36431c) 7414 je 0x360332
0x36031e (file 0x36431e) 4883e7fe and rdi, 0xfffffffffffffffe
0x360322 (file 0x364322) 740e je 0x360332
0x360324 (file 0x364324) 803d31eb030901 cmp byte ptr [rip + 0x903eb31], 1
0x36032b (file 0x36432b) 7505 jne 0x360332
0x36032d (file 0x36432d) e89e7f0000 call 0x3682d0
0x360332 (file 0x364332) 488d75d8 lea rsi, [rbp - 0x28]
0x360336 (file 0x364336) 48b90000000000000800 movabs rcx, 0x8000000000000
0x360340 (file 0x364340) 4889df mov rdi, rbx
0x360343 (file 0x364343) 4c89f2 mov rdx, r14
0x360346 (file 0x364346) e8d51a0800 call 0x3e1e20
0x36034b (file 0x36434b) 488d0516c1b707 lea rax, [rip + 0x7b7c116]
0x360352 (file 0x364352) 488903 mov qword ptr [rbx], rax
0x360355 (file 0x364355) 48c7437800000000 mov qword ptr [rbx + 0x78], 0
0x36035d (file 0x36435d) 498b4638 mov rax, qword ptr [r14 + 0x38]
0x360361 (file 0x364361) 4885c0 test rax, rax
0x360364 (file 0x364364) 741d je 0x360383
0x360366 (file 0x364366) ffd0 call rax
0x360368 (file 0x364368) 4989c6 mov r14, rax
0x36036b (file 0x36436b) 4885c0 test rax, rax
0x36036e (file 0x36436e) 7416 je 0x360386
0x360370 (file 0x364370) 803de5ea030900 cmp byte ptr [rip + 0x903eae5], 0
0x360377 (file 0x364377) 740d je 0x360386
0x360379 (file 0x364379) 4c89f7 mov rdi, r14
0x36037c (file 0x36437c) e84f7f0000 call 0x3682d0
0x360381 (file 0x364381) eb03 jmp 0x360386
0x360383 (file 0x364383) 4531f6 xor r14d, r14d
0x360386 (file 0x364386) 4c897378 mov qword ptr [rbx + 0x78], r14
0x36038a (file 0x36438a) 48c7437000000000 mov qword ptr [rbx + 0x70], 0
0x360392 (file 0x364392) 498b07 mov rax, qword ptr [r15]
0x360395 (file 0x364395) 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x360399 (file 0x364399) 750b jne 0x3603a6
0x36039b (file 0x36439b) 4883c418 add rsp, 0x18
0x36039f (file 0x36439f) 5b pop rbx
0x3603a0 (file 0x3643a0) 415e pop r14
0x3603a2 (file 0x3643a2) 415f pop r15
0x3603a4 (file 0x3643a4) 5d pop rbp
0x3603a5 (file 0x3643a5) c3 ret 
0x3603a6 (file 0x3643a6) e8b51a1107 call 0x7471e60

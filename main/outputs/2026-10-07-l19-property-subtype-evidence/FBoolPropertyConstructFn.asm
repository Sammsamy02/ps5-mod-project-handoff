0x3fe420 (file 0x402420) 55 push rbp
0x3fe421 (file 0x402421) 4889e5 mov rbp, rsp
0x3fe424 (file 0x402424) 4157 push r15
0x3fe426 (file 0x402426) 4156 push r14
0x3fe428 (file 0x402428) 4155 push r13
0x3fe42a (file 0x40242a) 4154 push r12
0x3fe42c (file 0x40242c) 53 push rbx
0x3fe42d (file 0x40242d) 4883ec18 sub rsp, 0x18
0x3fe431 (file 0x402431) 488b05d03ad308 mov rax, qword ptr [rip + 0x8d33ad0]
0x3fe438 (file 0x402438) 8955c4 mov dword ptr [rbp - 0x3c], edx
0x3fe43b (file 0x40243b) 4989fc mov r12, rdi
0x3fe43e (file 0x40243e) bf78000000 mov edi, 0x78
0x3fe443 (file 0x402443) 4989f7 mov r15, rsi
0x3fe446 (file 0x402446) 31f6 xor esi, esi
0x3fe448 (file 0x402448) 488b00 mov rax, qword ptr [rax]
0x3fe44b (file 0x40244b) 488945d0 mov qword ptr [rbp - 0x30], rax
0x3fe44f (file 0x40244f) e8cc95cbff call 0xb7a20
0x3fe454 (file 0x402454) 0fb60dad93fa08 movzx ecx, byte ptr [rip + 0x8fa93ad]
0x3fe45b (file 0x40245b) 4889c3 mov rbx, rax
0x3fe45e (file 0x40245e) 84c9 test cl, cl
0x3fe460 (file 0x402460) 0f84e3000000 je 0x3fe549
0x3fe466 (file 0x402466) 488d155b93fa08 lea rdx, [rip + 0x8fa935b]
0x3fe46d (file 0x40246d) 4889df mov rdi, rbx
0x3fe470 (file 0x402470) 31f6 xor esi, esi
0x3fe472 (file 0x402472) e84961f6ff call 0x3645c0
0x3fe477 (file 0x402477) 4c8d2dc214af07 lea r13, [rip + 0x7af14c2]
0x3fe47e (file 0x40247e) 48c7436800000000 mov qword ptr [rbx + 0x68], 0
0x3fe486 (file 0x402486) 4c892b mov qword ptr [rbx], r13
0x3fe489 (file 0x402489) 4d8b2424 mov r12, qword ptr [r12]
0x3fe48d (file 0x40248d) 41f6c401 test r12b, 1
0x3fe491 (file 0x402491) 7437 je 0x3fe4ca
0x3fe493 (file 0x402493) 4d89e6 mov r14, r12
0x3fe496 (file 0x402496) 4983e6fe and r14, 0xfffffffffffffffe
0x3fe49a (file 0x40249a) 742e je 0x3fe4ca
0x3fe49c (file 0x40249c) 488d05b909fa08 lea rax, [rip + 0x8fa09b9]
0x3fe4a3 (file 0x4024a3) 803801 cmp byte ptr [rax], 1
0x3fe4a6 (file 0x4024a6) 750f jne 0x3fe4b7
0x3fe4a8 (file 0x4024a8) 4c89f7 mov rdi, r14
0x3fe4ab (file 0x4024ab) e8109ef6ff call 0x3682c0
0x3fe4b0 (file 0x4024b0) 488d05a509fa08 lea rax, [rip + 0x8fa09a5]
0x3fe4b7 (file 0x4024b7) 803801 cmp byte ptr [rax], 1
0x3fe4ba (file 0x4024ba) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x3fe4be (file 0x4024be) 750e jne 0x3fe4ce
0x3fe4c0 (file 0x4024c0) 4c89f7 mov rdi, r14
0x3fe4c3 (file 0x4024c3) e8f89df6ff call 0x3682c0
0x3fe4c8 (file 0x4024c8) eb04 jmp 0x3fe4ce
0x3fe4ca (file 0x4024ca) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x3fe4ce (file 0x4024ce) 8b4dc4 mov ecx, dword ptr [rbp - 0x3c]
0x3fe4d1 (file 0x4024d1) 488d75c8 lea rsi, [rbp - 0x38]
0x3fe4d5 (file 0x4024d5) 4889df mov rdi, rbx
0x3fe4d8 (file 0x4024d8) 4c89fa mov rdx, r15
0x3fe4db (file 0x4024db) e89038feff call 0x3e1d70
0x3fe4e0 (file 0x4024e0) 4c892b mov qword ptr [rbx], r13
0x3fe4e3 (file 0x4024e3) c7437000000101 mov dword ptr [rbx + 0x70], 0x1010000
0x3fe4ea (file 0x4024ea) 48b8fffdffbfefffffff movabs rax, 0xffffffefbffffdff
0x3fe4f4 (file 0x4024f4) 48b90000000010000000 movabs rcx, 0x1000000000
0x3fe4fe (file 0x4024fe) 4889df mov rdi, rbx
0x3fe501 (file 0x402501) be01000000 mov esi, 1
0x3fe506 (file 0x402506) 48234338 and rax, qword ptr [rbx + 0x38]
0x3fe50a (file 0x40250a) 4809c1 or rcx, rax
0x3fe50d (file 0x40250d) 48894b38 mov qword ptr [rbx + 0x38], rcx
0x3fe511 (file 0x402511) e84a38feff call 0x3e1d60
0x3fe516 (file 0x402516) 0fb64330 movzx eax, byte ptr [rbx + 0x30]
0x3fe51a (file 0x40251a) 488b0de739d308 mov rcx, qword ptr [rip + 0x8d339e7]
0x3fe521 (file 0x402521) 884370 mov byte ptr [rbx + 0x70], al
0x3fe524 (file 0x402524) 66c743710001 mov word ptr [rbx + 0x71], 0x100
0x3fe52a (file 0x40252a) c6437301 mov byte ptr [rbx + 0x73], 1
0x3fe52e (file 0x40252e) 488b01 mov rax, qword ptr [rcx]
0x3fe531 (file 0x402531) 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x3fe535 (file 0x402535) 757c jne 0x3fe5b3
0x3fe537 (file 0x402537) 4889d8 mov rax, rbx
0x3fe53a (file 0x40253a) 4883c418 add rsp, 0x18
0x3fe53e (file 0x40253e) 5b pop rbx
0x3fe53f (file 0x40253f) 415c pop r12
0x3fe541 (file 0x402541) 415d pop r13
0x3fe543 (file 0x402543) 415e pop r14
0x3fe545 (file 0x402545) 415f pop r15
0x3fe547 (file 0x402547) 5d pop rbp
0x3fe548 (file 0x402548) c3 ret 

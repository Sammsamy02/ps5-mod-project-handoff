0x3605d0 (file 0x3645d0) 55 push rbp
0x3605d1 (file 0x3645d1) 4889e5 mov rbp, rsp
0x3605d4 (file 0x3645d4) 4157 push r15
0x3605d6 (file 0x3645d6) 4156 push r14
0x3605d8 (file 0x3645d8) 4155 push r13
0x3605da (file 0x3645da) 4154 push r12
0x3605dc (file 0x3645dc) 53 push rbx
0x3605dd (file 0x3645dd) 4883ec38 sub rsp, 0x38
0x3605e1 (file 0x3645e1) 4c8b2d2019dd08 mov r13, qword ptr [rip + 0x8dd1920]
0x3605e8 (file 0x3645e8) 4989fe mov r14, rdi
0x3605eb (file 0x3645eb) 488d7dc8 lea rdi, [rbp - 0x38]
0x3605ef (file 0x3645ef) 4989cf mov r15, rcx
0x3605f2 (file 0x3645f2) 4889d3 mov rbx, rdx
0x3605f5 (file 0x3645f5) 498b4500 mov rax, qword ptr [r13]
0x3605f9 (file 0x3645f9) 488945d0 mov qword ptr [rbp - 0x30], rax
0x3605fd (file 0x3645fd) 488975c8 mov qword ptr [rbp - 0x38], rsi
0x360601 (file 0x364601) e85ad1ecff call 0x22d760
0x360606 (file 0x364606) 49837e7800 cmp qword ptr [r14 + 0x78], 0
0x36060b (file 0x36460b) 4989c4 mov r12, rax
0x36060e (file 0x36460e) 745a je 0x36066a
0x360610 (file 0x364610) 498b0424 mov rax, qword ptr [r12]
0x360614 (file 0x364614) 4c89e7 mov rdi, r12
0x360617 (file 0x364617) ff9090000000 call qword ptr [rax + 0x90]
0x36061d (file 0x36461d) 84c0 test al, al
0x36061f (file 0x36461f) 7449 je 0x36066a
0x360621 (file 0x364621) 498b7e70 mov rdi, qword ptr [r14 + 0x70]
0x360625 (file 0x364625) 4889de mov rsi, rbx
0x360628 (file 0x364628) 488b07 mov rax, qword ptr [rdi]
0x36062b (file 0x36462b) ff9018020000 call qword ptr [rax + 0x218]
0x360631 (file 0x364631) 498b7e78 mov rdi, qword ptr [r14 + 0x78]
0x360635 (file 0x364635) 4c89e6 mov rsi, r12
0x360638 (file 0x364638) 4889c2 mov rdx, rax
0x36063b (file 0x36463b) 488b0f mov rcx, qword ptr [rdi]
0x36063e (file 0x36463e) ff91d8020000 call qword ptr [rcx + 0x2d8]
0x360644 (file 0x364644) 498b7e70 mov rdi, qword ptr [r14 + 0x70]
0x360648 (file 0x364648) 4889de mov rsi, rbx
0x36064b (file 0x36464b) 4889c2 mov rdx, rax
0x36064e (file 0x36464e) 488b0f mov rcx, qword ptr [rdi]
0x360651 (file 0x364651) ff91f8010000 call qword ptr [rcx + 0x1f8]
0x360657 (file 0x364657) 498b4500 mov rax, qword ptr [r13]
0x36065b (file 0x36465b) 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x36065f (file 0x36465f) 0f8482000000 je 0x3606e7
0x360665 (file 0x364665) e9a0010000 jmp 0x36080a
0x36066a (file 0x36466a) 498b442428 mov rax, qword ptr [r12 + 0x28]
0x36066f (file 0x36466f) a801 test al, 1
0x360671 (file 0x364671) 752a jne 0x36069d
0x360673 (file 0x364673) a804 test al, 4
0x360675 (file 0x364675) 757f jne 0x3606f6
0x360677 (file 0x364677) 498b7e70 mov rdi, qword ptr [r14 + 0x70]
0x36067b (file 0x36467b) 488b75c8 mov rsi, qword ptr [rbp - 0x38]
0x36067f (file 0x36467f) 4889da mov rdx, rbx
0x360682 (file 0x364682) 4c89f9 mov rcx, r15
0x360685 (file 0x364685) 488b07 mov rax, qword ptr [rdi]
0x360688 (file 0x364688) ff90c8000000 call qword ptr [rax + 0xc8]
0x36068e (file 0x36468e) 498b4500 mov rax, qword ptr [r13]
0x360692 (file 0x364692) 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x360696 (file 0x364696) 744f je 0x3606e7
0x360698 (file 0x364698) e96d010000 jmp 0x36080a
0x36069d (file 0x36469d) 48c745a800000000 mov qword ptr [rbp - 0x58], 0
0x3606a5 (file 0x3646a5) 48c745c000000000 mov qword ptr [rbp - 0x40], 0
0x3606ad (file 0x3646ad) 488b7dc8 mov rdi, qword ptr [rbp - 0x38]
0x3606b1 (file 0x3646b1) 488d4da8 lea rcx, [rbp - 0x58]
0x3606b5 (file 0x3646b5) 4c8d45c0 lea r8, [rbp - 0x40]
0x3606b9 (file 0x3646b9) 4c89e6 mov rsi, r12
0x3606bc (file 0x3646bc) 498b5678 mov rdx, qword ptr [r14 + 0x78]
0x3606c0 (file 0x3646c0) e87bf4ffff call 0x35fb40
0x3606c5 (file 0x3646c5) 498b7e70 mov rdi, qword ptr [r14 + 0x70]
0x3606c9 (file 0x3646c9) 488b55c0 mov rdx, qword ptr [rbp - 0x40]
0x3606cd (file 0x3646cd) 4889de mov rsi, rbx
0x3606d0 (file 0x3646d0) 488b07 mov rax, qword ptr [rdi]
0x3606d3 (file 0x3646d3) ff90f8010000 call qword ptr [rax + 0x1f8]
0x3606d9 (file 0x3646d9) 498b4500 mov rax, qword ptr [r13]

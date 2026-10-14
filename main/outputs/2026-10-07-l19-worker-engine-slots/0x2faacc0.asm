2faacc0 f041ff4f08 lock dec dword ptr [r15 + 8]
2faacc5 7518 jne 0x2faacdf
2faacc7 498b07 mov rax, qword ptr [r15]
2faacca 4c89ff mov rdi, r15
2faaccd ff10 call qword ptr [rax]
2faaccf f041ff4f0c lock dec dword ptr [r15 + 0xc]
2faacd4 7509 jne 0x2faacdf
2faacd6 498b07 mov rax, qword ptr [r15]
2faacd9 4c89ff mov rdi, r15
2faacdc ff5010 call qword ptr [rax + 0x10]
2faacdf 498b3c24 mov rdi, qword ptr [r12]
2faace3 be02000000 mov esi, 2
2faace8 e8732772fd call 0x6cd460
2faaced f041ff4d00 lock dec dword ptr [r13]
2faacf2 7517 jne 0x2faad0b
2faacf4 488b03 mov rax, qword ptr [rbx]
2faacf7 4889df mov rdi, rbx
2faacfa ff10 call qword ptr [rax]
2faacfc f0ff4b0c lock dec dword ptr [rbx + 0xc]
2faad00 7509 jne 0x2faad0b
2faad02 488b03 mov rax, qword ptr [rbx]
2faad05 4889df mov rdi, rbx
2faad08 ff5010 call qword ptr [rax + 0x10]
2faad0b 498b06 mov rax, qword ptr [r14]
2faad0e 483b45d0 cmp rax, qword ptr [rbp - 0x30]
2faad12 750f jne 0x2faad23
2faad14 4883c458 add rsp, 0x58
2faad18 5b pop rbx
2faad19 415c pop r12
2faad1b 415d pop r13
2faad1d 415e pop r14
2faad1f 415f pop r15
2faad21 5d pop rbp
2faad22 c3 ret 
2faad23 e838714c04 call 0x7471e60
2faad28 0f0b ud2 
2faad2a cc int3 
2faad2b cc int3 
2faad2c cc int3 
2faad2d cc int3 
2faad2e cc int3 
2faad2f cc int3 
2faad30 55 push rbp
2faad31 4889e5 mov rbp, rsp
2faad34 4156 push r14
2faad36 53 push rbx
2faad37 4989fe mov r14, rdi
2faad3a 488bbf100c0000 mov rdi, qword ptr [rdi + 0xc10]
2faad41 4885ff test rdi, rdi
2faad44 7426 je 0x2faad6c
2faad46 488b07 mov rax, qword ptr [rdi]
2faad49 89f3 mov ebx, esi
2faad4b ff9048040000 call qword ptr [rax + 0x448]
2faad51 498b86100c0000 mov rax, qword ptr [r14 + 0xc10]
2faad58 488bb8f0000000 mov rdi, qword ptr [rax + 0xf0]
2faad5f 4885ff test rdi, rdi
2faad62 7408 je 0x2faad6c
2faad64 0fb6f3 movzx esi, bl
2faad67 e8d4687700 call 0x3721640
2faad6c 5b pop rbx
2faad6d 415e pop r14
2faad6f 5d pop rbp
2faad70 c3 ret 
2faad71 cc int3 
2faad72 cc int3 
2faad73 cc int3 
2faad74 cc int3 
2faad75 cc int3 
2faad76 cc int3 
2faad77 cc int3 
2faad78 cc int3 
2faad79 cc int3 
2faad7a cc int3 
2faad7b cc int3 
2faad7c cc int3 
2faad7d cc int3 
2faad7e cc int3 
2faad7f cc int3 
2faad80 55 push rbp
2faad81 4889e5 mov rbp, rsp
2faad84 53 push rbx
2faad85 50 push rax
2faad86 488b1f mov rbx, qword ptr [rdi]
2faad89 4889fe mov rsi, rdi
2faad8c 4889df mov rdi, rbx
2faad8f e88cc1ffff call 0x2fa6f20
2faad94 488d056dc94c05 lea rax, [rip + 0x54cc96d]
2faad9b 488d0d0ecf4c05 lea rcx, [rip + 0x54ccf0e]
2faada2 c5f857c0 vxorps xmm0, xmm0, xmm0
2faada6 488903 mov qword ptr [rbx], rax
2faada9 48894b28 mov qword ptr [rbx + 0x28], rcx
2faadad 48c7839012000000000000 mov qword ptr [rbx + 0x1290], 0
2faadb8 c5fc118348120000 vmovups ymmword ptr [rbx + 0x1248], ymm0
2faadc0 c5fc118364120000 vmovups ymmword ptr [rbx + 0x1264], ymm0
2faadc8 4883c408 add rsp, 8
2faadcc 5b pop rbx
2faadcd 5d pop rbp
2faadce c3 ret 
2faadcf cc int3 
2faadd0 55 push rbp
2faadd1 4889e5 mov rbp, rsp
2faadd4 4157 push r15
2faadd6 4156 push r14
2faadd8 4154 push r12
2faadda 53 push rbx
2faaddb 4883ec10 sub rsp, 0x10
2faaddf 488b1d22711806 mov rbx, qword ptr [rip + 0x6187122]
2faade6 488b03 mov rax, qword ptr [rbx]
2faade9 488945d8 mov qword ptr [rbp - 0x28], rax
2faaded 488b0504655706 mov rax, qword ptr [rip + 0x6576504]
2faadf4 4885c0 test rax, rax
2faadf7 757c jne 0x2faae75
2faadf9 488d05508b8900 lea rax, [rip + 0x898b50]
2faae00 4c8d152923acff lea r10, [rip - 0x53dcd7]
2faae07 4c8d5dd0 lea r11, [rbp - 0x30]
2faae0b 4c8d35ce5205fd lea r14, [rip - 0x2faad32]
2faae12 4c8d3d67ffffff lea r15, [rip - 0x99]
2faae19 4c8d254ec28704 lea r12, [rip + 0x487c24e]
2faae20 488d3d5b6da104 lea rdi, [rip + 0x4a16d5b]
2faae27 488d3586ab9004 lea rsi, [rip + 0x490ab86]
2faae2e 488d15c3645706 lea rdx, [rip + 0x65764c3]
2faae35 488d0d540406fd lea rcx, [rip - 0x2f9fbac]
2faae3c 41b898120000 mov r8d, 0x1298
2faae42 41b908000000 mov r9d, 8
2faae48 488945d0 mov qword ptr [rbp - 0x30], rax
2faae4c 488d059d162ffd lea rax, [rip - 0x2d0e963]
2faae53 50 push rax
2faae54 4152 push r10
2faae56 4153 push r11
2faae58 4156 push r14
2faae5a 4157 push r15
2faae5c 4154 push r12
2faae5e 6a00 push 0
2faae60 680e000010 push 0x1000000e
2faae65 e8f62936fd call 0x30d860
2faae6a 4883c440 add rsp, 0x40
2faae6e 488b0583645706 mov rax, qword ptr [rip + 0x6576483]
2faae75 488b0b mov rcx, qword ptr [rbx]
2faae78 483b4dd8 cmp rcx, qword ptr [rbp - 0x28]
2faae7c 750d jne 0x2faae8b
2faae7e 4883c410 add rsp, 0x10
2faae82 5b pop rbx
2faae83 415c pop r12
2faae85 415e pop r14
2faae87 415f pop r15
2faae89 5d pop rbp
2faae8a c3 ret 
2faae8b e8d06f4c04 call 0x7471e60
2faae90 0f0b ud2 
2faae92 cc int3 
2faae93 cc int3 
2faae94 cc int3 
2faae95 cc int3 
2faae96 cc int3 
2faae97 cc int3 
2faae98 cc int3 
2faae99 cc int3 
2faae9a cc int3 
2faae9b cc int3 
2faae9c cc int3 
2faae9d cc int3 
2faae9e cc int3 
2faae9f cc int3 
2faaea0 55 push rbp
2faaea1 4889e5 mov rbp, rsp
2faaea4 4157 push r15
2faaea6 4156 push r14
2faaea8 53 push rbx
2faaea9 4883ec18 sub rsp, 0x18
2faaead 488b1d54701806 mov rbx, qword ptr [rip + 0x6187054]
2faaeb4 488b03 mov rax, qword ptr [rbx]
2faaeb7 488945e0 mov qword ptr [rbp - 0x20], rax
2faaebb 488b053e6b5706 mov rax, qword ptr [rip + 0x6576b3e]
2faaec2 4885c0 test rax, rax
2faaec5 7574 jne 0x2faaf3b
2faaec7 488d05f29f0100 lea rax, [rip + 0x19ff2]
2faaece 4c8d55d8 lea r10, [rbp - 0x28]
2faaed2 4c8d1d075205fd lea r11, [rip - 0x2faadf9]
2faaed9 4c8d35d09e0100 lea r14, [rip + 0x19ed0]
2faaee0 4c8d3d4f486804 lea r15, [rip + 0x468484f]
2faaee7 488d3d946ca104 lea rdi, [rip + 0x4a16c94]
2faaeee 488d35f5819d04 lea rsi, [rip + 0x49d81f5]
2faaef5 488d15046b5706 lea rdx, [rip + 0x6576b04]
2faaefc 488d0ddd9d0100 lea rcx, [rip + 0x19ddd]
2faaf03 41b8c0010000 mov r8d, 0x1c0
2faaf09 41b908000000 mov r9d, 8
2faaf0f 488945d8 mov qword ptr [rbp - 0x28], rax
2faaf13 488d05d6152ffd lea rax, [rip - 0x2d0ea2a]
2faaf1a 50 push rax
2faaf1b 50 push rax
2faaf1c 4152 push r10
2faaf1e 4153 push r11
2faaf20 4156 push r14
2faaf22 4157 push r15
2faaf24 6a00 push 0
2faaf26 6808000010 push 0x10000008
2faaf2b e8302936fd call 0x30d860
2faaf30 4883c440 add rsp, 0x40
2faaf34 488b05c56a5706 mov rax, qword ptr [rip + 0x6576ac5]
2faaf3b 488b0b mov rcx, qword ptr [rbx]
2faaf3e 483b4de0 cmp rcx, qword ptr [rbp - 0x20]
2faaf42 750b jne 0x2faaf4f
2faaf44 4883c418 add rsp, 0x18
2faaf48 5b pop rbx
2faaf49 415e pop r14
2faaf4b 415f pop r15
2faaf4d 5d pop rbp
2faaf4e c3 ret 
2faaf4f e80c6f4c04 call 0x7471e60
2faaf54 0f0b ud2 
2faaf56 cc int3 
2faaf57 cc int3 
2faaf58 cc int3 
2faaf59 cc int3 
2faaf5a cc int3 
2faaf5b cc int3 
2faaf5c cc int3 
2faaf5d cc int3 
2faaf5e cc int3 
2faaf5f cc int3 
2faaf60 488b0599635706 mov rax, qword ptr [rip + 0x6576399]
2faaf67 4885c0 test rax, rax
2faaf6a 7401 je 0x2faaf6d
2faaf6c c3 ret 
2faaf6d 55 push rbp
2faaf6e 4889e5 mov rbp, rsp
2faaf71 488d3d88635706 lea rdi, [rip + 0x6576388]
2faaf78 488d3541ce4c05 lea rsi, [rip + 0x54cce41]
2faaf7f e88ce64efd call 0x499610
2faaf84 488b0575635706 mov rax, qword ptr [rip + 0x6576375]
2faaf8b 5d pop rbp
2faaf8c c3 ret 
2faaf8d cc int3 
2faaf8e cc int3 
2faaf8f cc int3 
2faaf90 55 push rbp
2faaf91 4889e5 mov rbp, rsp
2faaf94 4156 push r14
2faaf96 53 push rbx
2faaf97 488d056ac74c05 lea rax, [rip + 0x54cc76a]
2faaf9e 488d0d0bcd4c05 lea rcx, [rip + 0x54ccd0b]
2faafa5 4889fb mov rbx, rdi
2faafa8 488907 mov qword ptr [rdi], rax
2faafab 48894f28 mov qword ptr [rdi + 0x28], rcx
2faafaf 488b8790120000 mov rax, qword ptr [rdi + 0x1290]
2faafb6 4885c0 test rax, rax
2faafb9 7407 je 0x2faafc2
2faafbb 488d78f0 lea rdi, [rax - 0x10]
2faafbf ff50f0 call qword ptr [rax - 0x10]
2faafc2 4c8bb378120000 mov r14, qword ptr [rbx + 0x1278]
2faafc9 4d85f6 test r14, r14
2faafcc 741f je 0x2faafed

0x363600 5d pop rbp
0x363601 c84c8d25 enter -0x72b4, 0x25
0x363605 47bcffff4c8d mov r12d, 0x8d4cffff
0x36360b 2df0bbffff sub eax, 0xffffbbf0
0x363610 488d3521a72707 lea rsi, [rip + 0x727a721]
0x363617 488d15aab50309 lea rdx, [rip + 0x903b5aa]
0x36361e 488d0d6b7ccaff lea rcx, [rip - 0x358395]
0x363625 41b868000000 mov r8d, 0x68
0x36362b 41b908000000 mov r9d, 8
0x363631 488945c8 mov qword ptr [rbp - 0x38], rax
0x363635 488d05b48ef3ff lea rax, [rip - 0xc714c]
0x36363c 50 push rax
0x36363d 4152 push r10
0x36363f 4153 push r11
0x363641 4154 push r12
0x363643 4155 push r13
0x363645 488d05223a4c07 lea rax, [rip + 0x74c3a22]
0x36364c 50 push rax
0x36364d 6a04 push 4
0x36364f 6800000010 push 0x10000000
0x363654 e807a2faff call 0x30d860
0x363659 4883c440 add rsp, 0x40
0x36365d 488b3d64b50309 mov rdi, qword ptr [rip + 0x903b564]
0x363664 4489fe mov esi, r15d
0x363667 e894160a00 call 0x404d00
0x36366c 4885c0 test rax, rax
0x36366f 0f84b3000000 je 0x363728
0x363675 488d7dc4 lea rdi, [rbp - 0x3c]
0x363679 be01000000 mov esi, 1
0x36367e 4989c7 mov r15, rax
0x363681 e88a440d00 call 0x437b10
0x363686 488d7dc0 lea rdi, [rbp - 0x40]
0x36368a 8945c0 mov dword ptr [rbp - 0x40], eax
0x36368d e8fe4e0d00 call 0x438590
0x363692 488d35d3f78507 lea rsi, [rip + 0x785f7d3]
0x363699 4889df mov rdi, rbx
0x36369c 488945c8 mov qword ptr [rbp - 0x38], rax
0x3636a0 48895db8 mov qword ptr [rbp - 0x48], rbx
0x3636a4 e867a3e4ff call 0x1ada10
0x3636a9 84c0 test al, al
0x3636ab 7406 je 0x3636b3
0x3636ad 488b4320 mov rax, qword ptr [rbx + 0x20]
0x3636b1 eb02 jmp 0x3636b5
0x3636b3 31c0 xor eax, eax
0x3636b5 488d7dc8 lea rdi, [rbp - 0x38]
0x3636b9 488d75b8 lea rsi, [rbp - 0x48]
0x3636bd 488d55b0 lea rdx, [rbp - 0x50]
0x3636c1 31c9 xor ecx, ecx
0x3636c3 488945b0 mov qword ptr [rbp - 0x50], rax
0x3636c7 4531ed xor r13d, r13d
0x3636ca e891000000 call 0x363760
0x3636cf 4885c0 test rax, rax
0x3636d2 7457 je 0x36372b
0x3636d4 4989c4 mov r12, rax
0x3636d7 488b4008 mov rax, qword ptr [rax + 8]
0x3636db f6401301 test byte ptr [rax + 0x13], 1
0x3636df 743d je 0x36371e
0x3636e1 8b75c0 mov esi, dword ptr [rbp - 0x40]
0x3636e4 498b0424 mov rax, qword ptr [r12]
0x3636e8 4c89e7 mov rdi, r12
0x3636eb 4c89f2 mov rdx, r14
0x3636ee ff9090010000 call qword ptr [rax + 0x190]
0x3636f4 84c0 test al, al
0x3636f6 7426 je 0x36371e
0x3636f8 803d5db7030901 cmp byte ptr [rip + 0x903b75d], 1
0x3636ff 7508 jne 0x363709
0x363701 4c89ff mov rdi, r15
0x363704 e8c74b0000 call 0x3682d0
0x363709 4c897b78 mov qword ptr [rbx + 0x78], r15
0x36370d 4889df mov rdi, rbx
0x363710 4c89e6 mov rsi, r12
0x363713 488b03 mov rax, qword ptr [rbx]
0x363716 ff5040 call qword ptr [rax + 0x40]
0x363719 41b501 mov r13b, 1
0x36371c eb0d jmp 0x36372b
0x36371e 498b0424 mov rax, qword ptr [r12]
0x363722 4c89e7 mov rdi, r12
0x363725 ff5010 call qword ptr [rax + 0x10]
0x363728 4531ed xor r13d, r13d
0x36372b 4c8b3dd6e7dc08 mov r15, qword ptr [rip + 0x8dce7d6]
0x363732 498b07 mov rax, qword ptr [r15]
0x363735 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x363739 7512 jne 0x36374d
0x36373b 4489e8 mov eax, r13d
0x36373e 4883c428 add rsp, 0x28
0x363742 5b pop rbx
0x363743 415c pop r12
0x363745 415d pop r13
0x363747 415e pop r14
0x363749 415f pop r15
0x36374b 5d pop rbp
0x36374c c3 ret 
0x36374d e80ee71007 call 0x7471e60
0x363752 0f0b ud2 
0x363754 cc int3 
0x363755 cc int3 
0x363756 cc int3 
0x363757 cc int3 
0x363758 cc int3 
0x363759 cc int3 
0x36375a cc int3 
0x36375b cc int3 
0x36375c cc int3 
0x36375d cc int3 
0x36375e cc int3 
0x36375f cc int3 
0x363760 55 push rbp
0x363761 4889e5 mov rbp, rsp
0x363764 4157 push r15
0x363766 4156 push r14
0x363768 4154 push r12
0x36376a 53 push rbx
0x36376b 0fb6051eb50309 movzx eax, byte ptr [rip + 0x903b51e]
0x363772 89cb mov ebx, ecx
0x363774 4989d6 mov r14, rdx
0x363777 4989f7 mov r15, rsi
0x36377a 84c0 test al, al
0x36377c 0f849f000000 je 0x363821
0x363782 4c8b27 mov r12, qword ptr [rdi]
0x363785 4489e7 mov edi, r12d
0x363788 e84363edff call 0x239ad0
0x36378d 8b0db5b40309 mov ecx, dword ptr [rip + 0x903b4b5]
0x363793 3b0ddbb40309 cmp ecx, dword ptr [rip + 0x903b4db]
0x363799 747b je 0x363816
0x36379b 4c89e1 mov rcx, r12
0x36379e 488d15d3b40309 lea rdx, [rip + 0x903b4d3]
0x3637a5 48c1e920 shr rcx, 0x20
0x3637a9 01c8 add eax, ecx
0x3637ab 488b0dceb40309 mov rcx, qword ptr [rip + 0x903b4ce]
0x3637b2 4885c9 test rcx, rcx
0x3637b5 480f45d1 cmovne rdx, rcx
0x3637b9 8b0dc9b40309 mov ecx, dword ptr [rip + 0x903b4c9]
0x3637bf ffc9 dec ecx
0x3637c1 21c1 and ecx, eax
0x3637c3 8b148a mov edx, dword ptr [rdx + rcx*4]
0x3637c6 83faff cmp edx, -1
0x3637c9 744b je 0x363816
0x3637cb 488b0d6eb40309 mov rcx, qword ptr [rip + 0x903b46e]
0x3637d2 31c0 xor eax, eax
0x3637d4 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3637e0 4863d2 movsxd rdx, edx
0x3637e3 488d3452 lea rsi, [rdx + rdx*2]
0x3637e7 488d14f1 lea rdx, [rcx + rsi*8]
0x3637eb 4c3924f1 cmp qword ptr [rcx + rsi*8], r12
0x3637ef 740a je 0x3637fb
0x3637f1 8b5210 mov edx, dword ptr [rdx + 0x10]
0x3637f4 83faff cmp edx, -1
0x3637f7 75e7 jne 0x3637e0
0x3637f9 eb1d jmp 0x363818
0x3637fb 4885d2 test rdx, rdx
0x3637fe 7416 je 0x363816
0x363800 488b4208 mov rax, qword ptr [rdx + 8]
0x363804 4885c0 test rax, rax
0x363807 740d je 0x363816
0x363809 4c89ff mov rdi, r15
0x36380c 4c89f6 mov rsi, r14
0x36380f 89da mov edx, ebx
0x363811 ff5030 call qword ptr [rax + 0x30]
0x363814 eb02 jmp 0x363818
0x363816 31c0 xor eax, eax
0x363818 5b pop rbx
0x363819 415c pop r12
0x36381b 415e pop r14
0x36381d 415f pop r15
0x36381f 5d pop rbp
0x363820 c3 ret 
0x363821 488d0568b40309 lea rax, [rip + 0x903b468]
0x363828 4989fc mov r12, rdi
0x36382b 4889c7 mov rdi, rax
0x36382e e87de61007 call 0x7471eb0
0x363833 4c89e7 mov rdi, r12
0x363836 85c0 test eax, eax
0x363838 0f8444ffffff je 0x363782
0x36383e c5f8100dda916d07 vmovups xmm1, xmmword ptr [rip + 0x76d91da]
0x363846 488d35f3b30309 lea rsi, [rip + 0x903b3f3]
0x36384d 488d3dac45cbff lea rdi, [rip - 0x34ba54]
0x363854 488d15a587f708 lea rdx, [rip + 0x8f787a5]
0x36385b 48c705fab3030900000000 mov qword ptr [rip + 0x903b3fa], 0
0x363866 c5f857c0 vxorps xmm0, xmm0, xmm0
0x36386a c5f81105ceb30309 vmovups xmmword ptr [rip + 0x903b3ce], xmm0
0x363872 c5f8110deeb30309 vmovups xmmword ptr [rip + 0x903b3ee], xmm1
0x36387a 48c705fbb3030900000000 mov qword ptr [rip + 0x903b3fb], 0
0x363885 c705f9b3030900000000 mov dword ptr [rip + 0x903b3f9], 0
0x36388f e83ce61007 call 0x7471ed0
0x363894 488d3df5b30309 lea rdi, [rip + 0x903b3f5]
0x36389b e820e61007 call 0x7471ec0
0x3638a0 4c89e7 mov rdi, r12
0x3638a3 e9dafeffff jmp 0x363782
0x3638a8 cc int3 
0x3638a9 cc int3 
0x3638aa cc int3 
0x3638ab cc int3 
0x3638ac cc int3 
0x3638ad cc int3 
0x3638ae cc int3 
0x3638af cc int3 
0x3638b0 55 push rbp
0x3638b1 4889e5 mov rbp, rsp
0x3638b4 4157 push r15
0x3638b6 4156 push r14
0x3638b8 53 push rbx
0x3638b9 50 push rax
0x3638ba 4889f3 mov rbx, rsi
0x3638bd 4989fe mov r14, rdi
0x3638c0 e8ab1e0800 call 0x3e5770
0x3638c5 4d8b7e78 mov r15, qword ptr [r14 + 0x78]
0x3638c9 4d85ff test r15, r15
0x3638cc 742b je 0x3638f9
0x3638ce 4889df mov rdi, rbx
0x3638d1 e8ea430d00 call 0x437cc0
0x3638d6 4889df mov rdi, rbx
0x3638d9 4c89fe mov rsi, r15
0x3638dc e8ff430d00 call 0x437ce0
0x3638e1 498b7e70 mov rdi, qword ptr [r14 + 0x70]
0x3638e5 4889de mov rsi, rbx
0x3638e8 488b07 mov rax, qword ptr [rdi]
0x3638eb ff9098010000 call qword ptr [rax + 0x198]
0x3638f1 4889df mov rdi, rbx
0x3638f4 e837450d00 call 0x437e30
0x3638f9 4883c408 add rsp, 8
0x3638fd 5b pop rbx
0x3638fe 415e pop r14
0x363900 415f pop r15
0x363902 5d pop rbp
0x363903 c3 ret 
0x363904 cc int3 
0x363905 cc int3 
0x363906 cc int3 
0x363907 cc int3 
0x363908 cc int3 
0x363909 cc int3 
0x36390a cc int3 
0x36390b cc int3 
0x36390c cc int3 
0x36390d cc int3 
0x36390e cc int3 
0x36390f cc int3 
0x363910 55 push rbp
0x363911 4889e5 mov rbp, rsp
0x363914 4156 push r14
0x363916 53 push rbx
0x363917 4883ec10 sub rsp, 0x10
0x36391b 4c8b35e6e5dc08 mov r14, qword ptr [rip + 0x8dce5e6]
0x363922 4889fb mov rbx, rdi
0x363925 498b06 mov rax, qword ptr [r14]
0x363928 488945e8 mov qword ptr [rbp - 0x18], rax
0x36392c 8975e0 mov dword ptr [rbp - 0x20], esi
0x36392f e86c1e0800 call 0x3e57a0
0x363934 84c0 test al, al
0x363936 7437 je 0x36396f
0x363938 488b5b78 mov rbx, qword ptr [rbx + 0x78]
0x36393c 4885db test rbx, rbx
0x36393f 742e je 0x36396f
0x363941 488d7de0 lea rdi, [rbp - 0x20]
0x363945 31f6 xor esi, esi
0x363947 e8c4410d00 call 0x437b10
0x36394c 488d7de4 lea rdi, [rbp - 0x1c]
0x363950 8945e4 mov dword ptr [rbp - 0x1c], eax
0x363953 e8384c0d00 call 0x438590
0x363958 483b4318 cmp rax, qword ptr [rbx + 0x18]
0x36395c 0f94c0 sete al
0x36395f 498b0e mov rcx, qword ptr [r14]
0x363962 483b4de8 cmp rcx, qword ptr [rbp - 0x18]
0x363966 7412 je 0x36397a
0x363968 e8f3e41007 call 0x7471e60
0x36396d 0f0b ud2 
0x36396f 31c0 xor eax, eax
0x363971 498b0e mov rcx, qword ptr [r14]
0x363974 483b4de8 cmp rcx, qword ptr [rbp - 0x18]
0x363978 75ee jne 0x363968
0x36397a 4883c410 add rsp, 0x10
0x36397e 5b pop rbx
0x36397f 415e pop r14
0x363981 5d pop rbp
0x363982 c3 ret 
0x363983 cc int3 
0x363984 cc int3 
0x363985 cc int3 
0x363986 cc int3 
0x363987 cc int3 
0x363988 cc int3 
0x363989 cc int3 
0x36398a cc int3 
0x36398b cc int3 
0x36398c cc int3 
0x36398d cc int3 
0x36398e cc int3 
0x36398f cc int3 
0x363990 55 push rbp
0x363991 4889e5 mov rbp, rsp
0x363994 4156 push r14
0x363996 53 push rbx
0x363997 4883ec20 sub rsp, 0x20
0x36399b 4c8b3566e5dc08 mov r14, qword ptr [rip + 0x8dce566]
0x3639a2 4889fb mov rbx, rdi
0x3639a5 4883c602 add rsi, 2
0x3639a9 498b06 mov rax, qword ptr [r14]
0x3639ac 488945e8 mov qword ptr [rbp - 0x18], rax
0x3639b0 48c70700000000 mov qword ptr [rdi], 0
0x3639b7 48895708 mov qword ptr [rdi + 8], rdx
0x3639bb 48894f10 mov qword ptr [rdi + 0x10], rcx
0x3639bf c7471800000000 mov dword ptr [rdi + 0x18], 0
0x3639c6 4c894720 mov qword ptr [rdi + 0x20], r8
0x3639ca 48c7472800000000 mov qword ptr [rdi + 0x28], 0
0x3639d2 4c894f30 mov qword ptr [rdi + 0x30], r9
0x3639d6 c7473800000000 mov dword ptr [rdi + 0x38], 0
0x3639dd 488d7dd8 lea rdi, [rbp - 0x28]
0x3639e1 ba01000000 mov edx, 1
0x3639e6 e8a59bedff call 0x23d590
0x3639eb 488b45d8 mov rax, qword ptr [rbp - 0x28]
0x3639ef 488903 mov qword ptr [rbx], rax
0x3639f2 0fb6053fb20309 movzx eax, byte ptr [rip + 0x903b23f]
0x3639f9 84c0 test al, al
0x3639fb 0f8483000000 je 0x363a84
0x363a01 8b0529b20309 mov eax, dword ptr [rip + 0x903b229]
0x363a07 3b0527b20309 cmp eax, dword ptr [rip + 0x903b227]
0x363a0d 7518 jne 0x363a27
0x363a0f 488d3512b20309 lea rsi, [rip + 0x903b212]
0x363a16 488d1517b20309 lea rdx, [rip + 0x903b217]
0x363a1d bf08080000 mov edi, 0x808
0x363a22 e8795eccff call 0x298a0
0x363a27 8d4801 lea ecx, [rax + 1]
0x363a2a 4898 cdqe 
0x363a2c 890dfeb10309 mov dword ptr [rip + 0x903b1fe], ecx
0x363a32 488b0defb10309 mov rcx, qword ptr [rip + 0x903b1ef]
0x363a39 48891cc1 mov qword ptr [rcx + rax*8], rbx
0x363a3d 0fb6054cb20309 movzx eax, byte ptr [rip + 0x903b24c]
0x363a44 84c0 test al, al
0x363a46 0f8483000000 je 0x363acf
0x363a4c 488d45d0 lea rax, [rbp - 0x30]
0x363a50 488d3de9b10309 lea rdi, [rip + 0x903b1e9]
0x363a57 488d75d8 lea rsi, [rbp - 0x28]
0x363a5b 48895dd8 mov qword ptr [rbp - 0x28], rbx
0x363a5f 31d2 xor edx, edx
0x363a61 48895dd0 mov qword ptr [rbp - 0x30], rbx
0x363a65 488945e0 mov qword ptr [rbp - 0x20], rax
0x363a69 e8a20fcdff call 0x34a10
0x363a6e 498b06 mov rax, qword ptr [r14]
0x363a71 483b45e8 cmp rax, qword ptr [rbp - 0x18]
0x363a75 0f85cf000000 jne 0x363b4a
0x363a7b 4883c420 add rsp, 0x20
0x363a7f 5b pop rbx
0x363a80 415e pop r14
0x363a82 5d pop rbp
0x363a83 c3 ret 
0x363a84 488d3dadb10309 lea rdi, [rip + 0x903b1ad]
0x363a8b e820e41007 call 0x7471eb0
0x363a90 85c0 test eax, eax
0x363a92 0f8469ffffff je 0x363a01
0x363a98 488d3589b10309 lea rsi, [rip + 0x903b189]
0x363a9f 488d3dda08caff lea rdi, [rip - 0x35f726]
0x363aa6 488d155385f708 lea rdx, [rip + 0x8f78553]
0x363aad c5f857c0 vxorps xmm0, xmm0, xmm0
0x363ab1 c5f811056fb10309 vmovups xmmword ptr [rip + 0x903b16f], xmm0
0x363ab9 e812e41007 call 0x7471ed0
0x363abe 488d3d73b10309 lea rdi, [rip + 0x903b173]
0x363ac5 e8f6e31007 call 0x7471ec0
0x363aca e932ffffff jmp 0x363a01
0x363acf 488d3dbab10309 lea rdi, [rip + 0x903b1ba]
0x363ad6 e8d5e31007 call 0x7471eb0
0x363adb 85c0 test eax, eax
0x363add 0f8469ffffff je 0x363a4c
0x363ae3 c5f8100d358f6d07 vmovups xmm1, xmmword ptr [rip + 0x76d8f35]
0x363aeb 488d354eb10309 lea rsi, [rip + 0x903b14e]
0x363af2 488d3d0743cbff lea rdi, [rip - 0x34bcf9]
0x363af9 488d150085f708 lea rdx, [rip + 0x8f78500]
0x363b00 48c70555b1030900000000 mov qword ptr [rip + 0x903b155], 0
0x363b0b c5f857c0 vxorps xmm0, xmm0, xmm0
0x363b0f c5f8110529b10309 vmovups xmmword ptr [rip + 0x903b129], xmm0
0x363b17 c5f8110d49b10309 vmovups xmmword ptr [rip + 0x903b149], xmm1
0x363b1f 48c70556b1030900000000 mov qword ptr [rip + 0x903b156], 0
0x363b2a c70554b1030900000000 mov dword ptr [rip + 0x903b154], 0
0x363b34 e897e31007 call 0x7471ed0
0x363b39 488d3d50b10309 lea rdi, [rip + 0x903b150]
0x363b40 e87be31007 call 0x7471ec0
0x363b45 e902ffffff jmp 0x363a4c
0x363b4a e811e31007 call 0x7471e60
0x363b4f 0f0b ud2 
0x363b51 cc int3 
0x363b52 cc int3 
0x363b53 cc int3 
0x363b54 cc int3 
0x363b55 cc int3 
0x363b56 cc int3 
0x363b57 cc int3 
0x363b58 cc int3 
0x363b59 cc int3 
0x363b5a cc int3 
0x363b5b cc int3 
0x363b5c cc int3 
0x363b5d cc int3 
0x363b5e cc int3 
0x363b5f cc int3 
0x363b60 0fb60529b10309 movzx eax, byte ptr [rip + 0x903b129]
0x363b67 84c0 test al, al
0x363b69 7408 je 0x363b73
0x363b6b 488d05ceb00309 lea rax, [rip + 0x903b0ce]
0x363b72 c3 ret 
0x363b73 55 push rbp
0x363b74 4889e5 mov rbp, rsp
0x363b77 488d3d12b10309 lea rdi, [rip + 0x903b112]
0x363b7e e82de31007 call 0x7471eb0
0x363b83 85c0 test eax, eax
0x363b85 7462 je 0x363be9
0x363b87 c5f8100d918e6d07 vmovups xmm1, xmmword ptr [rip + 0x76d8e91]
0x363b8f 488d35aab00309 lea rsi, [rip + 0x903b0aa]
0x363b96 488d3d6342cbff lea rdi, [rip - 0x34bd9d]
0x363b9d 488d155c84f708 lea rdx, [rip + 0x8f7845c]
0x363ba4 48c705b1b0030900000000 mov qword ptr [rip + 0x903b0b1], 0
0x363baf c5f857c0 vxorps xmm0, xmm0, xmm0
0x363bb3 c5f8110585b00309 vmovups xmmword ptr [rip + 0x903b085], xmm0
0x363bbb c5f8110da5b00309 vmovups xmmword ptr [rip + 0x903b0a5], xmm1
0x363bc3 48c705b2b0030900000000 mov qword ptr [rip + 0x903b0b2], 0
0x363bce c705b0b0030900000000 mov dword ptr [rip + 0x903b0b0], 0
0x363bd8 e8f3e21007 call 0x7471ed0
0x363bdd 488d3dacb00309 lea rdi, [rip + 0x903b0ac]
0x363be4 e8d7e21007 call 0x7471ec0
0x363be9 5d pop rbp
0x363bea 488d054fb00309 lea rax, [rip + 0x903b04f]
0x363bf1 c3 ret 
0x363bf2 cc int3 
0x363bf3 cc int3 
0x363bf4 cc int3 
0x363bf5 cc int3 
0x363bf6 cc int3 
0x363bf7 cc int3 
0x363bf8 cc int3 
0x363bf9 cc int3 
0x363bfa cc int3 
0x363bfb cc int3 
0x363bfc cc int3 
0x363bfd cc int3 
0x363bfe cc int3 
0x363bff cc int3 
0x363c00 55 push rbp
0x363c01 4889e5 mov rbp, rsp
0x363c04 53 push rbx
0x363c05 50 push rax
0x363c06 4889fb mov rbx, rdi
0x363c09 488b7f28 mov rdi, qword ptr [rdi + 0x28]
0x363c0d 4885ff test rdi, rdi
0x363c10 7406 je 0x363c18
0x363c12 488b07 mov rax, qword ptr [rdi]
0x363c15 ff5010 call qword ptr [rax + 0x10]
0x363c18 48c7432800000000 mov qword ptr [rbx + 0x28], 0
0x363c20 4883c408 add rsp, 8
0x363c24 5b pop rbx
0x363c25 5d pop rbp
0x363c26 c3 ret 
0x363c27 cc int3 
0x363c28 cc int3 
0x363c29 cc int3 
0x363c2a cc int3 
0x363c2b cc int3 
0x363c2c cc int3 
0x363c2d cc int3 
0x363c2e cc int3 
0x363c2f cc int3 
0x363c30 55 push rbp
0x363c31 4889e5 mov rbp, rsp
0x363c34 4157 push r15
0x363c36 4156 push r14
0x363c38 4154 push r12
0x363c3a 53 push rbx
0x363c3b 4883ec10 sub rsp, 0x10
0x363c3f 4c8b25c2e2dc08 mov r12, qword ptr [rip + 0x8dce2c2]
0x363c46 4989f6 mov r14, rsi
0x363c49 4889fb mov rbx, rdi
0x363c4c 498b0424 mov rax, qword ptr [r12]
0x363c50 488945d8 mov qword ptr [rbp - 0x28], rax
0x363c54 488b06 mov rax, qword ptr [rsi]
0x363c57 4885c0 test rax, rax
0x363c5a 7405 je 0x363c61
0x363c5c 488b00 mov rax, qword ptr [rax]
0x363c5f eb02 jmp 0x363c63
0x363c61 31c0 xor eax, eax
0x363c63 488945d0 mov qword ptr [rbp - 0x30], rax
0x363c67 488d75d0 lea rsi, [rbp - 0x30]
0x363c6b 4889df mov rdi, rbx
0x363c6e 488b03 mov rax, qword ptr [rbx]
0x363c71 ff9038010000 call qword ptr [rax + 0x138]
0x363c77 f6432801 test byte ptr [rbx + 0x28], 1
0x363c7b 0f849a000000 je 0x363d1b
0x363c81 48837dd000 cmp qword ptr [rbp - 0x30], 0
0x363c86 0f848a000000 je 0x363d16
0x363c8c 0fb605fdaf0309 movzx eax, byte ptr [rip + 0x903affd]
0x363c93 84c0 test al, al
0x363c95 0f84a1000000 je 0x363d3c
0x363c9b 4c8b7dd0 mov r15, qword ptr [rbp - 0x30]
0x363c9f 4489ff mov edi, r15d
0x363ca2 e8295eedff call 0x239ad0
0x363ca7 8b0d9baf0309 mov ecx, dword ptr [rip + 0x903af9b]
0x363cad 3b0dc1af0309 cmp ecx, dword ptr [rip + 0x903afc1]
0x363cb3 7461 je 0x363d16
0x363cb5 4c89f9 mov rcx, r15
0x363cb8 488d15b9af0309 lea rdx, [rip + 0x903afb9]
0x363cbf 48c1e920 shr rcx, 0x20
0x363cc3 01c8 add eax, ecx
0x363cc5 488b0db4af0309 mov rcx, qword ptr [rip + 0x903afb4]
0x363ccc 4885c9 test rcx, rcx
0x363ccf 480f45d1 cmovne rdx, rcx
0x363cd3 8b0dafaf0309 mov ecx, dword ptr [rip + 0x903afaf]
0x363cd9 ffc9 dec ecx
0x363cdb 21c1 and ecx, eax
0x363cdd 8b148a mov edx, dword ptr [rdx + rcx*4]
0x363ce0 83faff cmp edx, -1
0x363ce3 7431 je 0x363d16
0x363ce5 488b0d54af0309 mov rcx, qword ptr [rip + 0x903af54]
0x363cec 31c0 xor eax, eax
0x363cee 6690 nop 
0x363cf0 4863d2 movsxd rdx, edx
0x363cf3 488d3452 lea rsi, [rdx + rdx*2]
0x363cf7 488d14f1 lea rdx, [rcx + rsi*8]
0x363cfb 4c393cf1 cmp qword ptr [rcx + rsi*8], r15
0x363cff 740a je 0x363d0b
0x363d01 8b5210 mov edx, dword ptr [rdx + 0x10]
0x363d04 83faff cmp edx, -1
0x363d07 75e7 jne 0x363cf0
0x363d09 eb0d jmp 0x363d18
0x363d0b 4885d2 test rdx, rdx
0x363d0e 7406 je 0x363d16
0x363d10 488b4208 mov rax, qword ptr [rdx + 8]
0x363d14 eb02 jmp 0x363d18
0x363d16 31c0 xor eax, eax
0x363d18 498906 mov qword ptr [r14], rax
0x363d1b 498b0424 mov rax, qword ptr [r12]
0x363d1f 483b45d8 cmp rax, qword ptr [rbp - 0x28]
0x363d23 7510 jne 0x363d35
0x363d25 4889d8 mov rax, rbx
0x363d28 4883c410 add rsp, 0x10
0x363d2c 5b pop rbx
0x363d2d 415c pop r12
0x363d2f 415e pop r14
0x363d31 415f pop r15
0x363d33 5d pop rbp
0x363d34 c3 ret 
0x363d35 e826e11007 call 0x7471e60
0x363d3a 0f0b ud2 
0x363d3c 488d3d4daf0309 lea rdi, [rip + 0x903af4d]
0x363d43 e868e11007 call 0x7471eb0
0x363d48 85c0 test eax, eax
0x363d4a 0f844bffffff je 0x363c9b
0x363d50 c5f8100dc88c6d07 vmovups xmm1, xmmword ptr [rip + 0x76d8cc8]
0x363d58 488d35e1ae0309 lea rsi, [rip + 0x903aee1]
0x363d5f 488d3d9a40cbff lea rdi, [rip - 0x34bf66]
0x363d66 488d159382f708 lea rdx, [rip + 0x8f78293]
0x363d6d 48c705e8ae030900000000 mov qword ptr [rip + 0x903aee8], 0
0x363d78 c5f857c0 vxorps xmm0, xmm0, xmm0
0x363d7c c5f81105bcae0309 vmovups xmmword ptr [rip + 0x903aebc], xmm0
0x363d84 c5f8110ddcae0309 vmovups xmmword ptr [rip + 0x903aedc], xmm1
0x363d8c 48c705e9ae030900000000 mov qword ptr [rip + 0x903aee9], 0
0x363d97 c705e7ae030900000000 mov dword ptr [rip + 0x903aee7], 0
0x363da1 e82ae11007 call 0x7471ed0
0x363da6 488d3de3ae0309 lea rdi, [rip + 0x903aee3]
0x363dad e80ee11007 call 0x7471ec0
0x363db2 e9e4feffff jmp 0x363c9b
0x363db7 cc int3 
0x363db8 cc int3 
0x363db9 cc int3 
0x363dba cc int3 
0x363dbb cc int3 
0x363dbc cc int3 
0x363dbd cc int3 
0x363dbe cc int3 
0x363dbf cc int3 
0x363dc0 488b0e mov rcx, qword ptr [rsi]
0x363dc3 4889f8 mov rax, rdi
0x363dc6 f6c101 test cl, 1
0x363dc9 751d jne 0x363de8
0x363dcb 488b7910 mov rdi, qword ptr [rcx + 0x10]
0x363dcf 488938 mov qword ptr [rax], rdi
0x363dd2 40f6c701 test dil, 1
0x363dd6 740f je 0x363de7
0x363dd8 4883e7fe and rdi, 0xfffffffffffffffe
0x363ddc 7409 je 0x363de7
0x363dde 803d77b0030901 cmp byte ptr [rip + 0x903b077], 1
0x363de5 7422 je 0x363e09
0x363de7 c3 ret 
0x363de8 4883e1fe and rcx, 0xfffffffffffffffe
0x363dec 488b7920 mov rdi, qword ptr [rcx + 0x20]
0x363df0 4889f9 mov rcx, rdi
0x363df3 4883c901 or rcx, 1
0x363df7 4883e7fe and rdi, 0xfffffffffffffffe
0x363dfb 488908 mov qword ptr [rax], rcx
0x363dfe 74e7 je 0x363de7
0x363e00 803d55b0030900 cmp byte ptr [rip + 0x903b055], 0
0x363e07 74de je 0x363de7
0x363e09 55 push rbp
0x363e0a 4889e5 mov rbp, rsp
0x363e0d 53 push rbx
0x363e0e 50 push rax
0x363e0f 4889c3 mov rbx, rax
0x363e12 e8b9440000 call 0x3682d0
0x363e17 4889d8 mov rax, rbx
0x363e1a 4883c408 add rsp, 8
0x363e1e 5b pop rbx
0x363e1f 5d pop rbp
0x363e20 c3 ret 
0x363e21 cc int3 
0x363e22 cc int3 
0x363e23 cc int3 
0x363e24 cc int3 
0x363e25 cc int3 
0x363e26 cc int3 
0x363e27 cc int3 
0x363e28 cc int3 
0x363e29 cc int3 
0x363e2a cc int3 
0x363e2b cc int3 
0x363e2c cc int3 
0x363e2d cc int3 
0x363e2e cc int3 
0x363e2f cc int3 
0x363e30 488b07 mov rax, qword ptr [rdi]
0x363e33 a801 test al, 1
0x363e35 741b je 0x363e52
0x363e37 4883e0fe and rax, 0xfffffffffffffffe
0x363e3b 7415 je 0x363e52
0x363e3d 55 push rbp
0x363e3e 4889e5 mov rbp, rsp
0x363e41 488b4010 mov rax, qword ptr [rax + 0x10]
0x363e45 48634e38 movsxd rcx, dword ptr [rsi + 0x38]
0x363e49 3b4838 cmp ecx, dword ptr [rax + 0x38]
0x363e4c 7e07 jle 0x363e55
0x363e4e 31c0 xor eax, eax
0x363e50 5d pop rbp
0x363e51 c3 ret 
0x363e52 31c0 xor eax, eax
0x363e54 c3 ret 
0x363e55 488b4030 mov rax, qword ptr [rax + 0x30]
0x363e59 4883c630 add rsi, 0x30
0x363e5d 483934c8 cmp qword ptr [rax + rcx*8], rsi
0x363e61 0f94c0 sete al
0x363e64 5d pop rbp
0x363e65 c3 ret 
0x363e66 cc int3 
0x363e67 cc int3 
0x363e68 cc int3 
0x363e69 cc int3 
0x363e6a cc int3 
0x363e6b cc int3 
0x363e6c cc int3 
0x363e6d cc int3 
0x363e6e cc int3 
0x363e6f cc int3 
0x363e70 488b07 mov rax, qword ptr [rdi]
0x363e73 4885c0 test rax, rax
0x363e76 0f94c1 sete cl
0x363e79 08c1 or cl, al
0x363e7b f6c101 test cl, 1
0x363e7e 7403 je 0x363e83
0x363e80 31c0 xor eax, eax
0x363e82 c3 ret 
0x363e83 55 push rbp
0x363e84 4889e5 mov rbp, rsp
0x363e87 488b4808 mov rcx, qword ptr [rax + 8]
0x363e8b 488b4608 mov rax, qword ptr [rsi + 8]
0x363e8f 4885c0 test rax, rax
0x363e92 7409 je 0x363e9d
0x363e94 48854110 test qword ptr [rcx + 0x10], rax
0x363e98 0f95c0 setne al
0x363e9b 5d pop rbp
0x363e9c c3 ret 
0x363e9d 4885c9 test rcx, rcx
0x363ea0 0f95c0 setne al
0x363ea3 0f94c2 sete dl
0x363ea6 4839f1 cmp rcx, rsi
0x363ea9 400f94c7 sete dil
0x363ead 4008d7 or dil, dl
0x363eb0 7522 jne 0x363ed4
0x363eb2 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x363ec0 488b4920 mov rcx, qword ptr [rcx + 0x20]
0x363ec4 4885c9 test rcx, rcx
0x363ec7 0f95c0 setne al
0x363eca 4839f1 cmp rcx, rsi
0x363ecd 7405 je 0x363ed4
0x363ecf 4885c9 test rcx, rcx
0x363ed2 75ec jne 0x363ec0
0x363ed4 5d pop rbp
0x363ed5 c3 ret 
0x363ed6 cc int3 
0x363ed7 cc int3 
0x363ed8 cc int3 
0x363ed9 cc int3 
0x363eda cc int3 
0x363edb cc int3 
0x363edc cc int3 
0x363edd cc int3 
0x363ede cc int3 
0x363edf cc int3 
0x363ee0 55 push rbp
0x363ee1 4889e5 mov rbp, rsp
0x363ee4 53 push rbx
0x363ee5 50 push rax
0x363ee6 488b3f mov rdi, qword ptr [rdi]
0x363ee9 40f6c701 test dil, 1
0x363eed 7527 jne 0x363f16
0x363eef 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x363ef3 f6c301 test bl, 1
0x363ef6 7524 jne 0x363f1c
0x363ef8 31c0 xor eax, eax
0x363efa 4883fb02 cmp rbx, 2
0x363efe 720f jb 0x363f0f
0x363f00 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x363f04 f6c301 test bl, 1
0x363f07 7513 jne 0x363f1c
0x363f09 4883fb01 cmp rbx, 1
0x363f0d 77f1 ja 0x363f00
0x363f0f 4883c408 add rsp, 8
0x363f13 5b pop rbx
0x363f14 5d pop rbp
0x363f15 c3 ret 
0x363f16 4883e7fe and rdi, 0xfffffffffffffffe
0x363f1a eb3a jmp 0x363f56
0x363f1c 4883e3fe and rbx, 0xfffffffffffffffe
0x363f20 7440 je 0x363f62
0x363f22 803d33af030900 cmp byte ptr [rip + 0x903af33], 0
0x363f29 7408 je 0x363f33
0x363f2b 4889df mov rdi, rbx
0x363f2e e89d430000 call 0x3682d0
0x363f33 e88825fcff call 0x3264c0
0x363f38 488b4b10 mov rcx, qword ptr [rbx + 0x10]
0x363f3c 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x363f40 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x363f43 7f0e jg 0x363f53
0x363f45 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x363f49 4883c030 add rax, 0x30
0x363f4d 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x363f51 7418 je 0x363f6b
0x363f53 4889df mov rdi, rbx
0x363f56 e8a562fbff call 0x31a200
0x363f5b 4883c408 add rsp, 8
0x363f5f 5b pop rbx
0x363f60 5d pop rbp
0x363f61 c3 ret 
0x363f62 31c0 xor eax, eax
0x363f64 4883c408 add rsp, 8
0x363f68 5b pop rbx
0x363f69 5d pop rbp
0x363f6a c3 ret 
0x363f6b 4889d8 mov rax, rbx
0x363f6e 4883c408 add rsp, 8
0x363f72 5b pop rbx
0x363f73 5d pop rbp
0x363f74 c3 ret 
0x363f75 cc int3 
0x363f76 cc int3 
0x363f77 cc int3 
0x363f78 cc int3 
0x363f79 cc int3 
0x363f7a cc int3 
0x363f7b cc int3 
0x363f7c cc int3 
0x363f7d cc int3 
0x363f7e cc int3 
0x363f7f cc int3 
0x363f80 55 push rbp
0x363f81 4889e5 mov rbp, rsp
0x363f84 53 push rbx
0x363f85 50 push rax
0x363f86 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x363f8a f6c301 test bl, 1
0x363f8d 7527 jne 0x363fb6
0x363f8f 31c0 xor eax, eax
0x363f91 4883fb02 cmp rbx, 2
0x363f95 7218 jb 0x363faf
0x363f97 660f1f840000000000 nop word ptr [rax + rax]
0x363fa0 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x363fa4 f6c301 test bl, 1
0x363fa7 750d jne 0x363fb6
0x363fa9 4883fb01 cmp rbx, 1
0x363fad 77f1 ja 0x363fa0
0x363faf 4883c408 add rsp, 8
0x363fb3 5b pop rbx
0x363fb4 5d pop rbp
0x363fb5 c3 ret 
0x363fb6 4883e3fe and rbx, 0xfffffffffffffffe
0x363fba 7440 je 0x363ffc
0x363fbc 803d99ae030900 cmp byte ptr [rip + 0x903ae99], 0
0x363fc3 7408 je 0x363fcd
0x363fc5 4889df mov rdi, rbx
0x363fc8 e803430000 call 0x3682d0
0x363fcd e8ee24fcff call 0x3264c0
0x363fd2 488b4b10 mov rcx, qword ptr [rbx + 0x10]
0x363fd6 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x363fda 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x363fdd 7f0e jg 0x363fed
0x363fdf 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x363fe3 4883c030 add rax, 0x30
0x363fe7 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x363feb 7418 je 0x364005
0x363fed 4889df mov rdi, rbx
0x363ff0 e80b62fbff call 0x31a200
0x363ff5 4883c408 add rsp, 8
0x363ff9 5b pop rbx
0x363ffa 5d pop rbp
0x363ffb c3 ret 
0x363ffc 31c0 xor eax, eax
0x363ffe 4883c408 add rsp, 8
0x364002 5b pop rbx
0x364003 5d pop rbp
0x364004 c3 ret 
0x364005 4889d8 mov rax, rbx
0x364008 4883c408 add rsp, 8
0x36400c 5b pop rbx
0x36400d 5d pop rbp
0x36400e c3 ret 
0x36400f cc int3 
0x364010 55 push rbp
0x364011 4889e5 mov rbp, rsp
0x364014 4157 push r15
0x364016 4156 push r14
0x364018 4155 push r13
0x36401a 4154 push r12
0x36401c 53 push rbx
0x36401d 4881ec38020000 sub rsp, 0x238
0x364024 488b05dddedc08 mov rax, qword ptr [rip + 0x8dcdedd]
0x36402b 4989f6 mov r14, rsi
0x36402e 4889fb mov rbx, rdi
0x364031 488b00 mov rax, qword ptr [rax]
0x364034 488945d0 mov qword ptr [rbp - 0x30], rax
0x364038 488b7608 mov rsi, qword ptr [rsi + 8]
0x36403c e87fb0edff call 0x23f0c0
0x364041 488d35d2a16007 lea rsi, [rip + 0x760a1d2]
0x364048 4889df mov rdi, rbx
0x36404b ba01000000 mov edx, 1
0x364050 e82b0bd0ff call 0x64b80
0x364055 4c8dbdb0fdffff lea r15, [rbp - 0x250]
0x36405c 488d85cafdffff lea rax, [rbp - 0x236]
0x364063 488d4dca lea rcx, [rbp - 0x36]
0x364067 4c89f7 mov rdi, r14
0x36406a c685c8fdffff00 mov byte ptr [rbp - 0x238], 0
0x364071 4531ed xor r13d, r13d
0x364074 31f6 xor esi, esi
0x364076 4c89fa mov rdx, r15
0x364079 488985b0fdffff mov qword ptr [rbp - 0x250], rax
0x364080 488985b8fdffff mov qword ptr [rbp - 0x248], rax
0x364087 48898dc0fdffff mov qword ptr [rbp - 0x240], rcx
0x36408e e85d080000 call 0x3648f0
0x364093 488bb5b0fdffff mov rsi, qword ptr [rbp - 0x250]
0x36409a 488b95b8fdffff mov rdx, qword ptr [rbp - 0x248]
0x3640a1 488dbda0fdffff lea rdi, [rbp - 0x260]
0x3640a8 4829f2 sub rdx, rsi
0x3640ab 48d1ea shr rdx, 1
0x3640ae e85d03d0ff call 0x64410
0x3640b3 448ba5a8fdffff mov r12d, dword ptr [rbp - 0x258]
0x3640ba 4c8bb5a0fdffff mov r14, qword ptr [rbp - 0x260]
0x3640c1 4c89ff mov rdi, r15
0x3640c4 e81700eaff call 0x2040e0
0x3640c9 4183ec01 sub r12d, 1
0x3640cd 4889df mov rdi, rbx
0x3640d0 4c89f6 mov rsi, r14
0x3640d3 450f42e5 cmovb r12d, r13d
0x3640d7 4489e2 mov edx, r12d
0x3640da e8a10ad0ff call 0x64b80
0x3640df 4d85f6 test r14, r14
0x3640e2 7408 je 0x3640ec
0x3640e4 4c89f7 mov rdi, r14
0x3640e7 e8a439d5ff call 0xb7a90
0x3640ec 488b0515dedc08 mov rax, qword ptr [rip + 0x8dcde15]
0x3640f3 488b00 mov rax, qword ptr [rax]
0x3640f6 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x3640fa 7515 jne 0x364111
0x3640fc 4889d8 mov rax, rbx
0x3640ff 4881c438020000 add rsp, 0x238
0x364106 5b pop rbx
0x364107 415c pop r12
0x364109 415d pop r13
0x36410b 415e pop r14
0x36410d 415f pop r15
0x36410f 5d pop rbp
0x364110 c3 ret 
0x364111 e84add1007 call 0x7471e60
0x364116 0f0b ud2 
0x364118 cc int3 
0x364119 cc int3 
0x36411a cc int3 
0x36411b cc int3 
0x36411c cc int3 
0x36411d cc int3 
0x36411e cc int3 
0x36411f cc int3 
0x364120 55 push rbp
0x364121 4889e5 mov rbp, rsp
0x364124 4157 push r15
0x364126 4156 push r14
0x364128 53 push rbx
0x364129 4881ec38020000 sub rsp, 0x238
0x364130 4c8b3dd1dddc08 mov r15, qword ptr [rip + 0x8dcddd1]
0x364137 4889fb mov rbx, rdi
0x36413a 498b07 mov rax, qword ptr [r15]
0x36413d 488945e0 mov qword ptr [rbp - 0x20], rax
0x364141 488b36 mov rsi, qword ptr [rsi]
0x364144 40f6c601 test sil, 1
0x364148 757f jne 0x3641c9
0x36414a 4889f7 mov rdi, rsi
0x36414d 488d85dafdffff lea rax, [rbp - 0x226]
0x364154 488d4dda lea rcx, [rbp - 0x26]
0x364158 488d95c0fdffff lea rdx, [rbp - 0x240]
0x36415f c685d8fdffff00 mov byte ptr [rbp - 0x228], 0
0x364166 31f6 xor esi, esi
0x364168 488985c0fdffff mov qword ptr [rbp - 0x240], rax
0x36416f 488985c8fdffff mov qword ptr [rbp - 0x238], rax
0x364176 48898dd0fdffff mov qword ptr [rbp - 0x230], rcx
0x36417d e86e070000 call 0x3648f0
0x364182 488bb5c0fdffff mov rsi, qword ptr [rbp - 0x240]
0x364189 488b95c8fdffff mov rdx, qword ptr [rbp - 0x238]
0x364190 4c8db5b0fdffff lea r14, [rbp - 0x250]
0x364197 c5f857c0 vxorps xmm0, xmm0, xmm0
0x36419b c5f81103 vmovups xmmword ptr [rbx], xmm0
0x36419f 4c89f7 mov rdi, r14
0x3641a2 4829f2 sub rdx, rsi
0x3641a5 48d1ea shr rdx, 1
0x3641a8 e86302d0ff call 0x64410
0x3641ad 4939de cmp r14, rbx
0x3641b0 7427 je 0x3641d9
0x3641b2 488b85b0fdffff mov rax, qword ptr [rbp - 0x250]
0x3641b9 488b8db8fdffff mov rcx, qword ptr [rbp - 0x248]
0x3641c0 488903 mov qword ptr [rbx], rax
0x3641c3 48894b08 mov qword ptr [rbx + 8], rcx
0x3641c7 eb21 jmp 0x3641ea
0x3641c9 4883e6fe and rsi, 0xfffffffffffffffe
0x3641cd 4889df mov rdi, rbx
0x3641d0 31d2 xor edx, edx
0x3641d2 e8092b1200 call 0x486ce0
0x3641d7 eb1d jmp 0x3641f6
0x3641d9 488bbdb0fdffff mov rdi, qword ptr [rbp - 0x250]
0x3641e0 4885ff test rdi, rdi
0x3641e3 7405 je 0x3641ea
0x3641e5 e8a638d5ff call 0xb7a90
0x3641ea 488dbdc0fdffff lea rdi, [rbp - 0x240]
0x3641f1 e8eafee9ff call 0x2040e0
0x3641f6 498b07 mov rax, qword ptr [r15]
0x3641f9 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x3641fd 7511 jne 0x364210
0x3641ff 4889d8 mov rax, rbx
0x364202 4881c438020000 add rsp, 0x238
0x364209 5b pop rbx
0x36420a 415e pop r14
0x36420c 415f pop r15
0x36420e 5d pop rbp
0x36420f c3 ret 
0x364210 e84bdc1007 call 0x7471e60
0x364215 0f0b ud2 
0x364217 cc int3 
0x364218 cc int3 
0x364219 cc int3 
0x36421a cc int3 
0x36421b cc int3 
0x36421c cc int3 
0x36421d cc int3 
0x36421e cc int3 
0x36421f cc int3 
0x364220 55 push rbp
0x364221 4889e5 mov rbp, rsp
0x364224 4157 push r15
0x364226 4156 push r14
0x364228 53 push rbx
0x364229 4881ec38020000 sub rsp, 0x238
0x364230 4c8b3dd1dcdc08 mov r15, qword ptr [rip + 0x8dcdcd1]
0x364237 488d8ddafdffff lea rcx, [rbp - 0x226]
0x36423e 4889fb mov rbx, rdi
0x364241 488d7dda lea rdi, [rbp - 0x26]
0x364245 498b07 mov rax, qword ptr [r15]
0x364248 488945e0 mov qword ptr [rbp - 0x20], rax
0x36424c c685d8fdffff00 mov byte ptr [rbp - 0x228], 0
0x364253 48898dc0fdffff mov qword ptr [rbp - 0x240], rcx
0x36425a 48898dc8fdffff mov qword ptr [rbp - 0x238], rcx
0x364261 488d8dc0fdffff lea rcx, [rbp - 0x240]
0x364268 4889bdd0fdffff mov qword ptr [rbp - 0x230], rdi
0x36426f 4889f7 mov rdi, rsi
0x364272 4889d6 mov rsi, rdx
0x364275 4889ca mov rdx, rcx
0x364278 e873060000 call 0x3648f0
0x36427d 488bb5c0fdffff mov rsi, qword ptr [rbp - 0x240]
0x364284 488b95c8fdffff mov rdx, qword ptr [rbp - 0x238]
0x36428b 4c8db5b0fdffff lea r14, [rbp - 0x250]
0x364292 c5f857c0 vxorps xmm0, xmm0, xmm0
0x364296 c5f81103 vmovups xmmword ptr [rbx], xmm0
0x36429a 4c89f7 mov rdi, r14
0x36429d 4829f2 sub rdx, rsi
0x3642a0 48d1ea shr rdx, 1
0x3642a3 e86801d0ff call 0x64410
0x3642a8 4939de cmp r14, rbx
0x3642ab 7417 je 0x3642c4
0x3642ad 488b85b0fdffff mov rax, qword ptr [rbp - 0x250]
0x3642b4 488b8db8fdffff mov rcx, qword ptr [rbp - 0x248]
0x3642bb 488903 mov qword ptr [rbx], rax
0x3642be 48894b08 mov qword ptr [rbx + 8], rcx
0x3642c2 eb11 jmp 0x3642d5
0x3642c4 488bbdb0fdffff mov rdi, qword ptr [rbp - 0x250]
0x3642cb 4885ff test rdi, rdi
0x3642ce 7405 je 0x3642d5
0x3642d0 e8bb37d5ff call 0xb7a90
0x3642d5 488dbdc0fdffff lea rdi, [rbp - 0x240]
0x3642dc e8fffde9ff call 0x2040e0
0x3642e1 498b07 mov rax, qword ptr [r15]
0x3642e4 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x3642e8 7511 jne 0x3642fb
0x3642ea 4889d8 mov rax, rbx
0x3642ed 4881c438020000 add rsp, 0x238
0x3642f4 5b pop rbx
0x3642f5 415e pop r14
0x3642f7 415f pop r15
0x3642f9 5d pop rbp
0x3642fa c3 ret 
0x3642fb e860db1007 call 0x7471e60
0x364300 0f0b ud2 
0x364302 cc int3 
0x364303 cc int3 
0x364304 cc int3 
0x364305 cc int3 
0x364306 cc int3 
0x364307 cc int3 
0x364308 cc int3 
0x364309 cc int3 
0x36430a cc int3 
0x36430b cc int3 
0x36430c cc int3 
0x36430d cc int3 
0x36430e cc int3 
0x36430f cc int3 
0x364310 55 push rbp
0x364311 4889e5 mov rbp, rsp
0x364314 4157 push r15
0x364316 4156 push r14
0x364318 53 push rbx
0x364319 4883ec18 sub rsp, 0x18
0x36431d 4c8b3de4dbdc08 mov r15, qword ptr [rip + 0x8dcdbe4]
0x364324 4889fb mov rbx, rdi
0x364327 498b07 mov rax, qword ptr [r15]
0x36432a 488945e0 mov qword ptr [rbp - 0x20], rax
0x36432e 4c8b36 mov r14, qword ptr [rsi]
0x364331 41f6c601 test r14b, 1
0x364335 752d jne 0x364364
0x364337 488d354daf8707 lea rsi, [rip + 0x787af4d]
0x36433e 4c89f7 mov rdi, r14
0x364341 e8ca96e4ff call 0x1ada10
0x364346 84c0 test al, al
0x364348 7449 je 0x364393
0x36434a 4983c620 add r14, 0x20
0x36434e 4889df mov rdi, rbx
0x364351 4c89f6 mov rsi, r14
0x364354 e867adedff call 0x23f0c0
0x364359 498b07 mov rax, qword ptr [r15]
0x36435c 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x364360 7423 je 0x364385
0x364362 eb47 jmp 0x3643ab
0x364364 4983e6fe and r14, 0xfffffffffffffffe
0x364368 488d75d8 lea rsi, [rbp - 0x28]
0x36436c 4889df mov rdi, rbx
0x36436f 498b4618 mov rax, qword ptr [r14 + 0x18]
0x364373 488945d8 mov qword ptr [rbp - 0x28], rax
0x364377 e844adedff call 0x23f0c0
0x36437c 498b07 mov rax, qword ptr [r15]
0x36437f 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x364383 7526 jne 0x3643ab
0x364385 4889d8 mov rax, rbx
0x364388 4883c418 add rsp, 0x18
0x36438c 5b pop rbx
0x36438d 415e pop r14
0x36438f 415f pop r15
0x364391 5d pop rbp
0x364392 c3 ret 
0x364393 488d35843f5107 lea rsi, [rip + 0x7513f84]
0x36439a 4889df mov rdi, rbx
0x36439d e84efbcfff call 0x63ef0
0x3643a2 498b07 mov rax, qword ptr [r15]
0x3643a5 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x3643a9 74da je 0x364385
0x3643ab e8b0da1007 call 0x7471e60
0x3643b0 0f0b ud2 
0x3643b2 cc int3 
0x3643b3 cc int3 
0x3643b4 cc int3 
0x3643b5 cc int3 
0x3643b6 cc int3 
0x3643b7 cc int3 
0x3643b8 cc int3 
0x3643b9 cc int3 
0x3643ba cc int3 
0x3643bb cc int3 
0x3643bc cc int3 
0x3643bd cc int3 
0x3643be cc int3 
0x3643bf cc int3 
0x3643c0 55 push rbp
0x3643c1 4889e5 mov rbp, rsp
0x3643c4 53 push rbx
0x3643c5 50 push rax
0x3643c6 488b1f mov rbx, qword ptr [rdi]
0x3643c9 f6c301 test bl, 1
0x3643cc 7519 jne 0x3643e7
0x3643ce 488d3597ea8507 lea rsi, [rip + 0x785ea97]
0x3643d5 4889df mov rdi, rbx
0x3643d8 e83396e4ff call 0x1ada10
0x3643dd 84c0 test al, al
0x3643df 7418 je 0x3643f9
0x3643e1 4883c320 add rbx, 0x20
0x3643e5 eb08 jmp 0x3643ef
0x3643e7 4883e3fe and rbx, 0xfffffffffffffffe
0x3643eb 4883c318 add rbx, 0x18
0x3643ef 488b03 mov rax, qword ptr [rbx]
0x3643f2 4883c408 add rsp, 8
0x3643f6 5b pop rbx
0x3643f7 5d pop rbp
0x3643f8 c3 ret 
0x3643f9 31c0 xor eax, eax
0x3643fb 4883c408 add rsp, 8
0x3643ff 5b pop rbx
0x364400 5d pop rbp
0x364401 c3 ret 
0x364402 cc int3 
0x364403 cc int3 
0x364404 cc int3 
0x364405 cc int3 
0x364406 cc int3 
0x364407 cc int3 
0x364408 cc int3 
0x364409 cc int3 
0x36440a cc int3 
0x36440b cc int3 
0x36440c cc int3 
0x36440d cc int3 
0x36440e cc int3 
0x36440f cc int3 
0x364410 55 push rbp
0x364411 4889e5 mov rbp, rsp
0x364414 53 push rbx
0x364415 50 push rax
0x364416 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x36441a f6c301 test bl, 1
0x36441d 752c jne 0x36444b
0x36441f 4883fb02 cmp rbx, 2
0x364423 b001 mov al, 1
0x364425 0f92c1 setb cl
0x364428 08d9 or cl, bl
0x36442a f6c101 test cl, 1
0x36442d 7515 jne 0x364444
0x36442f 90 nop 
0x364430 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x364434 f6c301 test bl, 1
0x364437 751d jne 0x364456
0x364439 f6c301 test bl, 1
0x36443c 7506 jne 0x364444
0x36443e 4883fb01 cmp rbx, 1
0x364442 77ec ja 0x364430
0x364444 4883c408 add rsp, 8
0x364448 5b pop rbx
0x364449 5d pop rbp
0x36444a c3 ret 
0x36444b 4889df mov rdi, rbx
0x36444e 4883e7fe and rdi, 0xfffffffffffffffe
0x364452 750b jne 0x36445f
0x364454 eb45 jmp 0x36449b
0x364456 4889df mov rdi, rbx
0x364459 4883e7fe and rdi, 0xfffffffffffffffe
0x36445d 74e5 je 0x364444
0x36445f 803df6a9030900 cmp byte ptr [rip + 0x903a9f6], 0
0x364466 7405 je 0x36446d
0x364468 e8633e0000 call 0x3682d0
0x36446d 4883e3fe and rbx, 0xfffffffffffffffe
0x364471 7428 je 0x36449b
0x364473 8b430c mov eax, dword ptr [rbx + 0xc]
0x364476 488d0d935c0509 lea rcx, [rip + 0x9055c93]
0x36447d 394124 cmp dword ptr [rcx + 0x24], eax
0x364480 7e22 jle 0x3644a4
0x364482 488b4910 mov rcx, qword ptr [rcx + 0x10]
0x364486 89c2 mov edx, eax
0x364488 0fb7c0 movzx eax, ax
0x36448b 488d0440 lea rax, [rax + rax*2]
0x36448f c1ea10 shr edx, 0x10
0x364492 c1e003 shl eax, 3
0x364495 480304d1 add rax, qword ptr [rcx + rdx*8]
0x364499 eb0b jmp 0x3644a6
0x36449b b001 mov al, 1
0x36449d 4883c408 add rsp, 8
0x3644a1 5b pop rbx
0x3644a2 5d pop rbp
0x3644a3 c3 ret 
0x3644a4 31c0 xor eax, eax
0x3644a6 8b4008 mov eax, dword ptr [rax + 8]
0x3644a9 b919010000 mov ecx, 0x119
0x3644ae c4e270f7c0 bextr eax, eax, ecx
0x3644b3 4883c408 add rsp, 8
0x3644b7 5b pop rbx
0x3644b8 5d pop rbp
0x3644b9 c3 ret 
0x3644ba cc int3 
0x3644bb cc int3 
0x3644bc cc int3 
0x3644bd cc int3 
0x3644be cc int3 
0x3644bf cc int3 
0x3644c0 55 push rbp
0x3644c1 4889e5 mov rbp, rsp
0x3644c4 53 push rbx
0x3644c5 50 push rax
0x3644c6 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x3644ca f6c301 test bl, 1
0x3644cd 7527 jne 0x3644f6
0x3644cf 4883fb02 cmp rbx, 2
0x3644d3 b001 mov al, 1
0x3644d5 0f92c1 setb cl
0x3644d8 08d9 or cl, bl
0x3644da f6c101 test cl, 1
0x3644dd 7539 jne 0x364518
0x3644df 90 nop 
0x3644e0 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x3644e4 f6c301 test bl, 1
0x3644e7 750d jne 0x3644f6
0x3644e9 f6c301 test bl, 1
0x3644ec 752a jne 0x364518
0x3644ee 4883fb01 cmp rbx, 1
0x3644f2 77ec ja 0x3644e0
0x3644f4 eb22 jmp 0x364518
0x3644f6 4889df mov rdi, rbx
0x3644f9 4883e7fe and rdi, 0xfffffffffffffffe
0x3644fd 7412 je 0x364511
0x3644ff 803d56a9030900 cmp byte ptr [rip + 0x903a956], 0
0x364506 7405 je 0x36450d
0x364508 e8c33d0000 call 0x3682d0
0x36450d 31c0 xor eax, eax
0x36450f eb07 jmp 0x364518
0x364511 31c0 xor eax, eax
0x364513 bb01000000 mov ebx, 1
0x364518 4883e3fe and rbx, 0xfffffffffffffffe
0x36451c 31ff xor edi, edi
0x36451e 84c0 test al, al
0x364520 480f44fb cmove rdi, rbx
0x364524 e8271c1200 call 0x486150
0x364529 4883c408 add rsp, 8
0x36452d 5b pop rbx
0x36452e 5d pop rbp
0x36452f c3 ret 
0x364530 0fb605a1a70309 movzx eax, byte ptr [rip + 0x903a7a1]
0x364537 84c0 test al, al
0x364539 7408 je 0x364543
0x36453b 488d0556a70309 lea rax, [rip + 0x903a756]
0x364542 c3 ret 
0x364543 55 push rbp
0x364544 4889e5 mov rbp, rsp
0x364547 53 push rbx
0x364548 50 push rax
0x364549 488d3d88a70309 lea rdi, [rip + 0x903a788]
0x364550 e85bd91007 call 0x7471eb0
0x364555 85c0 test eax, eax
0x364557 744c je 0x3645a5
0x364559 488d1d38a70309 lea rbx, [rip + 0x903a738]
0x364560 488d355b924107 lea rsi, [rip + 0x741925b]
0x364567 4c8d0d72bbc9ff lea r9, [rip - 0x36448e]
0x36456e ba01000000 mov edx, 1
0x364573 b901000000 mov ecx, 1
0x364578 4531c0 xor r8d, r8d
0x36457b 4889df mov rdi, rbx
0x36457e e80df4ffff call 0x363990
0x364583 488d3d76f6ffff lea rdi, [rip - 0x98a]
0x36458a 488d156f7af708 lea rdx, [rip + 0x8f77a6f]
0x364591 4889de mov rsi, rbx
0x364594 e837d91007 call 0x7471ed0
0x364599 488d3d38a70309 lea rdi, [rip + 0x903a738]
0x3645a0 e81bd91007 call 0x7471ec0
0x3645a5 4883c408 add rsp, 8
0x3645a9 5b pop rbx
0x3645aa 5d pop rbp
0x3645ab 488d05e6a60309 lea rax, [rip + 0x903a6e6]
0x3645b2 c3 ret 
0x3645b3 cc int3 
0x3645b4 cc int3 
0x3645b5 cc int3 
0x3645b6 cc int3 
0x3645b7 cc int3 
0x3645b8 cc int3 
0x3645b9 cc int3 
0x3645ba cc int3 
0x3645bb cc int3 
0x3645bc cc int3 
0x3645bd cc int3 
0x3645be cc int3 
0x3645bf cc int3 
0x3645c0 55 push rbp
0x3645c1 4889e5 mov rbp, rsp
0x3645c4 488d058580b707 lea rax, [rip + 0x7b78085]
0x3645cb c5f857c0 vxorps xmm0, xmm0, xmm0
0x3645cf 488907 mov qword ptr [rdi], rax
0x3645d2 48895708 mov qword ptr [rdi + 8], rdx
0x3645d6 c5f8114710 vmovups xmmword ptr [rdi + 0x10], xmm0
0x3645db c5f811471c vmovups xmmword ptr [rdi + 0x1c], xmm0
0x3645e0 5d pop rbp
0x3645e1 c3 ret 
0x3645e2 cc int3 
0x3645e3 cc int3 
0x3645e4 cc int3 
0x3645e5 cc int3 
0x3645e6 cc int3 
0x3645e7 cc int3 
0x3645e8 cc int3 
0x3645e9 cc int3 
0x3645ea cc int3 
0x3645eb cc int3 
0x3645ec cc int3 
0x3645ed cc int3 
0x3645ee cc int3 
0x3645ef cc int3 
0x3645f0 488d055980b707 lea rax, [rip + 0x7b78059]
0x3645f7 488907 mov qword ptr [rdi], rax
0x3645fa 488b06 mov rax, qword ptr [rsi]
0x3645fd 48894710 mov qword ptr [rdi + 0x10], rax
0x364601 a801 test al, 1
0x364603 743d je 0x364642
0x364605 4883e0fe and rax, 0xfffffffffffffffe
0x364609 7437 je 0x364642
0x36460b 803d4aa8030901 cmp byte ptr [rip + 0x903a84a], 1
0x364612 752e jne 0x364642
0x364614 55 push rbp
0x364615 4889e5 mov rbp, rsp
0x364618 4157 push r15
0x36461a 4156 push r14
0x36461c 53 push rbx
0x36461d 50 push rax
0x36461e 4889fb mov rbx, rdi
0x364621 4889c7 mov rdi, rax
0x364624 4189ce mov r14d, ecx
0x364627 4989d7 mov r15, rdx
0x36462a e8a13c0000 call 0x3682d0
0x36462f 4c89fa mov rdx, r15
0x364632 4889df mov rdi, rbx
0x364635 4489f1 mov ecx, r14d
0x364638 4883c408 add rsp, 8
0x36463c 5b pop rbx
0x36463d 415e pop r14
0x36463f 415f pop r15
0x364641 5d pop rbp
0x364642 48c7471800000000 mov qword ptr [rdi + 0x18], 0
0x36464a 488b02 mov rax, qword ptr [rdx]
0x36464d 48894720 mov qword ptr [rdi + 0x20], rax
0x364651 894f28 mov dword ptr [rdi + 0x28], ecx
0x364654 c3 ret 
0x364655 cc int3 
0x364656 cc int3 
0x364657 cc int3 
0x364658 cc int3 
0x364659 cc int3 
0x36465a cc int3 
0x36465b cc int3 
0x36465c cc int3 
0x36465d cc int3 
0x36465e cc int3 
0x36465f cc int3 
0x364660 55 push rbp
0x364661 4889e5 mov rbp, rsp
0x364664 53 push rbx
0x364665 50 push rax
0x364666 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x36466a f6c301 test bl, 1
0x36466d 7524 jne 0x364693
0x36466f 90 nop 
0x364670 4883fb02 cmp rbx, 2
0x364674 7241 jb 0x3646b7
0x364676 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x36467a f6c301 test bl, 1
0x36467d 74f1 je 0x364670
0x36467f 4889df mov rdi, rbx
0x364682 4883e7fe and rdi, 0xfffffffffffffffe
0x364686 7424 je 0x3646ac
0x364688 803dcda7030901 cmp byte ptr [rip + 0x903a7cd], 1
0x36468f 7414 je 0x3646a5
0x364691 eb1e jmp 0x3646b1
0x364693 4889df mov rdi, rbx
0x364696 4883e7fe and rdi, 0xfffffffffffffffe
0x36469a 7410 je 0x3646ac
0x36469c 803db9a7030900 cmp byte ptr [rip + 0x903a7b9], 0
0x3646a3 740c je 0x3646b1
0x3646a5 e8263c0000 call 0x3682d0
0x3646aa eb05 jmp 0x3646b1
0x3646ac bb01000000 mov ebx, 1
0x3646b1 4883e3fe and rbx, 0xfffffffffffffffe
0x3646b5 eb12 jmp 0x3646c9
0x3646b7 31db xor ebx, ebx
0x3646b9 eb0e jmp 0x3646c9
0x3646bb 0f1f440000 nop dword ptr [rax + rax]
0x3646c0 488b5b20 mov rbx, qword ptr [rbx + 0x20]
0x3646c4 4885db test rbx, rbx
0x3646c7 7427 je 0x3646f0
0x3646c9 4885db test rbx, rbx
0x3646cc 74f2 je 0x3646c0
0x3646ce e81dc4fbff call 0x320af0
0x3646d3 488b4b10 mov rcx, qword ptr [rbx + 0x10]
0x3646d7 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x3646db 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x3646de 7fe0 jg 0x3646c0
0x3646e0 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x3646e4 4883c030 add rax, 0x30
0x3646e8 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x3646ec 75d2 jne 0x3646c0
0x3646ee eb02 jmp 0x3646f2
0x3646f0 31db xor ebx, ebx
0x3646f2 4889d8 mov rax, rbx
0x3646f5 4883c408 add rsp, 8
0x3646f9 5b pop rbx
0x3646fa 5d pop rbp
0x3646fb c3 ret 
0x3646fc cc int3 
0x3646fd cc int3 
0x3646fe cc int3 
0x3646ff cc int3 
0x364700 55 push rbp
0x364701 4889e5 mov rbp, rsp
0x364704 488b07 mov rax, qword ptr [rdi]
0x364707 ff5048 call qword ptr [rax + 0x48]
0x36470a 5d pop rbp
0x36470b c3 ret 
0x36470c cc int3 
0x36470d cc int3 
0x36470e cc int3 
0x36470f cc int3 
0x364710 55 push rbp
0x364711 4889e5 mov rbp, rsp
0x364714 4156 push r14
0x364716 53 push rbx
0x364717 4989f6 mov r14, rsi
0x36471a 4889fb mov rbx, rdi
0x36471d 488d7720 lea rsi, [rdi + 0x20]
0x364721 498b06 mov rax, qword ptr [r14]
0x364724 4c89f7 mov rdi, r14
0x364727 ff9038010000 call qword ptr [rax + 0x138]
0x36472d 498b4608 mov rax, qword ptr [r14 + 8]
0x364731 4883c328 add rbx, 0x28
0x364735 488b10 mov rdx, qword ptr [rax]
0x364738 488d4a04 lea rcx, [rdx + 4]
0x36473c 483b4808 cmp rcx, qword ptr [rax + 8]
0x364740 7620 jbe 0x364762
0x364742 41f6462a01 test byte ptr [r14 + 0x2a], 1
0x364747 7525 jne 0x36476e
0x364749 498b06 mov rax, qword ptr [r14]
0x36474c ba04000000 mov edx, 4
0x364751 4c89f7 mov rdi, r14
0x364754 4889de mov rsi, rbx
0x364757 ff9088010000 call qword ptr [rax + 0x188]
0x36475d 5b pop rbx
0x36475e 415e pop r14
0x364760 5d pop rbp
0x364761 c3 ret 
0x364762 8b12 mov edx, dword ptr [rdx]
0x364764 8913 mov dword ptr [rbx], edx
0x364766 488908 mov qword ptr [rax], rcx
0x364769 5b pop rbx
0x36476a 415e pop r14
0x36476c 5d pop rbp
0x36476d c3 ret 
0x36476e 4c89f7 mov rdi, r14
0x364771 4889de mov rsi, rbx
0x364774 e83783ebff call 0x21cab0
0x364779 5b pop rbx
0x36477a 415e pop r14
0x36477c 5d pop rbp
0x36477d c3 ret 
0x36477e cc int3 
0x36477f cc int3 
0x364780 55 push rbp
0x364781 4889e5 mov rbp, rsp
0x364784 53 push rbx
0x364785 50 push rax
0x364786 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x36478a f6c301 test bl, 1
0x36478d 752c jne 0x3647bb
0x36478f 90 nop 
0x364790 4883fb02 cmp rbx, 2
0x364794 0f8296000000 jb 0x364830
0x36479a 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x36479e f6c301 test bl, 1
0x3647a1 74ed je 0x364790
0x3647a3 4889df mov rdi, rbx
0x3647a6 4883e7fe and rdi, 0xfffffffffffffffe
0x3647aa 0f8480000000 je 0x364830
0x3647b0 803da5a6030901 cmp byte ptr [rip + 0x903a6a5], 1
0x3647b7 7414 je 0x3647cd
0x3647b9 eb17 jmp 0x3647d2
0x3647bb 4889df mov rdi, rbx
0x3647be 4883e7fe and rdi, 0xfffffffffffffffe
0x3647c2 746c je 0x364830
0x3647c4 803d91a6030900 cmp byte ptr [rip + 0x903a691], 0
0x3647cb 7405 je 0x3647d2
0x3647cd e8fe3a0000 call 0x3682d0
0x3647d2 4883e3fe and rbx, 0xfffffffffffffffe
0x3647d6 7458 je 0x364830
0x3647d8 488d0531590509 lea rax, [rip + 0x9055931]
0x3647df be1e010000 mov esi, 0x11e
0x3647e4 8b4824 mov ecx, dword ptr [rax + 0x24]
0x3647e7 488b5010 mov rdx, qword ptr [rax + 0x10]
0x3647eb eb18 jmp 0x364805
0x3647ed 0f1f00 nop dword ptr [rax]
0x3647f0 31c0 xor eax, eax
0x3647f2 8b4008 mov eax, dword ptr [rax + 8]
0x3647f5 c4e248f7c0 bextr eax, eax, esi
0x3647fa 752d jne 0x364829
0x3647fc 488b5b20 mov rbx, qword ptr [rbx + 0x20]
0x364800 4885db test rbx, rbx
0x364803 7424 je 0x364829
0x364805 8b430c mov eax, dword ptr [rbx + 0xc]
0x364808 39c1 cmp ecx, eax
0x36480a 7ee4 jle 0x3647f0
0x36480c 89c7 mov edi, eax
0x36480e 0fb7c0 movzx eax, ax
0x364811 488d0440 lea rax, [rax + rax*2]
0x364815 c1ef10 shr edi, 0x10
0x364818 c1e003 shl eax, 3
0x36481b 480304fa add rax, qword ptr [rdx + rdi*8]
0x36481f 8b4008 mov eax, dword ptr [rax + 8]
0x364822 c4e248f7c0 bextr eax, eax, esi
0x364827 74d3 je 0x3647fc
0x364829 4883c408 add rsp, 8
0x36482d 5b pop rbx
0x36482e 5d pop rbp
0x36482f c3 ret 
0x364830 31c0 xor eax, eax
0x364832 4883c408 add rsp, 8
0x364836 5b pop rbx
0x364837 5d pop rbp
0x364838 c3 ret 
0x364839 cc int3 
0x36483a cc int3 
0x36483b cc int3 
0x36483c cc int3 
0x36483d cc int3 
0x36483e cc int3 
0x36483f cc int3 
0x364840 55 push rbp
0x364841 4889e5 mov rbp, rsp
0x364844 488d3553838807 lea rsi, [rip + 0x7888353]
0x36484b e8c091e4ff call 0x1ada10
0x364850 5d pop rbp
0x364851 c3 ret 
0x364852 cc int3 
0x364853 cc int3 
0x364854 cc int3 
0x364855 cc int3 
0x364856 cc int3 
0x364857 cc int3 
0x364858 cc int3 
0x364859 cc int3 
0x36485a cc int3 
0x36485b cc int3 
0x36485c cc int3 
0x36485d cc int3 
0x36485e cc int3 
0x36485f cc int3 
0x364860 55 push rbp
0x364861 4889e5 mov rbp, rsp
0x364864 53 push rbx
0x364865 50 push rax
0x364866 488b5f10 mov rbx, qword ptr [rdi + 0x10]
0x36486a f6c301 test bl, 1
0x36486d 752c jne 0x36489b
0x36486f 4883fb02 cmp rbx, 2
0x364873 0f92c0 setb al
0x364876 08d8 or al, bl
0x364878 a801 test al, 1
0x36487a 754b jne 0x3648c7
0x36487c 31c0 xor eax, eax
0x36487e 6690 nop 
0x364880 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x364884 f6c301 test bl, 1
0x364887 7512 jne 0x36489b
0x364889 f6c301 test bl, 1
0x36488c 7506 jne 0x364894
0x36488e 4883fb01 cmp rbx, 1
0x364892 77ec ja 0x364880
0x364894 4883c408 add rsp, 8
0x364898 5b pop rbx
0x364899 5d pop rbp
0x36489a c3 ret 
0x36489b 4889df mov rdi, rbx
0x36489e 4883e7fe and rdi, 0xfffffffffffffffe
0x3648a2 7423 je 0x3648c7
0x3648a4 803db1a5030900 cmp byte ptr [rip + 0x903a5b1], 0
0x3648ab 7405 je 0x3648b2
0x3648ad e81e3a0000 call 0x3682d0
0x3648b2 4883e3fe and rbx, 0xfffffffffffffffe
0x3648b6 740f je 0x3648c7
0x3648b8 4889df mov rdi, rbx
0x3648bb e840041500 call 0x4b4d00
0x3648c0 4883c408 add rsp, 8
0x3648c4 5b pop rbx
0x3648c5 5d pop rbp
0x3648c6 c3 ret 
0x3648c7 31c0 xor eax, eax
0x3648c9 4883c408 add rsp, 8
0x3648cd 5b pop rbx
0x3648ce 5d pop rbp
0x3648cf c3 ret 
0x3648d0 55 push rbp
0x3648d1 4889e5 mov rbp, rsp
0x3648d4 488d3df12a6f07 lea rdi, [rip + 0x76f2af1]
0x3648db 488d15acd56507 lea rdx, [rip + 0x765d5ac]
0x3648e2 be20020000 mov esi, 0x220
0x3648e7 31c0 xor eax, eax
0x3648e9 e8c207e1ff call 0x1750b0
0x3648ee 5d pop rbp
0x3648ef c3 ret 
0x3648f0 55 push rbp
0x3648f1 4889e5 mov rbp, rsp
0x3648f4 4157 push r15
0x3648f6 4156 push r14
0x3648f8 4155 push r13
0x3648fa 4154 push r12
0x3648fc 53 push rbx
0x3648fd 4881ecb8000000 sub rsp, 0xb8
0x364904 488b05fdd5dc08 mov rax, qword ptr [rip + 0x8dcd5fd]
0x36490b 4889b530ffffff mov qword ptr [rbp - 0xd0], rsi
0x364912 48b90000000010000000 movabs rcx, 0x1000000000
0x36491c 4889d3 mov rbx, rdx
0x36491f 488b00 mov rax, qword ptr [rax]
0x364922 488945d0 mov qword ptr [rbp - 0x30], rax
0x364926 48c745c000000000 mov qword ptr [rbp - 0x40], 0
0x36492e 48894dc8 mov qword ptr [rbp - 0x38], rcx
0x364932 4889bd28ffffff mov qword ptr [rbp - 0xd8], rdi
0x364939 4c8b6710 mov r12, qword ptr [rdi + 0x10]
0x36493d 41f6c401 test r12b, 1
0x364941 741b je 0x36495e
0x364943 4c89e7 mov rdi, r12
0x364946 4883e7fe and rdi, 0xfffffffffffffffe
0x36494a 0f842b010000 je 0x364a7b
0x364950 803d05a5030901 cmp byte ptr [rip + 0x903a505], 1
0x364957 7505 jne 0x36495e
0x364959 e872390000 call 0x3682d0
0x36495e 4983fc02 cmp r12, 2
0x364962 0f8213010000 jb 0x364a7b
0x364968 4c8d3dfde48507 lea r15, [rip + 0x785e4fd]
0x36496f 4c8dad40ffffff lea r13, [rbp - 0xc0]
0x364976 eb12 jmp 0x36498a
0x364978 0f1f840000000000 nop dword ptr [rax + rax]
0x364980 4983fc01 cmp r12, 1
0x364984 0f86f1000000 jbe 0x364a7b
0x36498a 41f6c401 test r12b, 1
0x36498e 0f85a5000000 jne 0x364a39
0x364994 4c89e7 mov rdi, r12
0x364997 4c89fe mov rsi, r15
0x36499a e87190e4ff call 0x1ada10
0x36499f 84c0 test al, al
0x3649a1 741d je 0x3649c0
0x3649a3 4d8b742420 mov r14, qword ptr [r12 + 0x20]
0x3649a8 8b45c8 mov eax, dword ptr [rbp - 0x38]
0x3649ab 3b45cc cmp eax, dword ptr [rbp - 0x34]
0x3649ae 741b je 0x3649cb
0x3649b0 eb2a jmp 0x3649dc
0x3649b2 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3649c0 4531f6 xor r14d, r14d
0x3649c3 8b45c8 mov eax, dword ptr [rbp - 0x38]
0x3649c6 3b45cc cmp eax, dword ptr [rbp - 0x34]
0x3649c9 7511 jne 0x3649dc
0x3649cb bf08040000 mov edi, 0x408
0x3649d0 4c89ee mov rsi, r13
0x3649d3 488d55cc lea rdx, [rbp - 0x34]
0x3649d7 e8c49ce0ff call 0x16e6a0
0x3649dc 8d4801 lea ecx, [rax + 1]
0x3649df 4898 cdqe 
0x3649e1 894dc8 mov dword ptr [rbp - 0x38], ecx
0x3649e4 488b4dc0 mov rcx, qword ptr [rbp - 0x40]
0x3649e8 4885c9 test rcx, rcx
0x3649eb 490f44cd cmove rcx, r13
0x3649ef 4c8934c1 mov qword ptr [rcx + rax*8], r14
0x3649f3 4d8b642410 mov r12, qword ptr [r12 + 0x10]
0x3649f8 41f6c401 test r12b, 1
0x3649fc 7482 je 0x364980
0x3649fe 4c89e7 mov rdi, r12
0x364a01 4883e7fe and rdi, 0xfffffffffffffffe
0x364a05 7474 je 0x364a7b
0x364a07 803d4ea4030901 cmp byte ptr [rip + 0x903a44e], 1
0x364a0e 7505 jne 0x364a15
0x364a10 e8bb380000 call 0x3682d0
0x364a15 4c89e7 mov rdi, r12
0x364a18 4883e7fe and rdi, 0xfffffffffffffffe
0x364a1c 0f845effffff je 0x364980
0x364a22 803d33a4030901 cmp byte ptr [rip + 0x903a433], 1
0x364a29 0f8551ffffff jne 0x364980
0x364a2f e89c380000 call 0x3682d0
0x364a34 e947ffffff jmp 0x364980
0x364a39 488bb530ffffff mov rsi, qword ptr [rbp - 0xd0]
0x364a40 4983e4fe and r12, 0xfffffffffffffffe
0x364a44 4889da mov rdx, rbx
0x364a47 4c89e7 mov rdi, r12
0x364a4a e8e1231200 call 0x486e30
0x364a4f 488b4308 mov rax, qword ptr [rbx + 8]
0x364a53 488d4802 lea rcx, [rax + 2]
0x364a57 483b4b10 cmp rcx, qword ptr [rbx + 0x10]
0x364a5b 7211 jb 0x364a6e
0x364a5d be01000000 mov esi, 1
0x364a62 4889df mov rdi, rbx
0x364a65 e896fce9ff call 0x204700
0x364a6a 488b4308 mov rax, qword ptr [rbx + 8]
0x364a6e 488d4802 lea rcx, [rax + 2]
0x364a72 48894b08 mov qword ptr [rbx + 8], rcx
0x364a76 66c7003a00 mov word ptr [rax], 0x3a
0x364a7b 4c6365c8 movsxd r12, dword ptr [rbp - 0x38]
0x364a7f 4d85e4 test r12, r12
0x364a82 0f8e81000000 jle 0x364b09
0x364a88 4e8d2ce5f8ffffff lea r13, [r12*8 - 8]
0x364a90 49ffc4 inc r12
0x364a93 4c8db540ffffff lea r14, [rbp - 0xc0]
0x364a9a 4c8d3d712b5907 lea r15, [rip + 0x7592b71]
0x364aa1 eb2f jmp 0x364ad2
0x364aa3 666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x364ab0 ba02000000 mov edx, 2
0x364ab5 4c89fe mov rsi, r15
0x364ab8 e87385d2ff call 0x8d030
0x364abd 4883c002 add rax, 2
0x364ac1 49ffcc dec r12
0x364ac4 4983c5f8 add r13, -8
0x364ac8 48894308 mov qword ptr [rbx + 8], rax
0x364acc 4983fc01 cmp r12, 1
0x364ad0 7637 jbe 0x364b09
0x364ad2 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0x364ad6 4889de mov rsi, rbx
0x364ad9 4885ff test rdi, rdi
0x364adc 490f44fe cmove rdi, r14
0x364ae0 4c01ef add rdi, r13
0x364ae3 e858abedff call 0x23f640
0x364ae8 488b7b08 mov rdi, qword ptr [rbx + 8]
0x364aec 488d4702 lea rax, [rdi + 2]
0x364af0 483b4310 cmp rax, qword ptr [rbx + 0x10]
0x364af4 72ba jb 0x364ab0
0x364af6 be01000000 mov esi, 1
0x364afb 4889df mov rdi, rbx
0x364afe e8fdfbe9ff call 0x204700
0x364b03 488b7b08 mov rdi, qword ptr [rbx + 8]
0x364b07 eba7 jmp 0x364ab0
0x364b09 4c8bb528ffffff mov r14, qword ptr [rbp - 0xd8]
0x364b10 488d3555e38507 lea rsi, [rip + 0x785e355]
0x364b17 4c89f7 mov rdi, r14
0x364b1a e8f18ee4ff call 0x1ada10
0x364b1f 84c0 test al, al
0x364b21 7406 je 0x364b29
0x364b23 498b4620 mov rax, qword ptr [r14 + 0x20]
0x364b27 eb02 jmp 0x364b2b
0x364b29 31c0 xor eax, eax
0x364b2b 4c8b35d6d3dc08 mov r14, qword ptr [rip + 0x8dcd3d6]
0x364b32 488dbd38ffffff lea rdi, [rbp - 0xc8]
0x364b39 4889de mov rsi, rbx
0x364b3c 48898538ffffff mov qword ptr [rbp - 0xc8], rax
0x364b43 e8f8aaedff call 0x23f640
0x364b48 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0x364b4c 4885ff test rdi, rdi
0x364b4f 7405 je 0x364b56
0x364b51 e83a2fd5ff call 0xb7a90
0x364b56 498b06 mov rax, qword ptr [r14]
0x364b59 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x364b5d 7512 jne 0x364b71
0x364b5f 4881c4b8000000 add rsp, 0xb8
0x364b66 5b pop rbx
0x364b67 415c pop r12
0x364b69 415d pop r13
0x364b6b 415e pop r14
0x364b6d 415f pop r15
0x364b6f 5d pop rbp
0x364b70 c3 ret 
0x364b71 e8ead21007 call 0x7471e60
0x364b76 0f0b ud2 
0x364b78 cc int3 
0x364b79 cc int3 
0x364b7a cc int3 
0x364b7b cc int3 
0x364b7c cc int3 
0x364b7d cc int3 
0x364b7e cc int3 
0x364b7f cc int3 
0x364b80 55 push rbp
0x364b81 4889e5 mov rbp, rsp
0x364b84 4157 push r15
0x364b86 4156 push r14
0x364b88 53 push rbx
0x364b89 50 push rax
0x364b8a 4c8b7e10 mov r15, qword ptr [rsi + 0x10]
0x364b8e 4989f6 mov r14, rsi
0x364b91 4889fb mov rbx, rdi
0x364b94 41f6c701 test r15b, 1
0x364b98 752a jne 0x364bc4
0x364b9a 660f1f440000 nop word ptr [rax + rax]
0x364ba0 4983ff02 cmp r15, 2
0x364ba4 7243 jb 0x364be9
0x364ba6 4d8b7f10 mov r15, qword ptr [r15 + 0x10]
0x364baa 41f6c701 test r15b, 1
0x364bae 74f0 je 0x364ba0
0x364bb0 4c89ff mov rdi, r15
0x364bb3 4883e7fe and rdi, 0xfffffffffffffffe
0x364bb7 7424 je 0x364bdd
0x364bb9 803d9ca2030901 cmp byte ptr [rip + 0x903a29c], 1
0x364bc0 7414 je 0x364bd6
0x364bc2 eb1f jmp 0x364be3
0x364bc4 4c89ff mov rdi, r15
0x364bc7 4883e7fe and rdi, 0xfffffffffffffffe
0x364bcb 7410 je 0x364bdd
0x364bcd 803d88a2030900 cmp byte ptr [rip + 0x903a288], 0
0x364bd4 740d je 0x364be3
0x364bd6 e8f5360000 call 0x3682d0
0x364bdb eb06 jmp 0x364be3
0x364bdd 41bf01000000 mov r15d, 1
0x364be3 4983e7fe and r15, 0xfffffffffffffffe
0x364be7 eb10 jmp 0x364bf9
0x364be9 4531ff xor r15d, r15d
0x364bec eb0b jmp 0x364bf9
0x364bee 6690 nop 
0x364bf0 4d8b7f20 mov r15, qword ptr [r15 + 0x20]
0x364bf4 4d85ff test r15, r15
0x364bf7 7439 je 0x364c32
0x364bf9 4d85ff test r15, r15
0x364bfc 74f2 je 0x364bf0
0x364bfe e8edbefbff call 0x320af0
0x364c03 498b4f10 mov rcx, qword ptr [r15 + 0x10]
0x364c07 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x364c0b 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x364c0e 7fe0 jg 0x364bf0
0x364c10 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x364c14 4883c030 add rax, 0x30
0x364c18 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x364c1c 75d2 jne 0x364bf0
0x364c1e 498b07 mov rax, qword ptr [r15]
0x364c21 4889df mov rdi, rbx
0x364c24 4c89fe mov rsi, r15
0x364c27 4c89f2 mov rdx, r14
0x364c2a ff9038030000 call qword ptr [rax + 0x338]
0x364c30 eb08 jmp 0x364c3a
0x364c32 c5f857c0 vxorps xmm0, xmm0, xmm0
0x364c36 c5f81103 vmovups xmmword ptr [rbx], xmm0
0x364c3a 4889d8 mov rax, rbx
0x364c3d 4883c408 add rsp, 8
0x364c41 5b pop rbx
0x364c42 415e pop r14
0x364c44 415f pop r15
0x364c46 5d pop rbp
0x364c47 c3 ret 
0x364c48 cc int3 
0x364c49 cc int3 
0x364c4a cc int3 
0x364c4b cc int3 
0x364c4c cc int3 
0x364c4d cc int3 
0x364c4e cc int3 
0x364c4f cc int3 
0x364c50 55 push rbp
0x364c51 4889e5 mov rbp, rsp
0x364c54 4157 push r15
0x364c56 4156 push r14
0x364c58 4154 push r12
0x364c5a 53 push rbx
0x364c5b 0fb6052ea00309 movzx eax, byte ptr [rip + 0x903a02e]
0x364c62 89cb mov ebx, ecx
0x364c64 4989d6 mov r14, rdx
0x364c67 4989f7 mov r15, rsi
0x364c6a 84c0 test al, al
0x364c6c 0f849a000000 je 0x364d0c
0x364c72 4c8b27 mov r12, qword ptr [rdi]
0x364c75 4489e7 mov edi, r12d
0x364c78 e8534eedff call 0x239ad0
0x364c7d 8b0dc59f0309 mov ecx, dword ptr [rip + 0x9039fc5]
0x364c83 3b0deb9f0309 cmp ecx, dword ptr [rip + 0x9039feb]
0x364c89 745d je 0x364ce8
0x364c8b 4c89e1 mov rcx, r12
0x364c8e 488d15e39f0309 lea rdx, [rip + 0x9039fe3]
0x364c95 48c1e920 shr rcx, 0x20
0x364c99 01c8 add eax, ecx
0x364c9b 488b0dde9f0309 mov rcx, qword ptr [rip + 0x9039fde]
0x364ca2 4885c9 test rcx, rcx
0x364ca5 480f45d1 cmovne rdx, rcx
0x364ca9 8b0dd99f0309 mov ecx, dword ptr [rip + 0x9039fd9]
0x364caf ffc9 dec ecx
0x364cb1 21c1 and ecx, eax
0x364cb3 8b048a mov eax, dword ptr [rdx + rcx*4]
0x364cb6 83f8ff cmp eax, -1
0x364cb9 742d je 0x364ce8
0x364cbb 488b0d7e9f0309 mov rcx, qword ptr [rip + 0x9039f7e]
0x364cc2 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x364cd0 4898 cdqe 
0x364cd2 488d1440 lea rdx, [rax + rax*2]
0x364cd6 488d04d1 lea rax, [rcx + rdx*8]
0x364cda 4c3924d1 cmp qword ptr [rcx + rdx*8], r12
0x364cde 740a je 0x364cea
0x364ce0 8b4010 mov eax, dword ptr [rax + 0x10]
0x364ce3 83f8ff cmp eax, -1
0x364ce6 75e8 jne 0x364cd0
0x364ce8 31c0 xor eax, eax
0x364cea 488d4808 lea rcx, [rax + 8]
0x364cee 4885c0 test rax, rax
0x364cf1 4c89ff mov rdi, r15
0x364cf4 4c89f6 mov rsi, r14
0x364cf7 89da mov edx, ebx
0x364cf9 480f44c8 cmove rcx, rax
0x364cfd 488b01 mov rax, qword ptr [rcx]
0x364d00 ff5030 call qword ptr [rax + 0x30]
0x364d03 5b pop rbx
0x364d04 415c pop r12
0x364d06 415e pop r14
0x364d08 415f pop r15
0x364d0a 5d pop rbp
0x364d0b c3 ret 
0x364d0c 488d057d9f0309 lea rax, [rip + 0x9039f7d]
0x364d13 4989fc mov r12, rdi
0x364d16 4889c7 mov rdi, rax
0x364d19 e892d11007 call 0x7471eb0
0x364d1e 4c89e7 mov rdi, r12
0x364d21 85c0 test eax, eax
0x364d23 0f8449ffffff je 0x364c72
0x364d29 c5f8100def7c6d07 vmovups xmm1, xmmword ptr [rip + 0x76d7cef]
0x364d31 488d35089f0309 lea rsi, [rip + 0x9039f08]
0x364d38 488d3dc130cbff lea rdi, [rip - 0x34cf3f]
0x364d3f 488d15ba72f708 lea rdx, [rip + 0x8f772ba]
0x364d46 48c7050f9f030900000000 mov qword ptr [rip + 0x9039f0f], 0
0x364d51 c5f857c0 vxorps xmm0, xmm0, xmm0
0x364d55 c5f81105e39e0309 vmovups xmmword ptr [rip + 0x9039ee3], xmm0
0x364d5d c5f8110d039f0309 vmovups xmmword ptr [rip + 0x9039f03], xmm1
0x364d65 48c705109f030900000000 mov qword ptr [rip + 0x9039f10], 0
0x364d70 c7050e9f030900000000 mov dword ptr [rip + 0x9039f0e], 0
0x364d7a e851d11007 call 0x7471ed0
0x364d7f 488d3d0a9f0309 lea rdi, [rip + 0x9039f0a]
0x364d86 e835d11007 call 0x7471ec0
0x364d8b 4c89e7 mov rdi, r12
0x364d8e e9dffeffff jmp 0x364c72
0x364d93 cc int3 
0x364d94 cc int3 
0x364d95 cc int3 
0x364d96 cc int3 
0x364d97 cc int3 
0x364d98 cc int3 
0x364d99 cc int3 
0x364d9a cc int3 
0x364d9b cc int3 
0x364d9c cc int3 
0x364d9d cc int3 
0x364d9e cc int3 
0x364d9f cc int3 
0x364da0 55 push rbp
0x364da1 4889e5 mov rbp, rsp
0x364da4 53 push rbx
0x364da5 50 push rax
0x364da6 4889fb mov rbx, rdi
0x364da9 4885f6 test rsi, rsi
0x364dac 740a je 0x364db8
0x364dae 4889df mov rdi, rbx
0x364db1 e85af2ffff call 0x364010
0x364db6 eb0f jmp 0x364dc7
0x364db8 488d35c1c45b07 lea rsi, [rip + 0x75bc4c1]
0x364dbf 4889df mov rdi, rbx
0x364dc2 e829f1cfff call 0x63ef0
0x364dc7 4889d8 mov rax, rbx
0x364dca 4883c408 add rsp, 8
0x364dce 5b pop rbx
0x364dcf 5d pop rbp
0x364dd0 c3 ret 
0x364dd1 cc int3 
0x364dd2 cc int3 
0x364dd3 cc int3 
0x364dd4 cc int3 
0x364dd5 cc int3 
0x364dd6 cc int3 
0x364dd7 cc int3 
0x364dd8 cc int3 
0x364dd9 cc int3 
0x364dda cc int3 
0x364ddb cc int3 
0x364ddc cc int3 
0x364ddd cc int3 
0x364dde cc int3 
0x364ddf cc int3 
0x364de0 55 push rbp
0x364de1 4889e5 mov rbp, rsp
0x364de4 4157 push r15
0x364de6 4156 push r14
0x364de8 4155 push r13
0x364dea 4154 push r12
0x364dec 53 push rbx
0x364ded 4883ec28 sub rsp, 0x28
0x364df1 4c8b3d10d1dc08 mov r15, qword ptr [rip + 0x8dcd110]
0x364df8 c5f857c0 vxorps xmm0, xmm0, xmm0
0x364dfc 4889f3 mov rbx, rsi
0x364dff 4989fe mov r14, rdi
0x364e02 4c8d671c lea r12, [rdi + 0x1c]
0x364e06 498b07 mov rax, qword ptr [r15]
0x364e09 488945d0 mov qword ptr [rbp - 0x30], rax
0x364e0d c7471800000000 mov dword ptr [rdi + 0x18], 0
0x364e14 c5f81107 vmovups xmmword ptr [rdi], xmm0
0x364e18 837f1c00 cmp dword ptr [rdi + 0x1c], 0
0x364e1c 741b je 0x364e39
0x364e1e 498d4e10 lea rcx, [r14 + 0x10]
0x364e22 bf08000000 mov edi, 8
0x364e27 be04000000 mov esi, 4
0x364e2c 4d89e1 mov r9, r12
0x364e2f 31d2 xor edx, edx
0x364e31 4531c0 xor r8d, r8d
0x364e34 e8e70fccff call 0x25e20
0x364e39 4885db test rbx, rbx
0x364e3c 0f8459020000 je 0x36509b
0x364e42 4c8965b8 mov qword ptr [rbp - 0x48], r12
0x364e46 4c8b6310 mov r12, qword ptr [rbx + 0x10]
0x364e4a 41f6c401 test r12b, 1
0x364e4e 7525 jne 0x364e75
0x364e50 4983fc02 cmp r12, 2
0x364e54 7244 jb 0x364e9a
0x364e56 4d8b642410 mov r12, qword ptr [r12 + 0x10]
0x364e5b 41f6c401 test r12b, 1
0x364e5f 74ef je 0x364e50
0x364e61 4c89e7 mov rdi, r12
0x364e64 4883e7fe and rdi, 0xfffffffffffffffe
0x364e68 7424 je 0x364e8e
0x364e6a 803deb9f030901 cmp byte ptr [rip + 0x9039feb], 1
0x364e71 7414 je 0x364e87
0x364e73 eb1f jmp 0x364e94
0x364e75 4c89e7 mov rdi, r12
0x364e78 4883e7fe and rdi, 0xfffffffffffffffe
0x364e7c 7410 je 0x364e8e
0x364e7e 803dd79f030900 cmp byte ptr [rip + 0x9039fd7], 0
0x364e85 740d je 0x364e94
0x364e87 e844340000 call 0x3682d0
0x364e8c eb06 jmp 0x364e94
0x364e8e 41bc01000000 mov r12d, 1
0x364e94 4983e4fe and r12, 0xfffffffffffffffe
0x364e98 eb10 jmp 0x364eaa
0x364e9a 4531e4 xor r12d, r12d
0x364e9d eb0b jmp 0x364eaa
0x364e9f 90 nop 
0x364ea0 4d8b642420 mov r12, qword ptr [r12 + 0x20]
0x364ea5 4d85e4 test r12, r12
0x364ea8 7435 je 0x364edf
0x364eaa 4d85e4 test r12, r12
0x364ead 74f1 je 0x364ea0
0x364eaf e83cbcfbff call 0x320af0
0x364eb4 498b4c2410 mov rcx, qword ptr [r12 + 0x10]
0x364eb9 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x364ebd 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x364ec0 7fde jg 0x364ea0
0x364ec2 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x364ec6 4883c030 add rax, 0x30
0x364eca 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x364ece 75d0 jne 0x364ea0
0x364ed0 48895dc0 mov qword ptr [rbp - 0x40], rbx
0x364ed4 4883fb02 cmp rbx, 2
0x364ed8 7316 jae 0x364ef0
0x364eda e981010000 jmp 0x365060
0x364edf 4531e4 xor r12d, r12d
0x364ee2 48895dc0 mov qword ptr [rbp - 0x40], rbx
0x364ee6 4883fb02 cmp rbx, 2
0x364eea 0f8270010000 jb 0x365060
0x364ef0 488b5dc0 mov rbx, qword ptr [rbp - 0x40]
0x364ef4 4d8d6e10 lea r13, [r14 + 0x10]
0x364ef8 eb10 jmp 0x364f0a
0x364efa 660f1f440000 nop word ptr [rax + rax]
0x364f00 4883fb01 cmp rbx, 1
0x364f04 0f8656010000 jbe 0x365060
0x364f0a e8e1bbfbff call 0x320af0
0x364f0f f6c301 test bl, 1
0x364f12 753c jne 0x364f50
0x364f14 4d85e4 test r12, r12
0x364f17 0f845f010000 je 0x36507c
0x364f1d 4889df mov rdi, rbx
0x364f20 488d3545df8507 lea rsi, [rip + 0x785df45]
0x364f27 e8e48ae4ff call 0x1ada10
0x364f2c 84c0 test al, al
0x364f2e 7450 je 0x364f80
0x364f30 488d4b20 lea rcx, [rbx + 0x20]
0x364f34 4c8b39 mov r15, qword ptr [rcx]
0x364f37 418b4618 mov eax, dword ptr [r14 + 0x18]
0x364f3b 413b461c cmp eax, dword ptr [r14 + 0x1c]
0x364f3f 0f858a000000 jne 0x364fcf
0x364f45 eb77 jmp 0x364fbe
0x364f47 660f1f840000000000 nop word ptr [rax + rax]
0x364f50 4889d9 mov rcx, rbx
0x364f53 4883e1fe and rcx, 0xfffffffffffffffe
0x364f57 740d je 0x364f66
0x364f59 488b5110 mov rdx, qword ptr [rcx + 0x10]
0x364f5d 48637038 movsxd rsi, dword ptr [rax + 0x38]
0x364f61 3b7238 cmp esi, dword ptr [rdx + 0x38]
0x364f64 7e29 jle 0x364f8f
0x364f66 31c0 xor eax, eax
0x364f68 4c39e0 cmp rax, r12
0x364f6b 7540 jne 0x364fad
0x364f6d e9ee000000 jmp 0x365060
0x364f72 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x364f80 4531ff xor r15d, r15d
0x364f83 418b4618 mov eax, dword ptr [r14 + 0x18]
0x364f87 413b461c cmp eax, dword ptr [r14 + 0x1c]
0x364f8b 7431 je 0x364fbe
0x364f8d eb40 jmp 0x364fcf
0x364f8f 488b5230 mov rdx, qword ptr [rdx + 0x30]
0x364f93 4883c030 add rax, 0x30
0x364f97 483904f2 cmp qword ptr [rdx + rsi*8], rax
0x364f9b b800000000 mov eax, 0
0x364fa0 480f44c1 cmove rax, rcx
0x364fa4 4c39e0 cmp rax, r12
0x364fa7 0f84b3000000 je 0x365060
0x364fad 4883c118 add rcx, 0x18
0x364fb1 4c8b39 mov r15, qword ptr [rcx]
0x364fb4 418b4618 mov eax, dword ptr [r14 + 0x18]
0x364fb8 413b461c cmp eax, dword ptr [r14 + 0x1c]
0x364fbc 7511 jne 0x364fcf
0x364fbe 488b55b8 mov rdx, qword ptr [rbp - 0x48]
0x364fc2 bf08040000 mov edi, 0x408
0x364fc7 4c89ee mov rsi, r13
0x364fca e8d148ccff call 0x298a0
0x364fcf 8d4801 lea ecx, [rax + 1]
0x364fd2 4898 cdqe 
0x364fd4 41894e18 mov dword ptr [r14 + 0x18], ecx
0x364fd8 498b4e10 mov rcx, qword ptr [r14 + 0x10]
0x364fdc 4c893cc1 mov qword ptr [rcx + rax*8], r15
0x364fe0 f6c301 test bl, 1
0x364fe3 752b jne 0x365010
0x364fe5 488b5b10 mov rbx, qword ptr [rbx + 0x10]
0x364fe9 f6c301 test bl, 1
0x364fec 7445 je 0x365033
0x364fee 4889df mov rdi, rbx
0x364ff1 4883e7fe and rdi, 0xfffffffffffffffe
0x364ff5 743c je 0x365033
0x364ff7 803d5e9e030901 cmp byte ptr [rip + 0x9039e5e], 1
0x364ffe 742e je 0x36502e
0x365000 eb31 jmp 0x365033
0x365002 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x365010 4883e3fe and rbx, 0xfffffffffffffffe
0x365014 488b7b20 mov rdi, qword ptr [rbx + 0x20]
0x365018 4889fb mov rbx, rdi
0x36501b 4883cb01 or rbx, 1
0x36501f 4883e7fe and rdi, 0xfffffffffffffffe
0x365023 740e je 0x365033
0x365025 803d309e030900 cmp byte ptr [rip + 0x9039e30], 0
0x36502c 7405 je 0x365033
0x36502e e89d320000 call 0x3682d0
0x365033 f6c301 test bl, 1
0x365036 0f84c4feffff je 0x364f00
0x36503c 4889df mov rdi, rbx
0x36503f 4883e7fe and rdi, 0xfffffffffffffffe
0x365043 0f84b7feffff je 0x364f00
0x365049 803d0c9e030901 cmp byte ptr [rip + 0x9039e0c], 1
0x365050 0f85aafeffff jne 0x364f00
0x365056 e875320000 call 0x3682d0
0x36505b e9a0feffff jmp 0x364f00
0x365060 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x365064 4d85e4 test r12, r12
0x365067 7417 je 0x365080
0x365069 803dec9d030900 cmp byte ptr [rip + 0x9039dec], 0
0x365070 740e je 0x365080
0x365072 4c89e7 mov rdi, r12
0x365075 e856320000 call 0x3682d0
0x36507a eb04 jmp 0x365080
0x36507c 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x365080 498d7e08 lea rdi, [r14 + 8]
0x365084 488d75c8 lea rsi, [rbp - 0x38]
0x365088 e8f31a1500 call 0x4b6b80
0x36508d 488b45c0 mov rax, qword ptr [rbp - 0x40]
0x365091 4c8b3d70cedc08 mov r15, qword ptr [rip + 0x8dcce70]
0x365098 498906 mov qword ptr [r14], rax
0x36509b 498b07 mov rax, qword ptr [r15]
0x36509e 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x3650a2 750f jne 0x3650b3
0x3650a4 4883c428 add rsp, 0x28
0x3650a8 5b pop rbx
0x3650a9 415c pop r12
0x3650ab 415d pop r13
0x3650ad 415e pop r14
0x3650af 415f pop r15
0x3650b1 5d pop rbp
0x3650b2 c3 ret 
0x3650b3 e8a8cd1007 call 0x7471e60
0x3650b8 0f0b ud2 
0x3650ba cc int3 
0x3650bb cc int3 
0x3650bc cc int3 
0x3650bd cc int3 
0x3650be cc int3 
0x3650bf cc int3 
0x3650c0 55 push rbp
0x3650c1 4889e5 mov rbp, rsp
0x3650c4 4157 push r15
0x3650c6 4156 push r14
0x3650c8 4155 push r13
0x3650ca 4154 push r12
0x3650cc 53 push rbx
0x3650cd 4881ec28080000 sub rsp, 0x828
0x3650d4 488b052dcedc08 mov rax, qword ptr [rip + 0x8dcce2d]
0x3650db c5f857c0 vxorps xmm0, xmm0, xmm0
0x3650df 4989f4 mov r12, rsi
0x3650e2 4889fb mov rbx, rdi
0x3650e5 488b00 mov rax, qword ptr [rax]
0x3650e8 488945d0 mov qword ptr [rbp - 0x30], rax
0x3650ec c7471800000000 mov dword ptr [rdi + 0x18], 0
0x3650f3 c5f81107 vmovups xmmword ptr [rdi], xmm0
0x3650f7 837f1c00 cmp dword ptr [rdi + 0x1c], 0
0x3650fb 7420 je 0x36511d
0x3650fd 4c8d4b1c lea r9, [rbx + 0x1c]
0x365101 488d4b10 lea rcx, [rbx + 0x10]
0x365105 bf08000000 mov edi, 8
0x36510a be04000000 mov esi, 4
0x36510f 31d2 xor edx, edx
0x365111 4531c0 xor r8d, r8d
0x365114 e8070dccff call 0x25e20
0x365119 c5f857c0 vxorps xmm0, xmm0, xmm0
0x36511d 48899db0f7ffff mov qword ptr [rbp - 0x850], rbx
0x365124 4c8dbdccf7ffff lea r15, [rbp - 0x834]
0x36512b 49be0100000000400004 movabs r14, 0x400400000000001
0x365135 4c8dadb8f7ffff lea r13, [rbp - 0x848]
0x36513c 488d9dd0f7ffff lea rbx, [rbp - 0x830]
0x365143 31c0 xor eax, eax
0x365145 c5f81185c0f7ffff vmovups xmmword ptr [rbp - 0x840], xmm0
0x36514d eb2e jmp 0x36517d
0x36514f 90 nop 
0x365150 8d4801 lea ecx, [rax + 1]
0x365153 488b95b8f7ffff mov rdx, qword ptr [rbp - 0x848]
0x36515a 4898 cdqe 
0x36515c 898dc8f7ffff mov dword ptr [rbp - 0x838], ecx
0x365162 488b8dc0f7ffff mov rcx, qword ptr [rbp - 0x840]
0x365169 488914c1 mov qword ptr [rcx + rax*8], rdx
0x36516d 31c0 xor eax, eax
0x36516f 410fb70c24 movzx ecx, word ptr [r12]
0x365174 6685c9 test cx, cx
0x365177 7474 je 0x3651ed
0x365179 4983c402 add r12, 2
0x36517d 410fb70c24 movzx ecx, word ptr [r12]
0x365182 4863d0 movsxd rdx, eax
0x365185 4883f93a cmp rcx, 0x3a
0x365189 7755 ja 0x3651e0
0x36518b 490fa3ce bt r14, rcx
0x36518f 734f jae 0x3651e0
0x365191 66c78455d0f7ffff0000 mov word ptr [rbp + rdx*2 - 0x830], 0
0x36519b 85d2 test edx, edx
0x36519d 7ed5 jle 0x365174
0x36519f 4c89ef mov rdi, r13
0x3651a2 4889de mov rsi, rbx
0x3651a5 ba01000000 mov edx, 1
0x3651aa e8e183edff call 0x23d590
0x3651af 8b85c8f7ffff mov eax, dword ptr [rbp - 0x838]
0x3651b5 3b85ccf7ffff cmp eax, dword ptr [rbp - 0x834]
0x3651bb 7593 jne 0x365150
0x3651bd bf08040000 mov edi, 0x408
0x3651c2 488db5c0f7ffff lea rsi, [rbp - 0x840]
0x3651c9 4c89fa mov rdx, r15
0x3651cc e8cf46ccff call 0x298a0
0x3651d1 e97affffff jmp 0x365150
0x3651d6 662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3651e0 8d4201 lea eax, [rdx + 1]
0x3651e3 66898c55d0f7ffff mov word ptr [rbp + rdx*2 - 0x830], cx
0x3651eb eb8c jmp 0x365179
0x3651ed 486395c8f7ffff movsxd rdx, dword ptr [rbp - 0x838]
0x3651f4 4883fa02 cmp rdx, 2
0x3651f8 7c42 jl 0x36523c
0x3651fa 488b85c0f7ffff mov rax, qword ptr [rbp - 0x840]
0x365201 4c8bb5b0f7ffff mov r14, qword ptr [rbp - 0x850]
0x365208 89d1 mov ecx, edx
0x36520a 31f6 xor esi, esi
0x36520c d1e9 shr ecx, 1
0x36520e 48f7d9 neg rcx
0x365211 488d54d0f8 lea rdx, [rax + rdx*8 - 8]
0x365216 662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x365220 488b38 mov rdi, qword ptr [rax]
0x365223 4c8b04f2 mov r8, qword ptr [rdx + rsi*8]
0x365227 4c8900 mov qword ptr [rax], r8
0x36522a 48893cf2 mov qword ptr [rdx + rsi*8], rdi
0x36522e 4883c008 add rax, 8
0x365232 48ffce dec rsi
0x365235 4839f1 cmp rcx, rsi
0x365238 75e6 jne 0x365220
0x36523a eb4a jmp 0x365286
0x36523c 4c8bb5b0f7ffff mov r14, qword ptr [rbp - 0x850]
0x365243 83fa01 cmp edx, 1
0x365246 753e jne 0x365286
0x365248 488b85c0f7ffff mov rax, qword ptr [rbp - 0x840]
0x36524f 48833800 cmp qword ptr [rax], 0
0x365253 7531 jne 0x365286
0x365255 c785c8f7ffff00000000 mov dword ptr [rbp - 0x838], 0
0x36525f 83bdccf7ffff00 cmp dword ptr [rbp - 0x834], 0
0x365266 741e je 0x365286
0x365268 488d8dc0f7ffff lea rcx, [rbp - 0x840]
0x36526f bf08000000 mov edi, 8
0x365274 be04000000 mov esi, 4
0x365279 4d89f9 mov r9, r15
0x36527c 31d2 xor edx, edx
0x36527e 4531c0 xor r8d, r8d
0x365281 e89a0bccff call 0x25e20
0x365286 4d8d7e10 lea r15, [r14 + 0x10]
0x36528a 488d85c0f7ffff lea rax, [rbp - 0x840]
0x365291 4939c7 cmp r15, rax
0x365294 7439 je 0x3652cf
0x365296 498b3f mov rdi, qword ptr [r15]
0x365299 4885ff test rdi, rdi
0x36529c 7405 je 0x3652a3
0x36529e e8ed27d5ff call 0xb7a90
0x3652a3 488b85c0f7ffff mov rax, qword ptr [rbp - 0x840]
0x3652aa 49894610 mov qword ptr [r14 + 0x10], rax
0x3652ae 48c785c0f7ffff00000000 mov qword ptr [rbp - 0x840], 0
0x3652b9 488b85c8f7ffff mov rax, qword ptr [rbp - 0x838]
0x3652c0 49894618 mov qword ptr [r14 + 0x18], rax
0x3652c4 48c785c8f7ffff00000000 mov qword ptr [rbp - 0x838], 0
0x3652cf 0fb605029a0309 movzx eax, byte ptr [rip + 0x9039a02]
0x3652d6 84c0 test al, al
0x3652d8 0f841c010000 je 0x3653fa
0x3652de 498d7e08 lea rdi, [r14 + 8]
0x3652e2 e8191a1500 call 0x4b6d00
0x3652e7 4885c0 test rax, rax
0x3652ea 7515 jne 0x365301
0x3652ec 4c89f7 mov rdi, r14
0x3652ef 31f6 xor esi, esi
0x3652f1 31d2 xor edx, edx
0x3652f3 e878010000 call 0x365470
0x3652f8 4885c0 test rax, rax
0x3652fb 0f84b7000000 je 0x3653b8
0x365301 49635e18 movsxd rbx, dword ptr [r14 + 0x18]
0x365305 4885db test rbx, rbx
0x365308 0f8eaa000000 jle 0x3653b8
0x36530e 498b0f mov rcx, qword ptr [r15]
0x365311 4889c7 mov rdi, rax
0x365314 ba03000000 mov edx, 3
0x365319 488b74d9f8 mov rsi, qword ptr [rcx + rbx*8 - 8]
0x36531e e87d070000 call 0x365aa0
0x365323 83fb01 cmp ebx, 1
0x365326 7435 je 0x36535d
0x365328 4885c0 test rax, rax
0x36532b 7430 je 0x36535d
0x36532d 83c3fe add ebx, -2
0x365330 48c1e303 shl rbx, 3
0x365334 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x365340 498b37 mov rsi, qword ptr [r15]
0x365343 488b08 mov rcx, qword ptr [rax]
0x365346 4889c7 mov rdi, rax
0x365349 4801de add rsi, rbx
0x36534c ff5158 call qword ptr [rcx + 0x58]
0x36534f 4885db test rbx, rbx
0x365352 7409 je 0x36535d
0x365354 4883c3f8 add rbx, -8
0x365358 4885c0 test rax, rax
0x36535b 75e3 jne 0x365340
0x36535d 4885c0 test rax, rax
0x365360 7456 je 0x3653b8
0x365362 488b4808 mov rcx, qword ptr [rax + 8]
0x365366 488b1533990309 mov rdx, qword ptr [rip + 0x9039933]
0x36536d 4885d2 test rdx, rdx
0x365370 740b je 0x36537d
0x365372 48855110 test qword ptr [rcx + 0x10], rdx
0x365376 7440 je 0x3653b8
0x365378 498906 mov qword ptr [r14], rax
0x36537b eb4a jmp 0x3653c7
0x36537d 4885c9 test rcx, rcx
0x365380 488d3511990309 lea rsi, [rip + 0x9039911]
0x365387 0f95c2 setne dl
0x36538a 400f94c7 sete dil
0x36538e 4839f1 cmp rcx, rsi
0x365391 410f94c0 sete r8b
0x365395 4108f8 or r8b, dil
0x365398 751a jne 0x3653b4
0x36539a 660f1f440000 nop word ptr [rax + rax]
0x3653a0 488b4920 mov rcx, qword ptr [rcx + 0x20]
0x3653a4 4885c9 test rcx, rcx
0x3653a7 0f95c2 setne dl
0x3653aa 4839f1 cmp rcx, rsi
0x3653ad 7405 je 0x3653b4
0x3653af 4885c9 test rcx, rcx
0x3653b2 75ec jne 0x3653a0
0x3653b4 84d2 test dl, dl
0x3653b6 75c0 jne 0x365378
0x3653b8 49833e00 cmp qword ptr [r14], 0
0x3653bc 7409 je 0x3653c7
0x3653be c5f857c0 vxorps xmm0, xmm0, xmm0
0x3653c2 c4c1781106 vmovups xmmword ptr [r14], xmm0
0x3653c7 488bbdc0f7ffff mov rdi, qword ptr [rbp - 0x840]
0x3653ce 4885ff test rdi, rdi
0x3653d1 7405 je 0x3653d8
0x3653d3 e8b826d5ff call 0xb7a90
0x3653d8 488b0529cbdc08 mov rax, qword ptr [rip + 0x8dccb29]
0x3653df 488b00 mov rax, qword ptr [rax]
0x3653e2 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x3653e6 7577 jne 0x36545f
0x3653e8 4881c428080000 add rsp, 0x828
0x3653ef 5b pop rbx
0x3653f0 415c pop r12
0x3653f2 415d pop r13
0x3653f4 415e pop r14
0x3653f6 415f pop r15
0x3653f8 5d pop rbp
0x3653f9 c3 ret 
0x3653fa 48 .byte 0x48
0x3653fb 8d .byte 0x8d
0x3653fc 3d .byte 0x3d
0x3653fd d7 xlatb 
0x3653fe 98 cwde 
0x3653ff 03 .byte 0x03

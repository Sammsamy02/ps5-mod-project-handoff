0x3254c0 f0x3294c0 55                             push rbp
0x3254c1 f0x3294c1 4889e5                         mov rbp, rsp
0x3254c4 f0x3294c4 4157                           push r15
0x3254c6 f0x3294c6 4156                           push r14
0x3254c8 f0x3294c8 4155                           push r13
0x3254ca f0x3294ca 4154                           push r12
0x3254cc f0x3294cc 53                             push rbx
0x3254cd f0x3294cd 4883ec38                       sub rsp, 0x38
0x3254d1 f0x3294d1 488b0530cae008                 mov rax, qword ptr [rip + 0x8e0ca30] rip->0x9131f08
0x3254d8 f0x3294d8 4989fc                         mov r12, rdi
0x3254db f0x3294db 488b00                         mov rax, qword ptr [rax]
0x3254de f0x3294de 488945d0                       mov qword ptr [rbp - 0x30], rax
0x3254e2 f0x3294e2 488975b8                       mov qword ptr [rbp - 0x48], rsi
0x3254e6 f0x3294e6 48c745b000000000               mov qword ptr [rbp - 0x50], 0
0x3254ee f0x3294ee 85d2                           test edx, edx
0x3254f0 f0x3294f0 741d                           je 0x32550f
0x3254f2 f0x3294f2 4d8b6c2440                     mov r13, qword ptr [r12 + 0x40]
0x3254f7 f0x3294f7 4d85ed                         test r13, r13
0x3254fa f0x3294fa 0f85c2000000                   jne 0x3255c2
0x325500 f0x329500 4183bc24e001000000             cmp dword ptr [r12 + 0x1e0], 0
0x325509 f0x329509 0f85b3000000                   jne 0x3255c2
0x32550f f0x32950f b101                           mov cl, 1
0x325511 f0x329511 31c0                           xor eax, eax
0x325513 f0x329513 498d9c2478010000               lea rbx, [r12 + 0x178]
0x32551b f0x32951b f0410fb08c2478010000           lock cmpxchg byte ptr [r12 + 0x178], cl
0x325525 f0x329525 0f85e7020000                   jne 0x325812
0x32552b f0x32952b 4c8b7db8                       mov r15, qword ptr [rbp - 0x48]
0x32552f f0x32952f 4489ff                         mov edi, r15d
0x325532 f0x329532 e89945f1ff                     call 0x239ad0
0x325537 f0x329537 418b8c2430010000               mov ecx, dword ptr [r12 + 0x130]
0x32553f f0x32953f 413b8c245c010000               cmp ecx, dword ptr [r12 + 0x15c]
0x325547 f0x329547 750c                           jne 0x325555
0x325549 f0x329549 4c8b2db8c9e008                 mov r13, qword ptr [rip + 0x8e0c9b8] rip->0x9131f08
0x325550 f0x329550 e93a010000                     jmp 0x32568f
0x325555 f0x329555 498b942468010000               mov rdx, qword ptr [r12 + 0x168]
0x32555d f0x32955d 4c89f9                         mov rcx, r15
0x325560 f0x329560 4c8b2da1c9e008                 mov r13, qword ptr [rip + 0x8e0c9a1] rip->0x9131f08
0x325567 f0x329567 48c1e920                       shr rcx, 0x20
0x32556b f0x32956b 01c8                           add eax, ecx
0x32556d f0x32956d 498d8c2460010000               lea rcx, [r12 + 0x160]
0x325575 f0x329575 4885d2                         test rdx, rdx
0x325578 f0x329578 480f45ca                       cmovne rcx, rdx
0x32557c f0x32957c 418b942470010000               mov edx, dword ptr [r12 + 0x170]
0x325584 f0x329584 ffca                           dec edx
0x325586 f0x329586 21c2                           and edx, eax
0x325588 f0x329588 8b0c91                         mov ecx, dword ptr [rcx + rdx*4]
0x32558b f0x32958b 83f9ff                         cmp ecx, -1
0x32558e f0x32958e 0f84fb000000                   je 0x32568f
0x325594 f0x329594 498b842428010000               mov rax, qword ptr [r12 + 0x128]
0x32559c f0x32959c 4531f6                         xor r14d, r14d
0x32559f f0x32959f 90                             nop 
0x3255a0 f0x3295a0 4863c9                         movsxd rcx, ecx
0x3255a3 f0x3295a3 488d1449                       lea rdx, [rcx + rcx*2]
0x3255a7 f0x3295a7 488d0cd0                       lea rcx, [rax + rdx*8]
0x3255ab f0x3295ab 4c393cd0                       cmp qword ptr [rax + rdx*8], r15
0x3255af f0x3295af 0f84b6000000                   je 0x32566b
0x3255b5 f0x3295b5 8b4910                         mov ecx, dword ptr [rcx + 0x10]
0x3255b8 f0x3295b8 83f9ff                         cmp ecx, -1
0x3255bb f0x3295bb 75e3                           jne 0x3255a0
0x3255bd f0x3295bd e9d0000000                     jmp 0x325692
0x3255c2 f0x3295c2 b101                           mov cl, 1
0x3255c4 f0x3295c4 31c0                           xor eax, eax
0x3255c6 f0x3295c6 4d8db424d0010000               lea r14, [r12 + 0x1d0]
0x3255ce f0x3295ce f0410fb08c24d0010000           lock cmpxchg byte ptr [r12 + 0x1d0], cl
0x3255d8 f0x3295d8 0f8541020000                   jne 0x32581f
0x3255de f0x3295de 488b5db8                       mov rbx, qword ptr [rbp - 0x48]
0x3255e2 f0x3295e2 498d842480010000               lea rax, [r12 + 0x180]
0x3255ea f0x3295ea 488945a0                       mov qword ptr [rbp - 0x60], rax
0x3255ee f0x3295ee 89df                           mov edi, ebx
0x3255f0 f0x3295f0 e8db44f1ff                     call 0x239ad0
0x3255f5 f0x3295f5 418b8c2488010000               mov ecx, dword ptr [r12 + 0x188]
0x3255fd f0x3295fd 413b8c24b4010000               cmp ecx, dword ptr [r12 + 0x1b4]
0x325605 f0x329605 0f84b8000000                   je 0x3256c3
0x32560b f0x32960b 498b9424c0010000               mov rdx, qword ptr [r12 + 0x1c0]
0x325613 f0x329613 4889d9                         mov rcx, rbx
0x325616 f0x329616 48c1e920                       shr rcx, 0x20
0x32561a f0x32961a 01c8                           add eax, ecx
0x32561c f0x32961c 498d8c24b8010000               lea rcx, [r12 + 0x1b8]
0x325624 f0x329624 4885d2                         test rdx, rdx
0x325627 f0x329627 480f45ca                       cmovne rcx, rdx
0x32562b f0x32962b 418b9424c8010000               mov edx, dword ptr [r12 + 0x1c8]
0x325633 f0x329633 ffca                           dec edx
0x325635 f0x329635 21c2                           and edx, eax
0x325637 f0x329637 8b0c91                         mov ecx, dword ptr [rcx + rdx*4]
0x32563a f0x32963a 83f9ff                         cmp ecx, -1
0x32563d f0x32963d 0f8480000000                   je 0x3256c3
0x325643 f0x329643 488b45a0                       mov rax, qword ptr [rbp - 0x60]
0x325647 f0x329647 488b00                         mov rax, qword ptr [rax]
0x32564a f0x32964a 660f1f440000                   nop word ptr [rax + rax]
0x325650 f0x329650 4863c9                         movsxd rcx, ecx
0x325653 f0x329653 488d1449                       lea rdx, [rcx + rcx*2]
0x325657 f0x329657 488d0cd0                       lea rcx, [rax + rdx*8]
0x32565b f0x32965b 48391cd0                       cmp qword ptr [rax + rdx*8], rbx
0x32565f f0x32965f 7450                           je 0x3256b1
0x325661 f0x329661 8b4910                         mov ecx, dword ptr [rcx + 0x10]
0x325664 f0x329664 83f9ff                         cmp ecx, -1
0x325667 f0x329667 75e7                           jne 0x325650
0x325669 f0x329669 eb58                           jmp 0x3256c3
0x32566b f0x32966b 4885c9                         test rcx, rcx
0x32566e f0x32966e 741f                           je 0x32568f
0x325670 f0x329670 488d05e5970709                 lea rax, [rip + 0x90797e5] rip->0x939ee5c
0x325677 f0x329677 4c8b7108                       mov r14, qword ptr [rcx + 8]
0x32567b f0x32967b 803801                         cmp byte ptr [rax], 1
0x32567e f0x32967e 7512                           jne 0x325692
0x325680 f0x329680 4d85f6                         test r14, r14
0x325683 f0x329683 740d                           je 0x325692
0x325685 f0x329685 4c89f7                         mov rdi, r14
0x325688 f0x329688 e8332c0400                     call 0x3682c0
0x32568d f0x32968d eb03                           jmp 0x325692
0x32568f f0x32968f 4531f6                         xor r14d, r14d
0x325692 f0x329692 4c8975b0                       mov qword ptr [rbp - 0x50], r14
0x325696 f0x329696 b0ff                           mov al, 0xff
0x325698 f0x329698 f00fc003                       lock xadd byte ptr [rbx], al
0x32569c f0x32969c a802                           test al, 2
0x32569e f0x32969e 0f849d030000                   je 0x325a41
0x3256a4 f0x3296a4 4889df                         mov rdi, rbx
0x3256a7 f0x3296a7 e834a1d1ff                     call 0x3f7e0
0x3256ac f0x3296ac e990030000                     jmp 0x325a41
0x3256b1 f0x3296b1 4885c9                         test rcx, rcx
0x3256b4 f0x3296b4 740d                           je 0x3256c3
0x3256b6 f0x3296b6 488b4108                       mov rax, qword ptr [rcx + 8]
0x3256ba f0x3296ba 488945b0                       mov qword ptr [rbp - 0x50], rax
0x3256be f0x3296be e9fe000000                     jmp 0x3257c1
0x3256c3 f0x3296c3 b0ff                           mov al, 0xff
0x3256c5 f0x3296c5 f0410fc006                     lock xadd byte ptr [r14], al
0x3256ca f0x3296ca a802                           test al, 2
0x3256cc f0x3296cc 0f855a010000                   jne 0x32582c
0x3256d2 f0x3296d2 4c8975a8                       mov qword ptr [rbp - 0x58], r14
0x3256d6 f0x3296d6 b101                           mov cl, 1
0x3256d8 f0x3296d8 31c0                           xor eax, eax
0x3256da f0x3296da 498d9c2478010000               lea rbx, [r12 + 0x178]
0x3256e2 f0x3296e2 f0410fb08c2478010000           lock cmpxchg byte ptr [r12 + 0x178], cl
0x3256ec f0x3296ec 0f8547010000                   jne 0x325839
0x3256f2 f0x3296f2 4c8b75b8                       mov r14, qword ptr [rbp - 0x48]
0x3256f6 f0x3296f6 4489f7                         mov edi, r14d
0x3256f9 f0x3296f9 e8d243f1ff                     call 0x239ad0
0x3256fe f0x3296fe 418b8c2430010000               mov ecx, dword ptr [r12 + 0x130]
0x325706 f0x329706 413b8c245c010000               cmp ecx, dword ptr [r12 + 0x15c]
0x32570e f0x32970e 745f                           je 0x32576f
0x325710 f0x329710 498b942468010000               mov rdx, qword ptr [r12 + 0x168]
0x325718 f0x329718 4c89f1                         mov rcx, r14
0x32571b f0x32971b 48c1e920                       shr rcx, 0x20
0x32571f f0x32971f 01c8                           add eax, ecx
0x325721 f0x329721 498d8c2460010000               lea rcx, [r12 + 0x160]
0x325729 f0x329729 4885d2                         test rdx, rdx
0x32572c f0x32972c 480f45ca                       cmovne rcx, rdx
0x325730 f0x329730 418b942470010000               mov edx, dword ptr [r12 + 0x170]
0x325738 f0x329738 ffca                           dec edx
0x32573a f0x32973a 21c2                           and edx, eax
0x32573c f0x32973c 8b0c91                         mov ecx, dword ptr [rcx + rdx*4]
0x32573f f0x32973f 83f9ff                         cmp ecx, -1
0x325742 f0x329742 742b                           je 0x32576f
0x325744 f0x329744 498b842428010000               mov rax, qword ptr [r12 + 0x128]
0x32574c f0x32974c 4531ff                         xor r15d, r15d
0x32574f f0x32974f 90                             nop 
0x325750 f0x329750 4863c9                         movsxd rcx, ecx
0x325753 f0x329753 488d1449                       lea rdx, [rcx + rcx*2]
0x325757 f0x329757 488d0cd0                       lea rcx, [rax + rdx*8]
0x32575b f0x32975b 4c3934d0                       cmp qword ptr [rax + rdx*8], r14
0x32575f f0x32975f 747e                           je 0x3257df
0x325761 f0x329761 8b4910                         mov ecx, dword ptr [rcx + 0x10]
0x325764 f0x329764 83f9ff                         cmp ecx, -1
0x325767 f0x329767 75e7                           jne 0x325750
0x325769 f0x329769 4c8b75a8                       mov r14, qword ptr [rbp - 0x58]
0x32576d f0x32976d eb07                           jmp 0x325776
0x32576f f0x32976f 4c8b75a8                       mov r14, qword ptr [rbp - 0x58]
0x325773 f0x329773 4531ff                         xor r15d, r15d
0x325776 f0x329776 4c897db0                       mov qword ptr [rbp - 0x50], r15
0x32577a f0x32977a b0ff                           mov al, 0xff
0x32577c f0x32977c f00fc003                       lock xadd byte ptr [rbx], al
0x325780 f0x329780 a802                           test al, 2
0x325782 f0x329782 0f85be000000                   jne 0x325846
0x325788 f0x329788 48837db000                     cmp qword ptr [rbp - 0x50], 0
0x32578d f0x32978d 0f84c6000000                   je 0x325859
0x325793 f0x329793 b101                           mov cl, 1
0x325795 f0x329795 31c0                           xor eax, eax
0x325797 f0x329797 f0410fb00e                     lock cmpxchg byte ptr [r14], cl
0x32579c f0x32979c 0f85bc020000                   jne 0x325a5e
0x3257a2 f0x3297a2 488b7da0                       mov rdi, qword ptr [rbp - 0x60]
0x3257a6 f0x3297a6 488d45b8                       lea rax, [rbp - 0x48]
0x3257aa f0x3297aa 488d75c0                       lea rsi, [rbp - 0x40]
0x3257ae f0x3297ae 31d2                           xor edx, edx
0x3257b0 f0x3297b0 488945c0                       mov qword ptr [rbp - 0x40], rax
0x3257b4 f0x3297b4 488d45b0                       lea rax, [rbp - 0x50]
0x3257b8 f0x3297b8 488945c8                       mov qword ptr [rbp - 0x38], rax
0x3257bc f0x3297bc e84ff2d0ff                     call 0x34a10
0x3257c1 f0x3297c1 b0ff                           mov al, 0xff
0x3257c3 f0x3297c3 4c8b2d3ec7e008                 mov r13, qword ptr [rip + 0x8e0c73e] rip->0x9131f08
0x3257ca f0x3297ca 4c89f3                         mov rbx, r14
0x3257cd f0x3297cd f0410fc006                     lock xadd byte ptr [r14], al
0x3257d2 f0x3297d2 a802                           test al, 2
0x3257d4 f0x3297d4 0f8467020000                   je 0x325a41
0x3257da f0x3297da e9c5feffff                     jmp 0x3256a4
0x3257df f0x3297df 4c8b75a8                       mov r14, qword ptr [rbp - 0x58]
0x3257e3 f0x3297e3 4885c9                         test rcx, rcx
0x3257e6 f0x3297e6 748b                           je 0x325773
0x3257e8 f0x3297e8 488d056d960709                 lea rax, [rip + 0x907966d] rip->0x939ee5c
0x3257ef f0x3297ef 4c8b7908                       mov r15, qword ptr [rcx + 8]
0x3257f3 f0x3297f3 803801                         cmp byte ptr [rax], 1
0x3257f6 f0x3297f6 0f857affffff                   jne 0x325776
0x3257fc f0x3297fc 4d85ff                         test r15, r15
0x3257ff f0x3297ff 0f8471ffffff                   je 0x325776
0x325805 f0x329805 4c89ff                         mov rdi, r15
0x325808 f0x329808 e8b32a0400                     call 0x3682c0
0x32580d f0x32980d e964ffffff                     jmp 0x325776
0x325812 f0x329812 4889df                         mov rdi, rbx
0x325815 f0x329815 e8169ed1ff                     call 0x3f630
0x32581a f0x32981a e90cfdffff                     jmp 0x32552b
0x32581f f0x32981f 4c89f7                         mov rdi, r14
0x325822 f0x329822 e8099ed1ff                     call 0x3f630
0x325827 f0x329827 e9b2fdffff                     jmp 0x3255de
0x32582c f0x32982c 4c89f7                         mov rdi, r14
0x32582f f0x32982f e8ac9fd1ff                     call 0x3f7e0
0x325834 f0x329834 e999feffff                     jmp 0x3256d2
0x325839 f0x329839 4889df                         mov rdi, rbx
0x32583c f0x32983c e8ef9dd1ff                     call 0x3f630
0x325841 f0x329841 e9acfeffff                     jmp 0x3256f2
0x325846 f0x329846 4889df                         mov rdi, rbx
0x325849 f0x329849 e8929fd1ff                     call 0x3f7e0
0x32584e f0x32984e 48837db000                     cmp qword ptr [rbp - 0x50], 0
0x325853 f0x329853 0f853affffff                   jne 0x325793
0x325859 f0x329859 4d63b424e0010000               movsxd r14, dword ptr [r12 + 0x1e0]
0x325861 f0x329861 4d85f6                         test r14, r14
0x325864 f0x329864 7e55                           jle 0x3258bb
0x325866 f0x329866 4d8bbc24d8010000               mov r15, qword ptr [r12 + 0x1d8]
0x32586e f0x32986e 49c1e604                       shl r14, 4
0x325872 f0x329872 4d01fe                         add r14, r15
0x325875 f0x329875 eb1a                           jmp 0x325891
0x325877 f0x329877 660f1f840000000000             nop word ptr [rax + rax]
0x325880 f0x329880 4983c710                       add r15, 0x10
0x325884 f0x329884 48c745b000000000               mov qword ptr [rbp - 0x50], 0
0x32588c f0x32988c 4d39f7                         cmp r15, r14
0x32588f f0x32988f 743b                           je 0x3258cc
0x325891 f0x329891 498b3f                         mov rdi, qword ptr [r15]
0x325894 f0x329894 4885ff                         test rdi, rdi
0x325897 f0x329897 74e7                           je 0x325880
0x325899 f0x329899 488b75b8                       mov rsi, qword ptr [rbp - 0x48]
0x32589d f0x32989d ba01000000                     mov edx, 1
0x3258a2 f0x3298a2 e819fcffff                     call 0x3254c0
0x3258a7 f0x3298a7 488945b0                       mov qword ptr [rbp - 0x50], rax
0x3258ab f0x3298ab 4885c0                         test rax, rax
0x3258ae f0x3298ae 751e                           jne 0x3258ce
0x3258b0 f0x3298b0 4983c710                       add r15, 0x10
0x3258b4 f0x3298b4 4d39f7                         cmp r15, r14
0x3258b7 f0x3298b7 75d8                           jne 0x325891
0x3258b9 f0x3298b9 eb13                           jmp 0x3258ce
0x3258bb f0x3298bb 4d85ed                         test r13, r13
0x3258be f0x3298be 7513                           jne 0x3258d3
0x3258c0 f0x3298c0 4c8b2d41c6e008                 mov r13, qword ptr [rip + 0x8e0c641] rip->0x9131f08
0x3258c7 f0x3298c7 4531f6                         xor r14d, r14d
0x3258ca f0x3298ca eb6b                           jmp 0x325937
0x3258cc f0x3298cc 31c0                           xor eax, eax
0x3258ce f0x3298ce 4d85ed                         test r13, r13
0x3258d1 f0x3298d1 744c                           je 0x32591f
0x3258d3 f0x3298d3 488b75b8                       mov rsi, qword ptr [rbp - 0x48]
0x3258d7 f0x3298d7 4c89ef                         mov rdi, r13
0x3258da f0x3298da ba01000000                     mov edx, 1
0x3258df f0x3298df e8dcfbffff                     call 0x3254c0
0x3258e4 f0x3298e4 48837db000                     cmp qword ptr [rbp - 0x50], 0
0x3258e9 f0x3298e9 4989c6                         mov r14, rax
0x3258ec f0x3298ec 7442                           je 0x325930
0x3258ee f0x3298ee 4c8b2d13c6e008                 mov r13, qword ptr [rip + 0x8e0c613] rip->0x9131f08
0x3258f5 f0x3298f5 4d85f6                         test r14, r14
0x3258f8 f0x3298f8 7441                           je 0x32593b
0x3258fa f0x3298fa 4c89f7                         mov rdi, r14
0x3258fd f0x3298fd e8fe48ffff                     call 0x31a200
0x325902 f0x329902 488b7db0                       mov rdi, qword ptr [rbp - 0x50]
0x325906 f0x329906 4989c7                         mov r15, rax
0x325909 f0x329909 e8f248ffff                     call 0x31a200
0x32590e f0x32990e 4c89ff                         mov rdi, r15
0x325911 f0x329911 4889c6                         mov rsi, rax
0x325914 f0x329914 e837ecffff                     call 0x324550
0x325919 f0x329919 84c0                           test al, al
0x32591b f0x32991b 751a                           jne 0x325937
0x32591d f0x32991d eb1c                           jmp 0x32593b
0x32591f f0x32991f 4c8b2de2c5e008                 mov r13, qword ptr [rip + 0x8e0c5e2] rip->0x9131f08
0x325926 f0x329926 4885c0                         test rax, rax
0x325929 f0x329929 7510                           jne 0x32593b
0x32592b f0x32992b 4531f6                         xor r14d, r14d
0x32592e f0x32992e eb07                           jmp 0x325937
0x325930 f0x329930 4c8b2dd1c5e008                 mov r13, qword ptr [rip + 0x8e0c5d1] rip->0x9131f08
0x325937 f0x329937 4c8975b0                       mov qword ptr [rbp - 0x50], r14
0x32593b f0x32993b b101                           mov cl, 1
0x32593d f0x32993d 31c0                           xor eax, eax
0x32593f f0x32993f f00fb00b                       lock cmpxchg byte ptr [rbx], cl
0x325943 f0x329943 0f8522010000                   jne 0x325a6b
0x325949 f0x329949 4c8b7db8                       mov r15, qword ptr [rbp - 0x48]
0x32594d f0x32994d 4489ff                         mov edi, r15d

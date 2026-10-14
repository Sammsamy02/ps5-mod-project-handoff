0x3a14720 file0x3a18720 55 push rbp
0x3a14721 file0x3a18721 4889e5 mov rbp, rsp
0x3a14724 file0x3a18724 4157 push r15
0x3a14726 file0x3a18726 4156 push r14
0x3a14728 file0x3a18728 4155 push r13
0x3a1472a file0x3a1872a 4154 push r12
0x3a1472c file0x3a1872c 53 push rbx
0x3a1472d file0x3a1872d 4881ecd8030000 sub rsp, 0x3d8
0x3a14734 file0x3a18734 488b1dcdd77105 mov rbx, qword ptr [rip + 0x571d7cd] rip->0x9131f08
0x3a1473b file0x3a1873b 4989fe mov r14, rdi
0x3a1473e file0x3a1873e 31ff xor edi, edi
0x3a14740 file0x3a18740 4989f7 mov r15, rsi
0x3a14743 file0x3a18743 488b03 mov rax, qword ptr [rbx]
0x3a14746 file0x3a18746 488945d0 mov qword ptr [rbp - 0x30], rax
0x3a1474a file0x3a1874a e8a1cf7bfc call 0x1d16f0
0x3a1474f file0x3a1874f 488d359eac0c04 lea rsi, [rip + 0x40cac9e] rip->0x7adf3f4
0x3a14756 file0x3a18756 488dbdb8fdffff lea rdi, [rbp - 0x248]
0x3a1475d file0x3a1875d e86e9079fc call 0x1ad7d0
0x3a14762 file0x3a18762 e819067cfc call 0x1d4d80
0x3a14767 file0x3a18767 4885c0 test rax, rax
0x3a1476a file0x3a1876a 740d je 0x3a14779
0x3a1476c file0x3a1876c e80f067cfc call 0x1d4d80
0x3a14771 file0x3a18771 4889c7 mov rdi, rax
0x3a14774 file0x3a18774 e8570f7dfc call 0x1e56d0
0x3a14779 file0x3a18779 488d3546d6f903 lea rsi, [rip + 0x3f9d646] rip->0x79b1dc6
0x3a14780 file0x3a18780 4c89ff mov rdi, r15
0x3a14783 file0x3a18783 e8b85f7cfc call 0x1da740
0x3a14788 file0x3a18788 84c0 test al, al
0x3a1478a file0x3a1878a 7409 je 0x3a14795
0x3a1478c file0x3a1878c 488d0529439405 lea rax, [rip + 0x5944329] rip->0x9358abc
0x3a14793 file0x3a18793 ff00 inc dword ptr [rax]
0x3a14795 file0x3a18795 488d3516dbef03 lea rsi, [rip + 0x3efdb16] rip->0x79122b2
0x3a1479c file0x3a1879c 4c89ff mov rdi, r15
0x3a1479f file0x3a1879f e89c5f7cfc call 0x1da740
0x3a147a4 file0x3a187a4 84c0 test al, al
0x3a147a6 file0x3a187a6 7413 je 0x3a147bb
0x3a147a8 file0x3a187a8 488d050d439405 lea rax, [rip + 0x594430d] rip->0x9358abc
0x3a147af file0x3a187af ff00 inc dword ptr [rax]
0x3a147b1 file0x3a187b1 488d0508439405 lea rax, [rip + 0x5944308] rip->0x9358ac0
0x3a147b8 file0x3a187b8 c60001 mov byte ptr [rax], 1
0x3a147bb file0x3a187bb e8a0a66afc call 0xbee60
0x3a147c0 file0x3a187c0 e8cb6a5ffc call 0xb290
0x3a147c5 file0x3a187c5 e8f67167fc call 0x8b9c0
0x3a147ca file0x3a187ca 4c89ff mov rdi, r15
0x3a147cd file0x3a187cd e83e7a76fc call 0x17c210
0x3a147d2 file0x3a187d2 41bdffffffff mov r13d, 0xffffffff
0x3a147d8 file0x3a187d8 84c0 test al, al
0x3a147da file0x3a187da 0f84ec2e0000 je 0x3a176cc
0x3a147e0 file0x3a187e0 488d9d98feffff lea rbx, [rbp - 0x168]
0x3a147e7 file0x3a187e7 488d357a820404 lea rsi, [rip + 0x404827a] rip->0x7a5ca68
0x3a147ee file0x3a187ee c5f857c0 vxorps xmm0, xmm0, xmm0
0x3a147f2 file0x3a187f2 c5f81185a0fdffff vmovups xmmword ptr [rbp - 0x260], xmm0
0x3a147fa file0x3a187fa 4889df mov rdi, rbx
0x3a147fd file0x3a187fd e8ce8f79fc call 0x1ad7d0
0x3a14802 file0x3a18802 488db5a0fdffff lea rsi, [rbp - 0x260]
0x3a14809 file0x3a18809 4c89ff mov rdi, r15
0x3a1480c file0x3a1880c e8eff4ffff call 0x3a13d00
0x3a14811 file0x3a18811 4889df mov rdi, rbx
0x3a14814 file0x3a18814 e867fb5efc call 0x4380
0x3a14819 file0x3a18819 4c89ff mov rdi, r15
0x3a1481c file0x3a1881c e86f6a5ffc call 0xb290
0x3a14821 file0x3a18821 e86a6a5ffc call 0xb290
0x3a14826 file0x3a18826 488d35df290804 lea rsi, [rip + 0x40829df] rip->0x7a9720c
0x3a1482d file0x3a1882d 4889df mov rdi, rbx
0x3a14830 file0x3a18830 e89b8f79fc call 0x1ad7d0
0x3a14835 file0x3a18835 4889df mov rdi, rbx
0x3a14838 file0x3a18838 e843fb5efc call 0x4380
0x3a1483d file0x3a1883d 488d35d8e00a04 lea rsi, [rip + 0x40ae0d8] rip->0x7ac291c
0x3a14844 file0x3a18844 4889df mov rdi, rbx
0x3a14847 file0x3a18847 e8848f79fc call 0x1ad7d0
0x3a1484c file0x3a1884c 4889df mov rdi, rbx
0x3a1484f file0x3a1884f e82cfb5efc call 0x4380
0x3a14854 file0x3a18854 488d3d45569a05 lea rdi, [rip + 0x59a5645] rip->0x93b9ea0
0x3a1485b file0x3a1885b 488d352e6a5ffc lea rsi, [rip - 0x3a095d2] rip->0xb290
0x3a14862 file0x3a18862 e819d166fc call 0x81980
0x3a14867 file0x3a18867 488d3d22b69305 lea rdi, [rip + 0x593b622] rip->0x934fe90
0x3a1486e file0x3a1886e 488d354b2f0000 lea rsi, [rip + 0x2f4b] rip->0x3a177c0
0x3a14875 file0x3a18875 e806d166fc call 0x81980
0x3a1487a file0x3a1887a 488d35d51e1c04 lea rsi, [rip + 0x41c1ed5] rip->0x7bd6756
0x3a14881 file0x3a18881 488dbd98feffff lea rdi, [rbp - 0x168]
0x3a14888 file0x3a18888 e8438f79fc call 0x1ad7d0
0x3a1488d file0x3a1888d e84eb85efc call 0xe0
0x3a14892 file0x3a18892 488b3daf0fb405 mov rdi, qword ptr [rip + 0x5b40faf] rip->0x9555848
0x3a14899 file0x3a18899 488905a80fb405 mov qword ptr [rip + 0x5b40fa8], rax rip->0x9555848
0x3a148a0 file0x3a188a0 4885ff test rdi, rdi
0x3a148a3 file0x3a188a3 7406 je 0x3a148ab
0x3a148a5 file0x3a188a5 488b07 mov rax, qword ptr [rdi]
0x3a148a8 file0x3a188a8 ff5008 call qword ptr [rax + 8]
0x3a148ab file0x3a188ab 488dbd98feffff lea rdi, [rbp - 0x168]
0x3a148b2 file0x3a188b2 e8c9fa5efc call 0x4380
0x3a148b7 file0x3a188b7 e8c4047cfc call 0x1d4d80
0x3a148bc file0x3a188bc 4889c7 mov rdi, rax
0x3a148bf file0x3a188bf be01000000 mov esi, 1
0x3a148c4 file0x3a188c4 e8870d7dfc call 0x1e5650
0x3a148c9 file0x3a188c9 488d35f02ff203 lea rsi, [rip + 0x3f22ff0] rip->0x79378c0
0x3a148d0 file0x3a188d0 4c89ff mov rdi, r15
0x3a148d3 file0x3a188d3 e8685e7cfc call 0x1da740
0x3a148d8 file0x3a188d8 e8e37067fc call 0x8b9c0
0x3a148dd file0x3a188dd 488d35c15b0604 lea rsi, [rip + 0x4065bc1] rip->0x7a7a4a5
0x3a148e4 file0x3a188e4 488dbd98feffff lea rdi, [rbp - 0x168]
0x3a148eb file0x3a188eb e8e08e79fc call 0x1ad7d0
0x3a148f0 file0x3a188f0 e8ab457efc call 0x1f8ea0
0x3a148f5 file0x3a188f5 4c89b508fcffff mov qword ptr [rbp - 0x3f8], r14
0x3a148fc file0x3a188fc 84c0 test al, al
0x3a148fe file0x3a188fe 0f8436010000 je 0x3a14a3a
0x3a14904 file0x3a18904 488d5da0 lea rbx, [rbp - 0x60]
0x3a14908 file0x3a18908 4889df mov rdi, rbx
0x3a1490b file0x3a1890b e850467efc call 0x1f8f60
0x3a14910 file0x3a18910 4889df mov rdi, rbx
0x3a14913 file0x3a18913 e8e8527efc call 0x1f9c00
0x3a14918 file0x3a18918 84c0 test al, al
0x3a1491a file0x3a1891a 0f850c010000 jne 0x3a14a2c
0x3a14920 file0x3a18920 837da800 cmp dword ptr [rbp - 0x58], 0
0x3a14924 file0x3a18924 7406 je 0x3a1492c
0x3a14926 file0x3a18926 488b75a0 mov rsi, qword ptr [rbp - 0x60]
0x3a1492a file0x3a1892a eb07 jmp 0x3a14933
0x3a1492c file0x3a1892c 488d356dacc103 lea rsi, [rip + 0x3c1ac6d] rip->0x762f5a0
0x3a14933 file0x3a18933 488d3dd858d003 lea rdi, [rip + 0x3d058d8] rip->0x771a212
0x3a1493a file0x3a1893a 31c0 xor eax, eax
0x3a1493c file0x3a1893c e86f1f67fc call 0x868b0
0x3a14941 file0x3a18941 e81a917ffc call 0x20da60
0x3a14946 file0x3a18946 4889c3 mov rbx, rax
0x3a14949 file0x3a18949 e8e2b75efc call 0x130
0x3a1494e file0x3a1894e 4c8db5f0fdffff lea r14, [rbp - 0x210]
0x3a14955 file0x3a18955 4889c6 mov rsi, rax
0x3a14958 file0x3a18958 4c89f7 mov rdi, r14
0x3a1495b file0x3a1895b e890f564fc call 0x63ef0
0x3a14960 file0x3a18960 488d15f9088d05 lea rdx, [rip + 0x58d08f9] rip->0x92e5260
0x3a14967 file0x3a18967 488dbd00fdffff lea rdi, [rbp - 0x300]
0x3a1496e file0x3a1896e 4889de mov rsi, rbx
0x3a14971 file0x3a18971 4c89f1 mov rcx, r14
0x3a14974 file0x3a18974 e8a78e7ffc call 0x20d820
0x3a14979 file0x3a18979 488bbdf0fdffff mov rdi, qword ptr [rbp - 0x210]
0x3a14980 file0x3a18980 4885ff test rdi, rdi
0x3a14983 file0x3a18983 7405 je 0x3a1498a
0x3a14985 file0x3a18985 e806316afc call 0xb7a90
0x3a1498a file0x3a1898a 83bd08fdffff02 cmp dword ptr [rbp - 0x2f8], 2
0x3a14991 file0x3a18991 0f8c84000000 jl 0x3a14a1b
0x3a14997 file0x3a18997 488dbd00fdffff lea rdi, [rbp - 0x300]
0x3a1499e file0x3a1899e e8dd467efc call 0x1f9080
0x3a149a3 file0x3a189a3 e8780176fc call 0x174b20
0x3a149a8 file0x3a189a8 4c89f7 mov rdi, r14
0x3a149ab file0x3a189ab 4889c6 mov rsi, rax
0x3a149ae file0x3a189ae e83df564fc call 0x63ef0
0x3a149b3 file0x3a189b3 83bda8fdffff00 cmp dword ptr [rbp - 0x258], 0
0x3a149ba file0x3a189ba 488d1ddfabc103 lea rbx, [rip + 0x3c1abdf] rip->0x762f5a0
0x3a149c1 file0x3a189c1 4889de mov rsi, rbx
0x3a149c4 file0x3a189c4 7407 je 0x3a149cd
0x3a149c6 file0x3a189c6 488bb5a0fdffff mov rsi, qword ptr [rbp - 0x260]
0x3a149cd file0x3a189cd 83bd08fdffff00 cmp dword ptr [rbp - 0x2f8], 0
0x3a149d4 file0x3a189d4 4889da mov rdx, rbx
0x3a149d7 file0x3a189d7 7407 je 0x3a149e0
0x3a149d9 file0x3a189d9 488b9500fdffff mov rdx, qword ptr [rbp - 0x300]
0x3a149e0 file0x3a189e0 4c89f7 mov rdi, r14
0x3a149e3 file0x3a189e3 31c9 xor ecx, ecx
0x3a149e5 file0x3a189e5 e8a65e65fc call 0x6a890
0x3a149ea file0x3a189ea 83bdf8fdffff00 cmp dword ptr [rbp - 0x208], 0
0x3a149f1 file0x3a189f1 7407 je 0x3a149fa
0x3a149f3 file0x3a189f3 488b9df0fdffff mov rbx, qword ptr [rbp - 0x210]
0x3a149fa file0x3a189fa 4889df mov rdi, rbx
0x3a149fd file0x3a189fd e80e7876fc call 0x17c210
0x3a14a02 file0x3a18a02 e8190176fc call 0x174b20
0x3a14a07 file0x3a18a07 488bbdf0fdffff mov rdi, qword ptr [rbp - 0x210]
0x3a14a0e file0x3a18a0e 4989c7 mov r15, rax
0x3a14a11 file0x3a18a11 4885ff test rdi, rdi
0x3a14a14 file0x3a18a14 7405 je 0x3a14a1b
0x3a14a16 file0x3a18a16 e875306afc call 0xb7a90
0x3a14a1b file0x3a18a1b 488bbd00fdffff mov rdi, qword ptr [rbp - 0x300]
0x3a14a22 file0x3a18a22 4885ff test rdi, rdi
0x3a14a25 file0x3a18a25 7405 je 0x3a14a2c
0x3a14a27 file0x3a18a27 e864306afc call 0xb7a90
0x3a14a2c file0x3a18a2c 488b7da0 mov rdi, qword ptr [rbp - 0x60]
0x3a14a30 file0x3a18a30 4885ff test rdi, rdi
0x3a14a33 file0x3a18a33 7405 je 0x3a14a3a
0x3a14a35 file0x3a18a35 e856306afc call 0xb7a90
0x3a14a3a file0x3a18a3a 488dbd98feffff lea rdi, [rbp - 0x168]
0x3a14a41 file0x3a18a41 e83af95efc call 0x4380
0x3a14a46 file0x3a18a46 e8f54c60fc call 0x19740
0x3a14a4b file0x3a18a4b 488dbd90fdffff lea rdi, [rbp - 0x270]
0x3a14a52 file0x3a18a52 4889c6 mov rsi, rax
0x3a14a55 file0x3a18a55 e896f464fc call 0x63ef0
0x3a14a5a file0x3a18a5a 488d05ff078d05 lea rax, [rip + 0x58d07ff] rip->0x92e5260
0x3a14a61 file0x3a18a61 c5f857c0 vxorps xmm0, xmm0, xmm0
0x3a14a65 file0x3a18a65 c5f8118580fdffff vmovups xmmword ptr [rbp - 0x280], xmm0
0x3a14a6d file0x3a18a6d 66833800 cmp word ptr [rax], 0
0x3a14a71 file0x3a18a71 742b je 0x3a14a9e
0x3a14a73 file0x3a18a73 488d3de6078d05 lea rdi, [rip + 0x58d07e6] rip->0x92e5260
0x3a14a7a file0x3a18a7a 488d359d38e603 lea rsi, [rip + 0x3e6389d] rip->0x787831e
0x3a14a81 file0x3a18a81 e81a8267fc call 0x8cca0
0x3a14a86 file0x3a18a86 85c0 test eax, eax
0x3a14a88 file0x3a18a88 7414 je 0x3a14a9e
0x3a14a8a file0x3a18a8a e811685ffc call 0xb2a0
0x3a14a8f file0x3a18a8f 488dbd80fdffff lea rdi, [rbp - 0x280]
0x3a14a96 file0x3a18a96 4889c6 mov rsi, rax
0x3a14a99 file0x3a18a99 e8b2fa64fc call 0x64550
0x3a14a9e file0x3a18a9e 488d358f0e0904 lea rsi, [rip + 0x4090e8f] rip->0x7aa5934
0x3a14aa5 file0x3a18aa5 488dbd98feffff lea rdi, [rbp - 0x168]
0x3a14aac file0x3a18aac e81f8d79fc call 0x1ad7d0
0x3a14ab1 file0x3a18ab1 e80ab5affc call 0x50ffc0
0x3a14ab6 file0x3a18ab6 488d0d6b529405 lea rcx, [rip + 0x594526b] rip->0x9359d28
0x3a14abd file0x3a18abd 488901 mov qword ptr [rcx], rax
0x3a14ac0 file0x3a18ac0 e87bb4affc call 0x50ff40
0x3a14ac5 file0x3a18ac5 488d0d64be9305 lea rcx, [rip + 0x593be64] rip->0x9350930
0x3a14acc file0x3a18acc 488d1d9db29305 lea rbx, [rip + 0x593b29d] rip->0x934fd70
0x3a14ad3 file0x3a18ad3 488901 mov qword ptr [rcx], rax
0x3a14ad6 file0x3a18ad6 8b4b14 mov ecx, dword ptr [rbx + 0x14]
0x3a14ad9 file0x3a18ad9 8d4101 lea eax, [rcx + 1]
0x3a14adc file0x3a18adc 894314 mov dword ptr [rbx + 0x14], eax
0x3a14adf file0x3a18adf 48634308 movsxd rax, dword ptr [rbx + 8]
0x3a14ae3 file0x3a18ae3 4885c0 test rax, rax
0x3a14ae6 file0x3a18ae6 7e74 jle 0x3a14b5c
0x3a14ae8 file0x3a18ae8 4d89fd mov r13, r15
0x3a14aeb file0x3a18aeb 4531f6 xor r14d, r14d
0x3a14aee file0x3a18aee eb12 jmp 0x3a14b02
0x3a14af0 file0x3a18af0 498d4424ff lea rax, [r12 - 1]
0x3a14af5 file0x3a18af5 41b601 mov r14b, 1
0x3a14af8 file0x3a18af8 4983fc01 cmp r12, 1
0x3a14afc file0x3a18afc 0f8602070000 jbe 0x3a15204
0x3a14b02 file0x3a18b02 4989c7 mov r15, rax
0x3a14b05 file0x3a18b05 4989c4 mov r12, rax
0x3a14b08 file0x3a18b08 49c1e704 shl r15, 4
0x3a14b0c file0x3a18b0c 4983c7f8 add r15, -8
0x3a14b10 file0x3a18b10 488b03 mov rax, qword ptr [rbx]
0x3a14b13 file0x3a18b13 42833c3800 cmp dword ptr [rax + r15], 0
0x3a14b18 file0x3a18b18 74d6 je 0x3a14af0
0x3a14b1a file0x3a18b1a 4a8b7c38f8 mov rdi, qword ptr [rax + r15 - 8]
0x3a14b1f file0x3a18b1f 4885ff test rdi, rdi
0x3a14b22 file0x3a18b22 74cc je 0x3a14af0
0x3a14b24 file0x3a18b24 488b07 mov rax, qword ptr [rdi]
0x3a14b27 file0x3a18b27 ff5068 call qword ptr [rax + 0x68]
0x3a14b2a file0x3a18b2a 84c0 test al, al
0x3a14b2c file0x3a18b2c 74c2 je 0x3a14af0
0x3a14b2e file0x3a18b2e 49ffcc dec r12
0x3a14b31 file0x3a18b31 4983c7f0 add r15, -0x10
0x3a14b35 file0x3a18b35 498d442401 lea rax, [r12 + 1]
0x3a14b3a file0x3a18b3a 4883f801 cmp rax, 1
0x3a14b3e file0x3a18b3e 77d0 ja 0x3a14b10
0x3a14b40 file0x3a18b40 ff4b14 dec dword ptr [rbx + 0x14]
0x3a14b43 file0x3a18b43 4d89ef mov r15, r13
0x3a14b46 file0x3a18b46 41f6c601 test r14b, 1
0x3a14b4a file0x3a18b4a 7413 je 0x3a14b5f
0x3a14b4c file0x3a18b4c 488d3d1db29305 lea rdi, [rip + 0x593b21d] rip->0x934fd70
0x3a14b53 file0x3a18b53 31f6 xor esi, esi
0x3a14b55 file0x3a18b55 e896c261fc call 0x30df0
0x3a14b5a file0x3a18b5a eb03 jmp 0x3a14b5f
0x3a14b5c file0x3a18b5c 894b14 mov dword ptr [rbx + 0x14], ecx
0x3a14b5f file0x3a18b5f 488dbd98feffff lea rdi, [rbp - 0x168]
0x3a14b66 file0x3a18b66 e815f85efc call 0x4380
0x3a14b6b file0x3a18b6b 488d9d98feffff lea rbx, [rbp - 0x168]
0x3a14b72 file0x3a18b72 4889df mov rdi, rbx
0x3a14b75 file0x3a18b75 e8c63072fc call 0x137c40
0x3a14b7a file0x3a18b7a 448bb598feffff mov r14d, dword ptr [rbp - 0x168]
0x3a14b81 file0x3a18b81 4889df mov rdi, rbx
0x3a14b84 file0x3a18b84 e807675ffc call 0xb290
0x3a14b89 file0x3a18b89 41bd01000000 mov r13d, 1
0x3a14b8f file0x3a18b8f 4585f6 test r14d, r14d
0x3a14b92 file0x3a18b92 0f85fa2a0000 jne 0x3a17692
0x3a14b98 file0x3a18b98 488d35644f1604 lea rsi, [rip + 0x4164f64] rip->0x7b79b03
0x3a14b9f file0x3a18b9f 4889df mov rdi, rbx
0x3a14ba2 file0x3a18ba2 e8298c79fc call 0x1ad7d0
0x3a14ba7 file0x3a18ba7 e8645270fc call 0x119e10
0x3a14bac file0x3a18bac 4889df mov rdi, rbx
0x3a14baf file0x3a18baf e8ccf75efc call 0x4380
0x3a14bb4 file0x3a18bb4 488d35b68c2004 lea rsi, [rip + 0x4208cb6] rip->0x7c1d871
0x3a14bbb file0x3a18bbb 4889df mov rdi, rbx
0x3a14bbe file0x3a18bbe e80d8c79fc call 0x1ad7d0
0x3a14bc3 file0x3a18bc3 e8584202fd call 0xa38e20
0x3a14bc8 file0x3a18bc8 4889df mov rdi, rbx
0x3a14bcb file0x3a18bcb e8b0f75efc call 0x4380
0x3a14bd0 file0x3a18bd0 488d357fbf1504 lea rsi, [rip + 0x415bf7f] rip->0x7b70b56
0x3a14bd7 file0x3a18bd7 4889df mov rdi, rbx
0x3a14bda file0x3a18bda c6857ffdffff00 mov byte ptr [rbp - 0x281], 0
0x3a14be1 file0x3a18be1 e8ea8b79fc call 0x1ad7d0
0x3a14be6 file0x3a18be6 488db57ffdffff lea rsi, [rbp - 0x281]
0x3a14bed file0x3a18bed 4c89ff mov rdi, r15
0x3a14bf0 file0x3a18bf0 e85bf6ffff call 0x3a14250
0x3a14bf5 file0x3a18bf5 4889df mov rdi, rbx
0x3a14bf8 file0x3a18bf8 e883f75efc call 0x4380
0x3a14bfd file0x3a18bfd 488d35f2fb1c04 lea rsi, [rip + 0x41cfbf2] rip->0x7be47f6
0x3a14c04 file0x3a18c04 4889df mov rdi, rbx
0x3a14c07 file0x3a18c07 e8c48b79fc call 0x1ad7d0
0x3a14c0c file0x3a18c0c e81fa669fc call 0xaf230
0x3a14c11 file0x3a18c11 488b08 mov rcx, qword ptr [rax]
0x3a14c14 file0x3a18c14 4889c7 mov rdi, rax
0x3a14c17 file0x3a18c17 ff11 call qword ptr [rcx]
0x3a14c19 file0x3a18c19 4889df mov rdi, rbx
0x3a14c1c file0x3a18c1c e85ff75efc call 0x4380
0x3a14c21 file0x3a18c21 488d353a3d1004 lea rsi, [rip + 0x4103d3a] rip->0x7b18962
0x3a14c28 file0x3a18c28 4889df mov rdi, rbx
0x3a14c2b file0x3a18c2b e8a08b79fc call 0x1ad7d0
0x3a14c30 file0x3a18c30 e89b426afc call 0xb8ed0
0x3a14c35 file0x3a18c35 4889c7 mov rdi, rax
0x3a14c38 file0x3a18c38 e893416afc call 0xb8dd0
0x3a14c3d file0x3a18c3d 4889df mov rdi, rbx
0x3a14c40 file0x3a18c40 e83bf75efc call 0x4380
0x3a14c45 file0x3a18c45 bf01000000 mov edi, 1
0x3a14c4a file0x3a18c4a e8a1ca7bfc call 0x1d16f0
0x3a14c4f file0x3a18c4f 488d05fb80b705 lea rax, [rip + 0x5b780fb] rip->0x958cd51
0x3a14c56 file0x3a18c56 803801 cmp byte ptr [rax], 1
0x3a14c59 file0x3a18c59 0f852f020000 jne 0x3a14e8e
0x3a14c5f file0x3a18c5f e88cf7ffff call 0x3a143f0
0x3a14c64 file0x3a18c64 84c0 test al, al
0x3a14c66 file0x3a18c66 0f8422020000 je 0x3a14e8e
0x3a14c6c file0x3a18c6c 488d35ed058d05 lea rsi, [rip + 0x58d05ed] rip->0x92e5260
0x3a14c73 file0x3a18c73 488dbdf0fdffff lea rdi, [rbp - 0x210]
0x3a14c7a file0x3a18c7a e8b1cb6efc call 0x101830
0x3a14c7f file0x3a18c7f 488d9d48fdffff lea rbx, [rbp - 0x2b8]
0x3a14c86 file0x3a18c86 488d357f73b603 lea rsi, [rip + 0x3b6737f] rip->0x757c00c
0x3a14c8d file0x3a18c8d 4889df mov rdi, rbx
0x3a14c90 file0x3a18c90 e87b3870fc call 0x118510
0x3a14c95 file0x3a18c95 4c8db538fdffff lea r14, [rbp - 0x2c8]
0x3a14c9c file0x3a18c9c 488d358b73b603 lea rsi, [rip + 0x3b6738b] rip->0x757c02e
0x3a14ca3 file0x3a18ca3 4c89f7 mov rdi, r14
0x3a14ca6 file0x3a18ca6 e8653870fc call 0x118510
0x3a14cab file0x3a18cab 4c8dbd20feffff lea r15, [rbp - 0x1e0]
0x3a14cb2 file0x3a18cb2 488d0d79b9f403 lea rcx, [rip + 0x3f4b979] rip->0x7960632
0x3a14cb9 file0x3a18cb9 4889de mov rsi, rbx
0x3a14cbc file0x3a18cbc 4c89f2 mov rdx, r14
0x3a14cbf file0x3a18cbf 4c89ff mov rdi, r15
0x3a14cc2 file0x3a18cc2 e879176efc call 0xf6440
0x3a14cc7 file0x3a18cc7 488dbd60fdffff lea rdi, [rbp - 0x2a0]
0x3a14cce file0x3a18cce 4c89fe mov rsi, r15
0x3a14cd1 file0x3a18cd1 ba01000000 mov edx, 1
0x3a14cd6 file0x3a18cd6 e8254f6ffc call 0x109c00
0x3a14cdb file0x3a18cdb 488bbdf0fdffff mov rdi, qword ptr [rbp - 0x210]
0x3a14ce2 file0x3a18ce2 4889bd10feffff mov qword ptr [rbp - 0x1f0], rdi
0x3a14ce9 file0x3a18ce9 4885ff test rdi, rdi
0x3a14cec file0x3a18cec 7406 je 0x3a14cf4
0x3a14cee file0x3a18cee 488b07 mov rax, qword ptr [rdi]
0x3a14cf1 file0x3a18cf1 ff5010 call qword ptr [rax + 0x10]
0x3a14cf4 file0x3a18cf4 8b85f8fdffff mov eax, dword ptr [rbp - 0x208]
0x3a14cfa file0x3a18cfa c5f8108560fdffff vmovups xmm0, xmmword ptr [rbp - 0x2a0]
0x3a14d02 file0x3a18d02 898518feffff mov dword ptr [rbp - 0x1e8], eax
0x3a14d08 file0x3a18d08 488b8568fdffff mov rax, qword ptr [rbp - 0x298]
0x3a14d0f file0x3a18d0f c5f81145a0 vmovups xmmword ptr [rbp - 0x60], xmm0
0x3a14d14 file0x3a18d14 4885c0 test rax, rax
0x3a14d17 file0x3a18d17 7404 je 0x3a14d1d
0x3a14d19 file0x3a18d19 f0ff4008 lock inc dword ptr [rax + 8]
0x3a14d1d file0x3a18d1d 488dbda8feffff lea rdi, [rbp - 0x158]
0x3a14d24 file0x3a18d24 488db510feffff lea rsi, [rbp - 0x1f0]
0x3a14d2b file0x3a18d2b c78598feffff04000000 mov dword ptr [rbp - 0x168], 4
0x3a14d35 file0x3a18d35 e8667d6efc call 0xfcaa0
0x3a14d3a file0x3a18d3a 488d9d00fdffff lea rbx, [rbp - 0x300]
0x3a14d41 file0x3a18d41 488d850cfdffff lea rax, [rbp - 0x2f4]
0x3a14d48 file0x3a18d48 bf20000000 mov edi, 0x20
0x3a14d4d file0x3a18d4d be08000000 mov esi, 8
0x3a14d52 file0x3a18d52 ba01000000 mov edx, 1
0x3a14d57 file0x3a18d57 41b901000000 mov r9d, 1
0x3a14d5d file0x3a18d5d 48c78500fdffff00000000 mov qword ptr [rbp - 0x300], 0
0x3a14d68 file0x3a18d68 c78508fdffff01000000 mov dword ptr [rbp - 0x2f8], 1
0x3a14d72 file0x3a18d72 31c9 xor ecx, ecx
0x3a14d74 file0x3a18d74 4989d8 mov r8, rbx
0x3a14d77 file0x3a18d77 48890424 mov qword ptr [rsp], rax
0x3a14d7b file0x3a18d7b e8502861fc call 0x275d0
0x3a14d80 file0x3a18d80 488bbd00fdffff mov rdi, qword ptr [rbp - 0x300]
0x3a14d87 file0x3a18d87 488db598feffff lea rsi, [rbp - 0x168]
0x3a14d8e file0x3a18d8e ba01000000 mov edx, 1
0x3a14d93 file0x3a18d93 e8b86366fc call 0x7b150
0x3a14d98 file0x3a18d98 488dbdd0fdffff lea rdi, [rbp - 0x230]
0x3a14d9f file0x3a18d9f 488d75a0 lea rsi, [rbp - 0x60]
0x3a14da3 file0x3a18da3 4889da mov rdx, rbx
0x3a14da6 file0x3a18da6 e8558c6efc call 0xfda00
0x3a14dab file0x3a18dab 488bbd00fdffff mov rdi, qword ptr [rbp - 0x300]
0x3a14db2 file0x3a18db2 8bb508fdffff mov esi, dword ptr [rbp - 0x2f8]
0x3a14db8 file0x3a18db8 e8236466fc call 0x7b1e0
0x3a14dbd file0x3a18dbd 488bbd00fdffff mov rdi, qword ptr [rbp - 0x300]
0x3a14dc4 file0x3a18dc4 4885ff test rdi, rdi
0x3a14dc7 file0x3a18dc7 7405 je 0x3a14dce
0x3a14dc9 file0x3a18dc9 e8c22c6afc call 0xb7a90
0x3a14dce file0x3a18dce 488bbda8feffff mov rdi, qword ptr [rbp - 0x158]
0x3a14dd5 file0x3a18dd5 4885ff test rdi, rdi
0x3a14dd8 file0x3a18dd8 7406 je 0x3a14de0
0x3a14dda file0x3a18dda 488b07 mov rax, qword ptr [rdi]
0x3a14ddd file0x3a18ddd ff5018 call qword ptr [rax + 0x18]
0x3a14de0 file0x3a18de0 488b5da8 mov rbx, qword ptr [rbp - 0x58]
0x3a14de4 file0x3a18de4 4885db test rbx, rbx
0x3a14de7 file0x3a18de7 741d je 0x3a14e06
0x3a14de9 file0x3a18de9 f0ff4b08 lock dec dword ptr [rbx + 8]
0x3a14ded file0x3a18ded 7517 jne 0x3a14e06
0x3a14def file0x3a18def 488b03 mov rax, qword ptr [rbx]
0x3a14df2 file0x3a18df2 4889df mov rdi, rbx
0x3a14df5 file0x3a18df5 ff10 call qword ptr [rax]
0x3a14df7 file0x3a18df7 f0ff4b0c lock dec dword ptr [rbx + 0xc]
0x3a14dfb file0x3a18dfb 7509 jne 0x3a14e06
0x3a14dfd file0x3a18dfd 488b03 mov rax, qword ptr [rbx]
0x3a14e00 file0x3a18e00 4889df mov rdi, rbx
0x3a14e03 file0x3a18e03 ff5010 call qword ptr [rax + 0x10]
0x3a14e06 file0x3a18e06 488db5d0fdffff lea rsi, [rbp - 0x230]
0x3a14e0d file0x3a18e0d 31ff xor edi, edi
0x3a14e0f file0x3a18e0f e84cca7cfc call 0x1e1860
0x3a14e14 file0x3a18e14 488bbdd0fdffff mov rdi, qword ptr [rbp - 0x230]
0x3a14e1b file0x3a18e1b 4885ff test rdi, rdi
0x3a14e1e file0x3a18e1e 7406 je 0x3a14e26
0x3a14e20 file0x3a18e20 488b07 mov rax, qword ptr [rdi]
0x3a14e23 file0x3a18e23 ff5018 call qword ptr [rax + 0x18]
0x3a14e26 file0x3a18e26 488bbd10feffff mov rdi, qword ptr [rbp - 0x1f0]
0x3a14e2d file0x3a18e2d 4885ff test rdi, rdi
0x3a14e30 file0x3a18e30 7406 je 0x3a14e38
0x3a14e32 file0x3a18e32 488b07 mov rax, qword ptr [rdi]
0x3a14e35 file0x3a18e35 ff5018 call qword ptr [rax + 0x18]
0x3a14e38 file0x3a18e38 488b9d68fdffff mov rbx, qword ptr [rbp - 0x298]
0x3a14e3f file0x3a18e3f 4885db test rbx, rbx
0x3a14e42 file0x3a18e42 741d je 0x3a14e61
0x3a14e44 file0x3a18e44 f0ff4b08 lock dec dword ptr [rbx + 8]
0x3a14e48 file0x3a18e48 7517 jne 0x3a14e61
0x3a14e4a file0x3a18e4a 488b03 mov rax, qword ptr [rbx]
0x3a14e4d file0x3a18e4d 4889df mov rdi, rbx
0x3a14e50 file0x3a18e50 ff10 call qword ptr [rax]
0x3a14e52 file0x3a18e52 f0ff4b0c lock dec dword ptr [rbx + 0xc]
0x3a14e56 file0x3a18e56 7509 jne 0x3a14e61
0x3a14e58 file0x3a18e58 488b03 mov rax, qword ptr [rbx]
0x3a14e5b file0x3a18e5b 4889df mov rdi, rbx
0x3a14e5e file0x3a18e5e ff5010 call qword ptr [rax + 0x10]
0x3a14e61 file0x3a18e61 488bbd20feffff mov rdi, qword ptr [rbp - 0x1e0]
0x3a14e68 file0x3a18e68 4885ff test rdi, rdi
0x3a14e6b file0x3a18e6b 7406 je 0x3a14e73
0x3a14e6d file0x3a18e6d 488b07 mov rax, qword ptr [rdi]
0x3a14e70 file0x3a18e70 ff5018 call qword ptr [rax + 0x18]
0x3a14e73 file0x3a18e73 488bbdf0fdffff mov rdi, qword ptr [rbp - 0x210]
0x3a14e7a file0x3a18e7a 4885ff test rdi, rdi
0x3a14e7d file0x3a18e7d 0f840f280000 je 0x3a17692
0x3a14e83 file0x3a18e83 488b07 mov rax, qword ptr [rdi]
0x3a14e86 file0x3a18e86 ff5018 call qword ptr [rax + 0x18]
0x3a14e89 file0x3a18e89 e904280000 jmp 0x3a17692
0x3a14e8e file0x3a18e8e e80dd5a503 call 0x74723a0
0x3a14e93 file0x3a18e93 488d0dfe3b9405 lea rcx, [rip + 0x5943bfe] rip->0x9358a98
0x3a14e9a file0x3a18e9a 31ff xor edi, edi
0x3a14e9c file0x3a18e9c 8901 mov dword ptr [rcx], eax
0x3a14e9e file0x3a18e9e 488d0dff3b9405 lea rcx, [rip + 0x5943bff] rip->0x9358aa4
0x3a14ea5 file0x3a18ea5 c60101 mov byte ptr [rcx], 1
0x3a14ea8 file0x3a18ea8 e8136f60fc call 0x1bdc0
0x3a14ead file0x3a18ead e85eb65efc call 0x510
0x3a14eb2 file0x3a18eb2 4889c7 mov rdi, rax
0x3a14eb5 file0x3a18eb5 e8a6a45ffc call 0xf360
0x3a14eba file0x3a18eba 488dbd48fdffff lea rdi, [rbp - 0x2b8]
0x3a14ec1 file0x3a18ec1 4c89fe mov rsi, r15
0x3a14ec4 file0x3a18ec4 c5f857c0 vxorps xmm0, xmm0, xmm0
0x3a14ec8 file0x3a18ec8 31d2 xor edx, edx
0x3a14eca file0x3a18eca c6855ffdffff00 mov byte ptr [rbp - 0x2a1], 0
0x3a14ed1 file0x3a18ed1 c6855efdffff00 mov byte ptr [rbp - 0x2a2], 0
0x3a14ed8 file0x3a18ed8 c6855dfdffff00 mov byte ptr [rbp - 0x2a3], 0
0x3a14edf file0x3a18edf c6855cfdffff00 mov byte ptr [rbp - 0x2a4], 0
0x3a14ee6 file0x3a18ee6 c5f8118520feffff vmovups xmmword ptr [rbp - 0x1e0], xmm0
0x3a14eee file0x3a18eee e8ed280000 call 0x3a177e0
0x3a14ef3 file0x3a18ef3 488dbd38fdffff lea rdi, [rbp - 0x2c8]
0x3a14efa file0x3a18efa 4c89fe mov rsi, r15
0x3a14efd file0x3a18efd ba01000000 mov edx, 1
0x3a14f02 file0x3a18f02 4c89bd50fcffff mov qword ptr [rbp - 0x3b0], r15
0x3a14f09 file0x3a18f09 e8d2280000 call 0x3a177e0
0x3a14f0e file0x3a18f0e 83bd50fdffff00 cmp dword ptr [rbp - 0x2b0], 0
0x3a14f15 file0x3a18f15 0f8e28020000 jle 0x3a15143
0x3a14f1b file0x3a18f1b 4c8dbd98feffff lea r15, [rbp - 0x168]
0x3a14f22 file0x3a18f22 4531ed xor r13d, r13d
0x3a14f25 file0x3a18f25 31db xor ebx, ebx
0x3a14f27 file0x3a18f27 eb25 jmp 0x3a14f4e
0x3a14f4e file0x3a18f4e 488b8548fdffff mov rax, qword ptr [rbp - 0x2b8]
0x3a14f55 file0x3a18f55 48c78598feffff00000000 mov qword ptr [rbp - 0x168], 0
0x3a14f60 file0x3a18f60 4e8b3428 mov r14, qword ptr [rax + r13]
0x3a14f64 file0x3a18f64 4e63642808 movsxd r12, dword ptr [rax + r13 + 8]
0x3a14f69 file0x3a18f69 4489a5a0feffff mov dword ptr [rbp - 0x160], r12d
0x3a14f70 file0x3a18f70 4d85e4 test r12, r12
0x3a14f73 file0x3a18f73 744b je 0x3a14fc0
0x3a14f75 file0x3a18f75 488d85a4feffff lea rax, [rbp - 0x15c]
0x3a14f7c file0x3a18f7c bf02000000 mov edi, 2
0x3a14f81 file0x3a18f81 be02000000 mov esi, 2
0x3a14f86 file0x3a18f86 4489e2 mov edx, r12d
0x3a14f89 file0x3a18f89 4d89f8 mov r8, r15
0x3a14f8c file0x3a18f8c 4589e1 mov r9d, r12d
0x3a14f8f file0x3a18f8f 31c9 xor ecx, ecx
0x3a14f91 file0x3a18f91 48890424 mov qword ptr [rsp], rax

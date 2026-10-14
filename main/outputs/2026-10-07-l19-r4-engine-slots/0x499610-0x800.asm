0x499610: 55                       push rbp
0x499611: 4889e5                   mov rbp, rsp
0x499614: 4157                     push r15
0x499616: 4156                     push r14
0x499618: 4155                     push r13
0x49961a: 4154                     push r12
0x49961c: 53                       push rbx
0x49961d: 4881ec58010000           sub rsp, 0x158
0x499624: 488b05dd88c908           mov rax, qword ptr [rip + 0x8c988dd]
0x49962b: 4989f5                   mov r13, rsi
0x49962e: 4889fb                   mov rbx, rdi
0x499631: 488b00                   mov rax, qword ptr [rax]
0x499634: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x499638: 488b07                   mov rax, qword ptr [rdi]
0x49963b: 4885c0                   test rax, rax
0x49963e: 740d                     je 0x49964d
0x499640: f680d700000020           test byte ptr [rax + 0xd7], 0x20
0x499647: 0f8568020000             jne 0x4998b5
0x49964d: 458b7538                 mov r14d, dword ptr [r13 + 0x38]
0x499651: 4983e60f                 and r14, 0xf
0x499655: 7426                     je 0x49967d
0x499657: 4d8b7d18                 mov r15, qword ptr [r13 + 0x18]
0x49965b: 41c1e603                 shl r14d, 3
0x49965f: 4531e4                   xor r12d, r12d
0x499662: 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x499670: 43ff1427                 call qword ptr [r15 + r12]
0x499674: 4983c408                 add r12, 8
0x499678: 4d39e6                   cmp r14, r12
0x49967b: 75f3                     jne 0x499670
0x49967d: 41ff5500                 call qword ptr [r13]
0x499681: 488903                   mov qword ptr [rbx], rax
0x499684: f680d700000020           test byte ptr [rax + 0xd7], 0x20
0x49968b: 0f8524020000             jne 0x4998b5
0x499691: 4889c7                   mov rdi, rax
0x499694: 31f6                     xor esi, esi
0x499696: 4989c7                   mov r15, rax
0x499699: e852c7feff               call 0x485df0
0x49969e: 498b4f40                 mov rcx, qword ptr [r15 + 0x40]
0x4996a2: 418b87d4000000           mov eax, dword ptr [r15 + 0xd4]
0x4996a9: 4885c9                   test rcx, rcx
0x4996ac: 740d                     je 0x4996bb
0x4996ae: ba5e87a34a               mov edx, 0x4aa3875e
0x4996b3: 2391d4000000             and edx, dword ptr [rcx + 0xd4]
0x4996b9: 09d0                     or eax, edx
0x4996bb: 410b453c                 or eax, dword ptr [r13 + 0x3c]
0x4996bf: 89c1                     mov ecx, eax
0x4996c1: 81c900000020             or ecx, 0x20000000
0x4996c7: 41898fd4000000           mov dword ptr [r15 + 0xd4], ecx
0x4996ce: a900000010               test eax, 0x10000000
0x4996d3: 750c                     jne 0x4996e1
0x4996d5: 498dbfe8010000           lea rdi, [r15 + 0x1e8]
0x4996dc: e8ff41f0ff               call 0x39d8e0
0x4996e1: b8040b0000               mov eax, 0xb04
0x4996e6: 498b7520                 mov rsi, qword ptr [r13 + 0x20]
0x4996ea: 4c89ff                   mov rdi, r15
0x4996ed: c4c278f75538             bextr edx, dword ptr [r13 + 0x38], eax
0x4996f3: e8f8bae8ff               call 0x3251f0
0x4996f8: b80f0b0000               mov eax, 0xb0f
0x4996fd: 4c89ada0feffff           mov qword ptr [rbp - 0x160], r13
0x499704: c4c278f74538             bextr eax, dword ptr [r13 + 0x38], eax
0x49970a: 8d0cc500000000           lea ecx, [rax*8]
0x499711: 49034d28                 add rcx, qword ptr [r13 + 0x28]
0x499715: 8985b4feffff             mov dword ptr [rbp - 0x14c], eax
0x49971b: 48898db8feffff           mov qword ptr [rbp - 0x148], rcx
0x499722: 85c0                     test eax, eax
0x499724: 0f8494000000             je 0x4997be
0x49972a: 4c89fb                   mov rbx, r15
0x49972d: 4d89fc                   mov r12, r15
0x499730: 4883cb01                 or rbx, 1
0x499734: 4983e4fe                 and r12, 0xfffffffffffffffe
0x499738: 744e                     je 0x499788
0x49973a: 4c8dadb8feffff           lea r13, [rbp - 0x148]
0x499741: 4c8db5b4feffff           lea r14, [rbp - 0x14c]
0x499748: eb21                     jmp 0x49976b
0x49974a: 660f1f440000             nop word ptr [rax + rax]
0x499750: 488dbda8feffff           lea rdi, [rbp - 0x158]
0x499757: 4c89ee                   mov rsi, r13
0x49975a: 4c89f2                   mov rdx, r14
0x49975d: e80eadffff               call 0x494470
0x499762: 83bdb4feffff00           cmp dword ptr [rbp - 0x14c], 0
0x499769: 7453                     je 0x4997be
0x49976b: 488d05ea56f008           lea rax, [rip + 0x8f056ea]
0x499772: 48899da8feffff           mov qword ptr [rbp - 0x158], rbx
0x499779: 803801                   cmp byte ptr [rax], 1
0x49977c: 75d2                     jne 0x499750
0x49977e: 4c89e7                   mov rdi, r12
0x499781: e83aebecff               call 0x3682c0
0x499786: ebc8                     jmp 0x499750
0x499788: 4c8db5a8feffff           lea r14, [rbp - 0x158]
0x49978f: 4c8dadb8feffff           lea r13, [rbp - 0x148]
0x499796: 4c8da5b4feffff           lea r12, [rbp - 0x14c]
0x49979d: 0f1f00                   nop dword ptr [rax]
0x4997a0: 4c89f7                   mov rdi, r14
0x4997a3: 4c89ee                   mov rsi, r13
0x4997a6: 4c89e2                   mov rdx, r12
0x4997a9: 48899da8feffff           mov qword ptr [rbp - 0x158], rbx
0x4997b0: e8bbacffff               call 0x494470
0x4997b5: 83bdb4feffff00           cmp dword ptr [rbp - 0x14c], 0
0x4997bc: 75e2                     jne 0x4997a0
0x4997be: 4c8bb5a0feffff           mov r14, qword ptr [rbp - 0x160]
0x4997c5: 498b5e08                 mov rbx, qword ptr [r14 + 8]
0x4997c9: 4885db                   test rbx, rbx
0x4997cc: 0f84b9000000             je 0x49988b
0x4997d2: 4889df                   mov rdi, rbx
0x4997d5: 48c745b800000000         mov qword ptr [rbp - 0x48], 0
0x4997dd: e80e89fd06               call 0x74720f0
0x4997e2: 4989c6                   mov r14, rax
0x4997e5: 4889de                   mov rsi, rbx
0x4997e8: 4531e4                   xor r12d, r12d
0x4997eb: 31ff                     xor edi, edi
0x4997ed: 4489f2                   mov edx, r14d
0x4997f0: e8ab39bfff               call 0x8d1a0
0x4997f5: 4189c5                   mov r13d, eax
0x4997f8: 8945c8                   mov dword ptr [rbp - 0x38], eax
0x4997fb: 41ffc5                   inc r13d
0x4997fe: 4181fd80000000           cmp r13d, 0x80
0x499805: 7707                     ja 0x49980e
0x499807: 48837db800               cmp qword ptr [rbp - 0x48], 0
0x49980c: 7422                     je 0x499830
0x49980e: 4181fd81000000           cmp r13d, 0x81
0x499815: 488d7db8                 lea rdi, [rbp - 0x48]
0x499819: b902000000               mov ecx, 2
0x49981e: 450f43e5                 cmovae r12d, r13d
0x499822: 31f6                     xor esi, esi
0x499824: 4489e2                   mov edx, r12d
0x499827: e83497bcff               call 0x62f60
0x49982c: 4c8b65b8                 mov r12, qword ptr [rbp - 0x48]
0x499830: 4d85e4                   test r12, r12
0x499833: 488dbdb8feffff           lea rdi, [rbp - 0x148]
0x49983a: 4489ee                   mov esi, r13d
0x49983d: 4889da                   mov rdx, rbx
0x499840: 490f45fc                 cmovne rdi, r12
0x499844: 41ffc6                   inc r14d
0x499847: 4489f1                   mov ecx, r14d
0x49984a: 48897dc0                 mov qword ptr [rbp - 0x40], rdi
0x49984e: e8ad42bfff               call 0x8db00
0x499853: 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x499857: 488dbda8feffff           lea rdi, [rbp - 0x158]
0x49985e: ba01000000               mov edx, 1
0x499863: e8283ddaff               call 0x23d590
0x499868: 488b85a8feffff           mov rax, qword ptr [rbp - 0x158]
0x49986f: 498987e8000000           mov qword ptr [r15 + 0xe8], rax
0x499876: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x49987a: 4885ff                   test rdi, rdi
0x49987d: 7405                     je 0x499884
0x49987f: e80ce2c1ff               call 0xb7a90
0x499884: 4c8bb5a0feffff           mov r14, qword ptr [rbp - 0x160]
0x49988b: 418b5e38                 mov ebx, dword ptr [r14 + 0x38]
0x49988f: 4881fb00000004           cmp rbx, 0x4000000
0x499896: 7343                     jae 0x4998db
0x499898: 4c89ff                   mov rdi, r15
0x49989b: 31f6                     xor esi, esi
0x49989d: e8ced0e7ff               call 0x316970
0x4998a2: 4c89ff                   mov rdi, r15
0x4998a5: e856a0e8ff               call 0x323900
0x4998aa: 4c89ff                   mov rdi, r15
0x4998ad: 4889c6                   mov rsi, rax
0x4998b0: e86ba0e8ff               call 0x323920
0x4998b5: 488b054c86c908           mov rax, qword ptr [rip + 0x8c9864c]
0x4998bc: 488b00                   mov rax, qword ptr [rax]
0x4998bf: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x4998c3: 0f8560010000             jne 0x499a29
0x4998c9: 4881c458010000           add rsp, 0x158
0x4998d0: 5b                       pop rbx
0x4998d1: 415c                     pop r12
0x4998d3: 415d                     pop r13
0x4998d5: 415e                     pop r14
0x4998d7: 415f                     pop r15
0x4998d9: 5d                       pop rbp
0x4998da: c3                       ret 
0x4998db: 4c89f9                   mov rcx, r15
0x4998de: 498d87d8010000           lea rax, [r15 + 0x1d8]
0x4998e5: c1eb1a                   shr ebx, 0x1a
0x4998e8: 4881c1e4010000           add rcx, 0x1e4
0x4998ef: 48898590feffff           mov qword ptr [rbp - 0x170], rax
0x4998f6: 48898d98feffff           mov qword ptr [rbp - 0x168], rcx
0x4998fd: 41399fe4010000           cmp dword ptr [r15 + 0x1e4], ebx
0x499904: 7d26                     jge 0x49992c
0x499906: 458b87e0010000           mov r8d, dword ptr [r15 + 0x1e0]
0x49990d: 488b8d90feffff           mov rcx, qword ptr [rbp - 0x170]
0x499914: 4c8b8d98feffff           mov r9, qword ptr [rbp - 0x168]
0x49991b: bf10000000               mov edi, 0x10
0x499920: be08000000               mov esi, 8
0x499925: 89da                     mov edx, ebx
0x499927: e8f4c4b8ff               call 0x25e20
0x49992c: 4d8b6630                 mov r12, qword ptr [r14 + 0x30]
0x499930: c1e304                   shl ebx, 4
0x499933: 4c89bd88feffff           mov qword ptr [rbp - 0x178], r15
0x49993a: 4c01e3                   add rbx, r12
0x49993d: 48899da0feffff           mov qword ptr [rbp - 0x160], rbx
0x499944: eb2f                     jmp 0x499975
0x499946: 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x499950: 45897c1d08               mov dword ptr [r13 + rbx + 8], r15d
0x499955: 488b85a0feffff           mov rax, qword ptr [rbp - 0x160]
0x49995c: 4c8bbd88feffff           mov r15, qword ptr [rbp - 0x178]
0x499963: 4983c410                 add r12, 0x10
0x499967: 41884c1d0c               mov byte ptr [r13 + rbx + 0xc], cl
0x49996c: 4939c4                   cmp r12, rax
0x49996f: 0f8423ffffff             je 0x499898
0x499975: 498b0424                 mov rax, qword ptr [r12]
0x499979: 4885c0                   test rax, rax
0x49997c: 0f848e000000             je 0x499a10
0x499982: ffd0                     call rax
0x499984: 4989c6                   mov r14, rax
0x499987: 418b87e0010000           mov eax, dword ptr [r15 + 0x1e0]
0x49998e: 413b87e4010000           cmp eax, dword ptr [r15 + 0x1e4]
0x499995: 7518                     jne 0x4999af
0x499997: 488bb590feffff           mov rsi, qword ptr [rbp - 0x170]
0x49999e: 488b9598feffff           mov rdx, qword ptr [rbp - 0x168]
0x4999a5: bf10080000               mov edi, 0x810
0x4999aa: e8f1feb8ff               call 0x298a0
0x4999af: 8d4801                   lea ecx, [rax + 1]
0x4999b2: 4863d8                   movsxd rbx, eax
0x4999b5: 41898fe0010000           mov dword ptr [r15 + 0x1e0], ecx
0x4999bc: 48c1e304                 shl rbx, 4
0x4999c0: 4d8bafd8010000           mov r13, qword ptr [r15 + 0x1d8]
0x4999c7: 458b7c2408               mov r15d, dword ptr [r12 + 8]
0x4999cc: 410fb64c240c             movzx ecx, byte ptr [r12 + 0xc]
0x4999d2: 4d89741d00               mov qword ptr [r13 + rbx], r14
0x4999d7: 4d85f6                   test r14, r14
0x4999da: 0f8470ffffff             je 0x499950
0x4999e0: 488d057554f008           lea rax, [rip + 0x8f05475]
0x4999e7: 803800                   cmp byte ptr [rax], 0
0x4999ea: 0f8460ffffff             je 0x499950
0x4999f0: 4c89f7                   mov rdi, r14
0x4999f3: 4189ce                   mov r14d, ecx
0x4999f6: e8c5e8ecff               call 0x3682c0
0x4999fb: 4489f1                   mov ecx, r14d
0x4999fe: e94dffffff               jmp 0x499950
0x499a03: 666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x499a10: 4531f6                   xor r14d, r14d
0x499a13: 418b87e0010000           mov eax, dword ptr [r15 + 0x1e0]
0x499a1a: 413b87e4010000           cmp eax, dword ptr [r15 + 0x1e4]
0x499a21: 0f8470ffffff             je 0x499997
0x499a27: eb86                     jmp 0x4999af
0x499a29: e83284fd06               call 0x7471e60
0x499a2e: 0f0b                     ud2 
0x499a30: 55                       push rbp
0x499a31: 4889e5                   mov rbp, rsp
0x499a34: 4157                     push r15
0x499a36: 4156                     push r14
0x499a38: 4155                     push r13
0x499a3a: 4154                     push r12
0x499a3c: 53                       push rbx
0x499a3d: 4881ec78100000           sub rsp, 0x1078
0x499a44: 488b0dbd84c908           mov rcx, qword ptr [rip + 0x8c984bd]
0x499a4b: 488b01                   mov rax, qword ptr [rcx]
0x499a4e: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x499a52: 48833f00                 cmp qword ptr [rdi], 0
0x499a56: 0f850a020000             jne 0x499c66
0x499a5c: 4889f3                   mov rbx, rsi
0x499a5f: 4989fe                   mov r14, rdi
0x499a62: e839fcf3ff               call 0x3d96a0
0x499a67: 4c8b23                   mov r12, qword ptr [rbx]
0x499a6a: 4989c7                   mov r15, rax
0x499a6d: 48c785b0f8ffff00000000   mov qword ptr [rbp - 0x750], 0
0x499a78: 4d85e4                   test r12, r12
0x499a7b: 0f84ab000000             je 0x499b2c
0x499a81: 4c89e7                   mov rdi, r12
0x499a84: 4c89bd78efffff           mov qword ptr [rbp - 0x1088], r15
0x499a8b: 48899d70efffff           mov qword ptr [rbp - 0x1090], rbx
0x499a92: e85986fd06               call 0x74720f0
0x499a97: 4989c5                   mov r13, rax
0x499a9a: 4c89e6                   mov rsi, r12
0x499a9d: 31db                     xor ebx, ebx
0x499a9f: 31ff                     xor edi, edi
0x499aa1: 4489ea                   mov edx, r13d
0x499aa4: e8f736bfff               call 0x8d1a0
0x499aa9: 4189c7                   mov r15d, eax
0x499aac: 8985c0f8ffff             mov dword ptr [rbp - 0x740], eax
0x499ab2: 41ffc7                   inc r15d
0x499ab5: 4181ff80000000           cmp r15d, 0x80
0x499abc: 770a                     ja 0x499ac8
0x499abe: 4883bdb0f8ffff00         cmp qword ptr [rbp - 0x750], 0
0x499ac6: 7427                     je 0x499aef
0x499ac8: 4181ff81000000           cmp r15d, 0x81
0x499acf: 488dbdb0f8ffff           lea rdi, [rbp - 0x750]
0x499ad6: b902000000               mov ecx, 2
0x499adb: 410f43df                 cmovae ebx, r15d
0x499adf: 31f6                     xor esi, esi
0x499ae1: 89da                     mov edx, ebx
0x499ae3: e87894bcff               call 0x62f60
0x499ae8: 488b9db0f8ffff           mov rbx, qword ptr [rbp - 0x750]
0x499aef: 4885db                   test rbx, rbx
0x499af2: 488dbdb0f7ffff           lea rdi, [rbp - 0x850]
0x499af9: 4489fe                   mov esi, r15d
0x499afc: 4c89e2                   mov rdx, r12
0x499aff: 480f45fb                 cmovne rdi, rbx
0x499b03: 41ffc5                   inc r13d
0x499b06: 4489e9                   mov ecx, r13d
0x499b09: 4889bdb8f8ffff           mov qword ptr [rbp - 0x748], rdi
0x499b10: e8eb3fbfff               call 0x8db00
0x499b15: 488bb5b8f8ffff           mov rsi, qword ptr [rbp - 0x748]
0x499b1c: 488b9d70efffff           mov rbx, qword ptr [rbp - 0x1090]
0x499b23: 4c8bbd78efffff           mov r15, qword ptr [rbp - 0x1088]
0x499b2a: eb17                     jmp 0x499b43
0x499b2c: 48c785b8f8ffff00000000   mov qword ptr [rbp - 0x748], 0
0x499b37: 31f6                     xor esi, esi
0x499b39: c785c0f8ffff00000000     mov dword ptr [rbp - 0x740], 0
0x499b43: 488dbd88efffff           lea rdi, [rbp - 0x1078]
0x499b4a: ba01000000               mov edx, 1
0x499b4f: e83c3adaff               call 0x23d590
0x499b54: 4c8ba588efffff           mov r12, qword ptr [rbp - 0x1078]
0x499b5b: 31ff                     xor edi, edi
0x499b5d: e85ed6fbff               call 0x4571c0
0x499b62: 84c0                     test al, al
0x499b64: 0f8517010000             jne 0x499c81
0x499b6a: e8910eedff               call 0x36aa00
0x499b6f: 84c0                     test al, al
0x499b71: 0f8531010000             jne 0x499ca8
0x499b77: e8f4cdf3ff               call 0x3d6970
0x499b7c: 4c89ff                   mov rdi, r15
0x499b7f: 4c89e2                   mov rdx, r12
0x499b82: 4189c1                   mov r9d, eax
0x499b85: 31f6                     xor esi, esi
0x499b87: 31c9                     xor ecx, ecx
0x499b89: 4531c0                   xor r8d, r8d
0x499b8c: e8ef3b0100               call 0x4ad780
0x499b91: 488bbdb0f8ffff           mov rdi, qword ptr [rbp - 0x750]
0x499b98: 4989c7                   mov r15, rax
0x499b9b: 4885ff                   test rdi, rdi
0x499b9e: 7405                     je 0x499ba5
0x499ba0: e8ebdec1ff               call 0xb7a90
0x499ba5: 4d893e                   mov qword ptr [r14], r15
0x499ba8: 488dbd90efffff           lea rdi, [rbp - 0x1070]
0x499baf: 488db5b0f7ffff           lea rsi, [rbp - 0x850]
0x499bb6: ba0b040000               mov edx, 0x40b
0x499bbb: 8b4314                   mov eax, dword ptr [rbx + 0x14]
0x499bbe: 4109472c                 or dword ptr [r15 + 0x2c], eax
0x499bc2: 498b4718                 mov rax, qword ptr [r15 + 0x18]
0x499bc6: 48898590efffff           mov qword ptr [rbp - 0x1070], rax
0x499bcd: e82e5edaff               call 0x23fa00
0x499bd2: 4c637310                 movsxd r14, dword ptr [rbx + 0x10]
0x499bd6: 4d85f6                   test r14, r14
0x499bd9: 0f8480000000             je 0x499c5f
0x499bdf: 4c8b6b08                 mov r13, qword ptr [rbx + 8]
0x499be3: 49c1e603                 shl r14, 3
0x499be7: 4531e4                   xor r12d, r12d
0x499bea: eb0d                     jmp 0x499bf9
0x499bec: 0f1f4000                 nop dword ptr [rax]
0x499bf0: 4983c408                 add r12, 8
0x499bf4: 4d39e6                   cmp r14, r12
0x499bf7: 7466                     je 0x499c5f
0x499bf9: 43ff542500               call qword ptr [r13 + r12]
0x499bfe: 4c397820                 cmp qword ptr [rax + 0x20], r15
0x499c02: 75ec                     jne 0x499bf0
0x499c04: 488b4818                 mov rcx, qword ptr [rax + 0x18]
0x499c08: 488dbd80efffff           lea rdi, [rbp - 0x1080]
0x499c0f: 488db590efffff           lea rsi, [rbp - 0x1070]
0x499c16: ba0b040000               mov edx, 0x40b
0x499c1b: 4c89fb                   mov rbx, r15
0x499c1e: 4d89f7                   mov r15, r14
0x499c21: 4989c6                   mov r14, rax
0x499c24: 48898d80efffff           mov qword ptr [rbp - 0x1080], rcx
0x499c2b: e8d05ddaff               call 0x23fa00
0x499c30: 488dbdb0f7ffff           lea rdi, [rbp - 0x850]
0x499c37: 488db590efffff           lea rsi, [rbp - 0x1070]
0x499c3e: ba05000000               mov edx, 5
0x499c43: b902000000               mov ecx, 2
0x499c48: 4c893424                 mov qword ptr [rsp], r14
0x499c4c: 4d89fe                   mov r14, r15
0x499c4f: 4989df                   mov r15, rbx
0x499c52: 4531c0                   xor r8d, r8d
0x499c55: 4531c9                   xor r9d, r9d
0x499c58: e8c318e6ff               call 0x2fb520
0x499c5d: eb91                     jmp 0x499bf0
0x499c5f: 488b0da282c908           mov rcx, qword ptr [rip + 0x8c982a2]
0x499c66: 488b01                   mov rax, qword ptr [rcx]
0x499c69: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x499c6d: 7558                     jne 0x499cc7
0x499c6f: 4881c478100000           add rsp, 0x1078
0x499c76: 5b                       pop rbx
0x499c77: 415c                     pop r12
0x499c79: 415d                     pop r13
0x499c7b: 415e                     pop r14
0x499c7d: 415f                     pop r15
0x499c7f: 5d                       pop rbp
0x499c80: c3                       ret 
0x499c81: 488d3dba176307           lea rdi, [rip + 0x76317ba] ; ['尮畒瑮浩⽥潃敲何橢捥⽴牐癩瑡⽥何橢捥⽴何橢捥䝴潬慢獬挮灰一瑯晩䍹慬獳楆楮桳摥敒楧瑳慲楴湯癅湥獴䔀敖獲䕥晦捥却瑥㨺汁潬慣整s牔湡汳瑡潩䙮汩整r牔湡晳牯䍭湯瑳慲湩t灓捡䉥牡䴀匀刀杩瑨潃浭湡d祈桰湥䜀浡灥摡䱟晥呴楲杧牥硁獩䜀浡灥摡䱟晥呴畨扭瑳捩k慇敭慰彤楒桧却楴正啟p瑓慥彭潔捵彨2楖敶剟杩瑨呟慲正慰彤灕䴀']
0x499c88: 488d153da13307           lea rdx, [rip + 0x733a13d] ; ['Illegal call to StaticFindObjectFast() while serializing object data!']
0x499c8f: bee0010000               mov esi, 0x1e0
0x499c94: 31c0                     xor eax, eax
0x499c96: e815b4cdff               call 0x1750b0
0x499c9b: e8600dedff               call 0x36aa00
0x499ca0: 84c0                     test al, al
0x499ca2: 0f84cffeffff             je 0x499b77
0x499ca8: 488d3d93176307           lea rdi, [rip + 0x7631793] ; ['尮畒瑮浩⽥潃敲何橢捥⽴牐癩瑡⽥何橢捥⽴何橢捥䝴潬慢獬挮灰一瑯晩䍹慬獳楆楮桳摥敒楧瑳慲楴湯癅湥獴䔀敖獲䕥晦捥却瑥㨺汁潬慣整s牔湡汳瑡潩䙮汩整r牔湡晳牯䍭湯瑳慲湩t灓捡䉥牡䴀匀刀杩瑨潃浭湡d祈桰湥䜀浡灥摡䱟晥呴楲杧牥硁獩䜀浡灥摡䱟晥呴畨扭瑳捩k慇敭慰彤楒桧却楴正啟p瑓慥彭潔捵彨2楖敶剟杩瑨呟慲正慰彤灕䴀']
0x499caf: 488d1500844807           lea rdx, [rip + 0x7488400] ; ['Illegal call to StaticFindObjectFast() while garbage collecting!']
0x499cb6: bee1010000               mov esi, 0x1e1
0x499cbb: 31c0                     xor eax, eax
0x499cbd: e8eeb3cdff               call 0x1750b0
0x499cc2: e9b0feffff               jmp 0x499b77
0x499cc7: e89481fd06               call 0x7471e60
0x499ccc: 0f0b                     ud2 
0x499cce: cc                       int3 
0x499ccf: cc                       int3 
0x499cd0: 55                       push rbp
0x499cd1: 4889e5                   mov rbp, rsp
0x499cd4: 488b07                   mov rax, qword ptr [rdi]
0x499cd7: ff5020                   call qword ptr [rax + 0x20]
0x499cda: 5d                       pop rbp
0x499cdb: c3                       ret 
0x499cdc: cc                       int3 
0x499cdd: cc                       int3 
0x499cde: cc                       int3 
0x499cdf: cc                       int3 
0x499ce0: 85f6                     test esi, esi
0x499ce2: 7461                     je 0x499d45
0x499ce4: 55                       push rbp
0x499ce5: 4889e5                   mov rbp, rsp
0x499ce8: 4156                     push r14
0x499cea: 53                       push rbx
0x499ceb: 4989fe                   mov r14, rdi
0x499cee: 89f3                     mov ebx, esi
0x499cf0: 4983c638                 add r14, 0x38
0x499cf4: eb12                     jmp 0x499d08
0x499cf6: 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x499d00: 4983c650                 add r14, 0x50
0x499d04: ffcb                     dec ebx
0x499d06: 7439                     je 0x499d41
0x499d08: 498b3e                   mov rdi, qword ptr [r14]
0x499d0b: 4885ff                   test rdi, rdi
0x499d0e: 7405                     je 0x499d15
0x499d10: e87bddc1ff               call 0xb7a90
0x499d15: 498b7ef0                 mov rdi, qword ptr [r14 - 0x10]
0x499d19: 4885ff                   test rdi, rdi
0x499d1c: 7405                     je 0x499d23
0x499d1e: e86dddc1ff               call 0xb7a90
0x499d23: 498b7ee0                 mov rdi, qword ptr [r14 - 0x20]
0x499d27: 4885ff                   test rdi, rdi
0x499d2a: 7405                     je 0x499d31
0x499d2c: e85fddc1ff               call 0xb7a90
0x499d31: 498b7ed0                 mov rdi, qword ptr [r14 - 0x30]
0x499d35: 4885ff                   test rdi, rdi
0x499d38: 74c6                     je 0x499d00
0x499d3a: e851ddc1ff               call 0xb7a90
0x499d3f: ebbf                     jmp 0x499d00
0x499d41: 5b                       pop rbx
0x499d42: 415e                     pop r14
0x499d44: 5d                       pop rbp
0x499d45: c3                       ret 
0x499d46: cc                       int3 
0x499d47: cc                       int3 
0x499d48: cc                       int3 
0x499d49: cc                       int3 
0x499d4a: cc                       int3 
0x499d4b: cc                       int3 
0x499d4c: cc                       int3 
0x499d4d: cc                       int3 
0x499d4e: cc                       int3 
0x499d4f: cc                       int3 
0x499d50: 55                       push rbp
0x499d51: 4889e5                   mov rbp, rsp
0x499d54: 53                       push rbx
0x499d55: 50                       push rax
0x499d56: c7474800000000           mov dword ptr [rdi + 0x48], 0
0x499d5d: 4889fb                   mov rbx, rdi
0x499d60: 488b7f40                 mov rdi, qword ptr [rdi + 0x40]
0x499d64: 4885ff                   test rdi, rdi
0x499d67: 7405                     je 0x499d6e
0x499d69: e822ddc1ff               call 0xb7a90
0x499d6e: c7430800000000           mov dword ptr [rbx + 8], 0
0x499d75: 837b0c00                 cmp dword ptr [rbx + 0xc], 0
0x499d79: 741b                     je 0x499d96
0x499d7b: 4c8d4b0c                 lea r9, [rbx + 0xc]
0x499d7f: bf28000000               mov edi, 0x28
0x499d84: be08000000               mov esi, 8
0x499d89: 4889d9                   mov rcx, rbx
0x499d8c: 31d2                     xor edx, edx
0x499d8e: 4531c0                   xor r8d, r8d
0x499d91: e88ac0b8ff               call 0x25e20
0x499d96: b8ffffffff               mov eax, 0xffffffff
0x499d9b: 48894330                 mov qword ptr [rbx + 0x30], rax
0x499d9f: c7432800000000           mov dword ptr [rbx + 0x28], 0
0x499da6: 817b2c81000000           cmp dword ptr [rbx + 0x2c], 0x81
0x499dad: 7212                     jb 0x499dc1
0x499daf: 488d7b10                 lea rdi, [rbx + 0x10]
0x499db3: 31f6                     xor esi, esi
0x499db5: c7432c80000000           mov dword ptr [rbx + 0x2c], 0x80
0x499dbc: e8efc0b8ff               call 0x25eb0
0x499dc1: 488b7b20                 mov rdi, qword ptr [rbx + 0x20]
0x499dc5: 4885ff                   test rdi, rdi
0x499dc8: 7405                     je 0x499dcf
0x499dca: e8c1dcc1ff               call 0xb7a90
0x499dcf: 488b3b                   mov rdi, qword ptr [rbx]
0x499dd2: 4885ff                   test rdi, rdi
0x499dd5: 7405                     je 0x499ddc
0x499dd7: e8b4dcc1ff               call 0xb7a90
0x499ddc: 4883c408                 add rsp, 8
0x499de0: 5b                       pop rbx
0x499de1: 5d                       pop rbp
0x499de2: c3                       ret 
0x499de3: cc                       int3 
0x499de4: cc                       int3 
0x499de5: cc                       int3 
0x499de6: cc                       int3 
0x499de7: cc                       int3 
0x499de8: cc                       int3 
0x499de9: cc                       int3 
0x499dea: cc                       int3 
0x499deb: cc                       int3 
0x499dec: cc                       int3 
0x499ded: cc                       int3 
0x499dee: cc                       int3 
0x499def: cc                       int3 
0x499df0: 55                       push rbp
0x499df1: 4889e5                   mov rbp, rsp
0x499df4: 4157                     push r15
0x499df6: 4156                     push r14
0x499df8: 4154                     push r12
0x499dfa: 53                       push rbx
0x499dfb: 4883ec20                 sub rsp, 0x20
0x499dff: 4c8b250281c908           mov r12, qword ptr [rip + 0x8c98102]
0x499e06: 4989f6                   mov r14, rsi
0x499e09: 4889fb                   mov rbx, rdi
0x499e0c: 488d75c8                 lea rsi, [rbp - 0x38]

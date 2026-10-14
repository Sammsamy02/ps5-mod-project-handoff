0x494470: 55                       push rbp
0x494471: 4889e5                   mov rbp, rsp
0x494474: 4157                     push r15
0x494476: 4156                     push r14
0x494478: 4155                     push r13
0x49447a: 4154                     push r12
0x49447c: 53                       push rbx
0x49447d: 4881ec68020000           sub rsp, 0x268
0x494484: 488b057ddac908           mov rax, qword ptr [rip + 0x8c9da7d]
0x49448b: 4989d6                   mov r14, rdx
0x49448e: 488b00                   mov rax, qword ptr [rax]
0x494491: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x494495: 4889b570fdffff           mov qword ptr [rbp - 0x290], rsi
0x49449c: 488b06                   mov rax, qword ptr [rsi]
0x49449f: 488d48f8                 lea rcx, [rax - 8]
0x4944a3: 48890e                   mov qword ptr [rsi], rcx
0x4944a6: 4c8b78f8                 mov r15, qword ptr [rax - 8]
0x4944aa: 410fb64718               movzx eax, byte ptr [r15 + 0x18]
0x4944af: 83e03f                   and eax, 0x3f
0x4944b2: ffc8                     dec eax
0x4944b4: 83f824                   cmp eax, 0x24
0x4944b7: 0f8716080000             ja 0x494cd3
0x4944bd: 488d0d88117a07           lea rcx, [rip + 0x77a1188]
0x4944c4: 48630481                 movsxd rax, dword ptr [rcx + rax*4]
0x4944c8: 4801c8                   add rax, rcx
0x4944cb: ffe0                     jmp rax
0x4944cd: 488b3f                   mov rdi, qword ptr [rdi]
0x4944d0: 4889bd68feffff           mov qword ptr [rbp - 0x198], rdi
0x4944d7: 40f6c701                 test dil, 1
0x4944db: 7417                     je 0x4944f4
0x4944dd: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4944e1: 7411                     je 0x4944f4
0x4944e3: 488d0572a9f008           lea rax, [rip + 0x8f0a972]
0x4944ea: 803801                   cmp byte ptr [rax], 1
0x4944ed: 7505                     jne 0x4944f4
0x4944ef: e8cc3dedff               call 0x3682c0
0x4944f4: 488dbd68feffff           lea rdi, [rbp - 0x198]
0x4944fb: 4c89fe                   mov rsi, r15
0x4944fe: e8dd150000               call 0x495ae0
0x494503: e901080000               jmp 0x494d09
0x494508: 488b3f                   mov rdi, qword ptr [rdi]
0x49450b: 4889bdb0fdffff           mov qword ptr [rbp - 0x250], rdi
0x494512: 40f6c701                 test dil, 1
0x494516: 7417                     je 0x49452f
0x494518: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x49451c: 7411                     je 0x49452f
0x49451e: 488d0537a9f008           lea rax, [rip + 0x8f0a937]
0x494525: 803801                   cmp byte ptr [rax], 1
0x494528: 7505                     jne 0x49452f
0x49452a: e8913dedff               call 0x3682c0
0x49452f: 488dbdb0fdffff           lea rdi, [rbp - 0x250]
0x494536: 4c89fe                   mov rsi, r15
0x494539: e8e2360000               call 0x497c20
0x49453e: e9c6070000               jmp 0x494d09
0x494543: 488b3f                   mov rdi, qword ptr [rdi]
0x494546: 4889bd20feffff           mov qword ptr [rbp - 0x1e0], rdi
0x49454d: 40f6c701                 test dil, 1
0x494551: 7417                     je 0x49456a
0x494553: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494557: 7411                     je 0x49456a
0x494559: 488d05fca8f008           lea rax, [rip + 0x8f0a8fc]
0x494560: 803801                   cmp byte ptr [rax], 1
0x494563: 7505                     jne 0x49456a
0x494565: e8563dedff               call 0x3682c0
0x49456a: 488dbd20feffff           lea rdi, [rbp - 0x1e0]
0x494571: 4c89fe                   mov rsi, r15
0x494574: e877220000               call 0x4967f0
0x494579: e98b070000               jmp 0x494d09
0x49457e: 488b3f                   mov rdi, qword ptr [rdi]
0x494581: 4889bd08feffff           mov qword ptr [rbp - 0x1f8], rdi
0x494588: 40f6c701                 test dil, 1
0x49458c: 7417                     je 0x4945a5
0x49458e: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494592: 7411                     je 0x4945a5
0x494594: 488d05c1a8f008           lea rax, [rip + 0x8f0a8c1]
0x49459b: 803801                   cmp byte ptr [rax], 1
0x49459e: 7505                     jne 0x4945a5
0x4945a0: e81b3dedff               call 0x3682c0
0x4945a5: 488dbd08feffff           lea rdi, [rbp - 0x1f8]
0x4945ac: 4c89fe                   mov rsi, r15
0x4945af: e88c260000               call 0x496c40
0x4945b4: 4989c4                   mov r12, rax
0x4945b7: 41bd02000000             mov r13d, 2
0x4945bd: e99a060000               jmp 0x494c5c
0x4945c2: 488b3f                   mov rdi, qword ptr [rdi]
0x4945c5: 4889bd48feffff           mov qword ptr [rbp - 0x1b8], rdi
0x4945cc: 40f6c701                 test dil, 1
0x4945d0: 7417                     je 0x4945e9
0x4945d2: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4945d6: 7411                     je 0x4945e9
0x4945d8: 488d057da8f008           lea rax, [rip + 0x8f0a87d]
0x4945df: 803801                   cmp byte ptr [rax], 1
0x4945e2: 7505                     jne 0x4945e9
0x4945e4: e8d73cedff               call 0x3682c0
0x4945e9: 488dbd48feffff           lea rdi, [rbp - 0x1b8]
0x4945f0: 4c89fe                   mov rsi, r15
0x4945f3: e8a81a0000               call 0x4960a0
0x4945f8: e90c070000               jmp 0x494d09
0x4945fd: 488b3f                   mov rdi, qword ptr [rdi]
0x494600: 4889bde8fdffff           mov qword ptr [rbp - 0x218], rdi
0x494607: 40f6c701                 test dil, 1
0x49460b: 7417                     je 0x494624
0x49460d: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494611: 7411                     je 0x494624
0x494613: 488d0542a8f008           lea rax, [rip + 0x8f0a842]
0x49461a: 803801                   cmp byte ptr [rax], 1
0x49461d: 7505                     jne 0x494624
0x49461f: e89c3cedff               call 0x3682c0
0x494624: 488dbde8fdffff           lea rdi, [rbp - 0x218]
0x49462b: 4c89fe                   mov rsi, r15
0x49462e: e8cd2b0000               call 0x497200
0x494633: e9d1060000               jmp 0x494d09
0x494638: 488b3f                   mov rdi, qword ptr [rdi]
0x49463b: 4889bda8fdffff           mov qword ptr [rbp - 0x258], rdi
0x494642: 40f6c701                 test dil, 1
0x494646: 7417                     je 0x49465f
0x494648: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x49464c: 7411                     je 0x49465f
0x49464e: 488d0507a8f008           lea rax, [rip + 0x8f0a807]
0x494655: 803801                   cmp byte ptr [rax], 1
0x494658: 7505                     jne 0x49465f
0x49465a: e8613cedff               call 0x3682c0
0x49465f: 488dbda8fdffff           lea rdi, [rbp - 0x258]
0x494666: 4c89fe                   mov rsi, r15
0x494669: e822370000               call 0x497d90
0x49466e: e996060000               jmp 0x494d09
0x494673: 488b3f                   mov rdi, qword ptr [rdi]
0x494676: 4889bdf8fdffff           mov qword ptr [rbp - 0x208], rdi
0x49467d: 40f6c701                 test dil, 1
0x494681: 7417                     je 0x49469a
0x494683: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494687: 7411                     je 0x49469a
0x494689: 488d05cca7f008           lea rax, [rip + 0x8f0a7cc]
0x494690: 803801                   cmp byte ptr [rax], 1
0x494693: 7505                     jne 0x49469a
0x494695: e8263cedff               call 0x3682c0
0x49469a: 488dbdf8fdffff           lea rdi, [rbp - 0x208]
0x4946a1: 4c89fe                   mov rsi, r15
0x4946a4: e877280000               call 0x496f20
0x4946a9: e95b060000               jmp 0x494d09
0x4946ae: 488b3f                   mov rdi, qword ptr [rdi]
0x4946b1: 4889bd60feffff           mov qword ptr [rbp - 0x1a0], rdi
0x4946b8: 40f6c701                 test dil, 1
0x4946bc: 7417                     je 0x4946d5
0x4946be: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4946c2: 7411                     je 0x4946d5
0x4946c4: 488d0591a7f008           lea rax, [rip + 0x8f0a791]
0x4946cb: 803801                   cmp byte ptr [rax], 1
0x4946ce: 7505                     jne 0x4946d5
0x4946d0: e8eb3bedff               call 0x3682c0
0x4946d5: 488dbd60feffff           lea rdi, [rbp - 0x1a0]
0x4946dc: 4c89fe                   mov rsi, r15
0x4946df: e86c150000               call 0x495c50
0x4946e4: e920060000               jmp 0x494d09
0x4946e9: 488b3f                   mov rdi, qword ptr [rdi]
0x4946ec: 4889bdd0fdffff           mov qword ptr [rbp - 0x230], rdi
0x4946f3: 40f6c701                 test dil, 1
0x4946f7: 7417                     je 0x494710
0x4946f9: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4946fd: 7411                     je 0x494710
0x4946ff: 488d0556a7f008           lea rax, [rip + 0x8f0a756]
0x494706: 803801                   cmp byte ptr [rax], 1
0x494709: 7505                     jne 0x494710
0x49470b: e8b03bedff               call 0x3682c0
0x494710: 488dbdd0fdffff           lea rdi, [rbp - 0x230]
0x494717: 4c89fe                   mov rsi, r15
0x49471a: e8312f0000               call 0x497650
0x49471f: e92f050000               jmp 0x494c53
0x494724: 488b3f                   mov rdi, qword ptr [rdi]
0x494727: 4889bd78feffff           mov qword ptr [rbp - 0x188], rdi
0x49472e: 40f6c701                 test dil, 1
0x494732: 7417                     je 0x49474b
0x494734: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494738: 7411                     je 0x49474b
0x49473a: 488d051ba7f008           lea rax, [rip + 0x8f0a71b]
0x494741: 803801                   cmp byte ptr [rax], 1
0x494744: 7505                     jne 0x49474b
0x494746: e8753bedff               call 0x3682c0
0x49474b: 488dbd78feffff           lea rdi, [rbp - 0x188]
0x494752: 4c89fe                   mov rsi, r15
0x494755: e8b6100000               call 0x495810
0x49475a: e9aa050000               jmp 0x494d09
0x49475f: 488b3f                   mov rdi, qword ptr [rdi]
0x494762: 4889bd00feffff           mov qword ptr [rbp - 0x200], rdi
0x494769: 40f6c701                 test dil, 1
0x49476d: 7417                     je 0x494786
0x49476f: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494773: 7411                     je 0x494786
0x494775: 488d05e0a6f008           lea rax, [rip + 0x8f0a6e0]
0x49477c: 803801                   cmp byte ptr [rax], 1
0x49477f: 7505                     jne 0x494786
0x494781: e83a3bedff               call 0x3682c0
0x494786: 488dbd00feffff           lea rdi, [rbp - 0x200]
0x49478d: 4c89fe                   mov rsi, r15
0x494790: e81b260000               call 0x496db0
0x494795: e9b9040000               jmp 0x494c53
0x49479a: 488b3f                   mov rdi, qword ptr [rdi]
0x49479d: 4889bd70feffff           mov qword ptr [rbp - 0x190], rdi
0x4947a4: 40f6c701                 test dil, 1
0x4947a8: 7417                     je 0x4947c1
0x4947aa: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4947ae: 7411                     je 0x4947c1
0x4947b0: 488d05a5a6f008           lea rax, [rip + 0x8f0a6a5]
0x4947b7: 803801                   cmp byte ptr [rax], 1
0x4947ba: 7505                     jne 0x4947c1
0x4947bc: e8ff3aedff               call 0x3682c0
0x4947c1: 488dbd70feffff           lea rdi, [rbp - 0x190]
0x4947c8: 4c89fe                   mov rsi, r15
0x4947cb: e8a0110000               call 0x495970
0x4947d0: e934050000               jmp 0x494d09
0x4947d5: 488b3f                   mov rdi, qword ptr [rdi]
0x4947d8: 4889bd90feffff           mov qword ptr [rbp - 0x170], rdi
0x4947df: 40f6c701                 test dil, 1
0x4947e3: 7417                     je 0x4947fc
0x4947e5: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4947e9: 7411                     je 0x4947fc
0x4947eb: 488d056aa6f008           lea rax, [rip + 0x8f0a66a]
0x4947f2: 803801                   cmp byte ptr [rax], 1
0x4947f5: 7505                     jne 0x4947fc
0x4947f7: e8c43aedff               call 0x3682c0
0x4947fc: 488dbd90feffff           lea rdi, [rbp - 0x170]
0x494803: 4c89fe                   mov rsi, r15
0x494806: e8e50b0000               call 0x4953f0
0x49480b: e9f9040000               jmp 0x494d09
0x494810: 488b3f                   mov rdi, qword ptr [rdi]
0x494813: 4889bd50feffff           mov qword ptr [rbp - 0x1b0], rdi
0x49481a: 40f6c701                 test dil, 1
0x49481e: 7417                     je 0x494837
0x494820: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494824: 7411                     je 0x494837
0x494826: 488d052fa6f008           lea rax, [rip + 0x8f0a62f]
0x49482d: 803801                   cmp byte ptr [rax], 1
0x494830: 7505                     jne 0x494837
0x494832: e8893aedff               call 0x3682c0
0x494837: 488dbd50feffff           lea rdi, [rbp - 0x1b0]
0x49483e: 4c89fe                   mov rsi, r15
0x494841: e8ea160000               call 0x495f30
0x494846: e9be040000               jmp 0x494d09
0x49484b: 488b3f                   mov rdi, qword ptr [rdi]
0x49484e: 4889bd18feffff           mov qword ptr [rbp - 0x1e8], rdi
0x494855: 40f6c701                 test dil, 1
0x494859: 7417                     je 0x494872
0x49485b: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x49485f: 7411                     je 0x494872
0x494861: 488d05f4a5f008           lea rax, [rip + 0x8f0a5f4]
0x494868: 803801                   cmp byte ptr [rax], 1
0x49486b: 7505                     jne 0x494872
0x49486d: e84e3aedff               call 0x3682c0
0x494872: 488dbd18feffff           lea rdi, [rbp - 0x1e8]
0x494879: 4c89fe                   mov rsi, r15
0x49487c: e8df200000               call 0x496960
0x494881: e983040000               jmp 0x494d09
0x494886: 488b3f                   mov rdi, qword ptr [rdi]
0x494889: 4889bd88feffff           mov qword ptr [rbp - 0x178], rdi
0x494890: 40f6c701                 test dil, 1
0x494894: 7417                     je 0x4948ad
0x494896: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x49489a: 7411                     je 0x4948ad
0x49489c: 488d05b9a5f008           lea rax, [rip + 0x8f0a5b9]
0x4948a3: 803801                   cmp byte ptr [rax], 1
0x4948a6: 7505                     jne 0x4948ad
0x4948a8: e8133aedff               call 0x3682c0
0x4948ad: 488dbd88feffff           lea rdi, [rbp - 0x178]
0x4948b4: 4c89fe                   mov rsi, r15
0x4948b7: e8940c0000               call 0x495550
0x4948bc: e948040000               jmp 0x494d09
0x4948c1: 488b3f                   mov rdi, qword ptr [rdi]
0x4948c4: 4889bdc8fdffff           mov qword ptr [rbp - 0x238], rdi
0x4948cb: 40f6c701                 test dil, 1
0x4948cf: 7417                     je 0x4948e8
0x4948d1: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4948d5: 7411                     je 0x4948e8
0x4948d7: 488d057ea5f008           lea rax, [rip + 0x8f0a57e]
0x4948de: 803801                   cmp byte ptr [rax], 1
0x4948e1: 7505                     jne 0x4948e8
0x4948e3: e8d839edff               call 0x3682c0
0x4948e8: 488dbdc8fdffff           lea rdi, [rbp - 0x238]
0x4948ef: 4c89fe                   mov rsi, r15
0x4948f2: e8d92e0000               call 0x4977d0
0x4948f7: e90d040000               jmp 0x494d09
0x4948fc: 488b3f                   mov rdi, qword ptr [rdi]
0x4948ff: 4889bdb8fdffff           mov qword ptr [rbp - 0x248], rdi
0x494906: 40f6c701                 test dil, 1
0x49490a: 7417                     je 0x494923
0x49490c: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494910: 7411                     je 0x494923
0x494912: 488d0543a5f008           lea rax, [rip + 0x8f0a543]
0x494919: 803801                   cmp byte ptr [rax], 1
0x49491c: 7505                     jne 0x494923
0x49491e: e89d39edff               call 0x3682c0
0x494923: 488dbdb8fdffff           lea rdi, [rbp - 0x248]
0x49492a: 4c89fe                   mov rsi, r15
0x49492d: e87e310000               call 0x497ab0
0x494932: e9d2030000               jmp 0x494d09
0x494937: 488b3f                   mov rdi, qword ptr [rdi]
0x49493a: 4889bd30feffff           mov qword ptr [rbp - 0x1d0], rdi
0x494941: 40f6c701                 test dil, 1
0x494945: 7417                     je 0x49495e
0x494947: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x49494b: 7411                     je 0x49495e
0x49494d: 488d0508a5f008           lea rax, [rip + 0x8f0a508]
0x494954: 803801                   cmp byte ptr [rax], 1
0x494957: 7505                     jne 0x49495e
0x494959: e86239edff               call 0x3682c0
0x49495e: 488dbd30feffff           lea rdi, [rbp - 0x1d0]
0x494965: 4c89fe                   mov rsi, r15
0x494968: e8931b0000               call 0x496500
0x49496d: e997030000               jmp 0x494d09
0x494972: 488b3f                   mov rdi, qword ptr [rdi]
0x494975: 4889bda8feffff           mov qword ptr [rbp - 0x158], rdi
0x49497c: 40f6c701                 test dil, 1
0x494980: 7417                     je 0x494999
0x494982: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494986: 7411                     je 0x494999
0x494988: 488d05cda4f008           lea rax, [rip + 0x8f0a4cd]
0x49498f: 803801                   cmp byte ptr [rax], 1
0x494992: 7505                     jne 0x494999
0x494994: e82739edff               call 0x3682c0
0x494999: 488dbda8feffff           lea rdi, [rbp - 0x158]
0x4949a0: 4c89fe                   mov rsi, r15
0x4949a3: e828060000               call 0x494fd0
0x4949a8: e95c030000               jmp 0x494d09
0x4949ad: 488b3f                   mov rdi, qword ptr [rdi]
0x4949b0: 4889bdf0fdffff           mov qword ptr [rbp - 0x210], rdi
0x4949b7: 40f6c701                 test dil, 1
0x4949bb: 7417                     je 0x4949d4
0x4949bd: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4949c1: 7411                     je 0x4949d4
0x4949c3: 488d0592a4f008           lea rax, [rip + 0x8f0a492]
0x4949ca: 803801                   cmp byte ptr [rax], 1
0x4949cd: 7505                     jne 0x4949d4
0x4949cf: e8ec38edff               call 0x3682c0
0x4949d4: 488dbdf0fdffff           lea rdi, [rbp - 0x210]
0x4949db: 4c89fe                   mov rsi, r15
0x4949de: e8ad260000               call 0x497090
0x4949e3: e921030000               jmp 0x494d09
0x4949e8: 488b3f                   mov rdi, qword ptr [rdi]
0x4949eb: 4889bd98feffff           mov qword ptr [rbp - 0x168], rdi
0x4949f2: 40f6c701                 test dil, 1
0x4949f6: 7417                     je 0x494a0f
0x4949f8: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x4949fc: 7411                     je 0x494a0f
0x4949fe: 488d0557a4f008           lea rax, [rip + 0x8f0a457]
0x494a05: 803801                   cmp byte ptr [rax], 1
0x494a08: 7505                     jne 0x494a0f
0x494a0a: e8b138edff               call 0x3682c0
0x494a0f: 488dbd98feffff           lea rdi, [rbp - 0x168]
0x494a16: 4c89fe                   mov rsi, r15
0x494a19: e872080000               call 0x495290
0x494a1e: e9e6020000               jmp 0x494d09
0x494a23: 488b3f                   mov rdi, qword ptr [rdi]
0x494a26: 4889bda0feffff           mov qword ptr [rbp - 0x160], rdi
0x494a2d: 40f6c701                 test dil, 1
0x494a31: 7417                     je 0x494a4a
0x494a33: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494a37: 7411                     je 0x494a4a
0x494a39: 488d051ca4f008           lea rax, [rip + 0x8f0a41c]
0x494a40: 803801                   cmp byte ptr [rax], 1
0x494a43: 7505                     jne 0x494a4a
0x494a45: e87638edff               call 0x3682c0
0x494a4a: 488dbda0feffff           lea rdi, [rbp - 0x160]
0x494a51: 4c89fe                   mov rsi, r15
0x494a54: e8d7060000               call 0x495130
0x494a59: e9ab020000               jmp 0x494d09
0x494a5e: 488b3f                   mov rdi, qword ptr [rdi]
0x494a61: 4889bd80feffff           mov qword ptr [rbp - 0x180], rdi
0x494a68: 40f6c701                 test dil, 1
0x494a6c: 7417                     je 0x494a85
0x494a6e: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494a72: 7411                     je 0x494a85
0x494a74: 488d05e1a3f008           lea rax, [rip + 0x8f0a3e1]
0x494a7b: 803801                   cmp byte ptr [rax], 1
0x494a7e: 7505                     jne 0x494a85
0x494a80: e83b38edff               call 0x3682c0
0x494a85: 488dbd80feffff           lea rdi, [rbp - 0x180]
0x494a8c: 4c89fe                   mov rsi, r15
0x494a8f: e81c0c0000               call 0x4956b0
0x494a94: e970020000               jmp 0x494d09
0x494a99: 488b3f                   mov rdi, qword ptr [rdi]
0x494a9c: 4889bd40feffff           mov qword ptr [rbp - 0x1c0], rdi
0x494aa3: 40f6c701                 test dil, 1
0x494aa7: 7417                     je 0x494ac0
0x494aa9: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494aad: 7411                     je 0x494ac0
0x494aaf: 488d05a6a3f008           lea rax, [rip + 0x8f0a3a6]
0x494ab6: 803801                   cmp byte ptr [rax], 1
0x494ab9: 7505                     jne 0x494ac0
0x494abb: e80038edff               call 0x3682c0
0x494ac0: 488dbd40feffff           lea rdi, [rbp - 0x1c0]
0x494ac7: 4c89fe                   mov rsi, r15
0x494aca: e841170000               call 0x496210
0x494acf: e935020000               jmp 0x494d09
0x494ad4: 488b3f                   mov rdi, qword ptr [rdi]
0x494ad7: 4889bd38feffff           mov qword ptr [rbp - 0x1c8], rdi
0x494ade: 40f6c701                 test dil, 1
0x494ae2: 7417                     je 0x494afb
0x494ae4: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494ae8: 7411                     je 0x494afb
0x494aea: 488d056ba3f008           lea rax, [rip + 0x8f0a36b]
0x494af1: 803801                   cmp byte ptr [rax], 1
0x494af4: 7505                     jne 0x494afb
0x494af6: e8c537edff               call 0x3682c0
0x494afb: 488dbd38feffff           lea rdi, [rbp - 0x1c8]
0x494b02: 4c89fe                   mov rsi, r15
0x494b05: e876180000               call 0x496380
0x494b0a: e9e7000000               jmp 0x494bf6
0x494b0f: 488b3f                   mov rdi, qword ptr [rdi]
0x494b12: 4889bdc0fdffff           mov qword ptr [rbp - 0x240], rdi
0x494b19: 40f6c701                 test dil, 1
0x494b1d: 7417                     je 0x494b36
0x494b1f: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494b23: 7411                     je 0x494b36
0x494b25: 488d0530a3f008           lea rax, [rip + 0x8f0a330]
0x494b2c: 803801                   cmp byte ptr [rax], 1
0x494b2f: 7505                     jne 0x494b36
0x494b31: e88a37edff               call 0x3682c0
0x494b36: 488dbdc0fdffff           lea rdi, [rbp - 0x240]
0x494b3d: 4c89fe                   mov rsi, r15
0x494b40: e8fb2d0000               call 0x497940
0x494b45: e909010000               jmp 0x494c53
0x494b4a: 488b3f                   mov rdi, qword ptr [rdi]
0x494b4d: 4889bd28feffff           mov qword ptr [rbp - 0x1d8], rdi
0x494b54: 40f6c701                 test dil, 1
0x494b58: 7417                     je 0x494b71
0x494b5a: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494b5e: 7411                     je 0x494b71
0x494b60: 488d05f5a2f008           lea rax, [rip + 0x8f0a2f5]
0x494b67: 803801                   cmp byte ptr [rax], 1
0x494b6a: 7505                     jne 0x494b71
0x494b6c: e84f37edff               call 0x3682c0
0x494b71: 488dbd28feffff           lea rdi, [rbp - 0x1d8]
0x494b78: 4c89fe                   mov rsi, r15
0x494b7b: e8001b0000               call 0x496680
0x494b80: e984010000               jmp 0x494d09
0x494b85: 488b3f                   mov rdi, qword ptr [rdi]
0x494b88: 4889bda0fdffff           mov qword ptr [rbp - 0x260], rdi
0x494b8f: 40f6c701                 test dil, 1
0x494b93: 7417                     je 0x494bac
0x494b95: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494b99: 7411                     je 0x494bac
0x494b9b: 488d05baa2f008           lea rax, [rip + 0x8f0a2ba]
0x494ba2: 803801                   cmp byte ptr [rax], 1
0x494ba5: 7505                     jne 0x494bac
0x494ba7: e81437edff               call 0x3682c0
0x494bac: 488dbda0fdffff           lea rdi, [rbp - 0x260]
0x494bb3: 4c89fe                   mov rsi, r15
0x494bb6: e845330000               call 0x497f00
0x494bbb: e993000000               jmp 0x494c53
0x494bc0: 488b3f                   mov rdi, qword ptr [rdi]
0x494bc3: 4889bd58feffff           mov qword ptr [rbp - 0x1a8], rdi
0x494bca: 40f6c701                 test dil, 1
0x494bce: 7417                     je 0x494be7
0x494bd0: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494bd4: 7411                     je 0x494be7
0x494bd6: 488d057fa2f008           lea rax, [rip + 0x8f0a27f]
0x494bdd: 803801                   cmp byte ptr [rax], 1
0x494be0: 7505                     jne 0x494be7
0x494be2: e8d936edff               call 0x3682c0
0x494be7: 488dbd58feffff           lea rdi, [rbp - 0x1a8]
0x494bee: 4c89fe                   mov rsi, r15
0x494bf1: e8ca110000               call 0x495dc0
0x494bf6: 4531ed                   xor r13d, r13d
0x494bf9: 41f6471840               test byte ptr [r15 + 0x18], 0x40
0x494bfe: 4989c4                   mov r12, rax
0x494c01: b101                     mov cl, 1
0x494c03: 0f8408010000             je 0x494d11
0x494c09: 48b80000000000000401     movabs rax, 0x104000000000000
0x494c13: 4909442438               or qword ptr [r12 + 0x38], rax
0x494c18: e9f4000000               jmp 0x494d11
0x494c1d: 488b3f                   mov rdi, qword ptr [rdi]
0x494c20: 4889bd10feffff           mov qword ptr [rbp - 0x1f0], rdi
0x494c27: 40f6c701                 test dil, 1
0x494c2b: 7417                     je 0x494c44
0x494c2d: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494c31: 7411                     je 0x494c44
0x494c33: 488d0522a2f008           lea rax, [rip + 0x8f0a222]
0x494c3a: 803801                   cmp byte ptr [rax], 1
0x494c3d: 7505                     jne 0x494c44
0x494c3f: e87c36edff               call 0x3682c0
0x494c44: 488dbd10feffff           lea rdi, [rbp - 0x1f0]
0x494c4b: 4c89fe                   mov rsi, r15
0x494c4e: e87d1e0000               call 0x496ad0
0x494c53: 4989c4                   mov r12, rax
0x494c56: 41bd01000000             mov r13d, 1
0x494c5c: 31c9                     xor ecx, ecx
0x494c5e: e9ae000000               jmp 0x494d11
0x494c63: 488b3f                   mov rdi, qword ptr [rdi]
0x494c66: 4889bdd8fdffff           mov qword ptr [rbp - 0x228], rdi
0x494c6d: 40f6c701                 test dil, 1
0x494c71: 7417                     je 0x494c8a
0x494c73: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494c77: 7411                     je 0x494c8a
0x494c79: 488d05dca1f008           lea rax, [rip + 0x8f0a1dc]
0x494c80: 803801                   cmp byte ptr [rax], 1
0x494c83: 7505                     jne 0x494c8a
0x494c85: e83636edff               call 0x3682c0
0x494c8a: 488dbdd8fdffff           lea rdi, [rbp - 0x228]
0x494c91: 4c89fe                   mov rsi, r15
0x494c94: e847280000               call 0x4974e0
0x494c99: eb6e                     jmp 0x494d09
0x494c9b: 488b3f                   mov rdi, qword ptr [rdi]
0x494c9e: 4889bde0fdffff           mov qword ptr [rbp - 0x220], rdi
0x494ca5: 40f6c701                 test dil, 1
0x494ca9: 7417                     je 0x494cc2
0x494cab: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494caf: 7411                     je 0x494cc2
0x494cb1: 488d05a4a1f008           lea rax, [rip + 0x8f0a1a4]
0x494cb8: 803801                   cmp byte ptr [rax], 1
0x494cbb: 7505                     jne 0x494cc2
0x494cbd: e8fe35edff               call 0x3682c0
0x494cc2: 488dbde0fdffff           lea rdi, [rbp - 0x220]
0x494cc9: 4c89fe                   mov rsi, r15
0x494ccc: e89f260000               call 0x497370
0x494cd1: eb36                     jmp 0x494d09
0x494cd3: 488b3f                   mov rdi, qword ptr [rdi]
0x494cd6: 4889bdb0feffff           mov qword ptr [rbp - 0x150], rdi
0x494cdd: 40f6c701                 test dil, 1
0x494ce1: 7417                     je 0x494cfa
0x494ce3: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494ce7: 7411                     je 0x494cfa
0x494ce9: 488d056ca1f008           lea rax, [rip + 0x8f0a16c]
0x494cf0: 803801                   cmp byte ptr [rax], 1
0x494cf3: 7505                     jne 0x494cfa
0x494cf5: e8c635edff               call 0x3682c0
0x494cfa: 488dbdb0feffff           lea rdi, [rbp - 0x150]
0x494d01: 4c89fe                   mov rsi, r15
0x494d04: e857010000               call 0x494e60
0x494d09: 4989c4                   mov r12, rax
0x494d0c: 4531ed                   xor r13d, r13d
0x494d0f: b101                     mov cl, 1
0x494d11: 410fb74730               movzx eax, word ptr [r15 + 0x30]
0x494d16: 418944242c               mov dword ptr [r12 + 0x2c], eax
0x494d1b: 4d8b7f08                 mov r15, qword ptr [r15 + 8]
0x494d1f: 4d85ff                   test r15, r15
0x494d22: 0f84cb000000             je 0x494df3
0x494d28: 4c89ff                   mov rdi, r15
0x494d2b: 898d8cfdffff             mov dword ptr [rbp - 0x274], ecx
0x494d31: 4c89b578fdffff           mov qword ptr [rbp - 0x288], r14
0x494d38: 48c745b800000000         mov qword ptr [rbp - 0x48], 0
0x494d40: e8abd3fd06               call 0x74720f0
0x494d45: 4c89fe                   mov rsi, r15
0x494d48: 89c2                     mov edx, eax
0x494d4a: 31ff                     xor edi, edi
0x494d4c: 4531f6                   xor r14d, r14d
0x494d4f: 48898580fdffff           mov qword ptr [rbp - 0x280], rax
0x494d56: e84584bfff               call 0x8d1a0
0x494d5b: 89c3                     mov ebx, eax
0x494d5d: 8945c8                   mov dword ptr [rbp - 0x38], eax
0x494d60: ffc3                     inc ebx
0x494d62: 81fb80000000             cmp ebx, 0x80
0x494d68: 7707                     ja 0x494d71
0x494d6a: 48837db800               cmp qword ptr [rbp - 0x48], 0
0x494d6f: 7421                     je 0x494d92
0x494d71: 81fb81000000             cmp ebx, 0x81
0x494d77: 488d7db8                 lea rdi, [rbp - 0x48]
0x494d7b: b902000000               mov ecx, 2
0x494d80: 440f43f3                 cmovae r14d, ebx
0x494d84: 31f6                     xor esi, esi
0x494d86: 4489f2                   mov edx, r14d
0x494d89: e8d2e1bcff               call 0x62f60
0x494d8e: 4c8b75b8                 mov r14, qword ptr [rbp - 0x48]
0x494d92: 488b8d80fdffff           mov rcx, qword ptr [rbp - 0x280]
0x494d99: 4d85f6                   test r14, r14
0x494d9c: 488dbdb8feffff           lea rdi, [rbp - 0x148]
0x494da3: 89de                     mov esi, ebx
0x494da5: 4c89fa                   mov rdx, r15
0x494da8: 490f45fe                 cmovne rdi, r14
0x494dac: 48897dc0                 mov qword ptr [rbp - 0x40], rdi
0x494db0: ffc1                     inc ecx
0x494db2: e8498dbfff               call 0x8db00
0x494db7: 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x494dbb: 488dbd98fdffff           lea rdi, [rbp - 0x268]
0x494dc2: ba01000000               mov edx, 1
0x494dc7: e8c487daff               call 0x23d590
0x494dcc: 488b8598fdffff           mov rax, qword ptr [rbp - 0x268]
0x494dd3: 4989442468               mov qword ptr [r12 + 0x68], rax
0x494dd8: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x494ddc: 4885ff                   test rdi, rdi
0x494ddf: 7405                     je 0x494de6
0x494de1: e8aa2cc2ff               call 0xb7a90
0x494de6: 4c8bb578fdffff           mov r14, qword ptr [rbp - 0x288]
0x494ded: 8b8d8cfdffff             mov ecx, dword ptr [rbp - 0x274]
0x494df3: 41ff0e                   dec dword ptr [r14]
0x494df6: 4c8bbd70fdffff           mov r15, qword ptr [rbp - 0x290]
0x494dfd: 84c9                     test cl, cl
0x494dff: 7529                     jne 0x494e2a
0x494e01: 488d9d90fdffff           lea rbx, [rbp - 0x270]
0x494e08: 0f1f840000000000         nop dword ptr [rax + rax]
0x494e10: 4889df                   mov rdi, rbx
0x494e13: 4c89fe                   mov rsi, r15
0x494e16: 4c89f2                   mov rdx, r14
0x494e19: 4c89a590fdffff           mov qword ptr [rbp - 0x270], r12
0x494e20: e84bf6ffff               call 0x494470
0x494e25: 41ffcd                   dec r13d
0x494e28: 75e6                     jne 0x494e10
0x494e2a: 488b05d7d0c908           mov rax, qword ptr [rip + 0x8c9d0d7]
0x494e31: 488b00                   mov rax, qword ptr [rax]
0x494e34: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x494e38: 7512                     jne 0x494e4c
0x494e3a: 4881c468020000           add rsp, 0x268
0x494e41: 5b                       pop rbx
0x494e42: 415c                     pop r12
0x494e44: 415d                     pop r13
0x494e46: 415e                     pop r14
0x494e48: 415f                     pop r15
0x494e4a: 5d                       pop rbp
0x494e4b: c3                       ret 
0x494e4c: e80fd0fd06               call 0x7471e60
0x494e51: 0f0b                     ud2 
0x494e53: cc                       int3 
0x494e54: cc                       int3 
0x494e55: cc                       int3 
0x494e56: cc                       int3 
0x494e57: cc                       int3 
0x494e58: cc                       int3 
0x494e59: cc                       int3 
0x494e5a: cc                       int3 
0x494e5b: cc                       int3 
0x494e5c: cc                       int3 
0x494e5d: cc                       int3 
0x494e5e: cc                       int3 
0x494e5f: cc                       int3 
0x494e60: 55                       push rbp
0x494e61: 4889e5                   mov rbp, rsp
0x494e64: 4157                     push r15
0x494e66: 4156                     push r14
0x494e68: 4155                     push r13
0x494e6a: 4154                     push r12
0x494e6c: 53                       push rbx
0x494e6d: 4883ec18                 sub rsp, 0x18
0x494e71: 4c8b2590d0c908           mov r12, qword ptr [rip + 0x8c9d090]
0x494e78: 4889f3                   mov rbx, rsi
0x494e7b: 4989ff                   mov r15, rdi
0x494e7e: 498b0424                 mov rax, qword ptr [r12]
0x494e82: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x494e86: 488b4628                 mov rax, qword ptr [rsi + 0x28]
0x494e8a: 480b4620                 or rax, qword ptr [rsi + 0x20]
0x494e8e: 7571                     jne 0x494f01
0x494e90: bf78000000               mov edi, 0x78
0x494e95: 31f6                     xor esi, esi
0x494e97: e8842bc2ff               call 0xb7a20
0x494e9c: 4989c6                   mov r14, rax
0x494e9f: e8dcb1f6ff               call 0x400080
0x494ea4: 4c89f7                   mov rdi, r14
0x494ea7: 4889c2                   mov rdx, rax
0x494eaa: 31f6                     xor esi, esi
0x494eac: e80ff7ecff               call 0x3645c0
0x494eb1: 488d0560aca507           lea rax, [rip + 0x7a5ac60]
0x494eb8: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x494ebc: c4c178114668             vmovups xmmword ptr [r14 + 0x68], xmm0
0x494ec2: 4883c010                 add rax, 0x10
0x494ec6: 498906                   mov qword ptr [r14], rax
0x494ec9: 498b3f                   mov rdi, qword ptr [r15]
0x494ecc: 48897dc0                 mov qword ptr [rbp - 0x40], rdi
0x494ed0: 40f6c701                 test dil, 1
0x494ed4: 7417                     je 0x494eed
0x494ed6: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x494eda: 7411                     je 0x494eed
0x494edc: 488d05799ff008           lea rax, [rip + 0x8f09f79]
0x494ee3: 803801                   cmp byte ptr [rax], 1
0x494ee6: 7505                     jne 0x494eed
0x494ee8: e8d333edff               call 0x3682c0
0x494eed: 488d75c0                 lea rsi, [rbp - 0x40]
0x494ef1: 4c89f7                   mov rdi, r14
0x494ef4: 4889da                   mov rdx, rbx
0x494ef7: e874b4f6ff               call 0x400370
0x494efc: e9a1000000               jmp 0x494fa2
0x494f01: bf88000000               mov edi, 0x88
0x494f06: 31f6                     xor esi, esi
0x494f08: e8132bc2ff               call 0xb7a20
0x494f0d: 4989c6                   mov r14, rax
0x494f10: e86bb1f6ff               call 0x400080
0x494f15: 4c89f7                   mov rdi, r14
0x494f18: 4889c2                   mov rdx, rax
0x494f1b: 31f6                     xor esi, esi
0x494f1d: e89ef6ecff               call 0x3645c0
0x494f22: 488d05efaba507           lea rax, [rip + 0x7a5abef]
0x494f29: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x494f2d: c4c178114668             vmovups xmmword ptr [r14 + 0x68], xmm0
0x494f33: 4883c010                 add rax, 0x10
0x494f37: 498906                   mov qword ptr [r14], rax
0x494f3a: 4d8b2f                   mov r13, qword ptr [r15]
0x494f3d: 41f6c501                 test r13b, 1
0x494f41: 7437                     je 0x494f7a
0x494f43: 4d89ef                   mov r15, r13
0x494f46: 4983e7fe                 and r15, 0xfffffffffffffffe
0x494f4a: 742e                     je 0x494f7a
0x494f4c: 488d05099ff008           lea rax, [rip + 0x8f09f09]
0x494f53: 803801                   cmp byte ptr [rax], 1
0x494f56: 750f                     jne 0x494f67
0x494f58: 4c89ff                   mov rdi, r15
0x494f5b: e86033edff               call 0x3682c0
0x494f60: 488d05f59ef008           lea rax, [rip + 0x8f09ef5]
0x494f67: 803801                   cmp byte ptr [rax], 1
0x494f6a: 4c896dc8                 mov qword ptr [rbp - 0x38], r13
0x494f6e: 750e                     jne 0x494f7e

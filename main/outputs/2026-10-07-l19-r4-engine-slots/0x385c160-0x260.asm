0x385c160: c10349                   rol dword ptr [rbx], 0x49
0x385c163: 8b3f                     mov edi, dword ptr [rdi]
0x385c165: 4885ff                   test rdi, rdi
0x385c168: 7508                     jne 0x385c172
0x385c16a: e8618083fc               call 0x941d0
0x385c16f: 498b3f                   mov rdi, qword ptr [r15]
0x385c172: 488b07                   mov rax, qword ptr [rdi]
0x385c175: 488d354ccbf303           lea rsi, [rip + 0x3f3cb4c] ; ['Engine.SupressWarningsInOnScreenDisplay']
0x385c17c: 488d15dd77cf05           lea rdx, [rip + 0x5cf77dd]
0x385c183: 488d0d6ac1cc03           lea rcx, [rip + 0x3ccc16a] ; ['0: Show both errors and warnings on screen, 1: Show only errors on screen (in either case only when DurationOfErrorsAndWarningsOnHUD is greater than z']
0x385c18a: 4531c0                   xor r8d, r8d
0x385c18d: ff5030                   call qword ptr [rax + 0x30]
0x385c190: 488d1dd177cf05           lea rbx, [rip + 0x5cf77d1]
0x385c197: 4889c6                   mov rsi, rax
0x385c19a: 4889df                   mov rdi, rbx
0x385c19d: e8deec7cfc               call 0x2ae80
0x385c1a2: 4c8d25cf0e6604           lea r12, [rip + 0x4660ecf]
0x385c1a9: 488d3df0f17afc           lea rdi, [rip - 0x3850e10]
0x385c1b0: 488d1549fea705           lea rdx, [rip + 0x5a7fe49]
0x385c1b7: 4889de                   mov rsi, rbx
0x385c1ba: 4c8925a777cf05           mov qword ptr [rip + 0x5cf77a7], r12
0x385c1c1: e80a5dc103               call 0x7471ed0
0x385c1c6: 498b3f                   mov rdi, qword ptr [r15]
0x385c1c9: 4885ff                   test rdi, rdi
0x385c1cc: 7508                     jne 0x385c1d6
0x385c1ce: e8fd7f83fc               call 0x941d0
0x385c1d3: 498b3f                   mov rdi, qword ptr [r15]
0x385c1d6: 488b07                   mov rax, qword ptr [rdi]
0x385c1d9: 488d35b43d1004           lea rsi, [rip + 0x4103db4] ; ['Engine.DelayTrimMemoryDuringMapLoadMode']
0x385c1e0: 488d159177cf05           lea rdx, [rip + 0x5cf7791]
0x385c1e7: 488d0d2acbf303           lea rcx, [rip + 0x3f3cb2a] ; ['0: TrimMemory during LoadMap as normal\n1: Delay TrimMemory until the end of LoadMap (initial boot up)\n2: Delay TrimMemory in _every_ LoadMap call']
0x385c1ee: 4531c0                   xor r8d, r8d
0x385c1f1: ff5030                   call qword ptr [rax + 0x30]
0x385c1f4: 488d1d8577cf05           lea rbx, [rip + 0x5cf7785]
0x385c1fb: 4889c6                   mov rsi, rax
0x385c1fe: 4889df                   mov rdi, rbx
0x385c201: e87aec7cfc               call 0x2ae80
0x385c206: 488d3d93f17afc           lea rdi, [rip - 0x3850e6d]
0x385c20d: 488d15ecfda705           lea rdx, [rip + 0x5a7fdec]
0x385c214: 4889de                   mov rsi, rbx
0x385c217: 4c89256277cf05           mov qword ptr [rip + 0x5cf7762], r12
0x385c21e: e8ad5cc103               call 0x7471ed0
0x385c223: 498b3f                   mov rdi, qword ptr [r15]
0x385c226: 4885ff                   test rdi, rdi
0x385c229: 7508                     jne 0x385c233
0x385c22b: e8a07f83fc               call 0x941d0
0x385c230: 498b3f                   mov rdi, qword ptr [r15]
0x385c233: 488b07                   mov rax, qword ptr [rdi]
0x385c236: 488d3595541504           lea rsi, [rip + 0x4155495] ; ['Engine.SuspendAssetStreamerBeforePerformingFullGC']
0x385c23d: 488d154c77cf05           lea rdx, [rip + 0x5cf774c]
0x385c244: 488d0dbfba1204           lea rcx, [rip + 0x412babf] ; ['Suspend asset streamer before performing a full GC (default: 0)']
0x385c24b: 4531c0                   xor r8d, r8d
0x385c24e: ff5030                   call qword ptr [rax + 0x30]
0x385c251: 488d1d4077cf05           lea rbx, [rip + 0x5cf7740]
0x385c258: 4889c6                   mov rsi, rax
0x385c25b: 4889df                   mov rdi, rbx
0x385c25e: e81dec7cfc               call 0x2ae80
0x385c263: 488d3d36f17afc           lea rdi, [rip - 0x3850eca]
0x385c26a: 488d158ffda705           lea rdx, [rip + 0x5a7fd8f]
0x385c271: 4889de                   mov rsi, rbx
0x385c274: 4c89251d77cf05           mov qword ptr [rip + 0x5cf771d], r12
0x385c27b: e8505cc103               call 0x7471ed0
0x385c280: 498b3f                   mov rdi, qword ptr [r15]
0x385c283: 4885ff                   test rdi, rdi
0x385c286: 7508                     jne 0x385c290
0x385c288: e8437f83fc               call 0x941d0
0x385c28d: 498b3f                   mov rdi, qword ptr [r15]
0x385c290: 488b07                   mov rax, qword ptr [rdi]
0x385c293: 488d35faf6d103           lea rsi, [rip + 0x3d1f6fa] ; ['Engine.VerifyLoadMapWorldCleanup.Severity.Shipping']
0x385c29a: 488d15b78aa805           lea rdx, [rip + 0x5a88ab7]
0x385c2a1: 488d0d18a0d403           lea rcx, [rip + 0x3d4a018] ; ['Controls severity of logging when the engine detects that a UWorld was leaked during LoadMap.\n0 - all reference tracing and logging is disabled\n1 - lo']
0x385c2a8: 4531c0                   xor r8d, r8d
0x385c2ab: ff5030                   call qword ptr [rax + 0x30]
0x385c2ae: 488d1df376cf05           lea rbx, [rip + 0x5cf76f3]
0x385c2b5: 4889c6                   mov rsi, rax
0x385c2b8: 4889df                   mov rdi, rbx
0x385c2bb: e8c0eb7cfc               call 0x2ae80
0x385c2c0: 488d3dd9f07afc           lea rdi, [rip - 0x3850f27]
0x385c2c7: 488d1532fda705           lea rdx, [rip + 0x5a7fd32]
0x385c2ce: 4889de                   mov rsi, rbx
0x385c2d1: 4c8925d076cf05           mov qword ptr [rip + 0x5cf76d0], r12
0x385c2d8: e8f35bc103               call 0x7471ed0
0x385c2dd: 498b3f                   mov rdi, qword ptr [r15]
0x385c2e0: 4885ff                   test rdi, rdi
0x385c2e3: 7508                     jne 0x385c2ed
0x385c2e5: e8e67e83fc               call 0x941d0
0x385c2ea: 498b3f                   mov rdi, qword ptr [r15]
0x385c2ed: 488b07                   mov rax, qword ptr [rdi]
0x385c2f0: 488d355d5c0b04           lea rsi, [rip + 0x40b5c5d] ; ['Engine.VerifyLoadMapWorldCleanup.TraceMode.Shipping']
0x385c2f7: 488d15ca76cf05           lea rdx, [rip + 0x5cf76ca]
0x385c2fe: 488d0d25f6f803           lea rcx, [rip + 0x3f8f625] ; ['Controls detail level of reference tracing when the engine detects that a world was leaked during LoadMap.\n0 - direct references only\n1 - full referen']
0x385c305: 4531c0                   xor r8d, r8d
0x385c308: ff5030                   call qword ptr [rax + 0x30]
0x385c30b: 488d1da676cf05           lea rbx, [rip + 0x5cf76a6]
0x385c312: 4889c6                   mov rsi, rax
0x385c315: 4889df                   mov rdi, rbx
0x385c318: e863eb7cfc               call 0x2ae80
0x385c31d: 4c8d35dcfca705           lea r14, [rip + 0x5a7fcdc]
0x385c324: 488d3d75f07afc           lea rdi, [rip - 0x3850f8b]
0x385c32b: 4889de                   mov rsi, rbx
0x385c32e: 4c89258376cf05           mov qword ptr [rip + 0x5cf7683], r12
0x385c335: 4c89f2                   mov rdx, r14
0x385c338: e8935bc103               call 0x7471ed0
0x385c33d: c4e279180546cd1e04       vbroadcastss xmm0, dword ptr [rip + 0x41ecd46]
0x385c346: b8ffffffff               mov eax, 0xffffffff
0x385c34b: c605ae76cf0500           mov byte ptr [rip + 0x5cf76ae], 0
0x385c352: 488d35b778cf05           lea rsi, [rip + 0x5cf78b7]
0x385c359: 488d3d802c82fc           lea rdi, [rip - 0x37dd380]
0x385c360: 4c89f2                   mov rdx, r14
0x385c363: 48c7059a78cf0500000000   mov qword ptr [rip + 0x5cf789a], 0
0x385c36e: 4889058f76cf05           mov qword ptr [rip + 0x5cf768f], rax
0x385c375: c6059076cf0504           mov byte ptr [rip + 0x5cf7690], 4
0x385c37c: 48c7058976cf05ffffffff   mov qword ptr [rip + 0x5cf7689], 0xffffffffffffffff
0x385c387: c5f813058976cf05         vmovlps qword ptr [rip + 0x5cf7689], xmm0
0x385c38f: c4e2791805b0c71e04       vbroadcastss xmm0, dword ptr [rip + 0x41ec7b0]
0x385c398: c5f813058476cf05         vmovlps qword ptr [rip + 0x5cf7684], xmm0
0x385c3a0: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x385c3a4: c5f811056478cf05         vmovups xmmword ptr [rip + 0x5cf7864], xmm0
0x385c3ac: c7057676cf05ffffffff     mov dword ptr [rip + 0x5cf7676], 0xffffffff

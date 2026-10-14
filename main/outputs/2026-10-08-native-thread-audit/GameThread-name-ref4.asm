0x3c2e1f0 file0x3c321f0 55 push rbp
0x3c2e1f1 file0x3c321f1 4889e5 mov rbp, rsp
0x3c2e1f4 file0x3c321f4 4157 push r15
0x3c2e1f6 file0x3c321f6 4156 push r14
0x3c2e1f8 file0x3c321f8 4155 push r13
0x3c2e1fa file0x3c321fa 4154 push r12
0x3c2e1fc file0x3c321fc 53 push rbx
0x3c2e1fd file0x3c321fd 4883ec28 sub rsp, 0x28
0x3c2e201 file0x3c32201 0fb605907c9305 movzx eax, byte ptr [rip + 0x5937c90] rip->0x9565e98
0x3c2e208 file0x3c32208 48897dd0 mov qword ptr [rbp - 0x30], rdi
0x3c2e20c file0x3c3220c 84c0 test al, al
0x3c2e20e file0x3c3220e 0f84a3020000 je 0x3c2e4b7
0x3c2e214 file0x3c32214 0fb605957c9305 movzx eax, byte ptr [rip + 0x5937c95] rip->0x9565eb0
0x3c2e21b file0x3c3221b 84c0 test al, al
0x3c2e21d file0x3c3221d 0f84ed020000 je 0x3c2e510
0x3c2e223 file0x3c32223 0fb6059e7c9305 movzx eax, byte ptr [rip + 0x5937c9e] rip->0x9565ec8
0x3c2e22a file0x3c3222a 84c0 test al, al
0x3c2e22c file0x3c3222c 0f8437030000 je 0x3c2e569
0x3c2e232 file0x3c32232 0fb605a77c9305 movzx eax, byte ptr [rip + 0x5937ca7] rip->0x9565ee0
0x3c2e239 file0x3c32239 84c0 test al, al
0x3c2e23b file0x3c3223b 0f8481030000 je 0x3c2e5c2
0x3c2e241 file0x3c32241 4883c768 add rdi, 0x68
0x3c2e245 file0x3c32245 48897dc0 mov qword ptr [rbp - 0x40], rdi
0x3c2e249 file0x3c32249 e8326543fc call 0x64780
0x3c2e24e file0x3c3224e 488b5dd0 mov rbx, qword ptr [rbp - 0x30]
0x3c2e252 file0x3c32252 8b4340 mov eax, dword ptr [rbx + 0x40]
0x3c2e255 file0x3c32255 4c8d5328 lea r10, [rbx + 0x28]
0x3c2e259 file0x3c32259 85c0 test eax, eax
0x3c2e25b file0x3c3225b 741e je 0x3c2e27b
0x3c2e25d file0x3c3225d 488b5338 mov rdx, qword ptr [rbx + 0x38]
0x3c2e261 file0x3c32261 4885d2 test rdx, rdx
0x3c2e264 file0x3c32264 490f44d2 cmove rdx, r10
0x3c2e268 file0x3c32268 8b32 mov esi, dword ptr [rdx]
0x3c2e26a file0x3c3226a 85f6 test esi, esi
0x3c2e26c file0x3c3226c 7423 je 0x3c2e291
0x3c2e26e file0x3c3226e b93e000000 mov ecx, 0x3e
0x3c2e273 file0x3c32273 4531ed xor r13d, r13d
0x3c2e276 file0x3c32276 4531ff xor r15d, r15d
0x3c2e279 file0x3c32279 eb61 jmp 0x3c2e2dc
0x3c2e27b file0x3c3227b be01000000 mov esi, 1
0x3c2e280 file0x3c32280 4531ed xor r13d, r13d
0x3c2e283 file0x3c32283 31c9 xor ecx, ecx
0x3c2e285 file0x3c32285 4531ff xor r15d, r15d
0x3c2e288 file0x3c32288 39c1 cmp ecx, eax
0x3c2e28a file0x3c3228a 7c6e jl 0x3c2e2fa
0x3c2e28c file0x3c3228c e907020000 jmp 0x3c2e498
0x3c2e291 file0x3c32291 8d48ff lea ecx, [rax - 1]
0x3c2e294 file0x3c32294 8d701e lea esi, [rax + 0x1e]
0x3c2e297 file0x3c32297 85c9 test ecx, ecx
0x3c2e299 file0x3c32299 0f49f1 cmovns esi, ecx
0x3c2e29c file0x3c3229c 4531ed xor r13d, r13d
0x3c2e29f file0x3c3229f 89f1 mov ecx, esi
0x3c2e2a1 file0x3c322a1 c1fe1f sar esi, 0x1f
0x3c2e2a4 file0x3c322a4 c1f905 sar ecx, 5
0x3c2e2a7 file0x3c322a7 c4e248f2f9 andn edi, esi, ecx
0x3c2e2ac file0x3c322ac b93e000000 mov ecx, 0x3e
0x3c2e2b1 file0x3c322b1 6666666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3c2e2c0 file0x3c322c0 4c39ef cmp rdi, r13
0x3c2e2c3 file0x3c322c3 0f84cf010000 je 0x3c2e498
0x3c2e2c9 file0x3c322c9 428b74aa04 mov esi, dword ptr [rdx + r13*4 + 4]
0x3c2e2ce file0x3c322ce 49ffc5 inc r13
0x3c2e2d1 file0x3c322d1 83c120 add ecx, 0x20
0x3c2e2d4 file0x3c322d4 85f6 test esi, esi
0x3c2e2d6 file0x3c322d6 74e8 je 0x3c2e2c0
0x3c2e2d8 file0x3c322d8 448d79c2 lea r15d, [rcx - 0x3e]
0x3c2e2dc file0x3c322dc c4e248f3de blsi esi, esi
0x3c2e2e1 file0x3c322e1 488d543601 lea rdx, [rsi + rsi + 1]
0x3c2e2e6 file0x3c322e6 f3480fbdd2 lzcnt rdx, rdx
0x3c2e2eb file0x3c322eb 29d1 sub ecx, edx
0x3c2e2ed file0x3c322ed 39c1 cmp ecx, eax
0x3c2e2ef file0x3c322ef 0f4dc8 cmovge ecx, eax
0x3c2e2f2 file0x3c322f2 39c1 cmp ecx, eax
0x3c2e2f4 file0x3c322f4 0f8d9e010000 jge 0x3c2e498
0x3c2e2fa file0x3c322fa 4c8b65c0 mov r12, qword ptr [rbp - 0x40]
0x3c2e2fe file0x3c322fe 41b8ffffffff mov r8d, 0xffffffff
0x3c2e304 file0x3c32304 4c8955b8 mov qword ptr [rbp - 0x48], r10
0x3c2e308 file0x3c32308 eb2d jmp 0x3c2e337
0x3c2e30a file0x3c3230a 458d6c1501 lea r13d, [r13 + rdx + 1]
0x3c2e30f file0x3c3230f 41b8ffffffff mov r8d, 0xffffffff
0x3c2e315 file0x3c32315 c4e248f3d9 blsi esi, ecx
0x3c2e31a file0x3c3231a 488d4c3601 lea rcx, [rsi + rsi + 1]
0x3c2e31f file0x3c3231f f3480fbdd1 lzcnt rdx, rcx
0x3c2e324 file0x3c32324 4489f9 mov ecx, r15d
0x3c2e327 file0x3c32327 29d1 sub ecx, edx
0x3c2e329 file0x3c32329 83c13e add ecx, 0x3e
0x3c2e32c file0x3c3232c 39c1 cmp ecx, eax
0x3c2e32e file0x3c3232e 0f4dc8 cmovge ecx, eax
0x3c2e331 file0x3c32331 0f8d61010000 jge 0x3c2e498
0x3c2e337 file0x3c32337 488b5318 mov rdx, qword ptr [rbx + 0x18]
0x3c2e33b file0x3c3233b 4c63f1 movsxd r14, ecx
0x3c2e33e file0x3c3233e 4b8d0cb6 lea rcx, [r14 + r14*4]
0x3c2e342 file0x3c32342 837cca1800 cmp dword ptr [rdx + rcx*8 + 0x18], 0
0x3c2e347 file0x3c32347 0f84de000000 je 0x3c2e42b
0x3c2e34d file0x3c3234d 488d04ca lea rax, [rdx + rcx*8]
0x3c2e351 file0x3c32351 448945cc mov dword ptr [rbp - 0x34], r8d
0x3c2e355 file0x3c32355 488975b0 mov qword ptr [rbp - 0x50], rsi
0x3c2e359 file0x3c32359 8b4808 mov ecx, dword ptr [rax + 8]
0x3c2e35c file0x3c3235c 488d053d7b9305 lea rax, [rip + 0x5937b3d] rip->0x9565ea0
0x3c2e363 file0x3c32363 85c9 test ecx, ecx
0x3c2e365 file0x3c32365 7420 je 0x3c2e387
0x3c2e367 file0x3c32367 83f901 cmp ecx, 1
0x3c2e36a file0x3c3236a 7514 jne 0x3c2e380
0x3c2e36c file0x3c3236c 488d05457b9305 lea rax, [rip + 0x5937b45] rip->0x9565eb8
0x3c2e373 file0x3c32373 eb12 jmp 0x3c2e387
0x3c2e380 file0x3c32380 488d05497b9305 lea rax, [rip + 0x5937b49] rip->0x9565ed0
0x3c2e387 file0x3c32387 8b5008 mov edx, dword ptr [rax + 8]
0x3c2e38a file0x3c3238a 488b30 mov rsi, qword ptr [rax]
0x3c2e38d file0x3c3238d b800000000 mov eax, 0
0x3c2e392 file0x3c32392 4c89e7 mov rdi, r12
0x3c2e395 file0x3c32395 83ea01 sub edx, 1
0x3c2e398 file0x3c32398 0f42d0 cmovb edx, eax
0x3c2e39b file0x3c3239b e8e06743fc call 0x64b80
0x3c2e3a0 file0x3c323a0 4c89e7 mov rdi, r12
0x3c2e3a3 file0x3c323a3 be28000000 mov esi, 0x28
0x3c2e3a8 file0x3c323a8 e8f36443fc call 0x648a0
0x3c2e3ad file0x3c323ad 488b5dd0 mov rbx, qword ptr [rbp - 0x30]
0x3c2e3b1 file0x3c323b1 49c1e603 shl r14, 3
0x3c2e3b5 file0x3c323b5 4c89e7 mov rdi, r12
0x3c2e3b8 file0x3c323b8 4f8d34b6 lea r14, [r14 + r14*4]
0x3c2e3bc file0x3c323bc 488b4318 mov rax, qword ptr [rbx + 0x18]
0x3c2e3c0 file0x3c323c0 428b3430 mov esi, dword ptr [rax + r14]
0x3c2e3c4 file0x3c323c4 e847ac43fc call 0x69010
0x3c2e3c9 file0x3c323c9 4c89e7 mov rdi, r12
0x3c2e3cc file0x3c323cc 488d3509fef503 lea rsi, [rip + 0x3f5fe09] rip->0x7b8e1dc
0x3c2e3d3 file0x3c323d3 ba02000000 mov edx, 2
0x3c2e3d8 file0x3c323d8 e8336543fc call 0x64910
0x3c2e3dd file0x3c323dd 488b4318 mov rax, qword ptr [rbx + 0x18]
0x3c2e3e1 file0x3c323e1 4c89e7 mov rdi, r12
0x3c2e3e4 file0x3c323e4 428b4c3018 mov ecx, dword ptr [rax + r14 + 0x18]
0x3c2e3e9 file0x3c323e9 4a8b443010 mov rax, qword ptr [rax + r14 + 0x10]
0x3c2e3ee file0x3c323ee ffc9 dec ecx
0x3c2e3f0 file0x3c323f0 4863c9 movsxd rcx, ecx
0x3c2e3f3 file0x3c323f3 48c1e104 shl rcx, 4
0x3c2e3f7 file0x3c323f7 8b540808 mov edx, dword ptr [rax + rcx + 8]
0x3c2e3fb file0x3c323fb 488b3408 mov rsi, qword ptr [rax + rcx]
0x3c2e3ff file0x3c323ff b800000000 mov eax, 0
0x3c2e404 file0x3c32404 83ea01 sub edx, 1
0x3c2e407 file0x3c32407 0f42d0 cmovb edx, eax
0x3c2e40a file0x3c3240a e8716743fc call 0x64b80
0x3c2e40f file0x3c3240f 4c89e7 mov rdi, r12
0x3c2e412 file0x3c32412 be0a000000 mov esi, 0xa
0x3c2e417 file0x3c32417 e8846443fc call 0x648a0
0x3c2e41c file0x3c3241c 8b4340 mov eax, dword ptr [rbx + 0x40]
0x3c2e41f file0x3c3241f 4c8b55b8 mov r10, qword ptr [rbp - 0x48]
0x3c2e423 file0x3c32423 488b75b0 mov rsi, qword ptr [rbp - 0x50]
0x3c2e427 file0x3c32427 448b45cc mov r8d, dword ptr [rbp - 0x34]
0x3c2e42b file0x3c3242b 488b5338 mov rdx, qword ptr [rbx + 0x38]
0x3c2e42f file0x3c3242f c44248f2c0 andn r8d, esi, r8d
0x3c2e434 file0x3c32434 4963fd movsxd rdi, r13d
0x3c2e437 file0x3c32437 4885d2 test rdx, rdx
0x3c2e43a file0x3c3243a 490f44d2 cmove rdx, r10
0x3c2e43e file0x3c3243e 8b0cba mov ecx, dword ptr [rdx + rdi*4]
0x3c2e441 file0x3c32441 4421c1 and ecx, r8d
0x3c2e444 file0x3c32444 0f85cbfeffff jne 0x3c2e315
0x3c2e44a file0x3c3244a 8d70ff lea esi, [rax - 1]
0x3c2e44d file0x3c3244d 8d481e lea ecx, [rax + 0x1e]
0x3c2e450 file0x3c32450 85f6 test esi, esi
0x3c2e452 file0x3c32452 0f49ce cmovns ecx, esi
0x3c2e455 file0x3c32455 c1f905 sar ecx, 5
0x3c2e458 file0x3c32458 4139cd cmp r13d, ecx
0x3c2e45b file0x3c3245b 410f4fcd cmovg ecx, r13d
0x3c2e45f file0x3c3245f 7d37 jge 0x3c2e498
0x3c2e461 file0x3c32461 4863f1 movsxd rsi, ecx
0x3c2e464 file0x3c32464 4c8d44ba04 lea r8, [rdx + rdi*4 + 4]
0x3c2e469 file0x3c32469 4183c720 add r15d, 0x20
0x3c2e46d file0x3c3246d 31d2 xor edx, edx
0x3c2e46f file0x3c3246f 4829fe sub rsi, rdi
0x3c2e472 file0x3c32472 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3c2e480 file0x3c32480 418b0c90 mov ecx, dword ptr [r8 + rdx*4]
0x3c2e484 file0x3c32484 85c9 test ecx, ecx
0x3c2e486 file0x3c32486 0f857efeffff jne 0x3c2e30a
0x3c2e48c file0x3c3248c 4183c720 add r15d, 0x20
0x3c2e490 file0x3c32490 48ffc2 inc rdx
0x3c2e493 file0x3c32493 4839d6 cmp rsi, rdx
0x3c2e496 file0x3c32496 75e8 jne 0x3c2e480
0x3c2e498 file0x3c32498 488b75c0 mov rsi, qword ptr [rbp - 0x40]
0x3c2e49c file0x3c3249c 488d3de5799305 lea rdi, [rip + 0x59379e5] rip->0x9565e88
0x3c2e4a3 file0x3c324a3 e8782645fc call 0x80b20
0x3c2e4a8 file0x3c324a8 4883c428 add rsp, 0x28
0x3c2e4ac file0x3c324ac 5b pop rbx
0x3c2e4ad file0x3c324ad 415c pop r12
0x3c2e4af file0x3c324af 415d pop r13
0x3c2e4b1 file0x3c324b1 415e pop r14
0x3c2e4b3 file0x3c324b3 415f pop r15
0x3c2e4b5 file0x3c324b5 5d pop rbp
0x3c2e4b6 file0x3c324b6 c3 ret 
0x3c2e4b7 file0x3c324b7 488d3dda799305 lea rdi, [rip + 0x59379da] rip->0x9565e98
0x3c2e4be file0x3c324be e8ed398403 call 0x7471eb0
0x3c2e4c3 file0x3c324c3 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x3c2e4c7 file0x3c324c7 85c0 test eax, eax
0x3c2e4c9 file0x3c324c9 0f8445fdffff je 0x3c2e214
0x3c2e4cf file0x3c324cf 4c8d35b2799305 lea r14, [rip + 0x59379b2] rip->0x9565e88
0x3c2e4d6 file0x3c324d6 488d353701b403 lea rsi, [rip + 0x3b40137] rip->0x776e614
0x3c2e4dd file0x3c324dd 4c89f7 mov rdi, r14
0x3c2e4e0 file0x3c324e0 e80b5a43fc call 0x63ef0
0x3c2e4e5 file0x3c324e5 488d3d945e3dfc lea rdi, [rip - 0x3c2a16c] rip->0x4380
0x3c2e4ec file0x3c324ec 488d150ddb6a05 lea rdx, [rip + 0x56adb0d] rip->0x92dc000
0x3c2e4f3 file0x3c324f3 4c89f6 mov rsi, r14
0x3c2e4f6 file0x3c324f6 e8d5398403 call 0x7471ed0
0x3c2e4fb file0x3c324fb 488d3d96799305 lea rdi, [rip + 0x5937996] rip->0x9565e98
0x3c2e502 file0x3c32502 e8b9398403 call 0x7471ec0
0x3c2e507 file0x3c32507 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x3c2e50b file0x3c3250b e904fdffff jmp 0x3c2e214
0x3c2e510 file0x3c32510 488d3d99799305 lea rdi, [rip + 0x5937999] rip->0x9565eb0
0x3c2e517 file0x3c32517 e894398403 call 0x7471eb0
0x3c2e51c file0x3c3251c 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x3c2e520 file0x3c32520 85c0 test eax, eax
0x3c2e522 file0x3c32522 0f84fbfcffff je 0x3c2e223
0x3c2e528 file0x3c32528 4c8d3571799305 lea r14, [rip + 0x5937971] rip->0x9565ea0
0x3c2e52f file0x3c3252f 488d35b688aa03 lea rsi, [rip + 0x3aa88b6] rip->0x76d6dec
0x3c2e536 file0x3c32536 4c89f7 mov rdi, r14
0x3c2e539 file0x3c32539 e8b25943fc call 0x63ef0
0x3c2e53e file0x3c3253e 488d3d3b5e3dfc lea rdi, [rip - 0x3c2a1c5] rip->0x4380
0x3c2e545 file0x3c32545 488d15b4da6a05 lea rdx, [rip + 0x56adab4] rip->0x92dc000
0x3c2e54c file0x3c3254c 4c89f6 mov rsi, r14
0x3c2e54f file0x3c3254f e87c398403 call 0x7471ed0
0x3c2e554 file0x3c32554 488d3d55799305 lea rdi, [rip + 0x5937955] rip->0x9565eb0
0x3c2e55b file0x3c3255b e860398403 call 0x7471ec0
0x3c2e560 file0x3c32560 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x3c2e564 file0x3c32564 e9bafcffff jmp 0x3c2e223
0x3c2e569 file0x3c32569 488d3d58799305 lea rdi, [rip + 0x5937958] rip->0x9565ec8
0x3c2e570 file0x3c32570 e83b398403 call 0x7471eb0
0x3c2e575 file0x3c32575 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x3c2e579 file0x3c32579 85c0 test eax, eax
0x3c2e57b file0x3c3257b 0f84b1fcffff je 0x3c2e232
0x3c2e581 file0x3c32581 4c8d3530799305 lea r14, [rip + 0x5937930] rip->0x9565eb8
0x3c2e588 file0x3c32588 488d352bd9a203 lea rsi, [rip + 0x3a2d92b] rip->0x765beba
0x3c2e58f file0x3c3258f 4c89f7 mov rdi, r14
0x3c2e592 file0x3c32592 e8595943fc call 0x63ef0
0x3c2e597 file0x3c32597 488d3de25d3dfc lea rdi, [rip - 0x3c2a21e] rip->0x4380
0x3c2e59e file0x3c3259e 488d155bda6a05 lea rdx, [rip + 0x56ada5b] rip->0x92dc000
0x3c2e5a5 file0x3c325a5 4c89f6 mov rsi, r14
0x3c2e5a8 file0x3c325a8 e823398403 call 0x7471ed0
0x3c2e5ad file0x3c325ad 488d3d14799305 lea rdi, [rip + 0x5937914] rip->0x9565ec8

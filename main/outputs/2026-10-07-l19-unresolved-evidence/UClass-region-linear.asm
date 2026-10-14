0x320000 51 push rcx
0x320001 087671 or byte ptr [rsi + 0x71], dh
0x320004 488d75a0 lea rsi, [rbp - 0x60]
0x320008 4c89f7 mov rdi, r14
0x32000b e88053feff call 0x305390
0x320010 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x320014 8b03 mov eax, dword ptr [rbx]
0x320016 eb67 jmp 0x32007f
0x320018 8b36 mov esi, dword ptr [rsi]
0x32001a 8975a0 mov dword ptr [rbp - 0x60], esi
0x32001d 488911 mov qword ptr [rcx], rdx
0x320020 41f6450004 test byte ptr [r13], 4
0x320025 750d jne 0x320034
0x320027 498b09 mov rcx, qword ptr [r9]
0x32002a 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x32002d 4898 cdqe 
0x32002f 891401 mov dword ptr [rcx + rax], edx
0x320032 8b03 mov eax, dword ptr [rbx]
0x320034 83c004 add eax, 4
0x320037 8903 mov dword ptr [rbx], eax
0x320039 41f6450001 test byte ptr [r13], 1
0x32003e 7510 jne 0x320050
0x320040 498b09 mov rcx, qword ptr [r9]
0x320043 4863d0 movsxd rdx, eax
0x320046 c5fa100411 vmovss xmm0, dword ptr [rcx + rdx]
0x32004b c5fa1145a0 vmovss dword ptr [rbp - 0x60], xmm0
0x320050 498b4e08 mov rcx, qword ptr [r14 + 8]
0x320054 488b31 mov rsi, qword ptr [rcx]
0x320057 488d5604 lea rdx, [rsi + 4]
0x32005b 483b5108 cmp rdx, qword ptr [rcx + 8]
0x32005f 7675 jbe 0x3200d6
0x320061 488d75a0 lea rsi, [rbp - 0x60]
0x320065 4c89f7 mov rdi, r14
0x320068 e8f314d3ff call 0x51560
0x32006d 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x320071 8b03 mov eax, dword ptr [rbx]
0x320073 eb69 jmp 0x3200de
0x320075 488b36 mov rsi, qword ptr [rsi]
0x320078 488975a0 mov qword ptr [rbp - 0x60], rsi
0x32007c 488911 mov qword ptr [rcx], rdx
0x32007f 41f6450004 test byte ptr [r13], 4
0x320084 750f jne 0x320095
0x320086 498b09 mov rcx, qword ptr [r9]
0x320089 488b55a0 mov rdx, qword ptr [rbp - 0x60]
0x32008d 4898 cdqe 
0x32008f 48891401 mov qword ptr [rcx + rax], rdx
0x320093 8b03 mov eax, dword ptr [rbx]
0x320095 83c008 add eax, 8
0x320098 8903 mov dword ptr [rbx], eax
0x32009a 41f6450001 test byte ptr [r13], 1
0x32009f 7510 jne 0x3200b1
0x3200a1 498b09 mov rcx, qword ptr [r9]
0x3200a4 4863d0 movsxd rdx, eax
0x3200a7 c5fb100411 vmovsd xmm0, qword ptr [rcx + rdx]
0x3200ac c5fb1145a0 vmovsd qword ptr [rbp - 0x60], xmm0
0x3200b1 498b4e08 mov rcx, qword ptr [r14 + 8]
0x3200b5 488b31 mov rsi, qword ptr [rcx]
0x3200b8 488d5608 lea rdx, [rsi + 8]
0x3200bc 483b5108 cmp rdx, qword ptr [rcx + 8]
0x3200c0 7671 jbe 0x320133
0x3200c2 488d75a0 lea rsi, [rbp - 0x60]
0x3200c6 4c89f7 mov rdi, r14
0x3200c9 e8c252feff call 0x305390
0x3200ce 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x3200d2 8b03 mov eax, dword ptr [rbx]
0x3200d4 eb67 jmp 0x32013d
0x3200d6 8b36 mov esi, dword ptr [rsi]
0x3200d8 8975a0 mov dword ptr [rbp - 0x60], esi
0x3200db 488911 mov qword ptr [rcx], rdx
0x3200de 41f6450004 test byte ptr [r13], 4
0x3200e3 750d jne 0x3200f2
0x3200e5 498b09 mov rcx, qword ptr [r9]
0x3200e8 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x3200eb 4898 cdqe 
0x3200ed 891401 mov dword ptr [rcx + rax], edx
0x3200f0 8b03 mov eax, dword ptr [rbx]
0x3200f2 83c004 add eax, 4
0x3200f5 8903 mov dword ptr [rbx], eax
0x3200f7 41f6450001 test byte ptr [r13], 1
0x3200fc 7510 jne 0x32010e
0x3200fe 498b09 mov rcx, qword ptr [r9]
0x320101 4863d0 movsxd rdx, eax
0x320104 c5fa100411 vmovss xmm0, dword ptr [rcx + rdx]
0x320109 c5fa1145a0 vmovss dword ptr [rbp - 0x60], xmm0
0x32010e 498b4e08 mov rcx, qword ptr [r14 + 8]
0x320112 488b31 mov rsi, qword ptr [rcx]
0x320115 488d5604 lea rdx, [rsi + 4]
0x320119 483b5108 cmp rdx, qword ptr [rcx + 8]
0x32011d 7675 jbe 0x320194
0x32011f 488d75a0 lea rsi, [rbp - 0x60]
0x320123 4c89f7 mov rdi, r14
0x320126 e83514d3ff call 0x51560
0x32012b 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x32012f 8b03 mov eax, dword ptr [rbx]
0x320131 eb69 jmp 0x32019c
0x320133 488b36 mov rsi, qword ptr [rsi]
0x320136 488975a0 mov qword ptr [rbp - 0x60], rsi
0x32013a 488911 mov qword ptr [rcx], rdx
0x32013d 41f6450004 test byte ptr [r13], 4
0x320142 750f jne 0x320153
0x320144 498b09 mov rcx, qword ptr [r9]
0x320147 488b55a0 mov rdx, qword ptr [rbp - 0x60]
0x32014b 4898 cdqe 
0x32014d 48891401 mov qword ptr [rcx + rax], rdx
0x320151 8b03 mov eax, dword ptr [rbx]
0x320153 83c008 add eax, 8
0x320156 8903 mov dword ptr [rbx], eax
0x320158 41f6450001 test byte ptr [r13], 1
0x32015d 7510 jne 0x32016f
0x32015f 498b09 mov rcx, qword ptr [r9]
0x320162 4863d0 movsxd rdx, eax
0x320165 c5fb100411 vmovsd xmm0, qword ptr [rcx + rdx]
0x32016a c5fb1145a0 vmovsd qword ptr [rbp - 0x60], xmm0
0x32016f 498b4e08 mov rcx, qword ptr [r14 + 8]
0x320173 488b31 mov rsi, qword ptr [rcx]
0x320176 488d5608 lea rdx, [rsi + 8]
0x32017a 483b5108 cmp rdx, qword ptr [rcx + 8]
0x32017e 7671 jbe 0x3201f1
0x320180 488d75a0 lea rsi, [rbp - 0x60]
0x320184 4c89f7 mov rdi, r14
0x320187 e80452feff call 0x305390
0x32018c 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x320190 8b03 mov eax, dword ptr [rbx]
0x320192 eb67 jmp 0x3201fb
0x320194 8b36 mov esi, dword ptr [rsi]
0x320196 8975a0 mov dword ptr [rbp - 0x60], esi
0x320199 488911 mov qword ptr [rcx], rdx
0x32019c 41f6450004 test byte ptr [r13], 4
0x3201a1 750d jne 0x3201b0
0x3201a3 498b09 mov rcx, qword ptr [r9]
0x3201a6 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x3201a9 4898 cdqe 
0x3201ab 891401 mov dword ptr [rcx + rax], edx
0x3201ae 8b03 mov eax, dword ptr [rbx]
0x3201b0 83c004 add eax, 4
0x3201b3 8903 mov dword ptr [rbx], eax
0x3201b5 41f6450001 test byte ptr [r13], 1
0x3201ba 7510 jne 0x3201cc
0x3201bc 498b09 mov rcx, qword ptr [r9]
0x3201bf 4863d0 movsxd rdx, eax
0x3201c2 c5fa100411 vmovss xmm0, dword ptr [rcx + rdx]
0x3201c7 c5fa1145a0 vmovss dword ptr [rbp - 0x60], xmm0
0x3201cc 498b4e08 mov rcx, qword ptr [r14 + 8]
0x3201d0 488b31 mov rsi, qword ptr [rcx]
0x3201d3 488d5604 lea rdx, [rsi + 4]
0x3201d7 483b5108 cmp rdx, qword ptr [rcx + 8]
0x3201db 7675 jbe 0x320252
0x3201dd 488d75a0 lea rsi, [rbp - 0x60]
0x3201e1 4c89f7 mov rdi, r14
0x3201e4 e87713d3ff call 0x51560
0x3201e9 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x3201ed 8b03 mov eax, dword ptr [rbx]
0x3201ef eb69 jmp 0x32025a
0x3201f1 488b36 mov rsi, qword ptr [rsi]
0x3201f4 488975a0 mov qword ptr [rbp - 0x60], rsi
0x3201f8 488911 mov qword ptr [rcx], rdx
0x3201fb 41f6450004 test byte ptr [r13], 4
0x320200 750f jne 0x320211
0x320202 498b09 mov rcx, qword ptr [r9]
0x320205 488b55a0 mov rdx, qword ptr [rbp - 0x60]
0x320209 4898 cdqe 
0x32020b 48891401 mov qword ptr [rcx + rax], rdx
0x32020f 8b03 mov eax, dword ptr [rbx]
0x320211 83c008 add eax, 8
0x320214 8903 mov dword ptr [rbx], eax
0x320216 41f6450001 test byte ptr [r13], 1
0x32021b 7510 jne 0x32022d
0x32021d 498b09 mov rcx, qword ptr [r9]
0x320220 4863d0 movsxd rdx, eax
0x320223 c5fb100411 vmovsd xmm0, qword ptr [rcx + rdx]
0x320228 c5fb1145a0 vmovsd qword ptr [rbp - 0x60], xmm0
0x32022d 498b4e08 mov rcx, qword ptr [r14 + 8]
0x320231 488b31 mov rsi, qword ptr [rcx]
0x320234 488d5608 lea rdx, [rsi + 8]
0x320238 483b5108 cmp rdx, qword ptr [rcx + 8]
0x32023c 7671 jbe 0x3202af
0x32023e 488d75a0 lea rsi, [rbp - 0x60]
0x320242 4c89f7 mov rdi, r14
0x320245 e84651feff call 0x305390
0x32024a 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x32024e 8b03 mov eax, dword ptr [rbx]
0x320250 eb67 jmp 0x3202b9
0x320252 8b36 mov esi, dword ptr [rsi]
0x320254 8975a0 mov dword ptr [rbp - 0x60], esi
0x320257 488911 mov qword ptr [rcx], rdx
0x32025a 41f6450004 test byte ptr [r13], 4
0x32025f 750d jne 0x32026e
0x320261 498b09 mov rcx, qword ptr [r9]
0x320264 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x320267 4898 cdqe 
0x320269 891401 mov dword ptr [rcx + rax], edx
0x32026c 8b03 mov eax, dword ptr [rbx]
0x32026e 83c004 add eax, 4
0x320271 8903 mov dword ptr [rbx], eax
0x320273 41f6450001 test byte ptr [r13], 1
0x320278 7510 jne 0x32028a
0x32027a 498b09 mov rcx, qword ptr [r9]
0x32027d 4863d0 movsxd rdx, eax
0x320280 c5fa100411 vmovss xmm0, dword ptr [rcx + rdx]
0x320285 c5fa1145a0 vmovss dword ptr [rbp - 0x60], xmm0
0x32028a 498b4e08 mov rcx, qword ptr [r14 + 8]
0x32028e 488b31 mov rsi, qword ptr [rcx]
0x320291 488d5604 lea rdx, [rsi + 4]
0x320295 483b5108 cmp rdx, qword ptr [rcx + 8]
0x320299 7675 jbe 0x320310
0x32029b 488d75a0 lea rsi, [rbp - 0x60]
0x32029f 4c89f7 mov rdi, r14
0x3202a2 e8b912d3ff call 0x51560
0x3202a7 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x3202ab 8b03 mov eax, dword ptr [rbx]
0x3202ad eb69 jmp 0x320318
0x3202af 488b36 mov rsi, qword ptr [rsi]
0x3202b2 488975a0 mov qword ptr [rbp - 0x60], rsi
0x3202b6 488911 mov qword ptr [rcx], rdx
0x3202b9 41f6450004 test byte ptr [r13], 4
0x3202be 750f jne 0x3202cf
0x3202c0 498b09 mov rcx, qword ptr [r9]
0x3202c3 488b55a0 mov rdx, qword ptr [rbp - 0x60]
0x3202c7 4898 cdqe 
0x3202c9 48891401 mov qword ptr [rcx + rax], rdx
0x3202cd 8b03 mov eax, dword ptr [rbx]
0x3202cf 83c008 add eax, 8
0x3202d2 8903 mov dword ptr [rbx], eax
0x3202d4 41f6450001 test byte ptr [r13], 1
0x3202d9 7510 jne 0x3202eb
0x3202db 498b09 mov rcx, qword ptr [r9]
0x3202de 4863d0 movsxd rdx, eax
0x3202e1 c5fb100411 vmovsd xmm0, qword ptr [rcx + rdx]
0x3202e6 c5fb1145a0 vmovsd qword ptr [rbp - 0x60], xmm0
0x3202eb 498b4e08 mov rcx, qword ptr [r14 + 8]
0x3202ef 488b31 mov rsi, qword ptr [rcx]
0x3202f2 488d5608 lea rdx, [rsi + 8]
0x3202f6 483b5108 cmp rdx, qword ptr [rcx + 8]
0x3202fa 7671 jbe 0x32036d
0x3202fc 488d75a0 lea rsi, [rbp - 0x60]
0x320300 4c89f7 mov rdi, r14
0x320303 e88850feff call 0x305390
0x320308 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x32030c 8b03 mov eax, dword ptr [rbx]
0x32030e eb67 jmp 0x320377
0x320310 8b36 mov esi, dword ptr [rsi]
0x320312 8975a0 mov dword ptr [rbp - 0x60], esi
0x320315 488911 mov qword ptr [rcx], rdx
0x320318 41f6450004 test byte ptr [r13], 4
0x32031d 750d jne 0x32032c
0x32031f 498b09 mov rcx, qword ptr [r9]
0x320322 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x320325 4898 cdqe 
0x320327 891401 mov dword ptr [rcx + rax], edx
0x32032a 8b03 mov eax, dword ptr [rbx]
0x32032c 83c004 add eax, 4
0x32032f 8903 mov dword ptr [rbx], eax
0x320331 41f6450001 test byte ptr [r13], 1
0x320336 7510 jne 0x320348
0x320338 498b09 mov rcx, qword ptr [r9]
0x32033b 4863d0 movsxd rdx, eax
0x32033e c5fa100411 vmovss xmm0, dword ptr [rcx + rdx]
0x320343 c5fa1145a0 vmovss dword ptr [rbp - 0x60], xmm0
0x320348 498b4e08 mov rcx, qword ptr [r14 + 8]
0x32034c 488b31 mov rsi, qword ptr [rcx]
0x32034f 488d5604 lea rdx, [rsi + 4]
0x320353 483b5108 cmp rdx, qword ptr [rcx + 8]
0x320357 7675 jbe 0x3203ce
0x320359 488d75a0 lea rsi, [rbp - 0x60]
0x32035d 4c89f7 mov rdi, r14
0x320360 e8fb11d3ff call 0x51560
0x320365 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x320369 8b03 mov eax, dword ptr [rbx]
0x32036b eb69 jmp 0x3203d6
0x32036d 488b36 mov rsi, qword ptr [rsi]
0x320370 488975a0 mov qword ptr [rbp - 0x60], rsi
0x320374 488911 mov qword ptr [rcx], rdx
0x320377 41f6450004 test byte ptr [r13], 4
0x32037c 750f jne 0x32038d
0x32037e 498b09 mov rcx, qword ptr [r9]
0x320381 488b55a0 mov rdx, qword ptr [rbp - 0x60]
0x320385 4898 cdqe 
0x320387 48891401 mov qword ptr [rcx + rax], rdx
0x32038b 8b03 mov eax, dword ptr [rbx]
0x32038d 83c008 add eax, 8
0x320390 8903 mov dword ptr [rbx], eax
0x320392 41f6450001 test byte ptr [r13], 1
0x320397 7510 jne 0x3203a9
0x320399 498b09 mov rcx, qword ptr [r9]
0x32039c 4863d0 movsxd rdx, eax
0x32039f c5fb100411 vmovsd xmm0, qword ptr [rcx + rdx]
0x3203a4 c5fb1145a0 vmovsd qword ptr [rbp - 0x60], xmm0
0x3203a9 498b4e08 mov rcx, qword ptr [r14 + 8]
0x3203ad 488b31 mov rsi, qword ptr [rcx]
0x3203b0 488d5608 lea rdx, [rsi + 8]
0x3203b4 483b5108 cmp rdx, qword ptr [rcx + 8]
0x3203b8 7671 jbe 0x32042b
0x3203ba 488d75a0 lea rsi, [rbp - 0x60]
0x3203be 4c89f7 mov rdi, r14
0x3203c1 e8ca4ffeff call 0x305390
0x3203c6 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x3203ca 8b03 mov eax, dword ptr [rbx]
0x3203cc eb67 jmp 0x320435
0x3203ce 8b36 mov esi, dword ptr [rsi]
0x3203d0 8975a0 mov dword ptr [rbp - 0x60], esi
0x3203d3 488911 mov qword ptr [rcx], rdx
0x3203d6 41f6450004 test byte ptr [r13], 4
0x3203db 750d jne 0x3203ea
0x3203dd 498b09 mov rcx, qword ptr [r9]
0x3203e0 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x3203e3 4898 cdqe 
0x3203e5 891401 mov dword ptr [rcx + rax], edx
0x3203e8 8b03 mov eax, dword ptr [rbx]
0x3203ea 83c004 add eax, 4
0x3203ed 8903 mov dword ptr [rbx], eax
0x3203ef 41f6450001 test byte ptr [r13], 1
0x3203f4 7510 jne 0x320406
0x3203f6 498b09 mov rcx, qword ptr [r9]
0x3203f9 4863d0 movsxd rdx, eax
0x3203fc c5fa100411 vmovss xmm0, dword ptr [rcx + rdx]
0x320401 c5fa1145a0 vmovss dword ptr [rbp - 0x60], xmm0
0x320406 498b4e08 mov rcx, qword ptr [r14 + 8]
0x32040a 488b31 mov rsi, qword ptr [rcx]
0x32040d 488d5604 lea rdx, [rsi + 4]
0x320411 483b5108 cmp rdx, qword ptr [rcx + 8]
0x320415 7639 jbe 0x320450
0x320417 488d75a0 lea rsi, [rbp - 0x60]
0x32041b 4c89f7 mov rdi, r14
0x32041e e83d11d3ff call 0x51560
0x320423 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x320427 8b03 mov eax, dword ptr [rbx]
0x320429 eb2d jmp 0x320458
0x32042b 488b36 mov rsi, qword ptr [rsi]
0x32042e 488975a0 mov qword ptr [rbp - 0x60], rsi
0x320432 488911 mov qword ptr [rcx], rdx
0x320435 41f6450004 test byte ptr [r13], 4
0x32043a 750f jne 0x32044b
0x32043c 498b09 mov rcx, qword ptr [r9]
0x32043f 488b55a0 mov rdx, qword ptr [rbp - 0x60]
0x320443 4898 cdqe 
0x320445 48891401 mov qword ptr [rcx + rax], rdx
0x320449 8b03 mov eax, dword ptr [rbx]
0x32044b 83c008 add eax, 8
0x32044e eb1f jmp 0x32046f
0x320450 8b36 mov esi, dword ptr [rsi]
0x320452 8975a0 mov dword ptr [rbp - 0x60], esi
0x320455 488911 mov qword ptr [rcx], rdx
0x320458 41f6450004 test byte ptr [r13], 4
0x32045d 750d jne 0x32046c
0x32045f 498b09 mov rcx, qword ptr [r9]
0x320462 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x320465 4898 cdqe 
0x320467 891401 mov dword ptr [rcx + rax], edx
0x32046a 8b03 mov eax, dword ptr [rbx]
0x32046c 83c004 add eax, 4
0x32046f 8903 mov dword ptr [rbx], eax
0x320471 41b42b mov r12b, 0x2b
0x320474 e945daffff jmp 0x31debe
0x320479 8b36 mov esi, dword ptr [rsi]
0x32047b 8975a0 mov dword ptr [rbp - 0x60], esi
0x32047e 488911 mov qword ptr [rcx], rdx
0x320481 41f6450004 test byte ptr [r13], 4
0x320486 750d jne 0x320495
0x320488 498b09 mov rcx, qword ptr [r9]
0x32048b 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x32048e 4898 cdqe 
0x320490 891401 mov dword ptr [rcx + rax], edx
0x320493 8b03 mov eax, dword ptr [rbx]
0x320495 83c004 add eax, 4
0x320498 8903 mov dword ptr [rbx], eax
0x32049a 41f6450001 test byte ptr [r13], 1
0x32049f 750c jne 0x3204ad
0x3204a1 498b09 mov rcx, qword ptr [r9]
0x3204a4 4863d0 movsxd rdx, eax
0x3204a7 8b0c11 mov ecx, dword ptr [rcx + rdx]
0x3204aa 894da0 mov dword ptr [rbp - 0x60], ecx
0x3204ad 498b4e08 mov rcx, qword ptr [r14 + 8]
0x3204b1 488b31 mov rsi, qword ptr [rcx]
0x3204b4 488d5604 lea rdx, [rsi + 4]
0x3204b8 483b5108 cmp rdx, qword ptr [rcx + 8]
0x3204bc 7614 jbe 0x3204d2
0x3204be 488d75a0 lea rsi, [rbp - 0x60]
0x3204c2 4c89f7 mov rdi, r14
0x3204c5 e89610d3ff call 0x51560
0x3204ca 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x3204ce 8b03 mov eax, dword ptr [rbx]
0x3204d0 eb08 jmp 0x3204da
0x3204d2 8b36 mov esi, dword ptr [rsi]
0x3204d4 8975a0 mov dword ptr [rbp - 0x60], esi
0x3204d7 488911 mov qword ptr [rcx], rdx
0x3204da 41f6450004 test byte ptr [r13], 4
0x3204df 750d jne 0x3204ee
0x3204e1 498b09 mov rcx, qword ptr [r9]
0x3204e4 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x3204e7 4898 cdqe 
0x3204e9 891401 mov dword ptr [rcx + rax], edx
0x3204ec 8b03 mov eax, dword ptr [rbx]
0x3204ee 83c004 add eax, 4
0x3204f1 8903 mov dword ptr [rbx], eax
0x3204f3 41f6450001 test byte ptr [r13], 1
0x3204f8 750c jne 0x320506
0x3204fa 498b09 mov rcx, qword ptr [r9]
0x3204fd 4863d0 movsxd rdx, eax
0x320500 8b0c11 mov ecx, dword ptr [rcx + rdx]
0x320503 894da0 mov dword ptr [rbp - 0x60], ecx
0x320506 498b4e08 mov rcx, qword ptr [r14 + 8]
0x32050a 488b31 mov rsi, qword ptr [rcx]
0x32050d 488d5604 lea rdx, [rsi + 4]
0x320511 483b5108 cmp rdx, qword ptr [rcx + 8]
0x320515 7614 jbe 0x32052b
0x320517 488d75a0 lea rsi, [rbp - 0x60]
0x32051b 4c89f7 mov rdi, r14
0x32051e e83d10d3ff call 0x51560
0x320523 4c8b4d98 mov r9, qword ptr [rbp - 0x68]
0x320527 8b03 mov eax, dword ptr [rbx]
0x320529 eb08 jmp 0x320533
0x32052b 8b36 mov esi, dword ptr [rsi]
0x32052d 8975a0 mov dword ptr [rbp - 0x60], esi
0x320530 488911 mov qword ptr [rcx], rdx
0x320533 41f6450004 test byte ptr [r13], 4
0x320538 750d jne 0x320547
0x32053a 498b09 mov rcx, qword ptr [r9]
0x32053d 8b55a0 mov edx, dword ptr [rbp - 0x60]
0x320540 4898 cdqe 
0x320542 891401 mov dword ptr [rcx + rax], edx
0x320545 8b03 mov eax, dword ptr [rbx]
0x320547 83c004 add eax, 4
0x32054a 41b422 mov r12b, 0x22
0x32054d 8903 mov dword ptr [rbx], eax
0x32054f e96ad9ffff jmp 0x31debe
0x320554 48c745a000000000 mov qword ptr [rbp - 0x60], 0
0x32055c 41f6450001 test byte ptr [r13], 1
0x320561 750b jne 0x32056e
0x320563 4863c2 movsxd rax, edx
0x320566 488b0401 mov rax, qword ptr [rcx + rax]
0x32056a 488945a0 mov qword ptr [rbp - 0x60], rax
0x32056e 498b06 mov rax, qword ptr [r14]
0x320571 488d75a0 lea rsi, [rbp - 0x60]
0x320575 4c89f7 mov rdi, r14
0x320578 ff9048010000 call qword ptr [rax + 0x148]
0x32057e 41f6462804 test byte ptr [r14 + 0x28], 4
0x320583 488b7598 mov rsi, qword ptr [rbp - 0x68]
0x320587 750e jne 0x320597
0x320589 488b45a0 mov rax, qword ptr [rbp - 0x60]
0x32058d 48630b movsxd rcx, dword ptr [rbx]
0x320590 488b16 mov rdx, qword ptr [rsi]
0x320593 4889040a mov qword ptr [rdx + rcx], rax
0x320597 486303 movsxd rax, dword ptr [rbx]
0x32059a 8d4808 lea ecx, [rax + 8]
0x32059d 890b mov dword ptr [rbx], ecx
0x32059f 41f6450004 test byte ptr [r13], 4
0x3205a4 0f853ed6ffff jne 0x31dbe8
0x3205aa 488b0e mov rcx, qword ptr [rsi]
0x3205ad 488b3c01 mov rdi, qword ptr [rcx + rax]
0x3205b1 e81a000000 call 0x3205d0
0x3205b6 e92dd6ffff jmp 0x31dbe8
0x3205bb e8a0181507 call 0x7471e60
0x3205c0 0f0b ud2 
0x3205c2 cc int3 
0x3205c3 cc int3 
0x3205c4 cc int3 
0x3205c5 cc int3 
0x3205c6 cc int3 
0x3205c7 cc int3 
0x3205c8 cc int3 
0x3205c9 cc int3 
0x3205ca cc int3 
0x3205cb cc int3 
0x3205cc cc int3 
0x3205cd cc int3 
0x3205ce cc int3 
0x3205cf cc int3 
0x3205d0 55 push rbp
0x3205d1 4889e5 mov rbp, rsp
0x3205d4 4156 push r14
0x3205d6 53 push rbx
0x3205d7 4883ec20 sub rsp, 0x20
0x3205db 4c8b352619e108 mov r14, qword ptr [rip + 0x8e11926]
0x3205e2 4889fb mov rbx, rdi
0x3205e5 498b06 mov rax, qword ptr [r14]
0x3205e8 488945e8 mov qword ptr [rbp - 0x18], rax
0x3205ec 48897dd8 mov qword ptr [rbp - 0x28], rdi
0x3205f0 4885ff test rdi, rdi
0x3205f3 7416 je 0x32060b
0x3205f5 488d0560e80709 lea rax, [rip + 0x907e860]
0x3205fc 803800 cmp byte ptr [rax], 0
0x3205ff 740a je 0x32060b
0x320601 4889df mov rdi, rbx
0x320604 e8b77c0400 call 0x3682c0
0x320609 eb05 jmp 0x320610
0x32060b 4885db test rbx, rbx
0x32060e 7420 je 0x320630
0x320610 e86b0e0a00 call 0x3c1480
0x320615 488b4b10 mov rcx, qword ptr [rbx + 0x10]
0x320619 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x32061d 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x320620 7f0e jg 0x320630
0x320622 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x320626 4883c030 add rax, 0x30
0x32062a 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x32062e 7448 je 0x320678
0x320630 488b5dd8 mov rbx, qword ptr [rbp - 0x28]
0x320634 4885db test rbx, rbx
0x320637 745c je 0x320695
0x320639 e822170a00 call 0x3c1d60
0x32063e 488b4b10 mov rcx, qword ptr [rbx + 0x10]
0x320642 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x320646 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x320649 7f4a jg 0x320695
0x32064b 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x32064f 4883c030 add rax, 0x30
0x320653 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x320657 753c jne 0x320695
0x320659 4881c300020000 add rbx, 0x200
0x320660 488d45d8 lea rax, [rbp - 0x28]
0x320664 488d75e0 lea rsi, [rbp - 0x20]
0x320668 31d2 xor edx, edx
0x32066a 4889df mov rdi, rbx
0x32066d 488945e0 mov qword ptr [rbp - 0x20], rax
0x320671 e8aaa8d7ff call 0x9af20
0x320676 eb1d jmp 0x320695
0x320678 4881c320030000 add rbx, 0x320
0x32067f 488d45d8 lea rax, [rbp - 0x28]
0x320683 488d75e0 lea rsi, [rbp - 0x20]
0x320687 31d2 xor edx, edx
0x320689 4889df mov rdi, rbx
0x32068c 488945e0 mov qword ptr [rbp - 0x20], rax
0x320690 e88ba8d7ff call 0x9af20
0x320695 498b06 mov rax, qword ptr [r14]
0x320698 483b45e8 cmp rax, qword ptr [rbp - 0x18]
0x32069c 7509 jne 0x3206a7
0x32069e 4883c420 add rsp, 0x20
0x3206a2 5b pop rbx
0x3206a3 415e pop r14
0x3206a5 5d pop rbp
0x3206a6 c3 ret 
0x3206a7 e8b4171507 call 0x7471e60
0x3206ac 0f0b ud2 
0x3206ae cc int3 
0x3206af cc int3 
0x3206b0 55 push rbp
0x3206b1 4889e5 mov rbp, rsp
0x3206b4 53 push rbx
0x3206b5 4883ec18 sub rsp, 0x18
0x3206b9 488b1d4818e108 mov rbx, qword ptr [rip + 0x8e11848]
0x3206c0 c5f857c0 vxorps xmm0, xmm0, xmm0
0x3206c4 488b03 mov rax, qword ptr [rbx]
0x3206c7 488945f0 mov qword ptr [rbp - 0x10], rax
0x3206cb 488b4720 mov rax, qword ptr [rdi + 0x20]
0x3206cf 488b5718 mov rdx, qword ptr [rdi + 0x18]
0x3206d3 488d7de0 lea rdi, [rbp - 0x20]
0x3206d7 488b7018 mov rsi, qword ptr [rax + 0x18]
0x3206db c5f81145e0 vmovups xmmword ptr [rbp - 0x20], xmm0
0x3206e0 e8ab261400 call 0x462d90
0x3206e5 488b45e0 mov rax, qword ptr [rbp - 0x20]
0x3206e9 488b55e8 mov rdx, qword ptr [rbp - 0x18]
0x3206ed 488b0b mov rcx, qword ptr [rbx]
0x3206f0 483b4df0 cmp rcx, qword ptr [rbp - 0x10]
0x3206f4 7507 jne 0x3206fd
0x3206f6 4883c418 add rsp, 0x18
0x3206fa 5b pop rbx
0x3206fb 5d pop rbp
0x3206fc c3 ret 
0x3206fd e85e171507 call 0x7471e60
0x320702 0f0b ud2 
0x320704 cc int3 
0x320705 cc int3 
0x320706 cc int3 
0x320707 cc int3 
0x320708 cc int3 
0x320709 cc int3 
0x32070a cc int3 
0x32070b cc int3 
0x32070c cc int3 
0x32070d cc int3 
0x32070e cc int3 
0x32070f cc int3 
0x320710 55 push rbp
0x320711 4889e5 mov rbp, rsp
0x320714 4157 push r15
0x320716 4156 push r14
0x320718 4155 push r13
0x32071a 4154 push r12
0x32071c 53 push rbx
0x32071d 4883ec18 sub rsp, 0x18
0x320721 4989cc mov r12, rcx
0x320724 4989d7 mov r15, rdx
0x320727 4c8945c8 mov qword ptr [rbp - 0x38], r8
0x32072b 488975d0 mov qword ptr [rbp - 0x30], rsi
0x32072f 4d85c9 test r9, r9
0x320732 0f8417010000 je 0x32084f
0x320738 488b5f78 mov rbx, qword ptr [rdi + 0x78]
0x32073c 4885db test rbx, rbx
0x32073f 0f8400020000 je 0x320945
0x320745 450fb67116 movzx r14d, byte ptr [r9 + 0x16]
0x32074a 4d89cd mov r13, r9
0x32074d 498d81a0000000 lea rax, [r9 + 0xa0]
0x320754 488945c0 mov qword ptr [rbp - 0x40], rax
0x320758 eb2c jmp 0x320786
0x32075a 660f1f440000 nop word ptr [rax + rax]
0x320760 31d2 xor edx, edx
0x320762 480375d0 add rsi, qword ptr [rbp - 0x30]
0x320766 488b03 mov rax, qword ptr [rbx]
0x320769 488b4dc8 mov rcx, qword ptr [rbp - 0x38]
0x32076d 4889df mov rdi, rbx
0x320770 4d89e8 mov r8, r13
0x320773 ff9068010000 call qword ptr [rax + 0x168]
0x320779 488b5b50 mov rbx, qword ptr [rbx + 0x50]
0x32077d 4885db test rbx, rbx
0x320780 0f84bf010000 je 0x320945
0x320786 4584f6 test r14b, r14b
0x320789 7510 jne 0x32079b
0x32078b 48b80000080080000010 movabs rax, 0x1000008000080000
0x320795 48854338 test qword ptr [rbx + 0x38], rax
0x320799 74de je 0x320779
0x32079b 418b4570 mov eax, dword ptr [r13 + 0x70]
0x32079f 413b859c000000 cmp eax, dword ptr [r13 + 0x9c]
0x3207a6 7478 je 0x320820
0x3207a8 4889d8 mov rax, rbx
0x3207ab 498bb5a8000000 mov rsi, qword ptr [r13 + 0xa8]
0x3207b2 418b95b0000000 mov edx, dword ptr [r13 + 0xb0]
0x3207b9 48c1e804 shr rax, 4
0x3207bd 89c1 mov ecx, eax
0x3207bf c1e910 shr ecx, 0x10
0x3207c2 31c1 xor ecx, eax
0x3207c4 69c16bcaeb85 imul eax, ecx, 0x85ebca6b
0x3207ca 89c1 mov ecx, eax
0x3207cc c1e90d shr ecx, 0xd
0x3207cf 31c1 xor ecx, eax
0x3207d1 69c135aeb2c2 imul eax, ecx, 0xc2b2ae35
0x3207d7 89c1 mov ecx, eax
0x3207d9 c1e910 shr ecx, 0x10
0x3207dc 31c1 xor ecx, eax
0x3207de 4885f6 test rsi, rsi
0x3207e1 480f4475c0 cmove rsi, qword ptr [rbp - 0x40]
0x3207e6 ffca dec edx
0x3207e8 21ca and edx, ecx
0x3207ea 8b0c96 mov ecx, dword ptr [rsi + rdx*4]
0x3207ed 83f9ff cmp ecx, -1
0x3207f0 742e je 0x320820
0x3207f2 498b4568 mov rax, qword ptr [r13 + 0x68]
0x3207f6 662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x320800 4863c9 movsxd rcx, ecx
0x320803 48c1e104 shl rcx, 4
0x320807 48391c08 cmp qword ptr [rax + rcx], rbx
0x32080b 0f8468ffffff je 0x320779
0x320811 4801c1 add rcx, rax
0x320814 8b4908 mov ecx, dword ptr [rcx + 8]
0x320817 83f9ff cmp ecx, -1
0x32081a 75e4 jne 0x320800
0x32081c 0f1f4000 nop dword ptr [rax]
0x320820 48637344 movsxd rsi, dword ptr [rbx + 0x44]
0x320824 4d85ff test r15, r15
0x320827 0f8433ffffff je 0x320760
0x32082d 4d85e4 test r12, r12
0x320830 7414 je 0x320846
0x320832 8b4330 mov eax, dword ptr [rbx + 0x30]
0x320835 0faf432c imul eax, dword ptr [rbx + 0x2c]
0x320839 01f0 add eax, esi
0x32083b 413b442458 cmp eax, dword ptr [r12 + 0x58]
0x320840 0f8f1affffff jg 0x320760
0x320846 498d1437 lea rdx, [r15 + rsi]
0x32084a e913ffffff jmp 0x320762
0x32084f 488b5f78 mov rbx, qword ptr [rdi + 0x78]
0x320853 4885db test rbx, rbx
0x320856 0f84e9000000 je 0x320945
0x32085c 49be0000080080000010 movabs r14, 0x1000008000080000
0x320866 4d85ff test r15, r15
0x320869 7422 je 0x32088d
0x32086b 4d85e4 test r12, r12
0x32086e 7570 jne 0x3208e0
0x320870 e9a4000000 jmp 0x320919
0x320875 66662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x320880 488b5b50 mov rbx, qword ptr [rbx + 0x50]
0x320884 4885db test rbx, rbx
0x320887 0f84b8000000 je 0x320945
0x32088d 4c857338 test qword ptr [rbx + 0x38], r14
0x320891 74ed je 0x320880
0x320893 48637344 movsxd rsi, dword ptr [rbx + 0x44]
0x320897 480375d0 add rsi, qword ptr [rbp - 0x30]
0x32089b 488b03 mov rax, qword ptr [rbx]
0x32089e 488b4dc8 mov rcx, qword ptr [rbp - 0x38]
0x3208a2 4889df mov rdi, rbx
0x3208a5 31d2 xor edx, edx
0x3208a7 4531c0 xor r8d, r8d
0x3208aa ff9068010000 call qword ptr [rax + 0x168]
0x3208b0 ebce jmp 0x320880
0x3208b2 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3208c0 480375d0 add rsi, qword ptr [rbp - 0x30]
0x3208c4 488b03 mov rax, qword ptr [rbx]
0x3208c7 488b4dc8 mov rcx, qword ptr [rbp - 0x38]
0x3208cb 4889df mov rdi, rbx
0x3208ce 4531c0 xor r8d, r8d
0x3208d1 ff9068010000 call qword ptr [rax + 0x168]
0x3208d7 488b5b50 mov rbx, qword ptr [rbx + 0x50]
0x3208db 4885db test rbx, rbx
0x3208de 7465 je 0x320945
0x3208e0 4c857338 test qword ptr [rbx + 0x38], r14
0x3208e4 74f1 je 0x3208d7
0x3208e6 8b4330 mov eax, dword ptr [rbx + 0x30]
0x3208e9 0faf432c imul eax, dword ptr [rbx + 0x2c]
0x3208ed 48637344 movsxd rsi, dword ptr [rbx + 0x44]
0x3208f1 ba00000000 mov edx, 0
0x3208f6 01f0 add eax, esi
0x3208f8 413b442458 cmp eax, dword ptr [r12 + 0x58]
0x3208fd 7fc1 jg 0x3208c0
0x3208ff 498d1437 lea rdx, [r15 + rsi]
0x320903 ebbb jmp 0x3208c0
0x320905 66662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x320910 488b5b50 mov rbx, qword ptr [rbx + 0x50]
0x320914 4885db test rbx, rbx
0x320917 742c je 0x320945
0x320919 4c857338 test qword ptr [rbx + 0x38], r14
0x32091d 74f1 je 0x320910
0x32091f 48635344 movsxd rdx, dword ptr [rbx + 0x44]
0x320923 488b45d0 mov rax, qword ptr [rbp - 0x30]
0x320927 4c8b23 mov r12, qword ptr [rbx]
0x32092a 488b4dc8 mov rcx, qword ptr [rbp - 0x38]
0x32092e 4889df mov rdi, rbx
0x320931 4531c0 xor r8d, r8d
0x320934 488d3410 lea rsi, [rax + rdx]
0x320938 4c01fa add rdx, r15
0x32093b 41ff942468010000 call qword ptr [r12 + 0x168]
0x320943 ebcb jmp 0x320910
0x320945 4883c418 add rsp, 0x18
0x320949 5b pop rbx
0x32094a 415c pop r12
0x32094c 415d pop r13
0x32094e 415e pop r14
0x320950 415f pop r15
0x320952 5d pop rbp
0x320953 c3 ret 
0x320954 cc int3 
0x320955 cc int3 
0x320956 cc int3 
0x320957 cc int3 
0x320958 cc int3 
0x320959 cc int3 
0x32095a cc int3 
0x32095b cc int3 
0x32095c cc int3 
0x32095d cc int3 
0x32095e cc int3 
0x32095f cc int3 
0x320960 55 push rbp
0x320961 4889e5 mov rbp, rsp
0x320964 4157 push r15
0x320966 4156 push r14
0x320968 53 push rbx
0x320969 4881ecf8010000 sub rsp, 0x1f8
0x320970 4c8b3d9115e108 mov r15, qword ptr [rip + 0x8e11591]
0x320977 498b07 mov rax, qword ptr [r15]
0x32097a 488945e0 mov qword ptr [rbp - 0x20], rax
0x32097e 488b0593da0709 mov rax, qword ptr [rip + 0x907da93]
0x320985 4885c0 test rax, rax
0x320988 0f8538010000 jne 0x320ac6
0x32098e 48833d6ada070900 cmp qword ptr [rip + 0x907da6a], 0
0x320996 750c jne 0x3209a4
0x320998 e8539affff call 0x31a3f0
0x32099d 4889055cda0709 mov qword ptr [rip + 0x907da5c], rax
0x3209a4 e847010000 call 0x320af0
0x3209a9 4889c7 mov rdi, rax
0x3209ac be01000000 mov esi, 1
0x3209b1 4889c3 mov rbx, rax
0x3209b4 e837541600 call 0x485df0
0x3209b9 488d051063ceff lea rax, [rip - 0x319cf0]
0x3209c0 bf02000000 mov edi, 2
0x3209c5 488983c0000000 mov qword ptr [rbx + 0xc0], rax
0x3209cc e84fce0700 call 0x39d820
0x3209d1 bf02000000 mov edi, 2
0x3209d6 488985f0fdffff mov qword ptr [rbp - 0x210], rax
0x3209dd c785f8fdffff40000000 mov dword ptr [rbp - 0x208], 0x40
0x3209e7 c685fcfdffff02 mov byte ptr [rbp - 0x204], 2
0x3209ee 48c78500feffff00000000 mov qword ptr [rbp - 0x200], 0
0x3209f9 e822ce0700 call 0x39d820
0x3209fe bf03000000 mov edi, 3
0x320a03 48898508feffff mov qword ptr [rbp - 0x1f8], rax
0x320a0a c78510feffff48000000 mov dword ptr [rbp - 0x1f0], 0x48
0x320a14 c68514feffff02 mov byte ptr [rbp - 0x1ec], 2
0x320a1b 48c78518feffff00000000 mov qword ptr [rbp - 0x1e8], 0
0x320a26 e8f5cd0700 call 0x39d820
0x320a2b 4c8db538feffff lea r14, [rbp - 0x1c8]
0x320a32 488d8d38feffff lea rcx, [rbp - 0x1c8]
0x320a39 488d95f0fdffff lea rdx, [rbp - 0x210]
0x320a40 beb0000000 mov esi, 0xb0
0x320a45 48898520feffff mov qword ptr [rbp - 0x1e0], rax
0x320a4c c78528feffff90000000 mov dword ptr [rbp - 0x1d8], 0x90
0x320a56 c6852cfeffff03 mov byte ptr [rbp - 0x1d4], 3
0x320a5d 48c78530feffff00000000 mov qword ptr [rbp - 0x1d0], 0
0x320a68 4c89f7 mov rdi, r14
0x320a6b e830d20700 call 0x39dca0
0x320a70 4c89f7 mov rdi, r14
0x320a73 31f6 xor esi, esi
0x320a75 e8a6d90700 call 0x39e420
0x320a7a 4889df mov rdi, rbx
0x320a7d 4889c6 mov rsi, rax
0x320a80 e82bdf0700 call 0x39e9b0
0x320a85 4c89f7 mov rdi, r14
0x320a88 e833d30700 call 0x39ddc0
0x320a8d 4c89f7 mov rdi, r14
0x320a90 e86ba3efff call 0x21ae00
0x320a95 488d05344cbb07 lea rax, [rip + 0x7bb4c34]
0x320a9c 4889df mov rdi, rbx
0x320a9f 4c89f6 mov rsi, r14
0x320aa2 31d2 xor edx, edx
0x320aa4 48898538feffff mov qword ptr [rbp - 0x1c8], rax
0x320aab 488b03 mov rax, qword ptr [rbx]
0x320aae ff90d0020000 call qword ptr [rax + 0x2d0]
0x320ab4 4c89f7 mov rdi, r14
0x320ab7 e854aaefff call 0x21b510
0x320abc 4889d8 mov rax, rbx
0x320abf 48891d52d90709 mov qword ptr [rip + 0x907d952], rbx
0x320ac6 498b0f mov rcx, qword ptr [r15]
0x320ac9 483b4de0 cmp rcx, qword ptr [rbp - 0x20]
0x320acd 750e jne 0x320add
0x320acf 4881c4f8010000 add rsp, 0x1f8
0x320ad6 5b pop rbx
0x320ad7 415e pop r14
0x320ad9 415f pop r15
0x320adb 5d pop rbp
0x320adc c3 ret 
0x320add e87e131507 call 0x7471e60
0x320ae2 0f0b ud2 
0x320ae4 cc int3 
0x320ae5 cc int3 
0x320ae6 cc int3 
0x320ae7 cc int3 
0x320ae8 cc int3 
0x320ae9 cc int3 
0x320aea cc int3 
0x320aeb cc int3 
0x320aec cc int3 
0x320aed cc int3 
0x320aee cc int3 
0x320aef cc int3 
0x320af0 55 push rbp
0x320af1 4889e5 mov rbp, rsp
0x320af4 4157 push r15
0x320af6 4156 push r14
0x320af8 4154 push r12
0x320afa 53 push rbx
0x320afb 4883ec20 sub rsp, 0x20
0x320aff 4c8b3d0214e108 mov r15, qword ptr [rip + 0x8e11402]
0x320b06 498b07 mov rax, qword ptr [r15]
0x320b09 488945d8 mov qword ptr [rbp - 0x28], rax
0x320b0d 488b05fcd80709 mov rax, qword ptr [rip + 0x907d8fc]
0x320b14 4885c0 test rax, rax
0x320b17 0f8597020000 jne 0x320db4
0x320b1d 488d3d4d900909 lea rdi, [rip + 0x909904d]
0x320b24 be00020000 mov esi, 0x200
0x320b29 ba08000000 mov edx, 8
0x320b2e b901000000 mov ecx, 1
0x320b33 e838021600 call 0x480d70
0x320b38 488d3561c15a07 lea rsi, [rip + 0x75ac161]
0x320b3f 488d7dc8 lea rdi, [rbp - 0x38]
0x320b43 ba01000000 mov edx, 1
0x320b48 4889c3 mov rbx, rax
0x320b4b e840caf1ff call 0x23d590
0x320b50 4889df mov rdi, rbx
0x320b53 bac7000000 mov edx, 0xc7
0x320b58 31f6 xor esi, esi
0x320b5a e851550a00 call 0x3c60b0
0x320b5f 488d05c26ebb07 lea rax, [rip + 0x7bb6ec2]
0x320b66 488d0d73020000 lea rcx, [rip + 0x273]
0x320b6d 488d35cc020000 lea rsi, [rip + 0x2cc]
0x320b74 4c8d35f3645007 lea r14, [rip + 0x75064f3]
0x320b7b c5f857c0 vxorps xmm0, xmm0, xmm0
0x320b7f c5f8114328 vmovups xmmword ptr [rbx + 0x28], xmm0
0x320b84 c74338ffffffff mov dword ptr [rbx + 0x38], 0xffffffff
0x320b8b c5f8114340 vmovups xmmword ptr [rbx + 0x40], xmm0
0x320b90 48c7435000000000 mov qword ptr [rbx + 0x50], 0
0x320b98 c74358b0000000 mov dword ptr [rbx + 0x58], 0xb0
0x320b9f 66c7435c0800 mov word ptr [rbx + 0x5c], 8
0x320ba5 c5f857c0 vxorps xmm0, xmm0, xmm0
0x320ba9 c5fc11437e vmovups ymmword ptr [rbx + 0x7e], ymm0
0x320bae c5fc11435e vmovups ymmword ptr [rbx + 0x5e], ymm0
0x320bb3 c5fc118390000000 vmovups ymmword ptr [rbx + 0x90], ymm0
0x320bbb 488dbbe8000000 lea rdi, [rbx + 0xe8]
0x320bc2 ba01000000 mov edx, 1
0x320bc7 488903 mov qword ptr [rbx], rax
0x320bca 48898bb0000000 mov qword ptr [rbx + 0xb0], rcx
0x320bd1 4889b3b8000000 mov qword ptr [rbx + 0xb8], rsi
0x320bd8 488d0df160ceff lea rcx, [rip - 0x319f0f]
0x320bdf 4c89f6 mov rsi, r14
0x320be2 48898bc0000000 mov qword ptr [rbx + 0xc0], rcx
0x320be9 48c783c800000000000000 mov qword ptr [rbx + 0xc8], 0
0x320bf4 66c783d00000000000 mov word ptr [rbx + 0xd0], 0
0x320bfd c783d4000000a0000010 mov dword ptr [rbx + 0xd4], 0x100000a0
0x320c07 48c783d800000008000000 mov qword ptr [rbx + 0xd8], 8
0x320c12 48c783e000000000000000 mov qword ptr [rbx + 0xe0], 0
0x320c1d e86ec9f1ff call 0x23d590
0x320c22 c5f8101df6bd7107 vmovups xmm3, xmmword ptr [rip + 0x771bdf6]
0x320c2a 48c7834801000000000000 mov qword ptr [rbx + 0x148], 0
0x320c35 c5f857c0 vxorps xmm0, xmm0, xmm0
0x320c39 c5fc1183f0000000 vmovups ymmword ptr [rbx + 0xf0], ymm0
0x320c41 c5fc118310010000 vmovups ymmword ptr [rbx + 0x110], ymm0
0x320c49 48c7833001000000000000 mov qword ptr [rbx + 0x130], 0
0x320c54 c5e857d2 vxorps xmm2, xmm2, xmm2
0x320c58 c5f057c9 vxorps xmm1, xmm1, xmm1
0x320c5c c5f8119b50010000 vmovups xmmword ptr [rbx + 0x150], xmm3
0x320c64 48c7836801000000000000 mov qword ptr [rbx + 0x168], 0
0x320c6f c7837001000000000000 mov dword ptr [rbx + 0x170], 0
0x320c79 c6837801000000 mov byte ptr [rbx + 0x178], 0
0x320c80 48c783a001000000000000 mov qword ptr [rbx + 0x1a0], 0
0x320c8b c5f8118b80010000 vmovups xmmword ptr [rbx + 0x180], xmm1
0x320c93 c5f8119ba8010000 vmovups xmmword ptr [rbx + 0x1a8], xmm3
0x320c9b 48c783c001000000000000 mov qword ptr [rbx + 0x1c0], 0
0x320ca6 c783c801000000000000 mov dword ptr [rbx + 0x1c8], 0
0x320cb0 c683d001000000 mov byte ptr [rbx + 0x1d0], 0
0x320cb7 c5fc1193d8010000 vmovups ymmword ptr [rbx + 0x1d8], ymm2
0x320cbf 48c783f801000000000000 mov qword ptr [rbx + 0x1f8], 0
0x320cca 48891d3fd70709 mov qword ptr [rip + 0x907d73f], rbx
0x320cd1 4c8b2520d70709 mov r12, qword ptr [rip + 0x907d720]
0x320cd8 4d85e4 test r12, r12
0x320cdb 756d jne 0x320d4a
0x320cdd 488d05aca5ceff lea rax, [rip - 0x315a54]
0x320ce4 4c8d25859affff lea r12, [rip - 0x657b]
0x320ceb 4c8d55d0 lea r10, [rbp - 0x30]
0x320cef 4c8d1daa9affff lea r11, [rip - 0x6556]
0x320cf6 488d3dc7635007 lea rdi, [rip + 0x75063c7]
0x320cfd 488d35ec914807 lea rsi, [rip + 0x74891ec]
0x320d04 488d15edd60709 lea rdx, [rip + 0x907d6ed]
0x320d0b 488d0d7ea5ceff lea rcx, [rip - 0x315a82]
0x320d12 41b830000000 mov r8d, 0x30
0x320d18 41b908000000 mov r9d, 8
0x320d1e 488945d0 mov qword ptr [rbp - 0x30], rax
0x320d22 488d05c7b7f7ff lea rax, [rip - 0x84839]
0x320d29 50 push rax
0x320d2a 50 push rax
0x320d2b 4152 push r10
0x320d2d 4153 push r11
0x320d2f 4154 push r12
0x320d31 4156 push r14
0x320d33 6a01 push 1
0x320d35 6801000010 push 0x10000001
0x320d3a e821cbfeff call 0x30d860
0x320d3f 4883c440 add rsp, 0x40
0x320d43 4c8b25aed60709 mov r12, qword ptr [rip + 0x907d6ae]
0x320d4a e8614e0a00 call 0x3c5bb0
0x320d4f 4989c6 mov r14, rax
0x320d52 488b03 mov rax, qword ptr [rbx]
0x320d55 31f6 xor esi, esi
0x320d57 4939dc cmp r12, rbx
0x320d5a 4889df mov rdi, rbx
0x320d5d 490f45f4 cmovne rsi, r12
0x320d61 ff9028030000 call qword ptr [rax + 0x328]
0x320d67 4d85f6 test r14, r14
0x320d6a 7414 je 0x320d80
0x320d6c 488d05e9e00709 lea rax, [rip + 0x907e0e9]
0x320d73 803800 cmp byte ptr [rax], 0
0x320d76 7408 je 0x320d80
0x320d78 4c89f7 mov rdi, r14
0x320d7b e840750400 call 0x3682c0
0x320d80 4c89b3e0000000 mov qword ptr [rbx + 0xe0], r14
0x320d87 4889df mov rdi, rbx
0x320d8a 488b03 mov rax, qword ptr [rbx]
0x320d8d ff5010 call qword ptr [rax + 0x10]
0x320d90 488d35a9c0f7ff lea rsi, [rip - 0x83f57]
0x320d97 488d1526635007 lea rdx, [rip + 0x7506326]
0x320d9e 488d0dfbbe5a07 lea rcx, [rip + 0x75abefb]
0x320da5 4889df mov rdi, rbx
0x320da8 e8f34e1600 call 0x485ca0
0x320dad 488b055cd60709 mov rax, qword ptr [rip + 0x907d65c]
0x320db4 498b0f mov rcx, qword ptr [r15]
0x320db7 483b4dd8 cmp rcx, qword ptr [rbp - 0x28]
0x320dbb 750d jne 0x320dca
0x320dbd 4883c420 add rsp, 0x20
0x320dc1 5b pop rbx
0x320dc2 415c pop r12
0x320dc4 415e pop r14
0x320dc6 415f pop r15
0x320dc8 5d pop rbp
0x320dc9 c3 ret 
0x320dca e891101507 call 0x7471e60
0x320dcf 0f0b ud2 
0x320dd1 cc int3 
0x320dd2 cc int3 
0x320dd3 cc int3 
0x320dd4 cc int3 
0x320dd5 cc int3 
0x320dd6 cc int3 
0x320dd7 cc int3 
0x320dd8 cc int3 
0x320dd9 cc int3 
0x320dda cc int3 
0x320ddb cc int3 
0x320ddc cc int3 
0x320ddd cc int3 
0x320dde cc int3 
0x320ddf cc int3 
0x320de0 55 push rbp
0x320de1 4889e5 mov rbp, rsp
0x320de4 53 push rbx
0x320de5 50 push rax
0x320de6 488b1f mov rbx, qword ptr [rdi]
0x320de9 4889fe mov rsi, rdi
0x320dec 4889df mov rdi, rbx
0x320def e82c0e1700 call 0x491c20
0x320df4 488d05f564bb07 lea rax, [rip + 0x7bb64f5]
0x320dfb 48c7433000000000 mov qword ptr [rbx + 0x30], 0
0x320e03 c74338ffffffff mov dword ptr [rbx + 0x38], 0xffffffff
0x320e0a c5f857c0 vxorps xmm0, xmm0, xmm0
0x320e0e 488903 mov qword ptr [rbx], rax
0x320e11 48c783a800000000000000 mov qword ptr [rbx + 0xa8], 0
0x320e1c c5f8114340 vmovups xmmword ptr [rbx + 0x40], xmm0
0x320e21 c5f811435e vmovups xmmword ptr [rbx + 0x5e], xmm0
0x320e26 66c7436e0000 mov word ptr [rbx + 0x6e], 0
0x320e2c c5f8118390000000 vmovups xmmword ptr [rbx + 0x90], xmm0
0x320e34 4883c408 add rsp, 8
0x320e38 5b pop rbx
0x320e39 5d pop rbp
0x320e3a c3 ret 
0x320e3b cc int3 
0x320e3c cc int3 
0x320e3d cc int3 
0x320e3e cc int3 
0x320e3f cc int3 
0x320e40 55 push rbp
0x320e41 4889e5 mov rbp, rsp
0x320e44 4156 push r14
0x320e46 53 push rbx
0x320e47 4883ec30 sub rsp, 0x30
0x320e4b 4889fb mov rbx, rdi
0x320e4e e84d5b0a00 call 0x3c69a0
0x320e53 4989c6 mov r14, rax
0x320e56 e895fcffff call 0x320af0
0x320e5b 4889c7 mov rdi, rax
0x320e5e 4c89f6 mov rsi, r14
0x320e61 b910050000 mov ecx, 0x510
0x320e66 c5f857c0 vxorps xmm0, xmm0, xmm0
0x320e6a c5f811442418 vmovups xmmword ptr [rsp + 0x18], xmm0
0x320e70 c5f8110424 vmovups xmmword ptr [rsp], xmm0
0x320e75 c744241000000000 mov dword ptr [rsp + 0x10], 0
0x320e7d 31d2 xor edx, edx
0x320e7f 4531c0 xor r8d, r8d
0x320e82 4531c9 xor r9d, r9d
0x320e85 e826011700 call 0x490fb0
0x320e8a 4889c7 mov rdi, rax
0x320e8d 4889de mov rsi, rbx
0x320e90 4989c6 mov r14, rax
0x320e93 e8e8120a00 call 0x3c2180
0x320e98 488d055164bb07 lea rax, [rip + 0x7bb6451]
0x320e9f 49c7463000000000 mov qword ptr [r14 + 0x30], 0
0x320ea7 41c74638ffffffff mov dword ptr [r14 + 0x38], 0xffffffff
0x320eaf c5f857c0 vxorps xmm0, xmm0, xmm0
0x320eb3 498906 mov qword ptr [r14], rax
0x320eb6 49c786a800000000000000 mov qword ptr [r14 + 0xa8], 0
0x320ec1 c4c178114640 vmovups xmmword ptr [r14 + 0x40], xmm0
0x320ec7 c4c17811465e vmovups xmmword ptr [r14 + 0x5e], xmm0
0x320ecd 6641c7466e0000 mov word ptr [r14 + 0x6e], 0
0x320ed4 c4c178118690000000 vmovups xmmword ptr [r14 + 0x90], xmm0
0x320edd 4c89f0 mov rax, r14
0x320ee0 4883c430 add rsp, 0x30
0x320ee4 5b pop rbx
0x320ee5 415e pop r14
0x320ee7 5d pop rbp
0x320ee8 c3 ret 
0x320ee9 cc int3 
0x320eea cc int3 
0x320eeb cc int3 
0x320eec cc int3 
0x320eed cc int3 
0x320eee cc int3 
0x320eef cc int3 
0x320ef0 55 push rbp
0x320ef1 4889e5 mov rbp, rsp
0x320ef4 e8f7fbffff call 0x320af0
0x320ef9 5d pop rbp
0x320efa c3 ret 
0x320efb cc int3 
0x320efc cc int3 
0x320efd cc int3 
0x320efe cc int3 
0x320eff cc int3 
0x320f00 55 push rbp
0x320f01 4889e5 mov rbp, rsp
0x320f04 4157 push r15
0x320f06 4156 push r14
0x320f08 53 push rbx
0x320f09 50 push rax
0x320f0a 89f3 mov ebx, esi
0x320f0c 4989ff mov r15, rdi
0x320f0f e83ca30a00 call 0x3cb250
0x320f14 4d85ff test r15, r15
0x320f17 7430 je 0x320f49
0x320f19 4d8b7750 mov r14, qword ptr [r15 + 0x50]
0x320f1d 4d85f6 test r14, r14
0x320f20 7427 je 0x320f49
0x320f22 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x320f30 498b4608 mov rax, qword ptr [r14 + 8]
0x320f34 8b4010 mov eax, dword ptr [rax + 0x10]
0x320f37 f7d0 not eax
0x320f39 a901800000 test eax, 0x8001
0x320f3e 7414 je 0x320f54
0x320f40 4d8b7618 mov r14, qword ptr [r14 + 0x18]
0x320f44 4d85f6 test r14, r14
0x320f47 75e7 jne 0x320f30
0x320f49 4883c408 add rsp, 8
0x320f4d 5b pop rbx
0x320f4e 415e pop r14
0x320f50 415f pop r15
0x320f52 5d pop rbp
0x320f53 c3 ret 
0x320f54 4d85ff test r15, r15
0x320f57 7440 je 0x320f99
0x320f59 4c89f7 mov rdi, r14
0x320f5c e81f380400 call 0x364780
0x320f61 84c0 test al, al
0x320f63 750b jne 0x320f70
0x320f65 41095e28 or dword ptr [r14 + 0x28], ebx
0x320f69 0f1f8000000000 nop dword ptr [rax]
0x320f70 4d8b7618 mov r14, qword ptr [r14 + 0x18]
0x320f74 4d85f6 test r14, r14
0x320f77 74d0 je 0x320f49
0x320f79 498b4608 mov rax, qword ptr [r14 + 8]
0x320f7d 8b4010 mov eax, dword ptr [rax + 0x10]
0x320f80 f7d0 not eax
0x320f82 a901800000 test eax, 0x8001
0x320f87 75e7 jne 0x320f70
0x320f89 ebce jmp 0x320f59
0x320f8b 0f1f440000 nop dword ptr [rax + rax]
0x320f90 4d8b7618 mov r14, qword ptr [r14 + 0x18]
0x320f94 4d85f6 test r14, r14
0x320f97 74b0 je 0x320f49
0x320f99 4c89f7 mov rdi, r14
0x320f9c e8df370400 call 0x364780
0x320fa1 84c0 test al, al
0x320fa3 75eb jne 0x320f90
0x320fa5 41095e28 or dword ptr [r14 + 0x28], ebx
0x320fa9 ebe5 jmp 0x320f90
0x320fab cc int3 
0x320fac cc int3 
0x320fad cc int3 
0x320fae cc int3 
0x320faf cc int3 
0x320fb0 55 push rbp
0x320fb1 4889e5 mov rbp, rsp
0x320fb4 4157 push r15
0x320fb6 4156 push r14
0x320fb8 4154 push r12
0x320fba 53 push rbx
0x320fbb 4589c6 mov r14d, r8d
0x320fbe 4889cb mov rbx, rcx
0x320fc1 4989ff mov r15, rdi
0x320fc4 4885c9 test rcx, rcx
0x320fc7 740a je 0x320fd3
0x320fc9 4c634b08 movsxd r9, dword ptr [rbx + 8]
0x320fcd 4c63430c movsxd r8, dword ptr [rbx + 0xc]
0x320fd1 eb04 jmp 0x320fd7
0x320fd3 4c8b4510 mov r8, qword ptr [rbp + 0x10]
0x320fd7 4531e4 xor r12d, r12d
0x320fda 4885db test rbx, rbx
0x320fdd 4c89ff mov rdi, r15
0x320fe0 4c89c9 mov rcx, r9
0x320fe3 410f95c4 setne r12b
0x320fe7 e83498ffff call 0x31a820
0x320fec 488d057d66bb07 lea rax, [rip + 0x7bb667d]
0x320ff3 4c89ff mov rdi, r15
0x320ff6 4509f4 or r12d, r14d
0x320ff9 498907 mov qword ptr [r15], rax
0x320ffc 4589a7b0000000 mov dword ptr [r15 + 0xb0], r12d
0x321003 41c687b400000000 mov byte ptr [r15 + 0xb4], 0
0x32100b 49899fb8000000 mov qword ptr [r15 + 0xb8], rbx
0x321012 e8c9010000 call 0x3211e0
0x321017 5b pop rbx
0x321018 415c pop r12
0x32101a 415e pop r14
0x32101c 415f pop r15
0x32101e 5d pop rbp
0x32101f c3 ret 
0x321020 55 push rbp
0x321021 4889e5 mov rbp, rsp
0x321024 4157 push r15
0x321026 4156 push r14
0x321028 4154 push r12
0x32102a 53 push rbx
0x32102b 4883ec30 sub rsp, 0x30
0x32102f 4c8b25d20ee108 mov r12, qword ptr [rip + 0x8e10ed2]
0x321036 498b0424 mov rax, qword ptr [r12]
0x32103a 488945d8 mov qword ptr [rbp - 0x28], rax
0x32103e 48897db8 mov qword ptr [rbp - 0x48], rdi
0x321042 488975c0 mov qword ptr [rbp - 0x40], rsi
0x321046 488955b0 mov qword ptr [rbp - 0x50], rdx
0x32104a 0fb6053fd90709 movzx eax, byte ptr [rip + 0x907d93f]
0x321051 84c0 test al, al
0x321053 0f84f7000000 je 0x321150
0x321059 488b5db8 mov rbx, qword ptr [rbp - 0x48]
0x32105d 89df mov edi, ebx
0x32105f e86c8af1ff call 0x239ad0
0x321064 4c8b75c0 mov r14, qword ptr [rbp - 0x40]
0x321068 4189c7 mov r15d, eax
0x32106b 4489f7 mov edi, r14d
0x32106e e85d8af1ff call 0x239ad0
0x321073 8b0dcfd80709 mov ecx, dword ptr [rip + 0x907d8cf]
0x321079 3b0df5d80709 cmp ecx, dword ptr [rip + 0x907d8f5]
0x32107f 0f848e000000 je 0x321113
0x321085 4c89f1 mov rcx, r14
0x321088 488d15e9d80709 lea rdx, [rip + 0x907d8e9]
0x32108f 48c1e920 shr rcx, 0x20
0x321093 01c8 add eax, ecx
0x321095 4889d9 mov rcx, rbx
0x321098 48c1e920 shr rcx, 0x20
0x32109c 4101cf add r15d, ecx
0x32109f 4489f9 mov ecx, r15d
0x3210a2 c1e106 shl ecx, 6
0x3210a5 01c1 add ecx, eax
0x3210a7 4489f8 mov eax, r15d
0x3210aa c1e802 shr eax, 2
0x3210ad 8d8408b979379e lea eax, [rax + rcx - 0x61c88647]
0x3210b4 488b0dc5d80709 mov rcx, qword ptr [rip + 0x907d8c5]
0x3210bb 4431f8 xor eax, r15d
0x3210be 4885c9 test rcx, rcx
0x3210c1 480f45d1 cmovne rdx, rcx
0x3210c5 8b0dbdd80709 mov ecx, dword ptr [rip + 0x907d8bd]
0x3210cb ffc9 dec ecx
0x3210cd 21c1 and ecx, eax
0x3210cf 8b0c8a mov ecx, dword ptr [rdx + rcx*4]
0x3210d2 83f9ff cmp ecx, -1
0x3210d5 743c je 0x321113
0x3210d7 488b0562d80709 mov rax, qword ptr [rip + 0x907d862]
0x3210de eb08 jmp 0x3210e8
0x3210e0 8b4918 mov ecx, dword ptr [rcx + 0x18]
0x3210e3 83f9ff cmp ecx, -1
0x3210e6 742b je 0x321113
0x3210e8 4863d1 movsxd rdx, ecx
0x3210eb 48c1e205 shl rdx, 5
0x3210ef 488d0c10 lea rcx, [rax + rdx]
0x3210f3 48391c10 cmp qword ptr [rax + rdx], rbx
0x3210f7 75e7 jne 0x3210e0
0x3210f9 4c397108 cmp qword ptr [rcx + 8], r14
0x3210fd 75e1 jne 0x3210e0
0x3210ff 4885c9 test rcx, rcx
0x321102 740f je 0x321113
0x321104 488b7910 mov rdi, qword ptr [rcx + 0x10]
0x321108 4885ff test rdi, rdi
0x32110b 7406 je 0x321113
0x32110d 488b07 mov rax, qword ptr [rdi]
0x321110 ff5008 call qword ptr [rax + 8]
0x321113 488d45b8 lea rax, [rbp - 0x48]
0x321117 488d3d22d80709 lea rdi, [rip + 0x907d822]
0x32111e 488d75c8 lea rsi, [rbp - 0x38]
0x321122 31d2 xor edx, edx
0x321124 488945c8 mov qword ptr [rbp - 0x38], rax
0x321128 488d45b0 lea rax, [rbp - 0x50]
0x32112c 488945d0 mov qword ptr [rbp - 0x30], rax
0x321130 e8abf10000 call 0x3302e0
0x321135 498b0424 mov rax, qword ptr [r12]
0x321139 483b45d8 cmp rax, qword ptr [rbp - 0x28]
0x32113d 0f8588000000 jne 0x3211cb
0x321143 4883c430 add rsp, 0x30
0x321147 5b pop rbx
0x321148 415c pop r12
0x32114a 415e pop r14
0x32114c 415f pop r15
0x32114e 5d pop rbp
0x32114f c3 ret 
0x321150 488d3d39d80709 lea rdi, [rip + 0x907d839]
0x321157 e8540d1507 call 0x7471eb0
0x32115c 85c0 test eax, eax
0x32115e 0f84f5feffff je 0x321059
0x321164 c5f8100db4b87107 vmovups xmm1, xmmword ptr [rip + 0x771b8b4]
0x32116c 488d35cdd70709 lea rsi, [rip + 0x907d7cd]
0x321173 488d3d96cf0000 lea rdi, [rip + 0xcf96]
0x32117a 488d157faefb08 lea rdx, [rip + 0x8fbae7f]
0x321181 48c705d4d7070900000000 mov qword ptr [rip + 0x907d7d4], 0
0x32118c c5f857c0 vxorps xmm0, xmm0, xmm0
0x321190 c5f81105a8d70709 vmovups xmmword ptr [rip + 0x907d7a8], xmm0
0x321198 c5f8110dc8d70709 vmovups xmmword ptr [rip + 0x907d7c8], xmm1
0x3211a0 48c705d5d7070900000000 mov qword ptr [rip + 0x907d7d5], 0
0x3211ab c705d3d7070900000000 mov dword ptr [rip + 0x907d7d3], 0
0x3211b5 e8160d1507 call 0x7471ed0
0x3211ba 488d3dcfd70709 lea rdi, [rip + 0x907d7cf]
0x3211c1 e8fa0c1507 call 0x7471ec0
0x3211c6 e98efeffff jmp 0x321059
0x3211cb e8900c1507 call 0x7471e60
0x3211d0 0f0b ud2 
0x3211d2 cc int3 
0x3211d3 cc int3 
0x3211d4 cc int3 
0x3211d5 cc int3 
0x3211d6 cc int3 
0x3211d7 cc int3 
0x3211d8 cc int3 
0x3211d9 cc int3 
0x3211da cc int3 
0x3211db cc int3 
0x3211dc cc int3 
0x3211dd cc int3 
0x3211de cc int3 
0x3211df cc int3 
0x3211e0 55 push rbp
0x3211e1 4889e5 mov rbp, rsp
0x3211e4 4157 push r15
0x3211e6 4156 push r14
0x3211e8 4155 push r13
0x3211ea 4154 push r12
0x3211ec 53 push rbx
0x3211ed 4883ec28 sub rsp, 0x28
0x3211f1 4c8b2d100de108 mov r13, qword ptr [rip + 0x8e10d10]
0x3211f8 498b4500 mov rax, qword ptr [r13]
0x3211fc 488945d0 mov qword ptr [rbp - 0x30], rax
0x321200 80bfb400000000 cmp byte ptr [rdi + 0xb4], 0
0x321207 0f8510040000 jne 0x32161d
0x32120d 4889fb mov rbx, rdi
0x321210 488bbfb8000000 mov rdi, qword ptr [rdi + 0xb8]
0x321217 4885ff test rdi, rdi
0x32121a 0f84d9010000 je 0x3213f9
0x321220 488b07 mov rax, qword ptr [rdi]
0x321223 ff5010 call qword ptr [rax + 0x10]
0x321226 480fbae223 bt rdx, 0x23
0x32122b 7214 jb 0x321241
0x32122d 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321234 488b07 mov rax, qword ptr [rdi]
0x321237 ff5010 call qword ptr [rax + 0x10]
0x32123a 480fbae224 bt rdx, 0x24
0x32123f 7307 jae 0x321248
0x321241 808bb100000008 or byte ptr [rbx + 0xb1], 8
0x321248 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x32124f 488b07 mov rax, qword ptr [rdi]
0x321252 ff5010 call qword ptr [rax + 0x10]
0x321255 480fbae225 bt rdx, 0x25
0x32125a 7307 jae 0x321263
0x32125c 808bb200000004 or byte ptr [rbx + 0xb2], 4
0x321263 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x32126a 488b07 mov rax, qword ptr [rdi]
0x32126d ff5010 call qword ptr [rax + 0x10]
0x321270 480fbae229 bt rdx, 0x29
0x321275 7307 jae 0x32127e
0x321277 808bb200000020 or byte ptr [rbx + 0xb2], 0x20
0x32127e 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321285 488b07 mov rax, qword ptr [rdi]
0x321288 ff5010 call qword ptr [rax + 0x10]
0x32128b 480fbae226 bt rdx, 0x26
0x321290 7322 jae 0x3212b4
0x321292 808bb100000004 or byte ptr [rbx + 0xb1], 4
0x321299 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3212a0 488b07 mov rax, qword ptr [rdi]
0x3212a3 ff5010 call qword ptr [rax + 0x10]
0x3212a6 480fbae227 bt rdx, 0x27
0x3212ab 7307 jae 0x3212b4
0x3212ad 808bb200000040 or byte ptr [rbx + 0xb2], 0x40
0x3212b4 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3212bb 488b07 mov rax, qword ptr [rdi]
0x3212be ff5010 call qword ptr [rax + 0x10]
0x3212c1 480fbae228 bt rdx, 0x28
0x3212c6 7307 jae 0x3212cf
0x3212c8 808bb200000010 or byte ptr [rbx + 0xb2], 0x10
0x3212cf 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3212d6 488b07 mov rax, qword ptr [rdi]
0x3212d9 ff5010 call qword ptr [rax + 0x10]
0x3212dc b800600000 mov eax, 0x6000
0x3212e1 480fbae22a bt rdx, 0x2a
0x3212e6 7234 jb 0x32131c
0x3212e8 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3212ef 488b07 mov rax, qword ptr [rdi]
0x3212f2 ff5010 call qword ptr [rax + 0x10]
0x3212f5 480fbae22d bt rdx, 0x2d
0x3212fa 7307 jae 0x321303
0x3212fc 808bb100000010 or byte ptr [rbx + 0xb1], 0x10
0x321303 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x32130a 488b07 mov rax, qword ptr [rdi]
0x32130d ff5010 call qword ptr [rax + 0x10]
0x321310 b800400000 mov eax, 0x4000
0x321315 480fbae222 bt rdx, 0x22
0x32131a 7206 jb 0x321322
0x32131c 0983b0000000 or dword ptr [rbx + 0xb0], eax
0x321322 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321329 49be0000000000040000 movabs r14, 0x40000000000
0x321333 488b07 mov rax, qword ptr [rdi]
0x321336 ff5010 call qword ptr [rax + 0x10]
0x321339 480fbae221 bt rdx, 0x21
0x32133e 7307 jae 0x321347
0x321340 808bb100000080 or byte ptr [rbx + 0xb1], 0x80
0x321347 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x32134e 488b07 mov rax, qword ptr [rdi]
0x321351 ff5010 call qword ptr [rax + 0x10]
0x321354 4c85f2 test rdx, r14
0x321357 0f8403020000 je 0x321560
0x32135d 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321364 49be0000000002000000 movabs r14, 0x200000000
0x32136e 488b07 mov rax, qword ptr [rdi]
0x321371 ff5010 call qword ptr [rax + 0x10]
0x321374 4c85f2 test rdx, r14
0x321377 0f85e3010000 jne 0x321560
0x32137d 488b83b8000000 mov rax, qword ptr [rbx + 0xb8]
0x321384 31f6 xor esi, esi
0x321386 4c637008 movsxd r14, dword ptr [rax + 8]
0x32138a 4c89f7 mov rdi, r14
0x32138d e88e66d9ff call 0xb7a20
0x321392 4889c7 mov rdi, rax
0x321395 4c89f2 mov rdx, r14
0x321398 31f6 xor esi, esi
0x32139a 4989c7 mov r15, rax
0x32139d e89e0a1507 call 0x7471e40
0x3213a2 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3213a9 4c89fe mov rsi, r15
0x3213ac 488b07 mov rax, qword ptr [rdi]
0x3213af ff5018 call qword ptr [rax + 0x18]
0x3213b2 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3213b9 4c89fe mov rsi, r15
0x3213bc 488b07 mov rax, qword ptr [rdi]
0x3213bf ff5018 call qword ptr [rax + 0x18]
0x3213c2 4d85f6 test r14, r14
0x3213c5 0f8e86010000 jle 0x321551
0x3213cb b801000000 mov eax, 1
0x3213d0 450fb66407ff movzx r12d, byte ptr [r15 + rax - 1]
0x3213d6 4c39f0 cmp rax, r14
0x3213d9 7d08 jge 0x3213e3
0x3213db 48ffc0 inc rax
0x3213de 4584e4 test r12b, r12b
0x3213e1 74ed je 0x3213d0
0x3213e3 4c89ff mov rdi, r15
0x3213e6 e8a566d9ff call 0xb7a90
0x3213eb 4584e4 test r12b, r12b
0x3213ee 0f856c010000 jne 0x321560
0x3213f4 e960010000 jmp 0x321559
0x3213f9 0fb60590d50709 movzx eax, byte ptr [rip + 0x907d590]
0x321400 84c0 test al, al
0x321402 0f8466020000 je 0x32166e
0x321408 4889df mov rdi, rbx
0x32140b e8404d1600 call 0x486150
0x321410 488b7018 mov rsi, qword ptr [rax + 0x18]
0x321414 488b5318 mov rdx, qword ptr [rbx + 0x18]
0x321418 488d7dc0 lea rdi, [rbp - 0x40]
0x32141c c5f857c0 vxorps xmm0, xmm0, xmm0
0x321420 c5f81145c0 vmovups xmmword ptr [rbp - 0x40], xmm0
0x321425 e866191400 call 0x462d90
0x32142a 4c8b75c0 mov r14, qword ptr [rbp - 0x40]
0x32142e 4c8b7dc8 mov r15, qword ptr [rbp - 0x38]
0x321432 4489f7 mov edi, r14d
0x321435 e89686f1ff call 0x239ad0
0x32143a 4489ff mov edi, r15d
0x32143d 4189c4 mov r12d, eax
0x321440 e88b86f1ff call 0x239ad0
0x321445 8b0dfdd40709 mov ecx, dword ptr [rip + 0x907d4fd]
0x32144b 3b0d23d50709 cmp ecx, dword ptr [rip + 0x907d523]
0x321451 0f84ba000000 je 0x321511
0x321457 4c89f2 mov rdx, r14
0x32145a 4c89f9 mov rcx, r15
0x32145d 48c1ea20 shr rdx, 0x20
0x321461 48c1e920 shr rcx, 0x20
0x321465 4101d4 add r12d, edx
0x321468 01c1 add ecx, eax
0x32146a 4489e2 mov edx, r12d
0x32146d 4489e6 mov esi, r12d
0x321470 c1e206 shl edx, 6
0x321473 c1ee02 shr esi, 2
0x321476 01d1 add ecx, edx
0x321478 488d15f9d40709 lea rdx, [rip + 0x907d4f9]
0x32147f 8d840eb979379e lea eax, [rsi + rcx - 0x61c88647]
0x321486 488b0df3d40709 mov rcx, qword ptr [rip + 0x907d4f3]
0x32148d 4431e0 xor eax, r12d
0x321490 4885c9 test rcx, rcx
0x321493 480f45d1 cmovne rdx, rcx
0x321497 8b0debd40709 mov ecx, dword ptr [rip + 0x907d4eb]
0x32149d ffc9 dec ecx
0x32149f 21c1 and ecx, eax
0x3214a1 8b0c8a mov ecx, dword ptr [rdx + rcx*4]
0x3214a4 83f9ff cmp ecx, -1
0x3214a7 7468 je 0x321511
0x3214a9 488b0590d40709 mov rax, qword ptr [rip + 0x907d490]
0x3214b0 eb16 jmp 0x3214c8
0x3214b2 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3214c0 8b4918 mov ecx, dword ptr [rcx + 0x18]
0x3214c3 83f9ff cmp ecx, -1
0x3214c6 7449 je 0x321511
0x3214c8 4863d1 movsxd rdx, ecx
0x3214cb 48c1e205 shl rdx, 5
0x3214cf 488d0c10 lea rcx, [rax + rdx]
0x3214d3 4c393410 cmp qword ptr [rax + rdx], r14
0x3214d7 75e7 jne 0x3214c0
0x3214d9 4c397908 cmp qword ptr [rcx + 8], r15
0x3214dd 75e1 jne 0x3214c0
0x3214df 4885c9 test rcx, rcx
0x3214e2 742d je 0x321511
0x3214e4 488b7910 mov rdi, qword ptr [rcx + 0x10]
0x3214e8 4889bbb8000000 mov qword ptr [rbx + 0xb8], rdi
0x3214ef 4885ff test rdi, rdi
0x3214f2 7428 je 0x32151c
0x3214f4 808bb000000001 or byte ptr [rbx + 0xb0], 1
0x3214fb 488b07 mov rax, qword ptr [rdi]
0x3214fe ff5010 call qword ptr [rax + 0x10]
0x321501 480fbae223 bt rdx, 0x23
0x321506 0f8321fdffff jae 0x32122d
0x32150c e930fdffff jmp 0x321241
0x321511 48c783b800000000000000 mov qword ptr [rbx + 0xb8], 0
0x32151c f683b000000001 test byte ptr [rbx + 0xb0], 1
0x321523 0f84ed000000 je 0x321616
0x321529 488b4318 mov rax, qword ptr [rbx + 0x18]
0x32152d 488d7dc0 lea rdi, [rbp - 0x40]
0x321531 488d75b8 lea rsi, [rbp - 0x48]
0x321535 488945b8 mov qword ptr [rbp - 0x48], rax
0x321539 e882dbf1ff call 0x23f0c0
0x32153e 837dc800 cmp dword ptr [rbp - 0x38], 0
0x321542 0f84ee000000 je 0x321636
0x321548 488b4dc0 mov rcx, qword ptr [rbp - 0x40]
0x32154c e9ec000000 jmp 0x32163d
0x321551 4c89ff mov rdi, r15
0x321554 e83765d9ff call 0xb7a90
0x321559 808bb100000080 or byte ptr [rbx + 0xb1], 0x80
0x321560 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321567 488b07 mov rax, qword ptr [rdi]
0x32156a ff5010 call qword ptr [rax + 0x10]
0x32156d 480fbae22e bt rdx, 0x2e
0x321572 7307 jae 0x32157b
0x321574 808bb000000002 or byte ptr [rbx + 0xb0], 2
0x32157b 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321582 488b07 mov rax, qword ptr [rdi]
0x321585 ff5010 call qword ptr [rax + 0x10]
0x321588 480fbae231 bt rdx, 0x31
0x32158d 7307 jae 0x321596
0x32158f 808bb000000040 or byte ptr [rbx + 0xb0], 0x40
0x321596 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x32159d 488b07 mov rax, qword ptr [rdi]
0x3215a0 ff5010 call qword ptr [rax + 0x10]
0x3215a3 480fbae22f bt rdx, 0x2f
0x3215a8 7307 jae 0x3215b1
0x3215aa 808bb200000001 or byte ptr [rbx + 0xb2], 1
0x3215b1 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3215b8 488b07 mov rax, qword ptr [rdi]
0x3215bb ff5010 call qword ptr [rax + 0x10]
0x3215be 480fbae230 bt rdx, 0x30
0x3215c3 7307 jae 0x3215cc
0x3215c5 808bb200000002 or byte ptr [rbx + 0xb2], 2
0x3215cc 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3215d3 488b07 mov rax, qword ptr [rdi]
0x3215d6 ff5010 call qword ptr [rax + 0x10]
0x3215d9 480fbae232 bt rdx, 0x32
0x3215de 7214 jb 0x3215f4
0x3215e0 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x3215e7 488b07 mov rax, qword ptr [rdi]
0x3215ea ff5010 call qword ptr [rax + 0x10]
0x3215ed 480fbae233 bt rdx, 0x33
0x3215f2 7307 jae 0x3215fb
0x3215f4 808bb200000008 or byte ptr [rbx + 0xb2], 8
0x3215fb 488bbbb8000000 mov rdi, qword ptr [rbx + 0xb8]
0x321602 488b07 mov rax, qword ptr [rdi]
0x321605 ff5010 call qword ptr [rax + 0x10]
0x321608 480fbae239 bt rdx, 0x39
0x32160d 7307 jae 0x321616
0x32160f 808bb300000004 or byte ptr [rbx + 0xb3], 4
0x321616 c683b400000001 mov byte ptr [rbx + 0xb4], 1
0x32161d 498b4500 mov rax, qword ptr [r13]
0x321621 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x321625 7540 jne 0x321667
0x321627 4883c428 add rsp, 0x28
0x32162b 5b pop rbx
0x32162c 415c pop r12
0x32162e 415d pop r13
0x321630 415e pop r14
0x321632 415f pop r15
0x321634 5d pop rbp
0x321635 c3 ret 
0x321636 488d0d63df3007 lea rcx, [rip + 0x730df63]
0x32163d 488d3dee9f8407 lea rdi, [rip + 0x7849fee]
0x321644 488d150d735d07 lea rdx, [rip + 0x75d730d]
0x32164b be280c0000 mov esi, 0xc28
0x321650 31c0 xor eax, eax
0x321652 e8593ae5ff call 0x1750b0
0x321657 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0x32165b 4885ff test rdi, rdi
0x32165e 74b6 je 0x321616
0x321660 e82b64d9ff call 0xb7a90
0x321665 ebaf jmp 0x321616
0x321667 e8f4071507 call 0x7471e60
0x32166c 0f0b ud2 
0x32166e 488d3d1bd30709 lea rdi, [rip + 0x907d31b]
0x321675 e836081507 call 0x7471eb0
0x32167a 85c0 test eax, eax
0x32167c 0f8486fdffff je 0x321408
0x321682 c5f8100d96b37107 vmovups xmm1, xmmword ptr [rip + 0x771b396]
0x32168a 488d35afd20709 lea rsi, [rip + 0x907d2af]
0x321691 488d3d78ca0000 lea rdi, [rip + 0xca78]
0x321698 488d1561a9fb08 lea rdx, [rip + 0x8fba961]
0x32169f 48c705b6d2070900000000 mov qword ptr [rip + 0x907d2b6], 0
0x3216aa c5f857c0 vxorps xmm0, xmm0, xmm0
0x3216ae c5f811058ad20709 vmovups xmmword ptr [rip + 0x907d28a], xmm0
0x3216b6 c5f8110daad20709 vmovups xmmword ptr [rip + 0x907d2aa], xmm1
0x3216be 48c705b7d2070900000000 mov qword ptr [rip + 0x907d2b7], 0
0x3216c9 c705b5d2070900000000 mov dword ptr [rip + 0x907d2b5], 0
0x3216d3 e8f8071507 call 0x7471ed0
0x3216d8 488d3db1d20709 lea rdi, [rip + 0x907d2b1]
0x3216df e8dc071507 call 0x7471ec0
0x3216e4 e91ffdffff jmp 0x321408
0x3216e9 cc int3 
0x3216ea cc int3 
0x3216eb cc int3 
0x3216ec cc int3 
0x3216ed cc int3 
0x3216ee cc int3 
0x3216ef cc int3 
0x3216f0 55 push rbp
0x3216f1 4889e5 mov rbp, rsp
0x3216f4 4157 push r15
0x3216f6 4156 push r14
0x3216f8 4154 push r12
0x3216fa 53 push rbx
0x3216fb 4883ec10 sub rsp, 0x10
0x3216ff 4c8b3d0208e108 mov r15, qword ptr [rip + 0x8e10802]
0x321706 4989f6 mov r14, rsi
0x321709 4889fb mov rbx, rdi
0x32170c 498b07 mov rax, qword ptr [r15]
0x32170f 488945d8 mov qword ptr [rbp - 0x28], rax
0x321713 e8d8b4ffff call 0x31cbf0
0x321718 448ba3b0000000 mov r12d, dword ptr [rbx + 0xb0]
0x32171f 4489e0 mov eax, r12d
0x321722 25bd0380ff and eax, 0xff8003bd
0x321727 8945d4 mov dword ptr [rbp - 0x2c], eax
0x32172a 498b4608 mov rax, qword ptr [r14 + 8]
0x32172e 488b10 mov rdx, qword ptr [rax]
0x321731 488d4a04 lea rcx, [rdx + 4]
0x321735 483b4808 cmp rcx, qword ptr [rax + 8]
0x321739 7625 jbe 0x321760
0x32173b 41f6462a01 test byte ptr [r14 + 0x2a], 1
0x321740 752f jne 0x321771
0x321742 498b06 mov rax, qword ptr [r14]
0x321745 488d75d4 lea rsi, [rbp - 0x2c]
0x321749 ba04000000 mov edx, 4
0x32174e 4c89f7 mov rdi, r14
0x321751 ff9088010000 call qword ptr [rax + 0x188]
0x321757 41f6462801 test byte ptr [r14 + 0x28], 1
0x32175c 7526 jne 0x321784
0x32175e eb76 jmp 0x3217d6
0x321760 8b12 mov edx, dword ptr [rdx]
0x321762 8955d4 mov dword ptr [rbp - 0x2c], edx
0x321765 488908 mov qword ptr [rax], rcx
0x321768 41f6462801 test byte ptr [r14 + 0x28], 1
0x32176d 7515 jne 0x321784
0x32176f eb65 jmp 0x3217d6
0x321771 488d75d4 lea rsi, [rbp - 0x2c]
0x321775 4c89f7 mov rdi, r14
0x321778 e833b3efff call 0x21cab0
0x32177d 41f6462801 test byte ptr [r14 + 0x28], 1
0x321782 7452 je 0x3217d6
0x321784 8b45d4 mov eax, dword ptr [rbp - 0x2c]
0x321787 41bebd0380ff mov r14d, 0xff8003bd
0x32178d 4889df mov rdi, rbx
0x321790 4421f0 and eax, r14d
0x321793 8983b0000000 mov dword ptr [rbx + 0xb0], eax
0x321799 c683b400000000 mov byte ptr [rbx + 0xb4], 0
0x3217a0 48c783b800000000000000 mov qword ptr [rbx + 0xb8], 0
0x3217ab 488b03 mov rax, qword ptr [rbx]
0x3217ae ff9070030000 call qword ptr [rax + 0x370]
0x3217b4 4883bbb800000000 cmp qword ptr [rbx + 0xb8], 0
0x3217bc 7518 jne 0x3217d6
0x3217be 4423b3b0000000 and r14d, dword ptr [rbx + 0xb0]
0x3217c5 4181e442fc7f00 and r12d, 0x7ffc42
0x3217cc 4509e6 or r14d, r12d
0x3217cf 4489b3b0000000 mov dword ptr [rbx + 0xb0], r14d
0x3217d6 498b07 mov rax, qword ptr [r15]
0x3217d9 483b45d8 cmp rax, qword ptr [rbp - 0x28]
0x3217dd 750d jne 0x3217ec
0x3217df 4883c410 add rsp, 0x10
0x3217e3 5b pop rbx
0x3217e4 415c pop r12
0x3217e6 415e pop r14
0x3217e8 415f pop r15
0x3217ea 5d pop rbp
0x3217eb c3 ret 
0x3217ec e86f061507 call 0x7471e60
0x3217f1 0f0b ud2 
0x3217f3 cc int3 
0x3217f4 cc int3 
0x3217f5 cc int3 
0x3217f6 cc int3 
0x3217f7 cc int3 
0x3217f8 cc int3 
0x3217f9 cc int3 
0x3217fa cc int3 
0x3217fb cc int3 
0x3217fc cc int3 
0x3217fd cc int3 
0x3217fe cc int3 
0x3217ff cc int3 
0x321800 55 push rbp
0x321801 4889e5 mov rbp, rsp
0x321804 488b4628 mov rax, qword ptr [rsi + 0x28]
0x321808 a805 test al, 5
0x32180a 0f94c1 sete cl
0x32180d 83e020 and eax, 0x20
0x321810 c1e805 shr eax, 5
0x321813 08c8 or al, cl
0x321815 b001 mov al, 1
0x321817 750c jne 0x321825
0x321819 0fb687b0000000 movzx eax, byte ptr [rdi + 0xb0]
0x321820 2420 and al, 0x20
0x321822 c0e805 shr al, 5
0x321825 5d pop rbp
0x321826 c3 ret 
0x321827 cc int3 
0x321828 cc int3 
0x321829 cc int3 
0x32182a cc int3 
0x32182b cc int3 
0x32182c cc int3 
0x32182d cc int3 
0x32182e cc int3 
0x32182f cc int3 
0x321830 55 push rbp
0x321831 4889e5 mov rbp, rsp
0x321834 53 push rbx
0x321835 4883ec28 sub rsp, 0x28
0x321839 488b1dc806e108 mov rbx, qword ptr [rip + 0x8e106c8]
0x321840 0fb64518 movzx eax, byte ptr [rbp + 0x18]
0x321844 4c8b5510 mov r10, qword ptr [rbp + 0x10]
0x321848 4c8b1b mov r11, qword ptr [rbx]
0x32184b 0fb6c0 movzx eax, al
0x32184e 4c895df0 mov qword ptr [rbp - 0x10], r11
0x321852 4c8955d8 mov qword ptr [rbp - 0x28], r10
0x321856 4c8d55d8 lea r10, [rbp - 0x28]
0x32185a 4c8d1ddfef0000 lea r11, [rip + 0xefdf]
0x321861 4c8955e8 mov qword ptr [rbp - 0x18], r10
0x321865 4c895de0 mov qword ptr [rbp - 0x20], r11
0x321869 4c8d55e0 lea r10, [rbp - 0x20]
0x32186d 50 push rax
0x32186e 4152 push r10
0x321870 e88b8efeff call 0x30a700
0x321875 4883c410 add rsp, 0x10
0x321879 488b0b mov rcx, qword ptr [rbx]
0x32187c 483b4df0 cmp rcx, qword ptr [rbp - 0x10]
0x321880 7507 jne 0x321889
0x321882 4883c428 add rsp, 0x28
0x321886 5b pop rbx
0x321887 5d pop rbp
0x321888 c3 ret 
0x321889 e8d2051507 call 0x7471e60
0x32188e 0f0b ud2 
0x321890 55 push rbp
0x321891 4889e5 mov rbp, rsp
0x321894 4157 push r15
0x321896 4156 push r14
0x321898 53 push rbx
0x321899 4883ec38 sub rsp, 0x38
0x32189d 4c8b3d6406e108 mov r15, qword ptr [rip + 0x8e10664]
0x3218a4 4189f6 mov r14d, esi
0x3218a7 4889fb mov rbx, rdi
0x3218aa 498b07 mov rax, qword ptr [r15]
0x3218ad 488945e0 mov qword ptr [rbp - 0x20], rax
0x3218b1 e89aa7f7ff call 0x29c050
0x3218b6 4489f6 mov esi, r14d
0x3218b9 4889c7 mov rdi, rax
0x3218bc e8afaa0300 call 0x35c370
0x3218c1 4c8d75c8 lea r14, [rbp - 0x38]
0x3218c5 488d75b8 lea rsi, [rbp - 0x48]
0x3218c9 488945b8 mov qword ptr [rbp - 0x48], rax
0x3218cd 4c89f7 mov rdi, r14
0x3218d0 e8ebd7f1ff call 0x23f0c0
0x3218d5 e876a7f7ff call 0x29c050
0x3218da 488b4018 mov rax, qword ptr [rax + 0x18]
0x3218de 488d7db8 lea rdi, [rbp - 0x48]
0x3218e2 488d75d8 lea rsi, [rbp - 0x28]
0x3218e6 488945d8 mov qword ptr [rbp - 0x28], rax
0x3218ea e8d1d7f1ff call 0x23f0c0
0x3218ef 8b55c0 mov edx, dword ptr [rbp - 0x40]
0x3218f2 31c0 xor eax, eax
0x3218f4 4c89f7 mov rdi, r14
0x3218f7 83ea01 sub edx, 1
0x3218fa 0f42d0 cmovb edx, eax
0x3218fd 31f6 xor esi, esi
0x3218ff 31c9 xor ecx, ecx
0x321901 83c202 add edx, 2
0x321904 e89756d4ff call 0x66fa0
0x321909 488b45c8 mov rax, qword ptr [rbp - 0x38]
0x32190d 48c745c800000000 mov qword ptr [rbp - 0x38], 0
0x321915 488b7db8 mov rdi, qword ptr [rbp - 0x48]
0x321919 488b4dd0 mov rcx, qword ptr [rbp - 0x30]
0x32191d 48c745d000000000 mov qword ptr [rbp - 0x30], 0
0x321925 488903 mov qword ptr [rbx], rax
0x321928 48894b08 mov qword ptr [rbx + 8], rcx
0x32192c 4885ff test rdi, rdi
0x32192f 7413 je 0x321944
0x321931 e85a61d9ff call 0xb7a90
0x321936 488b7dc8 mov rdi, qword ptr [rbp - 0x38]
0x32193a 4885ff test rdi, rdi
0x32193d 7405 je 0x321944
0x32193f e84c61d9ff call 0xb7a90
0x321944 498b07 mov rax, qword ptr [r15]
0x321947 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x32194b 750e jne 0x32195b
0x32194d 4889d8 mov rax, rbx
0x321950 4883c438 add rsp, 0x38
0x321954 5b pop rbx
0x321955 415e pop r14
0x321957 415f pop r15
0x321959 5d pop rbp
0x32195a c3 ret 
0x32195b e800051507 call 0x7471e60
0x321960 0f0b ud2 
0x321962 cc int3 
0x321963 cc int3 
0x321964 cc int3 
0x321965 cc int3 
0x321966 cc int3 
0x321967 cc int3 
0x321968 cc int3 
0x321969 cc int3 
0x32196a cc int3 
0x32196b cc int3 
0x32196c cc int3 
0x32196d cc int3 
0x32196e cc int3 
0x32196f cc int3 
0x321970 55 push rbp
0x321971 4889e5 mov rbp, rsp
0x321974 53 push rbx
0x321975 50 push rax
0x321976 4889fb mov rbx, rdi
0x321979 e89293ffff call 0x31ad10
0x32197e b8ffff7fff mov eax, 0xff7fffff
0x321983 2383b0000000 and eax, dword ptr [rbx + 0xb0]
0x321989 8983b0000000 mov dword ptr [rbx + 0xb0], eax
0x32198f 4883bbb800000000 cmp qword ptr [rbx + 0xb8], 0
0x321997 7407 je 0x3219a0
0x321999 4883c408 add rsp, 8
0x32199d 5b pop rbx
0x32199e 5d pop rbp
0x32199f c3 ret 
0x3219a0 0d00e00000 or eax, 0xe000
0x3219a5 8983b0000000 mov dword ptr [rbx + 0xb0], eax
0x3219ab 488b4b70 mov rcx, qword ptr [rbx + 0x70]
0x3219af 4885c9 test rcx, rcx
0x3219b2 74e5 je 0x321999
0x3219b4 c5fd6f05044c7307 vmovdqa ymm0, ymmword ptr [rip + 0x7734c04]
0x3219bc c5fa7e0de4457207 vmovq xmm1, qword ptr [rip + 0x77245e4]
0x3219c4 eb13 jmp 0x3219d9
0x3219c6 662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3219d0 488b4948 mov rcx, qword ptr [rcx + 0x48]
0x3219d4 4885c9 test rcx, rcx
0x3219d7 74c0 je 0x321999
0x3219d9 c4e27d595138 vpbroadcastq ymm2, qword ptr [rcx + 0x38]
0x3219df 89c2 mov edx, eax
0x3219e1 81e2ff7fffff and edx, 0xffff7fff
0x3219e7 c5eddbd0 vpand ymm2, ymm2, ymm0
0x3219eb c4e26d29d1 vpcmpeqq ymm2, ymm2, ymm1
0x3219f0 c5fd50f2 vmovmskpd esi, ymm2
0x3219f4 40f6c608 test sil, 8
0x3219f8 0f44d0 cmove edx, eax
0x3219fb 89d7 mov edi, edx
0x3219fd 81e7ffbfffff and edi, 0xffffbfff
0x321a03 40f6c604 test sil, 4
0x321a07 0f44fa cmove edi, edx
0x321a0a 89f8 mov eax, edi
0x321a0c 25ffdfffff and eax, 0xffffdfff
0x321a11 40f6c602 test sil, 2
0x321a15 0f44c7 cmove eax, edi
0x321a18 40f6c601 test sil, 1
0x321a1c 75b2 jne 0x3219d0
0x321a1e 8983b0000000 mov dword ptr [rbx + 0xb0], eax
0x321a24 ebaa jmp 0x3219d0
0x321a26 cc int3 
0x321a27 cc int3 
0x321a28 cc int3 
0x321a29 cc int3 
0x321a2a cc int3 
0x321a2b cc int3 
0x321a2c cc int3 
0x321a2d cc int3 
0x321a2e cc int3 
0x321a2f cc int3 
0x321a30 55 push rbp
0x321a31 4889e5 mov rbp, rsp
0x321a34 488bbfb8000000 mov rdi, qword ptr [rdi + 0xb8]
0x321a3b 488b07 mov rax, qword ptr [rdi]
0x321a3e ff90b8000000 call qword ptr [rax + 0xb8]
0x321a44 5d pop rbp
0x321a45 c3 ret 
0x321a46 cc int3 
0x321a47 cc int3 
0x321a48 cc int3 
0x321a49 cc int3 
0x321a4a cc int3 
0x321a4b cc int3 
0x321a4c cc int3 
0x321a4d cc int3 
0x321a4e cc int3 
0x321a4f cc int3 
0x321a50 55 push rbp
0x321a51 4889e5 mov rbp, rsp
0x321a54 488b07 mov rax, qword ptr [rdi]
0x321a57 ba01000000 mov edx, 1
0x321a5c ff9000030000 call qword ptr [rax + 0x300]
0x321a62 5d pop rbp
0x321a63 c3 ret 
0x321a64 cc int3 
0x321a65 cc int3 
0x321a66 cc int3 
0x321a67 cc int3 
0x321a68 cc int3 
0x321a69 cc int3 
0x321a6a cc int3 
0x321a6b cc int3 
0x321a6c cc int3 
0x321a6d cc int3 
0x321a6e cc int3 
0x321a6f cc int3 
0x321a70 55 push rbp
0x321a71 4889e5 mov rbp, rsp
0x321a74 4157 push r15
0x321a76 4156 push r14
0x321a78 4155 push r13
0x321a7a 4154 push r12
0x321a7c 53 push rbx
0x321a7d 50 push rax
0x321a7e 4c8bb7b8000000 mov r14, qword ptr [rdi + 0xb8]
0x321a85 4d85f6 test r14, r14
0x321a88 741c je 0x321aa6
0x321a8a 498b06 mov rax, qword ptr [r14]
0x321a8d 4c89f7 mov rdi, r14
0x321a90 4989f5 mov r13, rsi
0x321a93 4989d4 mov r12, rdx
0x321a96 4989cf mov r15, rcx
0x321a99 4c89c3 mov rbx, r8
0x321a9c ff5010 call qword ptr [rax + 0x10]
0x321a9f 480fbae237 bt rdx, 0x37
0x321aa4 7204 jb 0x321aaa
0x321aa6 31c0 xor eax, eax
0x321aa8 eb18 jmp 0x321ac2
0x321aaa 498b06 mov rax, qword ptr [r14]
0x321aad 4c89f7 mov rdi, r14
0x321ab0 4c89ee mov rsi, r13
0x321ab3 4c89e2 mov rdx, r12
0x321ab6 4c89f9 mov rcx, r15
0x321ab9 4989d8 mov r8, rbx
0x321abc ff9098000000 call qword ptr [rax + 0x98]
0x321ac2 4883c408 add rsp, 8
0x321ac6 5b pop rbx
0x321ac7 415c pop r12
0x321ac9 415d pop r13
0x321acb 415e pop r14
0x321acd 415f pop r15
0x321acf 5d pop rbp
0x321ad0 c3 ret 
0x321ad1 cc int3 
0x321ad2 cc int3 
0x321ad3 cc int3 
0x321ad4 cc int3 
0x321ad5 cc int3 
0x321ad6 cc int3 
0x321ad7 cc int3 
0x321ad8 cc int3 
0x321ad9 cc int3 
0x321ada cc int3 
0x321adb cc int3 
0x321adc cc int3 
0x321add cc int3 
0x321ade cc int3 
0x321adf cc int3 
0x321ae0 55 push rbp
0x321ae1 4889e5 mov rbp, rsp
0x321ae4 4157 push r15
0x321ae6 4156 push r14
0x321ae8 4155 push r13
0x321aea 4154 push r12
0x321aec 53 push rbx
0x321aed 4883ec48 sub rsp, 0x48
0x321af1 488b051004e108 mov rax, qword ptr [rip + 0x8e10410]
0x321af8 488955a8 mov qword ptr [rbp - 0x58], rdx
0x321afc 31c9 xor ecx, ecx
0x321afe 4889f3 mov rbx, rsi
0x321b01 4989ff mov r15, rdi
0x321b04 488b00 mov rax, qword ptr [rax]
0x321b07 488945d0 mov qword ptr [rbp - 0x30], rax
0x321b0b 0fb64618 movzx eax, byte ptr [rsi + 0x18]
0x321b0f 3c01 cmp al, 1
0x321b11 0f94c1 sete cl
0x321b14 4c8b64cf70 mov r12, qword ptr [rdi + rcx*8 + 0x70]
0x321b19 4d85e4 test r12, r12
0x321b1c 7461 je 0x321b7f
0x321b1e 488b4da8 mov rcx, qword ptr [rbp - 0x58]
0x321b22 4c8d6dc0 lea r13, [rbp - 0x40]
0x321b26 c5f81001 vmovups xmm0, xmmword ptr [rcx]
0x321b2a 31c9 xor ecx, ecx
0x321b2c 3c01 cmp al, 1
0x321b2e 0f94c1 sete cl
0x321b31 4c8d34cd48000000 lea r14, [rcx*8 + 0x48]
0x321b39 c5f8294590 vmovaps xmmword ptr [rbp - 0x70], xmm0
0x321b3e 6690 nop 
0x321b40 c5f8284590 vmovaps xmm0, xmmword ptr [rbp - 0x70]
0x321b45 4c89ff mov rdi, r15
0x321b48 4c89e6 mov rsi, r12
0x321b4b 4889da mov rdx, rbx
0x321b4e 4c89e9 mov rcx, r13
0x321b51 c5f81145c0 vmovups xmmword ptr [rbp - 0x40], xmm0
0x321b56 e8458d1100 call 0x43a8a0
0x321b5b 84c0 test al, al
0x321b5d 7415 je 0x321b74
0x321b5f 0fb6c8 movzx ecx, al
0x321b62 83f902 cmp ecx, 2
0x321b65 7418 je 0x321b7f
0x321b67 4f8b2434 mov r12, qword ptr [r12 + r14]
0x321b6b 4d85e4 test r12, r12
0x321b6e 75d0 jne 0x321b40
0x321b70 84c0 test al, al
0x321b72 750d jne 0x321b81
0x321b74 4c8b358d03e108 mov r14, qword ptr [rip + 0x8e1038d]
0x321b7b 31c0 xor eax, eax
0x321b7d eb37 jmp 0x321bb6
0x321b7f b001 mov al, 1
0x321b81 41f687b300000004 test byte ptr [r15 + 0xb3], 4
0x321b89 4c8b357803e108 mov r14, qword ptr [rip + 0x8e10378]
0x321b90 7424 je 0x321bb6
0x321b92 488b45a8 mov rax, qword ptr [rbp - 0x58]
0x321b96 498bbfb8000000 mov rdi, qword ptr [r15 + 0xb8]
0x321b9d 488d55b0 lea rdx, [rbp - 0x50]
0x321ba1 4889de mov rsi, rbx
0x321ba4 c5f81000 vmovups xmm0, xmmword ptr [rax]
0x321ba8 c5f81145b0 vmovups xmmword ptr [rbp - 0x50], xmm0
0x321bad 488b07 mov rax, qword ptr [rdi]
0x321bb0 ff90e0000000 call qword ptr [rax + 0xe0]
0x321bb6 498b0e mov rcx, qword ptr [r14]
0x321bb9 483b4dd0 cmp rcx, qword ptr [rbp - 0x30]
0x321bbd 750f jne 0x321bce
0x321bbf 4883c448 add rsp, 0x48
0x321bc3 5b pop rbx
0x321bc4 415c pop r12
0x321bc6 415d pop r13
0x321bc8 415e pop r14
0x321bca 415f pop r15
0x321bcc 5d pop rbp
0x321bcd c3 ret 
0x321bce e88d021507 call 0x7471e60
0x321bd3 0f0b ud2 
0x321bd5 cc int3 
0x321bd6 cc int3 
0x321bd7 cc int3 
0x321bd8 cc int3 
0x321bd9 cc int3 
0x321bda cc int3 
0x321bdb cc int3 
0x321bdc cc int3 
0x321bdd cc int3 
0x321bde cc int3 
0x321bdf cc int3 
0x321be0 55 push rbp
0x321be1 4889e5 mov rbp, rsp
0x321be4 4157 push r15
0x321be6 4156 push r14
0x321be8 53 push rbx
0x321be9 50 push rax
0x321bea 48837a0800 cmp qword ptr [rdx + 8], 0
0x321bef 4889d3 mov rbx, rdx
0x321bf2 4989f6 mov r14, rsi
0x321bf5 4989ff mov r15, rdi
0x321bf8 7425 je 0x321c1f
0x321bfa 488b3b mov rdi, qword ptr [rbx]
0x321bfd e85e2a0400 call 0x364660
0x321c02 4885c0 test rax, rax
0x321c05 7418 je 0x321c1f
0x321c07 48634838 movsxd rcx, dword ptr [rax + 0x38]
0x321c0b 413b4f38 cmp ecx, dword ptr [r15 + 0x38]
0x321c0f 7f0e jg 0x321c1f
0x321c11 498b5730 mov rdx, qword ptr [r15 + 0x30]
0x321c15 4883c030 add rax, 0x30
0x321c19 483904ca cmp qword ptr [rdx + rcx*8], rax
0x321c1d 742f je 0x321c4e
0x321c1f 41f687b300000004 test byte ptr [r15 + 0xb3], 4
0x321c27 7504 jne 0x321c2d
0x321c29 31c0 xor eax, eax
0x321c2b eb16 jmp 0x321c43
0x321c2d 498bbfb8000000 mov rdi, qword ptr [r15 + 0xb8]
0x321c34 4c89f6 mov rsi, r14
0x321c37 4889da mov rdx, rbx
0x321c3a 488b07 mov rax, qword ptr [rdi]
0x321c3d ff90e8000000 call qword ptr [rax + 0xe8]
0x321c43 4883c408 add rsp, 8
0x321c47 5b pop rbx
0x321c48 415e pop r14
0x321c4a 415f pop r15
0x321c4c 5d pop rbp
0x321c4d c3 ret 
0x321c4e 488b0b mov rcx, qword ptr [rbx]
0x321c51 31c0 xor eax, eax
0x321c53 48637310 movsxd rsi, dword ptr [rbx + 0x10]
0x321c57 48635144 movsxd rdx, dword ptr [rcx + 0x44]
0x321c5b 48634930 movsxd rcx, dword ptr [rcx + 0x30]
0x321c5f 4c01f2 add rdx, r14
0x321c62 807b1401 cmp byte ptr [rbx + 0x14], 1
0x321c66 480f44c6 cmove rax, rsi
0x321c6a 480fafc1 imul rax, rcx
0x321c6e 4801d0 add rax, rdx
0x321c71 75d0 jne 0x321c43
0x321c73 ebaa jmp 0x321c1f
0x321c75 cc int3 
0x321c76 cc int3 
0x321c77 cc int3 
0x321c78 cc int3 
0x321c79 cc int3 
0x321c7a cc int3 
0x321c7b cc int3 
0x321c7c cc int3 
0x321c7d cc int3 
0x321c7e cc int3 
0x321c7f cc int3 
0x321c80 55 push rbp
0x321c81 4889e5 mov rbp, rsp
0x321c84 0fb687b2000000 movzx eax, byte ptr [rdi + 0xb2]
0x321c8b c0e807 shr al, 7
0x321c8e 5d pop rbp
0x321c8f c3 ret 
0x321c90 55 push rbp
0x321c91 4889e5 mov rbp, rsp
0x321c94 4156 push r14
0x321c96 53 push rbx
0x321c97 488b06 mov rax, qword ptr [rsi]
0x321c9a 4989fe mov r14, rdi
0x321c9d 4889f3 mov rbx, rsi
0x321ca0 4889f7 mov rdi, rsi
0x321ca3 4c89f6 mov rsi, r14
0x321ca6 ff9060010000 call qword ptr [rax + 0x160]
0x321cac 488b03 mov rax, qword ptr [rbx]
0x321caf 498d7608 lea rsi, [r14 + 8]
0x321cb3 4889df mov rdi, rbx
0x321cb6 ff9068010000 call qword ptr [rax + 0x168]
0x321cbc 488b03 mov rax, qword ptr [rbx]
0x321cbf 498d7630 lea rsi, [r14 + 0x30]
0x321cc3 4889df mov rdi, rbx
0x321cc6 ff9070010000 call qword ptr [rax + 0x170]
0x321ccc 488b08 mov rcx, qword ptr [rax]
0x321ccf 4983c650 add r14, 0x50
0x321cd3 4889c7 mov rdi, rax
0x321cd6 4c89f6 mov rsi, r14
0x321cd9 ff9178010000 call qword ptr [rcx + 0x178]
0x321cdf b001 mov al, 1
0x321ce1 5b pop rbx
0x321ce2 415e pop r14
0x321ce4 5d pop rbp
0x321ce5 c3 ret 
0x321ce6 cc int3 
0x321ce7 cc int3 
0x321ce8 cc int3 
0x321ce9 cc int3 
0x321cea cc int3 
0x321ceb cc int3 
0x321cec cc int3 
0x321ced cc int3 
0x321cee cc int3 
0x321cef cc int3 
0x321cf0 55 push rbp
0x321cf1 4889e5 mov rbp, rsp
0x321cf4 4157 push r15
0x321cf6 4156 push r14
0x321cf8 4155 push r13
0x321cfa 4154 push r12
0x321cfc 53 push rbx
0x321cfd 4883ec18 sub rsp, 0x18
0x321d01 4c8b3d0002e108 mov r15, qword ptr [rip + 0x8e10200]
0x321d08 498b07 mov rax, qword ptr [r15]
0x321d0b 488945d0 mov qword ptr [rbp - 0x30], rax
0x321d0f 488b0512c70709 mov rax, qword ptr [rip + 0x907c712]
0x321d16 4885c0 test rax, rax
0x321d19 0f859a020000 jne 0x321fb9
0x321d1f 488d3d4b7e0909 lea rdi, [rip + 0x9097e4b]
0x321d26 be00020000 mov esi, 0x200
0x321d2b ba08000000 mov edx, 8
0x321d30 b901000000 mov ecx, 1
0x321d35 e836f01500 call 0x480d70
0x321d3a 488d3533ba4507 lea rsi, [rip + 0x745ba33]
0x321d41 488d7dc0 lea rdi, [rbp - 0x40]
0x321d45 ba01000000 mov edx, 1
0x321d4a 4889c3 mov rbx, rax
0x321d4d e83eb8f1ff call 0x23d590
0x321d52 4889df mov rdi, rbx
0x321d55 bac7000000 mov edx, 0xc7
0x321d5a 31f6 xor esi, esi
0x321d5c e84f430a00 call 0x3c60b0
0x321d61 488d05c05cbb07 lea rax, [rip + 0x7bb5cc0]
0x321d68 488d3571020000 lea rsi, [rip + 0x271]
0x321d6f 4c8d35f8525007 lea r14, [rip + 0x75052f8]
0x321d76 c5f857c0 vxorps xmm0, xmm0, xmm0
0x321d7a c5f8114328 vmovups xmmword ptr [rbx + 0x28], xmm0
0x321d7f c74338ffffffff mov dword ptr [rbx + 0x38], 0xffffffff
0x321d86 c5f8114340 vmovups xmmword ptr [rbx + 0x40], xmm0
0x321d8b 48c7435000000000 mov qword ptr [rbx + 0x50], 0
0x321d93 c74358c0000000 mov dword ptr [rbx + 0x58], 0xc0
0x321d9a 66c7435c0800 mov word ptr [rbx + 0x5c], 8
0x321da0 c5f857c0 vxorps xmm0, xmm0, xmm0
0x321da4 c5fc11437e vmovups ymmword ptr [rbx + 0x7e], ymm0
0x321da9 c5fc11435e vmovups ymmword ptr [rbx + 0x5e], ymm0
0x321dae c5fc118390000000 vmovups ymmword ptr [rbx + 0x90], ymm0
0x321db6 488d0da3020000 lea rcx, [rip + 0x2a3]
0x321dbd 4c8d250c4fceff lea r12, [rip - 0x31b0f4]
0x321dc4 488dbbe8000000 lea rdi, [rbx + 0xe8]
0x321dcb ba01000000 mov edx, 1
0x321dd0 488903 mov qword ptr [rbx], rax
0x321dd3 4889b3b0000000 mov qword ptr [rbx + 0xb0], rsi
0x321dda 4c89f6 mov rsi, r14
0x321ddd 48898bb8000000 mov qword ptr [rbx + 0xb8], rcx
0x321de4 4c89a3c0000000 mov qword ptr [rbx + 0xc0], r12
0x321deb 48c783c800000000000000 mov qword ptr [rbx + 0xc8], 0
0x321df6 66c783d00000000000 mov word ptr [rbx + 0xd0], 0
0x321dff c783d4000000a0000010 mov dword ptr [rbx + 0xd4], 0x100000a0
0x321e09 48c783d800000010000000 mov qword ptr [rbx + 0xd8], 0x10
0x321e14 48c783e000000000000000 mov qword ptr [rbx + 0xe0], 0
0x321e1f e86cb7f1ff call 0x23d590
0x321e24 c5f8101df4ab7107 vmovups xmm3, xmmword ptr [rip + 0x771abf4]
0x321e2c 48c7834801000000000000 mov qword ptr [rbx + 0x148], 0
0x321e37 c5f857c0 vxorps xmm0, xmm0, xmm0
0x321e3b c5fc1183f0000000 vmovups ymmword ptr [rbx + 0xf0], ymm0
0x321e43 c5fc118310010000 vmovups ymmword ptr [rbx + 0x110], ymm0
0x321e4b 48c7833001000000000000 mov qword ptr [rbx + 0x130], 0
0x321e56 c5e857d2 vxorps xmm2, xmm2, xmm2
0x321e5a c5f057c9 vxorps xmm1, xmm1, xmm1
0x321e5e c5f8119b50010000 vmovups xmmword ptr [rbx + 0x150], xmm3
0x321e66 48c7836801000000000000 mov qword ptr [rbx + 0x168], 0
0x321e71 c7837001000000000000 mov dword ptr [rbx + 0x170], 0
0x321e7b c6837801000000 mov byte ptr [rbx + 0x178], 0
0x321e82 48c783a001000000000000 mov qword ptr [rbx + 0x1a0], 0
0x321e8d c5f8118b80010000 vmovups xmmword ptr [rbx + 0x180], xmm1
0x321e95 c5f8119ba8010000 vmovups xmmword ptr [rbx + 0x1a8], xmm3
0x321e9d 48c783c001000000000000 mov qword ptr [rbx + 0x1c0], 0
0x321ea8 c783c801000000000000 mov dword ptr [rbx + 0x1c8], 0
0x321eb2 c683d001000000 mov byte ptr [rbx + 0x1d0], 0
0x321eb9 c5fc1193d8010000 vmovups ymmword ptr [rbx + 0x1d8], ymm2
0x321ec1 48c783f801000000000000 mov qword ptr [rbx + 0x1f8], 0
0x321ecc 48891d55c50709 mov qword ptr [rip + 0x907c555], rbx
0x321ed3 4c8b2d36c50709 mov r13, qword ptr [rip + 0x907c536]
0x321eda 4d85ed test r13, r13
0x321edd 756e jne 0x321f4d
0x321edf 488d050aa6f7ff lea rax, [rip - 0x859f6]
0x321ee6 4c8d2df3eeffff lea r13, [rip - 0x110d]
0x321eed 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x321ef1 4c8d151889ffff lea r10, [rip - 0x76e8]
0x321ef8 4c8d5dc8 lea r11, [rbp - 0x38]
0x321efc 4c8d253defffff lea r12, [rip - 0x10c3]
0x321f03 488d3dba515007 lea rdi, [rip + 0x75051ba]
0x321f0a 488d358fad5a07 lea rsi, [rip + 0x75aad8f]
0x321f11 488d15f8c40709 lea rdx, [rip + 0x907c4f8]
0x321f18 488d0d7193ceff lea rcx, [rip - 0x316c8f]
0x321f1f 41b8b0000000 mov r8d, 0xb0
0x321f25 41b908000000 mov r9d, 8
0x321f2b 50 push rax
0x321f2c 4152 push r10
0x321f2e 4153 push r11
0x321f30 4154 push r12
0x321f32 4155 push r13
0x321f34 4156 push r14
0x321f36 6a08 push 8
0x321f38 6820000010 push 0x10000020
0x321f3d e81eb9feff call 0x30d860
0x321f42 4883c440 add rsp, 0x40
0x321f46 4c8b2dc3c40709 mov r13, qword ptr [rip + 0x907c4c3]
0x321f4d e85e3c0a00 call 0x3c5bb0
0x321f52 4c8b23 mov r12, qword ptr [rbx]
0x321f55 31f6 xor esi, esi
0x321f57 4939dd cmp r13, rbx
0x321f5a 4889df mov rdi, rbx
0x321f5d 4989c6 mov r14, rax
0x321f60 490f45f5 cmovne rsi, r13
0x321f64 41ff942428030000 call qword ptr [r12 + 0x328]
0x321f6c 4d85f6 test r14, r14
0x321f6f 7414 je 0x321f85
0x321f71 488d05e4ce0709 lea rax, [rip + 0x907cee4]
0x321f78 803800 cmp byte ptr [rax], 0
0x321f7b 7408 je 0x321f85
0x321f7d 4c89f7 mov rdi, r14
0x321f80 e83b630400 call 0x3682c0
0x321f85 4c89b3e0000000 mov qword ptr [rbx + 0xe0], r14
0x321f8c 4889df mov rdi, rbx
0x321f8f 488b03 mov rax, qword ptr [rbx]
0x321f92 ff5010 call qword ptr [rax + 0x10]
0x321f95 488d35a4aef7ff lea rsi, [rip - 0x8515c]
0x321f9c 488d1521515007 lea rdx, [rip + 0x7505121]
0x321fa3 488d0dcab74507 lea rcx, [rip + 0x745b7ca]
0x321faa 4889df mov rdi, rbx
0x321fad e8ee3c1600 call 0x485ca0
0x321fb2 488b056fc40709 mov rax, qword ptr [rip + 0x907c46f]
0x321fb9 498b0f mov rcx, qword ptr [r15]
0x321fbc 483b4dd0 cmp rcx, qword ptr [rbp - 0x30]
0x321fc0 750f jne 0x321fd1
0x321fc2 4883c418 add rsp, 0x18
0x321fc6 5b pop rbx
0x321fc7 415c pop r12
0x321fc9 415d pop r13
0x321fcb 415e pop r14
0x321fcd 415f pop r15
0x321fcf 5d pop rbp
0x321fd0 c3 ret 
0x321fd1 e88afe1407 call 0x7471e60
0x321fd6 0f0b ud2 
0x321fd8 cc int3 
0x321fd9 cc int3 
0x321fda cc int3 
0x321fdb cc int3 
0x321fdc cc int3 
0x321fdd cc int3 
0x321fde cc int3 
0x321fdf cc int3 
0x321fe0 55 push rbp
0x321fe1 4889e5 mov rbp, rsp
0x321fe4 53 push rbx
0x321fe5 50 push rax
0x321fe6 488b1f mov rbx, qword ptr [rdi]
0x321fe9 4889fe mov rsi, rdi
0x321fec 4889df mov rdi, rbx
0x321fef e82cfc1600 call 0x491c20
0x321ff4 488d057556bb07 lea rax, [rip + 0x7bb5675]
0x321ffb 48c7433000000000 mov qword ptr [rbx + 0x30], 0
0x322003 c74338ffffffff mov dword ptr [rbx + 0x38], 0xffffffff
0x32200a 48c783a800000000000000 mov qword ptr [rbx + 0xa8], 0
0x322015 c5f857c0 vxorps xmm0, xmm0, xmm0
0x322019 c5f8114340 vmovups xmmword ptr [rbx + 0x40], xmm0
0x32201e c5f811435e vmovups xmmword ptr [rbx + 0x5e], xmm0
0x322023 66c7436e0000 mov word ptr [rbx + 0x6e], 0
0x322029 c5f8118390000000 vmovups xmmword ptr [rbx + 0x90], xmm0
0x322031 488903 mov qword ptr [rbx], rax
0x322034 c783b000000000000000 mov dword ptr [rbx + 0xb0], 0
0x32203e c683b400000000 mov byte ptr [rbx + 0xb4], 0
0x322045 48c783b800000000000000 mov qword ptr [rbx + 0xb8], 0
0x322050 4883c408 add rsp, 8
0x322054 5b pop rbx
0x322055 5d pop rbp
0x322056 c3 ret 
0x322057 cc int3 
0x322058 cc int3 
0x322059 cc int3 
0x32205a cc int3 
0x32205b cc int3 
0x32205c cc int3 
0x32205d cc int3 
0x32205e cc int3 
0x32205f cc int3 
0x322060 55 push rbp
0x322061 4889e5 mov rbp, rsp
0x322064 4156 push r14
0x322066 53 push rbx
0x322067 4883ec30 sub rsp, 0x30
0x32206b 4889fb mov rbx, rdi
0x32206e e82d490a00 call 0x3c69a0
0x322073 4989c6 mov r14, rax
0x322076 e875fcffff call 0x321cf0
0x32207b 4889c7 mov rdi, rax
0x32207e 4c89f6 mov rsi, r14
0x322081 b910050000 mov ecx, 0x510
0x322086 c5f857c0 vxorps xmm0, xmm0, xmm0
0x32208a c5f811442418 vmovups xmmword ptr [rsp + 0x18], xmm0
0x322090 c5f8110424 vmovups xmmword ptr [rsp], xmm0
0x322095 c744241000000000 mov dword ptr [rsp + 0x10], 0
0x32209d 31d2 xor edx, edx
0x32209f 4531c0 xor r8d, r8d
0x3220a2 4531c9 xor r9d, r9d
0x3220a5 e806ef1600 call 0x490fb0
0x3220aa 4889c7 mov rdi, rax
0x3220ad 4889de mov rsi, rbx
0x3220b0 4989c6 mov r14, rax
0x3220b3 e8c8000a00 call 0x3c2180
0x3220b8 488d05b155bb07 lea rax, [rip + 0x7bb55b1]
0x3220bf 49c7463000000000 mov qword ptr [r14 + 0x30], 0
0x3220c7 41c74638ffffffff mov dword ptr [r14 + 0x38], 0xffffffff
0x3220cf 49c786a800000000000000 mov qword ptr [r14 + 0xa8], 0
0x3220da c5f857c0 vxorps xmm0, xmm0, xmm0
0x3220de c4c178114640 vmovups xmmword ptr [r14 + 0x40], xmm0
0x3220e4 c4c17811465e vmovups xmmword ptr [r14 + 0x5e], xmm0
0x3220ea 6641c7466e0000 mov word ptr [r14 + 0x6e], 0
0x3220f1 c4c178118690000000 vmovups xmmword ptr [r14 + 0x90], xmm0
0x3220fa 498906 mov qword ptr [r14], rax
0x3220fd 4c89f0 mov rax, r14
0x322100 4883c430 add rsp, 0x30
0x322104 5b pop rbx
0x322105 415e pop r14
0x322107 5d pop rbp
0x322108 c3 ret 
0x322109 cc int3 
0x32210a cc int3 
0x32210b cc int3 
0x32210c cc int3 
0x32210d cc int3 
0x32210e cc int3 
0x32210f cc int3 
0x322110 55 push rbp
0x322111 4889e5 mov rbp, rsp
0x322114 53 push rbx
0x322115 50 push rax
0x322116 4889fb mov rbx, rdi
0x322119 e892f91600 call 0x491ab0
0x32211e 8b4308 mov eax, dword ptr [rbx + 8]
0x322121 a810 test al, 0x10
0x322123 740a je 0x32212f
0x322125 4883bbc000000000 cmp qword ptr [rbx + 0xc0], 0
0x32212d 7407 je 0x322136
0x32212f 4883c408 add rsp, 8
0x322133 5b pop rbx
0x322134 5d pop rbp
0x322135 c3 ret 
0x322136 488d05b37bffff lea rax, [rip - 0x844d]
0x32213d 488983c0000000 mov qword ptr [rbx + 0xc0], rax
0x322144 4883c408 add rsp, 8
0x322148 5b pop rbx
0x322149 5d pop rbp
0x32214a c3 ret 
0x32214b cc int3 
0x32214c cc int3 
0x32214d cc int3 
0x32214e cc int3 
0x32214f cc int3 
0x322150 55 push rbp
0x322151 4889e5 mov rbp, rsp
0x322154 4156 push r14
0x322156 53 push rbx
0x322157 4889fb mov rbx, rdi
0x32215a 488bbf10010000 mov rdi, qword ptr [rdi + 0x110]
0x322161 4885ff test rdi, rdi
0x322164 751e jne 0x322184
0x322166 488b0b mov rcx, qword ptr [rbx]
0x322169 4889df mov rdi, rbx
0x32216c 4989f6 mov r14, rsi
0x32216f ff9100040000 call qword ptr [rcx + 0x400]
0x322175 488bbb10010000 mov rdi, qword ptr [rbx + 0x110]
0x32217c 4c89f6 mov rsi, r14
0x32217f 4885ff test rdi, rdi
0x322182 740a je 0x32218e
0x322184 e807420a00 call 0x3c6390
0x322189 5b pop rbx
0x32218a 415e pop r14
0x32218c 5d pop rbp
0x32218d c3 ret 
0x32218e 31c0 xor eax, eax
0x322190 5b pop rbx
0x322191 415e pop r14
0x322193 5d pop rbp
0x322194 c3 ret 
0x322195 cc int3 
0x322196 cc int3 
0x322197 cc int3 
0x322198 cc int3 
0x322199 cc int3 
0x32219a cc int3 
0x32219b cc int3 
0x32219c cc int3 
0x32219d cc int3 
0x32219e cc int3 
0x32219f cc int3 
0x3221a0 55 push rbp
0x3221a1 4889e5 mov rbp, rsp
0x3221a4 4157 push r15
0x3221a6 4156 push r14
0x3221a8 4154 push r12
0x3221aa 53 push rbx
0x3221ab 4883ec10 sub rsp, 0x10
0x3221af 4889fb mov rbx, rdi
0x3221b2 bf58000000 mov edi, 0x58
0x3221b7 e8c44a5704 call 0x4896c80
0x3221bc 488d0dbd81bb07 lea rcx, [rip + 0x7bb81bd]
0x3221c3 488908 mov qword ptr [rax], rcx
0x3221c6 48895808 mov qword ptr [rax + 8], rbx
0x3221ca 488b8be0000000 mov rcx, qword ptr [rbx + 0xe0]
0x3221d1 48894810 mov qword ptr [rax + 0x10], rcx
0x3221d5 488b8b10010000 mov rcx, qword ptr [rbx + 0x110]
0x3221dc 4885c9 test rcx, rcx
0x3221df 740c je 0x3221ed
0x3221e1 31d2 xor edx, edx
0x3221e3 837b5800 cmp dword ptr [rbx + 0x58], 0
0x3221e7 480f44ca cmove rcx, rdx
0x3221eb eb02 jmp 0x3221ef
0x3221ed 31c9 xor ecx, ecx
0x3221ef 48894818 mov qword ptr [rax + 0x18], rcx
0x3221f3 8b8bd4000000 mov ecx, dword ptr [rbx + 0xd4]
0x3221f9 83e101 and ecx, 1
0x3221fc 894820 mov dword ptr [rax + 0x20], ecx
0x3221ff 488b8bd8000000 mov rcx, qword ptr [rbx + 0xd8]
0x322206 48894828 mov qword ptr [rax + 0x28], rcx
0x32220a c5f81083b0000000 vmovups xmm0, xmmword ptr [rbx + 0xb0]
0x322212 c5f8114030 vmovups xmmword ptr [rax + 0x30], xmm0
0x322217 488b8bc0000000 mov rcx, qword ptr [rbx + 0xc0]
0x32221e 48894840 mov qword ptr [rax + 0x40], rcx
0x322222 48c7404800000000 mov qword ptr [rax + 0x48], 0
0x32222a 488d4854 lea rcx, [rax + 0x54]
0x32222e 4c8bb3f0010000 mov r14, qword ptr [rbx + 0x1f0]
0x322235 48639bf8010000 movsxd rbx, dword ptr [rbx + 0x1f8]
0x32223c 895850 mov dword ptr [rax + 0x50], ebx
0x32223f 4885db test rbx, rbx
0x322242 743e je 0x322282
0x322244 4989c7 mov r15, rax
0x322247 bf10000000 mov edi, 0x10
0x32224c be08000000 mov esi, 8
0x322251 89da mov edx, ebx
0x322253 4189d9 mov r9d, ebx
0x322256 48890c24 mov qword ptr [rsp], rcx
0x32225a 31c9 xor ecx, ecx
0x32225c 4989c4 mov r12, rax
0x32225f 4983c748 add r15, 0x48
0x322263 4d89f8 mov r8, r15
0x322266 e86553d0ff call 0x275d0
0x32226b 498b3f mov rdi, qword ptr [r15]
0x32226e 48c1e304 shl rbx, 4
0x322272 4c89f6 mov rsi, r14
0x322275 4889da mov rdx, rbx
0x322278 e8d3fb1407 call 0x7471e50
0x32227d 4c89e0 mov rax, r12
0x322280 eb06 jmp 0x322288
0x322282 c70100000000 mov dword ptr [rcx], 0
0x322288 4883c410 add rsp, 0x10
0x32228c 5b pop rbx
0x32228d 415c pop r12
0x32228f 415e pop r14
0x322291 415f pop r15
0x322293 5d pop rbp
0x322294 c3 ret 
0x322295 cc int3 
0x322296 cc int3 
0x322297 cc int3 
0x322298 cc int3 
0x322299 cc int3 
0x32229a cc int3 
0x32229b cc int3 
0x32229c cc int3 
0x32229d cc int3 
0x32229e cc int3 
0x32229f cc int3 
0x3222a0 55 push rbp
0x3222a1 4889e5 mov rbp, rsp
0x3222a4 4157 push r15
0x3222a6 4156 push r14
0x3222a8 4155 push r13
0x3222aa 4154 push r12
0x3222ac 53 push rbx
0x3222ad 4881ec88100000 sub rsp, 0x1088
0x3222b4 4c8b2d4dfce008 mov r13, qword ptr [rip + 0x8e0fc4d]
0x3222bb 4889fb mov rbx, rdi
0x3222be 498b4500 mov rax, qword ptr [r13]
0x3222c2 488945d0 mov qword ptr [rbp - 0x30], rax
0x3222c6 4883bf1001000000 cmp qword ptr [rdi + 0x110], 0
0x3222ce 0f8551030000 jne 0x322625
0x3222d4 4c8b7b40 mov r15, qword ptr [rbx + 0x40]
0x3222d8 4d85ff test r15, r15
0x3222db 7431 je 0x32230e
0x3222dd 4c89ff mov rdi, r15
0x3222e0 be01000000 mov esi, 1
0x3222e5 e8063b1600 call 0x485df0
0x3222ea 4d8bb710010000 mov r14, qword ptr [r15 + 0x110]
0x3222f1 4d85f6 test r14, r14
0x3222f4 7529 jne 0x32231f
0x3222f6 498b07 mov rax, qword ptr [r15]
0x3222f9 4c89ff mov rdi, r15
0x3222fc ff9000040000 call qword ptr [rax + 0x400]
0x322302 4d8bb710010000 mov r14, qword ptr [r15 + 0x110]
0x322309 4d85f6 test r14, r14
0x32230c 7511 jne 0x32231f
0x32230e e89d380a00 call 0x3c5bb0
0x322313 4839d8 cmp rax, rbx
0x322316 0f8509030000 jne 0x322625
0x32231c 4531f6 xor r14d, r14d
0x32231f f683d600000004 test byte ptr [rbx + 0xd6], 4
0x322326 7470 je 0x322398
0x322328 48837b7000 cmp qword ptr [rbx + 0x70], 0
0x32232d 7569 jne 0x322398
0x32232f 488d3dd9660309 lea rdi, [rip + 0x90366d9]
0x322336 e8a5b4e8ff call 0x1ad7e0
0x32233b 84c0 test al, al
0x32233d 7559 jne 0x322398
0x32233f 4889df mov rdi, rbx
0x322342 e8b9291900 call 0x4b4d00
0x322347 4885c0 test rax, rax
0x32234a 744c je 0x322398
0x32234c 488d0d1d670309 lea rcx, [rip + 0x903671d]
0x322353 4989c7 mov r15, rax
0x322356 803900 cmp byte ptr [rcx], 0
0x322359 0f84e9020000 je 0x322648
0x32235f 4c8dbda0f7ffff lea r15, [rbp - 0x860]
0x322366 4c89ff mov rdi, r15
0x322369 e8928aefff call 0x21ae00
0x32236e 488d055b33bb07 lea rax, [rip + 0x7bb335b]
0x322375 4889df mov rdi, rbx
0x322378 4c89fe mov rsi, r15
0x32237b ba01000000 mov edx, 1
0x322380 488985a0f7ffff mov qword ptr [rbp - 0x860], rax
0x322387 488b03 mov rax, qword ptr [rbx]
0x32238a ff90d0020000 call qword ptr [rax + 0x2d0]
0x322390 4c89ff mov rdi, r15
0x322393 e87891efff call 0x21b510
0x322398 4883bb1001000000 cmp qword ptr [rbx + 0x110], 0
0x3223a0 0f857f020000 jne 0x322625
0x3223a6 488b7320 mov rsi, qword ptr [rbx + 0x20]
0x3223aa 4889df mov rdi, rbx
0x3223ad b931000000 mov ecx, 0x31
0x3223b2 c5f857c0 vxorps xmm0, xmm0, xmm0
0x3223b6 c5f811442418 vmovups xmmword ptr [rsp + 0x18], xmm0
0x3223bc c5f8110424 vmovups xmmword ptr [rsp], xmm0
0x3223c1 c744241000000000 mov dword ptr [rsp + 0x10], 0
0x3223c9 31d2 xor edx, edx
0x3223cb 4531c0 xor r8d, r8d
0x3223ce 4531c9 xor r9d, r9d
0x3223d1 e8daeb1600 call 0x490fb0
0x3223d6 4989c7 mov r15, rax
0x3223d9 4885c0 test rax, rax
0x3223dc 7414 je 0x3223f2
0x3223de 488d0577ca0709 lea rax, [rip + 0x907ca77]
0x3223e5 803800 cmp byte ptr [rax], 0
0x3223e8 7408 je 0x3223f2
0x3223ea 4c89ff mov rdi, r15
0x3223ed e8ce5e0400 call 0x3682c0
0x3223f2 4c89bb10010000 mov qword ptr [rbx + 0x110], r15
0x3223f9 4885db test rbx, rbx
0x3223fc 0f84b1000000 je 0x3224b3
0x322402 4c8b6350 mov r12, qword ptr [rbx + 0x50]
0x322406 4d85e4 test r12, r12
0x322409 0f84a4000000 je 0x3224b3
0x32240f 49bf0180000200000800 movabs r15, 0x8000002008001
0x322419 eb13 jmp 0x32242e
0x32241b 0f1f440000 nop dword ptr [rax + rax]
0x322420 4d8b642418 mov r12, qword ptr [r12 + 0x18]
0x322425 4d85e4 test r12, r12
0x322428 0f8485000000 je 0x3224b3
0x32242e 498b442408 mov rax, qword ptr [r12 + 8]
0x322433 488b4010 mov rax, qword ptr [rax + 0x10]
0x322437 c4c2f8f2c7 andn rax, rax, r15
0x32243c 75e2 jne 0x322420
0x32243e 41f644243b20 test byte ptr [r12 + 0x3b], 0x20
0x322444 75da jne 0x322420
0x322446 4885db test rbx, rbx
0x322449 7441 je 0x32248c
0x32244b 498b442470 mov rax, qword ptr [r12 + 0x70]
0x322450 488bbb10010000 mov rdi, qword ptr [rbx + 0x110]
0x322457 4963542444 movsxd rdx, dword ptr [r12 + 0x44]
0x32245c 488bb0e8000000 mov rsi, qword ptr [rax + 0xe8]
0x322463 e8882a1400 call 0x464ef0
0x322468 4d8b642418 mov r12, qword ptr [r12 + 0x18]
0x32246d 4d85e4 test r12, r12
0x322470 7441 je 0x3224b3
0x322472 498b442408 mov rax, qword ptr [r12 + 8]
0x322477 488b4010 mov rax, qword ptr [rax + 0x10]
0x32247b c4c2f8f2c7 andn rax, rax, r15
0x322480 75e6 jne 0x322468
0x322482 41f644243b20 test byte ptr [r12 + 0x3b], 0x20
0x322488 74c1 je 0x32244b
0x32248a ebdc jmp 0x322468
0x32248c 498b442470 mov rax, qword ptr [r12 + 0x70]
0x322491 488bbb10010000 mov rdi, qword ptr [rbx + 0x110]
0x322498 4963542444 movsxd rdx, dword ptr [r12 + 0x44]
0x32249d 488bb0e8000000 mov rsi, qword ptr [rax + 0xe8]
0x3224a4 e8472a1400 call 0x464ef0
0x3224a9 4d8b642418 mov r12, qword ptr [r12 + 0x18]
0x3224ae 4d85e4 test r12, r12
0x3224b1 75d9 jne 0x32248c
0x3224b3 31c9 xor ecx, ecx
0x3224b5 f783d400000080000010 test dword ptr [rbx + 0xd4], 0x10000080
0x3224bf 488bb310010000 mov rsi, qword ptr [rbx + 0x110]
0x3224c6 4c8ba3b0000000 mov r12, qword ptr [rbx + 0xb0]
0x3224cd 4c8dbda0f7ffff lea r15, [rbp - 0x860]
0x3224d4 4c89f2 mov rdx, r14
0x3224d7 4c89ff mov rdi, r15
0x3224da 0f94c1 sete cl
0x3224dd 4531c0 xor r8d, r8d
0x3224e0 01c9 add ecx, ecx
0x3224e2 e839fa1600 call 0x491f20
0x3224e7 4c89ff mov rdi, r15
0x3224ea 41ffd4 call r12
0x3224ed 4c89ff mov rdi, r15
0x3224f0 e8ebfa1600 call 0x491fe0
0x3224f5 4889df mov rdi, rbx
0x3224f8 e8533c1600 call 0x486150
0x3224fd f6402c10 test byte ptr [rax + 0x2c], 0x10
0x322501 0f8411010000 je 0x322618
0x322507 4889df mov rdi, rbx
0x32250a e8413c1600 call 0x486150
0x32250f f6402f20 test byte ptr [rax + 0x2f], 0x20
0x322513 0f85ff000000 jne 0x322618
0x322519 4889df mov rdi, rbx
0x32251c e82f3c1600 call 0x486150
0x322521 488b4018 mov rax, qword ptr [rax + 0x18]
0x322525 4c8db580efffff lea r14, [rbp - 0x1080]
0x32252c 488dbda0f7ffff lea rdi, [rbp - 0x860]
0x322533 ba0b040000 mov edx, 0x40b
0x322538 4c89f6 mov rsi, r14
0x32253b 488985a0f7ffff mov qword ptr [rbp - 0x860], rax
0x322542 e8b9d4f1ff call 0x23fa00
0x322547 488dbdbaf7ffff lea rdi, [rbp - 0x846]
0x32254e 488d45d0 lea rax, [rbp - 0x30]
0x322552 488d3599565007 lea rsi, [rip + 0x7505699]
0x322559 ba12000000 mov edx, 0x12
0x32255e c685b8f7ffff00 mov byte ptr [rbp - 0x848], 0
0x322565 4889bda0f7ffff mov qword ptr [rbp - 0x860], rdi
0x32256c 4889bda8f7ffff mov qword ptr [rbp - 0x858], rdi
0x322573 488985b0f7ffff mov qword ptr [rbp - 0x850], rax
0x32257a e8b1aad6ff call 0x8d030
0x32257f 4883c012 add rax, 0x12
0x322583 488dbd78efffff lea rdi, [rbp - 0x1088]
0x32258a 4c89fe mov rsi, r15
0x32258d 488985a8f7ffff mov qword ptr [rbp - 0x858], rax
0x322594 488b4318 mov rax, qword ptr [rbx + 0x18]
0x322598 48898578efffff mov qword ptr [rbp - 0x1088], rax
0x32259f e89cd0f1ff call 0x23f640
0x3225a4 488b85a8f7ffff mov rax, qword ptr [rbp - 0x858]
0x3225ab 488dbd78efffff lea rdi, [rbp - 0x1088]
0x3225b2 ba01000000 mov edx, 1
0x3225b7 66c7000000 mov word ptr [rax], 0
0x3225bc 488bb5a0f7ffff mov rsi, qword ptr [rbp - 0x860]
0x3225c3 e8c8aff1ff call 0x23d590
0x3225c8 4c89ff mov rdi, r15
0x3225cb e8101beeff call 0x2040e0
0x3225d0 488b8578efffff mov rax, qword ptr [rbp - 0x1088]
0x3225d7 488dbd78efffff lea rdi, [rbp - 0x1088]
0x3225de 4c89fe mov rsi, r15
0x3225e1 ba0b040000 mov edx, 0x40b
0x3225e6 48898578efffff mov qword ptr [rbp - 0x1088], rax
0x3225ed e80ed4f1ff call 0x23fa00
0x3225f2 488b8310010000 mov rax, qword ptr [rbx + 0x110]
0x3225f9 4c89f7 mov rdi, r14
0x3225fc 4c89fe mov rsi, r15
0x3225ff ba01000000 mov edx, 1
0x322604 b902000000 mov ecx, 2
0x322609 4531c0 xor r8d, r8d
0x32260c 4531c9 xor r9d, r9d
0x32260f 48890424 mov qword ptr [rsp], rax
0x322613 e8088ffdff call 0x2fb520
0x322618 488bbb10010000 mov rdi, qword ptr [rbx + 0x110]
0x32261f 488b07 mov rax, qword ptr [rdi]
0x322622 ff5068 call qword ptr [rax + 0x68]
0x322625 488b8310010000 mov rax, qword ptr [rbx + 0x110]
0x32262c 498b4d00 mov rcx, qword ptr [r13]
0x322630 483b4dd0 cmp rcx, qword ptr [rbp - 0x30]
0x322634 754a jne 0x322680
0x322636 4881c488100000 add rsp, 0x1088
0x32263d 5b pop rbx
0x32263e 415c pop r12
0x322640 415d pop r13
0x322642 415e pop r14
0x322644 415f pop r15
0x322646 5d pop rbp
0x322647 c3 ret 
0x322648 4c8b6348 mov r12, qword ptr [rbx + 0x48]
0x32264c eb05 jmp 0x322653
0x32264e 4d8b642428 mov r12, qword ptr [r12 + 0x28]
0x322653 4d85e4 test r12, r12
0x322656 0f8403fdffff je 0x32235f
0x32265c 49395c2420 cmp qword ptr [r12 + 0x20], rbx
0x322661 0f85f8fcffff jne 0x32235f
0x322667 418b442408 mov eax, dword ptr [r12 + 8]
0x32266c a900040000 test eax, 0x400
0x322671 74db je 0x32264e
0x322673 4c89ff mov rdi, r15
0x322676 4c89e6 mov rsi, r12
0x322679 e802e30800 call 0x3b0980
0x32267e ebce jmp 0x32264e
0x322680 e8dbf71407 call 0x7471e60
0x322685 0f0b ud2 
0x322687 cc int3 
0x322688 cc int3 
0x322689 cc int3 
0x32268a cc int3 
0x32268b cc int3 
0x32268c cc int3 
0x32268d cc int3 
0x32268e cc int3 
0x32268f cc int3 
0x322690 55 push rbp
0x322691 4889e5 mov rbp, rsp
0x322694 4156 push r14
0x322696 53 push rbx
0x322697 4881ec50080000 sub rsp, 0x850
0x32269e 4c8b3563f8e008 mov r14, qword ptr [rip + 0x8e0f863]
0x3226a5 4889fb mov rbx, rdi
0x3226a8 488dbdd2f7ffff lea rdi, [rbp - 0x82e]
0x3226af 488d4de8 lea rcx, [rbp - 0x18]
0x3226b3 488d3538555007 lea rsi, [rip + 0x7505538]
0x3226ba ba12000000 mov edx, 0x12
0x3226bf 498b06 mov rax, qword ptr [r14]
0x3226c2 488945e8 mov qword ptr [rbp - 0x18], rax
0x3226c6 c685d0f7ffff00 mov byte ptr [rbp - 0x830], 0
0x3226cd 4889bdb8f7ffff mov qword ptr [rbp - 0x848], rdi
0x3226d4 4889bdc0f7ffff mov qword ptr [rbp - 0x840], rdi
0x3226db 48898dc8f7ffff mov qword ptr [rbp - 0x838], rcx
0x3226e2 e849a9d6ff call 0x8d030
0x3226e7 4883c012 add rax, 0x12
0x3226eb 488dbda8f7ffff lea rdi, [rbp - 0x858]
0x3226f2 488985c0f7ffff mov qword ptr [rbp - 0x840], rax
0x3226f9 488b4318 mov rax, qword ptr [rbx + 0x18]
0x3226fd 488d9db8f7ffff lea rbx, [rbp - 0x848]
0x322704 4889de mov rsi, rbx
0x322707 488985a8f7ffff mov qword ptr [rbp - 0x858], rax
0x32270e e82dcff1ff call 0x23f640
0x322713 488b85c0f7ffff mov rax, qword ptr [rbp - 0x840]
0x32271a 488dbdb0f7ffff lea rdi, [rbp - 0x850]
0x322721 ba01000000 mov edx, 1
0x322726 66c7000000 mov word ptr [rax], 0
0x32272b 488bb5b8f7ffff mov rsi, qword ptr [rbp - 0x848]
0x322732 e859aef1ff call 0x23d590
0x322737 4889df mov rdi, rbx
0x32273a e8a119eeff call 0x2040e0
0x32273f 488b85b0f7ffff mov rax, qword ptr [rbp - 0x850]
0x322746 498b0e mov rcx, qword ptr [r14]
0x322749 483b4de8 cmp rcx, qword ptr [rbp - 0x18]
0x32274d 750c jne 0x32275b
0x32274f 4881c450080000 add rsp, 0x850
0x322756 5b pop rbx
0x322757 415e pop r14
0x322759 5d pop rbp
0x32275a c3 ret 
0x32275b e800f71407 call 0x7471e60
0x322760 0f0b ud2 
0x322762 cc int3 
0x322763 cc int3 
0x322764 cc int3 
0x322765 cc int3 
0x322766 cc int3 
0x322767 cc int3 
0x322768 cc int3 
0x322769 cc int3 
0x32276a cc int3 
0x32276b cc int3 
0x32276c cc int3 
0x32276d cc int3 
0x32276e cc int3 
0x32276f cc int3 
0x322770 0fb605d1bd0709 movzx eax, byte ptr [rip + 0x907bdd1]
0x322777 84c0 test al, al
0x322779 7408 je 0x322783
0x32277b 488d05bebc0709 lea rax, [rip + 0x907bcbe]
0x322782 c3 ret 
0x322783 55 push rbp
0x322784 4889e5 mov rbp, rsp
0x322787 53 push rbx
0x322788 50 push rax
0x322789 488d3db8bd0709 lea rdi, [rip + 0x907bdb8]
0x322790 e81bf71407 call 0x7471eb0
0x322795 85c0 test eax, eax
0x322797 743f je 0x3227d8
0x322799 488d1da0bc0709 lea rbx, [rip + 0x907bca0]
0x3227a0 4889df mov rdi, rbx
0x3227a3 e8d81debff call 0x1d4580
0x3227a8 488d05f97bbb07 lea rax, [rip + 0x7bb7bf9]
0x3227af 488d3d3a000000 lea rdi, [rip + 0x3a]
0x3227b6 488d154398fb08 lea rdx, [rip + 0x8fb9843]
0x3227bd 4889de mov rsi, rbx
0x3227c0 48890579bc0709 mov qword ptr [rip + 0x907bc79], rax
0x3227c7 e804f71407 call 0x7471ed0
0x3227cc 488d3d75bd0709 lea rdi, [rip + 0x907bd75]
0x3227d3 e8e8f61407 call 0x7471ec0
0x3227d8 4883c408 add rsp, 8
0x3227dc 5b pop rbx
0x3227dd 5d pop rbp
0x3227de 488d055bbc0709 lea rax, [rip + 0x907bc5b]
0x3227e5 c3 ret 
0x3227e6 cc int3 
0x3227e7 cc int3 
0x3227e8 cc int3 
0x3227e9 cc int3 
0x3227ea cc int3 
0x3227eb cc int3 
0x3227ec cc int3 
0x3227ed cc int3 
0x3227ee cc int3 
0x3227ef cc int3 
0x3227f0 55 push rbp
0x3227f1 4889e5 mov rbp, rsp
0x3227f4 e8a71eebff call 0x1d46a0
0x3227f9 5d pop rbp
0x3227fa c3 ret 
0x3227fb cc int3 
0x3227fc cc int3 
0x3227fd cc int3 
0x3227fe cc int3 
0x3227ff cc int3 
0x322800 55 push rbp
0x322801 4889e5 mov rbp, rsp
0x322804 53 push rbx
0x322805 50 push rax
0x322806 4889fb mov rbx, rdi
0x322809 e8222d1600 call 0x485530
0x32280e 488b4340 mov rax, qword ptr [rbx + 0x40]
0x322812 4885c0 test rax, rax
0x322815 741f je 0x322836
0x322817 b95e87a34a mov ecx, 0x4aa3875e
0x32281c 2388d4000000 and ecx, dword ptr [rax + 0xd4]
0x322822 098bd4000000 or dword ptr [rbx + 0xd4], ecx
0x322828 488b80d8000000 mov rax, qword ptr [rax + 0xd8]
0x32282f 480983d8000000 or qword ptr [rbx + 0xd8], rax
0x322836 4883c408 add rsp, 8
0x32283a 5b pop rbx
0x32283b 5d pop rbp
0x32283c c3 ret 
0x32283d cc int3 
0x32283e cc int3 
0x32283f cc int3 
0x322840 55 push rbp
0x322841 4889e5 mov rbp, rsp
0x322844 4157 push r15
0x322846 4156 push r14
0x322848 4155 push r13
0x32284a 4154 push r12
0x32284c 53 push rbx
0x32284d 4881ec48080000 sub rsp, 0x848
0x322854 4c8b2dadf6e008 mov r13, qword ptr [rip + 0x8e0f6ad]
0x32285b 89cb mov ebx, ecx
0x32285d 4989d6 mov r14, rdx
0x322860 4989fc mov r12, rdi
0x322863 498b4500 mov rax, qword ptr [r13]
0x322867 488945d0 mov qword ptr [rbp - 0x30], rax
0x32286b e8d03c0a00 call 0x3c6540
0x322870 84c0 test al, al
0x322872 0f840b010000 je 0x322983
0x322878 4d8bbc2410010000 mov r15, qword ptr [r12 + 0x110]
0x322880 4d85ff test r15, r15
0x322883 0f84fa000000 je 0x322983
0x322889 888597f7ffff mov byte ptr [rbp - 0x869], al
0x32288f 488dbdbaf7ffff lea rdi, [rbp - 0x846]
0x322896 488d45d0 lea rax, [rbp - 0x30]
0x32289a 488d3551535007 lea rsi, [rip + 0x7505351]
0x3228a1 ba12000000 mov edx, 0x12
0x3228a6 c685b8f7ffff00 mov byte ptr [rbp - 0x848], 0
0x3228ad 4889bda0f7ffff mov qword ptr [rbp - 0x860], rdi
0x3228b4 4889bda8f7ffff mov qword ptr [rbp - 0x858], rdi
0x3228bb 488985b0f7ffff mov qword ptr [rbp - 0x850], rax
0x3228c2 e869a7d6ff call 0x8d030
0x3228c7 4883c012 add rax, 0x12
0x3228cb 488dbd98f7ffff lea rdi, [rbp - 0x868]
0x3228d2 488985a8f7ffff mov qword ptr [rbp - 0x858], rax
0x3228d9 498b442418 mov rax, qword ptr [r12 + 0x18]
0x3228de 4c8da5a0f7ffff lea r12, [rbp - 0x860]
0x3228e5 4c89e6 mov rsi, r12
0x3228e8 48898598f7ffff mov qword ptr [rbp - 0x868], rax
0x3228ef e84ccdf1ff call 0x23f640
0x3228f4 488b85a8f7ffff mov rax, qword ptr [rbp - 0x858]
0x3228fb 488dbd98f7ffff lea rdi, [rbp - 0x868]
0x322902 ba01000000 mov edx, 1
0x322907 66c7000000 mov word ptr [rax], 0
0x32290c 488bb5a0f7ffff mov rsi, qword ptr [rbp - 0x860]
0x322913 e878acf1ff call 0x23d590
0x322918 4c89e7 mov rdi, r12
0x32291b e8c017eeff call 0x2040e0
0x322920 488b8598f7ffff mov rax, qword ptr [rbp - 0x868]
0x322927 488dbda0f7ffff lea rdi, [rbp - 0x860]
0x32292e 488db598f7ffff lea rsi, [rbp - 0x868]
0x322935 48898598f7ffff mov qword ptr [rbp - 0x868], rax
0x32293c e87fc7f1ff call 0x23f0c0
0x322941 83bda8f7ffff00 cmp dword ptr [rbp - 0x858], 0
0x322948 7409 je 0x322953
0x32294a 488bb5a0f7ffff mov rsi, qword ptr [rbp - 0x860]
0x322951 eb07 jmp 0x32295a
0x322953 488d3546cc3007 lea rsi, [rip + 0x730cc46]
0x32295a 498b07 mov rax, qword ptr [r15]
0x32295d 4c89ff mov rdi, r15
0x322960 4c89f2 mov rdx, r14
0x322963 89d9 mov ecx, ebx
0x322965 ff9078010000 call qword ptr [rax + 0x178]
0x32296b 488bbda0f7ffff mov rdi, qword ptr [rbp - 0x860]
0x322972 4885ff test rdi, rdi
0x322975 7405 je 0x32297c
0x322977 e81451d9ff call 0xb7a90
0x32297c 0fb68597f7ffff movzx eax, byte ptr [rbp - 0x869]
0x322983 498b4d00 mov rcx, qword ptr [r13]
0x322987 483b4dd0 cmp rcx, qword ptr [rbp - 0x30]
0x32298b 7512 jne 0x32299f
0x32298d 4881c448080000 add rsp, 0x848
0x322994 5b pop rbx
0x322995 415c pop r12
0x322997 415d pop r13
0x322999 415e pop r14
0x32299b 415f pop r15
0x32299d 5d pop rbp
0x32299e c3 ret 
0x32299f e8bcf41407 call 0x7471e60
0x3229a4 0f0b ud2 
0x3229a6 cc int3 
0x3229a7 cc int3 
0x3229a8 cc int3 
0x3229a9 cc int3 
0x3229aa cc int3 
0x3229ab cc int3 
0x3229ac cc int3 
0x3229ad cc int3 
0x3229ae cc int3 
0x3229af cc int3 
0x3229b0 55 push rbp
0x3229b1 4889e5 mov rbp, rsp
0x3229b4 4156 push r14
0x3229b6 53 push rbx
0x3229b7 89f3 mov ebx, esi
0x3229b9 4989fe mov r14, rdi
0x3229bc e83fe5ffff call 0x320f00
0x3229c1 498bbe10010000 mov rdi, qword ptr [r14 + 0x110]
0x3229c8 4885ff test rdi, rdi
0x3229cb 745a je 0x322a27
0x3229cd 8b4708 mov eax, dword ptr [rdi + 8]
0x3229d0 8b470c mov eax, dword ptr [rdi + 0xc]
0x3229d3 488d0d36770909 lea rcx, [rip + 0x9097736]
0x3229da 394124 cmp dword ptr [rcx + 0x24], eax
0x3229dd 7e23 jle 0x322a02
0x3229df 488b4910 mov rcx, qword ptr [rcx + 0x10]
0x3229e3 89c2 mov edx, eax
0x3229e5 0fb7c0 movzx eax, ax
0x3229e8 488d0440 lea rax, [rax + rax*2]
0x3229ec c1ea10 shr edx, 0x10
0x3229ef c1e003 shl eax, 3
0x3229f2 480304d1 add rax, qword ptr [rcx + rdx*8]
0x3229f6 8b4008 mov eax, dword ptr [rax + 8]
0x3229f9 a900000040 test eax, 0x40000000
0x3229fe 740e je 0x322a0e
0x322a00 eb25 jmp 0x322a27
0x322a02 31c0 xor eax, eax
0x322a04 8b4008 mov eax, dword ptr [rax + 8]
0x322a07 a900000040 test eax, 0x40000000
0x322a0c 7519 jne 0x322a27
0x322a0e 89de mov esi, ebx
0x322a10 e8fb34faff call 0x2c5f10
0x322a15 498bbe10010000 mov rdi, qword ptr [r14 + 0x110]
0x322a1c 89de mov esi, ebx
0x322a1e 488b07 mov rax, qword ptr [rdi]
0x322a21 ff90e8010000 call qword ptr [rax + 0x1e8]
0x322a27 5b pop rbx
0x322a28 415e pop r14
0x322a2a 5d pop rbp
0x322a2b c3 ret 
0x322a2c cc int3 
0x322a2d cc int3 
0x322a2e cc int3 
0x322a2f cc int3 
0x322a30 55 push rbp
0x322a31 4889e5 mov rbp, rsp
0x322a34 4157 push r15
0x322a36 4156 push r14
0x322a38 53 push rbx
0x322a39 4883ec18 sub rsp, 0x18
0x322a3d 4c8b3dc4f4e008 mov r15, qword ptr [rip + 0x8e0f4c4]
0x322a44 4889fb mov rbx, rdi
0x322a47 498b07 mov rax, qword ptr [r15]
0x322a4a 488945e0 mov qword ptr [rbp - 0x20], rax
0x322a4e 488b87b0000000 mov rax, qword ptr [rdi + 0xb0]
0x322a55 4885c0 test rax, rax
0x322a58 7430 je 0x322a8a
0x322a5a 4c8b7340 mov r14, qword ptr [rbx + 0x40]
0x322a5e 4d85f6 test r14, r14
0x322a61 0f84c7000000 je 0x322b2e
0x322a67 4885c0 test rax, rax
0x322a6a 0f8484000000 je 0x322af4
0x322a70 4883bbc000000000 cmp qword ptr [rbx + 0xc0], 0
0x322a78 747a je 0x322af4
0x322a7a 4883bbb800000000 cmp qword ptr [rbx + 0xb8], 0
0x322a82 0f85ab000000 jne 0x322b33
0x322a88 eb6a jmp 0x322af4
0x322a8a 8b430c mov eax, dword ptr [rbx + 0xc]
0x322a8d 488d0d7c760909 lea rcx, [rip + 0x909767c]
0x322a94 394124 cmp dword ptr [rcx + 0x24], eax
0x322a97 7e42 jle 0x322adb
0x322a99 488b4910 mov rcx, qword ptr [rcx + 0x10]
0x322a9d 89c2 mov edx, eax
0x322a9f 0fb7c0 movzx eax, ax
0x322aa2 488d0440 lea rax, [rax + rax*2]
0x322aa6 c1ea10 shr edx, 0x10
0x322aa9 c1e003 shl eax, 3
0x322aac 480304d1 add rax, qword ptr [rcx + rdx*8]
0x322ab0 8b4008 mov eax, dword ptr [rax + 8]
0x322ab3 a900000002 test eax, 0x2000000
0x322ab8 742d je 0x322ae7
0x322aba 488d7dd0 lea rdi, [rbp - 0x30]
0x322abe 4889de mov rsi, rbx
0x322ac1 31d2 xor edx, edx
0x322ac3 e818421600 call 0x486ce0
0x322ac8 837dd800 cmp dword ptr [rbp - 0x28], 0
0x322acc 0f840b010000 je 0x322bdd
0x322ad2 488b4dd0 mov rcx, qword ptr [rbp - 0x30]
0x322ad6 e909010000 jmp 0x322be4
0x322adb 31c0 xor eax, eax
0x322add 8b4008 mov eax, dword ptr [rax + 8]
0x322ae0 a900000002 test eax, 0x2000000
0x322ae5 75d3 jne 0x322aba
0x322ae7 4c8b7340 mov r14, qword ptr [rbx + 0x40]
0x322aeb 4d85f6 test r14, r14
0x322aee 0f8497000000 je 0x322b8b
0x322af4 498b06 mov rax, qword ptr [r14]
0x322af7 4c89f7 mov rdi, r14
0x322afa ff90c0020000 call qword ptr [rax + 0x2c0]
0x322b00 488b83b0000000 mov rax, qword ptr [rbx + 0xb0]
0x322b07 4885c0 test rax, rax
0x322b0a 743f je 0x322b4b
0x322b0c 4883bbb800000000 cmp qword ptr [rbx + 0xb8], 0
0x322b14 744d je 0x322b63
0x322b16 4883bbc000000000 cmp qword ptr [rbx + 0xc0], 0
0x322b1e 745b je 0x322b7b
0x322b20 498b8ed8000000 mov rcx, qword ptr [r14 + 0xd8]
0x322b27 48098bd8000000 or qword ptr [rbx + 0xd8], rcx
0x322b2e 4885c0 test rax, rax
0x322b31 7458 je 0x322b8b
0x322b33 498b07 mov rax, qword ptr [r15]
0x322b36 483b45e0 cmp rax, qword ptr [rbp - 0x20]
0x322b3a 0f85e5000000 jne 0x322c25
0x322b40 4883c418 add rsp, 0x18
0x322b44 5b pop rbx
0x322b45 415e pop r14
0x322b47 415f pop r15
0x322b49 5d pop rbp
0x322b4a c3 ret 
0x322b4b 498b86b0000000 mov rax, qword ptr [r14 + 0xb0]
0x322b52 488983b0000000 mov qword ptr [rbx + 0xb0], rax
0x322b59 4883bbb800000000 cmp qword ptr [rbx + 0xb8], 0
0x322b61 75b3 jne 0x322b16
0x322b63 498b8eb8000000 mov rcx, qword ptr [r14 + 0xb8]
0x322b6a 48898bb8000000 mov qword ptr [rbx + 0xb8], rcx
0x322b71 4883bbc000000000 cmp qword ptr [rbx + 0xc0], 0
0x322b79 75a5 jne 0x322b20
0x322b7b 498b8ec0000000 mov rcx, qword ptr [r14 + 0xc0]
0x322b82 48898bc0000000 mov qword ptr [rbx + 0xc0], rcx
0x322b89 eb95 jmp 0x322b20
0x322b8b 488d7dd0 lea rdi, [rbp - 0x30]
0x322b8f 4889de mov rsi, rbx
0x322b92 31d2 xor edx, edx
0x322b94 e847411600 call 0x486ce0
0x322b99 837dd800 cmp dword ptr [rbp - 0x28], 0
0x322b9d 7406 je 0x322ba5
0x322b9f 488b4dd0 mov rcx, qword ptr [rbp - 0x30]
0x322ba3 eb07 jmp 0x322bac
0x322ba5 488d0df4c93007 lea rcx, [rip + 0x730c9f4]
0x322bac 488d3d7f8a8407 lea rdi, [rip + 0x7848a7f]
0x322bb3 488d1542734807 lea rdx, [rip + 0x7487342]
0x322bba be27140000 mov esi, 0x1427
0x322bbf 31c0 xor eax, eax
0x322bc1 e8ea24e5ff call 0x1750b0
0x322bc6 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x322bca 4885ff test rdi, rdi
0x322bcd 0f8460ffffff je 0x322b33
0x322bd3 e8b84ed9ff call 0xb7a90
0x322bd8 e956ffffff jmp 0x322b33
0x322bdd 488d0dbcc93007 lea rcx, [rip + 0x730c9bc]
0x322be4 488d3d478a8407 lea rdi, [rip + 0x7848a47]
0x322beb 488d152aa15a07 lea rdx, [rip + 0x75aa12a]
0x322bf2 be07140000 mov esi, 0x1407
0x322bf7 31c0 xor eax, eax
0x322bf9 e8b224e5ff call 0x1750b0
0x322bfe 488b7dd0 mov rdi, qword ptr [rbp - 0x30]
0x322c02 4885ff test rdi, rdi
0x322c05 7405 je 0x322c0c
0x322c07 e8844ed9ff call 0xb7a90
0x322c0c 488b83b0000000 mov rax, qword ptr [rbx + 0xb0]
0x322c13 4c8b7340 mov r14, qword ptr [rbx + 0x40]
0x322c17 4d85f6 test r14, r14
0x322c1a 0f8547feffff jne 0x322a67
0x322c20 e909ffffff jmp 0x322b2e
0x322c25 e836f21407 call 0x7471e60
0x322c2a 0f0b ud2 
0x322c2c cc int3 
0x322c2d cc int3 
0x322c2e cc int3 
0x322c2f cc int3 
0x322c30 55 push rbp
0x322c31 4889e5 mov rbp, rsp
0x322c34 4157 push r15
0x322c36 4156 push r14
0x322c38 53 push rbx
0x322c39 50 push rax
0x322c3a 448bb7d4000000 mov r14d, dword ptr [rdi + 0xd4]
0x322c41 4885ff test rdi, rdi
0x322c44 745e je 0x322ca4
0x322c46 4889fb mov rbx, rdi
0x322c49 0f1f8000000000 nop dword ptr [rax]
0x322c50 4c8b7b18 mov r15, qword ptr [rbx + 0x18]
0x322c54 bf66000000 mov edi, 0x66
0x322c59 e892d6f1ff call 0x2402f0
0x322c5e 89c0 mov eax, eax
0x322c60 4939c7 cmp r15, rax
0x322c63 0f94c1 sete cl
0x322c66 7409 je 0x322c71
0x322c68 488b5b40 mov rbx, qword ptr [rbx + 0x40]
0x322c6c 4885db test rbx, rbx
0x322c6f 75df jne 0x322c50
0x322c71 41f7c600000002 test r14d, 0x2000000
0x322c78 488d0595ad6c07 lea rax, [rip + 0x76cad95]
0x322c7f 488d15bef16907 lea rdx, [rip + 0x769f1be]
0x322c86 488d3551676707 lea rsi, [rip + 0x7676751]
0x322c8d 480f44d0 cmove rdx, rax
0x322c91 488d051c685507 lea rax, [rip + 0x755681c]
0x322c98 480f44c6 cmove rax, rsi
0x322c9c 84c9 test cl, cl
0x322c9e 480f45c2 cmovne rax, rdx
0x322ca2 eb19 jmp 0x322cbd
0x322ca4 41f7c600000002 test r14d, 0x2000000
0x322cab 488d0d2c676707 lea rcx, [rip + 0x767672c]
0x322cb2 488d05fb675507 lea rax, [rip + 0x75567fb]
0x322cb9 480f44c1 cmove rax, rcx
0x322cbd 4883c408 add rsp, 8
0x322cc1 5b pop rbx
0x322cc2 415e pop r14
0x322cc4 415f pop r15
0x322cc6 5d pop rbp
0x322cc7 c3 ret 
0x322cc8 cc int3 
0x322cc9 cc int3 
0x322cca cc int3 
0x322ccb cc int3 
0x322ccc cc int3 
0x322ccd cc int3 
0x322cce cc int3 
0x322ccf cc int3 
0x322cd0 55 push rbp
0x322cd1 4889e5 mov rbp, rsp
0x322cd4 4156 push r14
0x322cd6 53 push rbx
0x322cd7 4883ec10 sub rsp, 0x10
0x322cdb 4c8b3526f2e008 mov r14, qword ptr [rip + 0x8e0f226]
0x322ce2 4889fb mov rbx, rdi
0x322ce5 498b06 mov rax, qword ptr [r14]
0x322ce8 488945e8 mov qword ptr [rbp - 0x18], rax
0x322cec 488b4618 mov rax, qword ptr [rsi + 0x18]
0x322cf0 488d75e0 lea rsi, [rbp - 0x20]
0x322cf4 488945e0 mov qword ptr [rbp - 0x20], rax
0x322cf8 e8c3c3f1ff call 0x23f0c0
0x322cfd 498b06 mov rax, qword ptr [r14]
0x322d00 483b45e8 cmp rax, qword ptr [rbp - 0x18]
0x322d04 750c jne 0x322d12
0x322d06 4889d8 mov rax, rbx
0x322d09 4883c410 add rsp, 0x10
0x322d0d 5b pop rbx
0x322d0e 415e pop r14
0x322d10 5d pop rbp
0x322d11 c3 ret 
0x322d12 e849f11407 call 0x7471e60
0x322d17 0f0b ud2 
0x322d19 cc int3 
0x322d1a cc int3 
0x322d1b cc int3 
0x322d1c cc int3 
0x322d1d cc int3 
0x322d1e cc int3 
0x322d1f cc int3 
0x322d20 55 push rbp
0x322d21 4889e5 mov rbp, rsp
0x322d24 53 push rbx
0x322d25 50 push rax
0x322d26 c7870801000000000000 mov dword ptr [rdi + 0x108], 0
0x322d30 4889fb mov rbx, rdi
0x322d33 83bf0c01000000 cmp dword ptr [rdi + 0x10c], 0
0x322d3a 7422 je 0x322d5e
0x322d3c 4c8d8b0c010000 lea r9, [rbx + 0x10c]
0x322d43 488d8b00010000 lea rcx, [rbx + 0x100]
0x322d4a bf08000000 mov edi, 8
0x322d4f be08000000 mov esi, 8
0x322d54 31d2 xor edx, edx
0x322d56 4531c0 xor r8d, r8d
0x322d59 e8c230d0ff call 0x25e20
0x322d5e c783f800000000000000 mov dword ptr [rbx + 0xf8], 0
0x322d68 83bbfc00000000 cmp dword ptr [rbx + 0xfc], 0
0x322d6f 7422 je 0x322d93
0x322d71 4c8d8bfc000000 lea r9, [rbx + 0xfc]
0x322d78 488d8bf0000000 lea rcx, [rbx + 0xf0]
0x322d7f bf10000000 mov edi, 0x10
0x322d84 be08000000 mov esi, 8
0x322d89 31d2 xor edx, edx
0x322d8b 4531c0 xor r8d, r8d
0x322d8e e88d30d0ff call 0x25e20
0x322d93 48c7831001000000000000 mov qword ptr [rbx + 0x110], 0
0x322d9e 488bb318010000 mov rsi, qword ptr [rbx + 0x118]
0x322da5 4885f6 test rsi, rsi
0x322da8 742c je 0x322dd6
0x322daa 488bbb20010000 mov rdi, qword ptr [rbx + 0x120]
0x322db1 ba01000000 mov edx, 1
0x322db6 488b07 mov rax, qword ptr [rdi]
0x322db9 ff9008030000 call qword ptr [rax + 0x308]
0x322dbf 488bbb18010000 mov rdi, qword ptr [rbx + 0x118]
0x322dc6 e8c54cd9ff call 0xb7a90
0x322dcb 48c7831801000000000000 mov qword ptr [rbx + 0x118], 0
0x322dd6 48c7832001000000000000 mov qword ptr [rbx + 0x120], 0
0x322de1 488bbba8000000 mov rdi, qword ptr [rbx + 0xa8]
0x322de8 4885ff test rdi, rdi
0x322deb 7405 je 0x322df2
0x322ded e89e4cd9ff call 0xb7a90
0x322df2 31c0 xor eax, eax
0x322df4 488783a8000000 xchg qword ptr [rbx + 0xa8], rax
0x322dfb c7436800000000 mov dword ptr [rbx + 0x68], 0
0x322e02 837b6c00 cmp dword ptr [rbx + 0x6c], 0
0x322e06 741c je 0x322e24
0x322e08 4c8d4b6c lea r9, [rbx + 0x6c]
0x322e0c 488d4b60 lea rcx, [rbx + 0x60]
0x322e10 bf01000000 mov edi, 1
0x322e15 be01000000 mov esi, 1
0x322e1a 31d2 xor edx, edx
0x322e1c 4531c0 xor r8d, r8d
0x322e1f e8fc2fd0ff call 0x25e20
0x322e24 4889df mov rdi, rbx
0x322e27 e884620a00 call 0x3c90b0
0x322e2c 4883c408 add rsp, 8
0x322e30 5b pop rbx
0x322e31 5d pop rbp
0x322e32 c3 ret 
0x322e33 cc int3 
0x322e34 cc int3 
0x322e35 cc int3 
0x322e36 cc int3 
0x322e37 cc int3 
0x322e38 cc int3 
0x322e39 cc int3 
0x322e3a cc int3 
0x322e3b cc int3 
0x322e3c cc int3 
0x322e3d cc int3 
0x322e3e cc int3 
0x322e3f cc int3 
0x322e40 488bb718010000 mov rsi, qword ptr [rdi + 0x118]
0x322e47 4885f6 test rsi, rsi
0x322e4a 743e je 0x322e8a
0x322e4c 55 push rbp
0x322e4d 4889e5 mov rbp, rsp
0x322e50 53 push rbx
0x322e51 50 push rax
0x322e52 488b8720010000 mov rax, qword ptr [rdi + 0x120]
0x322e59 4889fb mov rbx, rdi
0x322e5c ba01000000 mov edx, 1
0x322e61 488b08 mov rcx, qword ptr [rax]
0x322e64 4889c7 mov rdi, rax
0x322e67 ff9108030000 call qword ptr [rcx + 0x308]
0x322e6d 488bbb18010000 mov rdi, qword ptr [rbx + 0x118]
0x322e74 e8174cd9ff call 0xb7a90
0x322e79 48c7831801000000000000 mov qword ptr [rbx + 0x118], 0
0x322e84 4883c408 add rsp, 8
0x322e88 5b pop rbx
0x322e89 5d pop rbp
0x322e8a c3 ret 
0x322e8b cc int3 
0x322e8c cc int3 
0x322e8d cc int3 
0x322e8e cc int3 
0x322e8f cc int3 
0x322e90 55 push rbp
0x322e91 4889e5 mov rbp, rsp
0x322e94 53 push rbx
0x322e95 50 push rax
0x322e96 4889fb mov rbx, rdi
0x322e99 e812a9ffff call 0x31d7b0
0x322e9e 488b7b40 mov rdi, qword ptr [rbx + 0x40]
0x322ea2 4885ff test rdi, rdi
0x322ea5 7405 je 0x322eac
0x322ea7 e864640a00 call 0x3c9310
0x322eac 8b83d4000000 mov eax, dword ptr [rbx + 0xd4]
0x322eb2 84c0 test al, al
0x322eb4 780b js 0x322ec1
0x322eb6 257ff7ffff and eax, 0xfffff77f
0x322ebb 8983d4000000 mov dword ptr [rbx + 0xd4], eax
0x322ec1 4883c408 add rsp, 8
0x322ec5 5b pop rbx
0x322ec6 5d pop rbp
0x322ec7 c3 ret 
0x322ec8 cc int3 
0x322ec9 cc int3 
0x322eca cc int3 
0x322ecb cc int3 
0x322ecc cc int3 
0x322ecd cc int3 
0x322ece cc int3 
0x322ecf cc int3 
0x322ed0 55 push rbp
0x322ed1 4889e5 mov rbp, rsp
0x322ed4 e8d7700a00 call 0x3c9fb0
0x322ed9 5d pop rbp
0x322eda c3 ret 
0x322edb cc int3 
0x322edc cc int3 
0x322edd cc int3 
0x322ede cc int3 
0x322edf cc int3 
0x322ee0 55 push rbp
0x322ee1 4889e5 mov rbp, rsp
0x322ee4 53 push rbx
0x322ee5 4883ec18 sub rsp, 0x18
0x322ee9 488b1d18f0e008 mov rbx, qword ptr [rip + 0x8e0f018]
0x322ef0 488b03 mov rax, qword ptr [rbx]
0x322ef3 488945f0 mov qword ptr [rbp - 0x10], rax
0x322ef7 488b06 mov rax, qword ptr [rsi]
0x322efa 488d75e8 lea rsi, [rbp - 0x18]
0x322efe 488945e8 mov qword ptr [rbp - 0x18], rax
0x322f02 e8d9750a00 call 0x3ca4e0
0x322f07 488b03 mov rax, qword ptr [rbx]
0x322f0a 483b45f0 cmp rax, qword ptr [rbp - 0x10]
0x322f0e 7507 jne 0x322f17
0x322f10 4883c418 add rsp, 0x18
0x322f14 5b pop rbx
0x322f15 5d pop rbp
0x322f16 c3 ret 
0x322f17 e844ef1407 call 0x7471e60
0x322f1c 0f0b ud2 
0x322f1e cc int3 
0x322f1f cc int3 
0x322f20 55 push rbp
0x322f21 4889e5 mov rbp, rsp
0x322f24 4157 push r15
0x322f26 4156 push r14
0x322f28 4155 push r13
0x322f2a 4154 push r12
0x322f2c 53 push rbx
0x322f2d 4881ec38070000 sub rsp, 0x738
0x322f34 4c8b2dcdefe008 mov r13, qword ptr [rip + 0x8e0efcd]
0x322f3b 4889f3 mov rbx, rsi
0x322f3e 4989fe mov r14, rdi
0x322f41 498b4500 mov rax, qword ptr [r13]
0x322f45 488945d0 mov qword ptr [rbp - 0x30], rax
0x322f49 e8727affff call 0x31a9c0
0x322f4e 4d8bbe20010000 mov r15, qword ptr [r14 + 0x120]
0x322f55 4d85ff test r15, r15
0x322f58 0f8417010000 je 0x323075
0x322f5e 8b4308 mov eax, dword ptr [rbx + 8]
0x322f61 3b430c cmp eax, dword ptr [rbx + 0xc]
0x322f64 7511 jne 0x322f77
0x322f66 488d530c lea rdx, [rbx + 0xc]
0x322f6a bf08080000 mov edi, 0x808
0x322f6f 4889de mov rsi, rbx
0x322f72 e82969d0ff call 0x298a0
0x322f77 8d4801 lea ecx, [rax + 1]
0x322f7a 4898 cdqe 
0x322f7c 4c89f7 mov rdi, r14
0x322f7f be01000000 mov esi, 1
0x322f84 894b08 mov dword ptr [rbx + 8], ecx
0x322f87 488b0b mov rcx, qword ptr [rbx]
0x322f8a 4c893cc1 mov qword ptr [rcx + rax*8], r15
0x322f8e e80d010000 call 0x3230a0
0x322f93 4d8ba620010000 mov r12, qword ptr [r14 + 0x120]
0x322f9a 4989c7 mov r15, rax
0x322f9d 498bbc24b8000000 mov rdi, qword ptr [r12 + 0xb8]
0x322fa5 4885ff test rdi, rdi
0x322fa8 7413 je 0x322fbd
0x322faa 488b07 mov rax, qword ptr [rdi]
0x322fad 4c89fe mov rsi, r15
0x322fb0 4889da mov rdx, rbx
0x322fb3 ff5070 call qword ptr [rax + 0x70]
0x322fb6 4d8ba620010000 mov r12, qword ptr [r14 + 0x120]
0x322fbd e88e381100 call 0x436850
0x322fc2 4c8db5a8f8ffff lea r14, [rbp - 0x758]
0x322fc9 4889c6 mov rsi, rax
0x322fcc 4c89e2 mov rdx, r12
0x322fcf 4c89f9 mov rcx, r15
0x322fd2 41b801000000 mov r8d, 1
0x322fd8 41b902000000 mov r9d, 2
0x322fde 4c89f7 mov rdi, r14
0x322fe1 e8ca861400 call 0x46b6b0
0x322fe6 eb10 jmp 0x322ff8
0x322fe8 0f1f840000000000 nop dword ptr [rax + rax]
0x322ff0 4c89f7 mov rdi, r14
0x322ff3 e8d89e1400 call 0x46ced0
0x322ff8 8b75b0 mov esi, dword ptr [rbp - 0x50]
0x322ffb 85f6 test esi, esi
0x322ffd 7e55 jle 0x323054
0x322fff 488b4da8 mov rcx, qword ptr [rbp - 0x58]
0x323003 89f0 mov eax, esi
0x323005 4885c9 test rcx, rcx
0x323008 490f44ce cmove rcx, r14
0x32300c 4869d0e0000000 imul rdx, rax, 0xe0
0x323013 488d841128ffffff lea rax, [rcx + rdx - 0xd8]
0x32301b 48637411f8 movsxd rsi, dword ptr [rcx + rdx - 8]
0x323020 488b4c11e8 mov rcx, qword ptr [rcx + rdx - 0x18]
0x323025 4885c9 test rcx, rcx
0x323028 480f45c1 cmovne rax, rcx
0x32302c 488d0c76 lea rcx, [rsi + rsi*2]
0x323030 488b14c8 mov rdx, qword ptr [rax + rcx*8]
0x323034 488b5270 mov rdx, qword ptr [rdx + 0x70]
0x323038 488bbab8000000 mov rdi, qword ptr [rdx + 0xb8]
0x32303f 4885ff test rdi, rdi
0x323042 74ac je 0x322ff0
0x323044 488b74c808 mov rsi, qword ptr [rax + rcx*8 + 8]
0x323049 488b07 mov rax, qword ptr [rdi]
0x32304c 4889da mov rdx, rbx
0x32304f ff5070 call qword ptr [rax + 0x70]
0x323052 eb9c jmp 0x322ff0
0x323054 488b45a8 mov rax, qword ptr [rbp - 0x58]
0x323058 4885c0 test rax, rax
0x32305b 4c0f45f0 cmovne r14, rax
0x32305f 4c89f7 mov rdi, r14
0x323062 e8692efaff call 0x2c5ed0
0x323067 488b7da8 mov rdi, qword ptr [rbp - 0x58]
0x32306b 4885ff test rdi, rdi
0x32306e 7405 je 0x323075
0x323070 e81b4ad9ff call 0xb7a90
0x323075 498b4500 mov rax, qword ptr [r13]
0x323079 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x32307d 7512 jne 0x323091
0x32307f 4881c438070000 add rsp, 0x738
0x323086 5b pop rbx
0x323087 415c pop r12
0x323089 415d pop r13
0x32308b 415e pop r14
0x32308d 415f pop r15
0x32308f 5d pop rbp
0x323090 c3 ret 
0x323091 e8caed1407 call 0x7471e60
0x323096 0f0b ud2 
0x323098 cc int3 
0x323099 cc int3 
0x32309a cc int3 
0x32309b cc int3 
0x32309c cc int3 
0x32309d cc int3 
0x32309e cc int3 
0x32309f cc int3 
0x3230a0 55 push rbp
0x3230a1 4889e5 mov rbp, rsp
0x3230a4 488b8718010000 mov rax, qword ptr [rdi + 0x118]
0x3230ab 4885c0 test rax, rax
0x3230ae 7402 je 0x3230b2
0x3230b0 5d pop rbp
0x3230b1 c3 ret 
0x3230b2 4080fe01 cmp sil, 1
0x3230b6 740c je 0x3230c4
0x3230b8 400fb6c6 movzx eax, sil
0x3230bc 85c0 test eax, eax
0x3230be 7436 je 0x3230f6
0x3230c0 31c0 xor eax, eax
0x3230c2 5d pop rbp
0x3230c3 c3 ret 
0x3230c4 488b4740 mov rax, qword ptr [rdi + 0x40]
0x3230c8 4885c0 test rax, rax
0x3230cb 741f je 0x3230ec
0x3230cd 488b8f20010000 mov rcx, qword ptr [rdi + 0x120]
0x3230d4 483b8820010000 cmp rcx, qword ptr [rax + 0x120]
0x3230db 7519 jne 0x3230f6
0x3230dd 4889c7 mov rdi, rax
0x3230e0 be01000000 mov esi, 1
0x3230e5 e8b6ffffff call 0x3230a0
0x3230ea 5d pop rbp
0x3230eb c3 ret 
0x3230ec 4883bf2001000000 cmp qword ptr [rdi + 0x120], 0
0x3230f4 74ca je 0x3230c0
0x3230f6 e8051f0000 call 0x325000
0x3230fb 5d pop rbp
0x3230fc c3 ret 
0x3230fd cc int3 
0x3230fe cc int3 
0x3230ff cc int3 
0x323100 55 push rbp
0x323101 4889e5 mov rbp, rsp
0x323104 e8077cffff call 0x31ad10
0x323109 5d pop rbp
0x32310a c3 ret 
0x32310b cc int3 
0x32310c cc int3 
0x32310d cc int3 
0x32310e cc int3 
0x32310f cc int3 
0x323110 55 push rbp
0x323111 4889e5 mov rbp, rsp
0x323114 4157 push r15
0x323116 4156 push r14
0x323118 4155 push r13
0x32311a 4154 push r12
0x32311c 53 push rbx
0x32311d 4883ec38 sub rsp, 0x38
0x323121 4c8b35e0ede008 mov r14, qword ptr [rip + 0x8e0ede0]
0x323128 498b06 mov rax, qword ptr [r14]
0x32312b 488945d0 mov qword ptr [rbp - 0x30], rax
0x32312f f687d500000008 test byte ptr [rdi + 0xd5], 8
0x323136 0f8546040000 jne 0x323582
0x32313c c7870801000000000000 mov dword ptr [rdi + 0x108], 0
0x323146 4889fb mov rbx, rdi
0x323149 4c8db700010000 lea r14, [rdi + 0x100]
0x323150 4c8dbf0c010000 lea r15, [rdi + 0x10c]
0x323157 83bf0c01000000 cmp dword ptr [rdi + 0x10c], 0
0x32315e 741a je 0x32317a
0x323160 bf08000000 mov edi, 8
0x323165 be08000000 mov esi, 8
0x32316a 4c89f1 mov rcx, r14
0x32316d 4d89f9 mov r9, r15
0x323170 31d2 xor edx, edx
0x323172 4531c0 xor r8d, r8d
0x323175 e8a62cd0ff call 0x25e20
0x32317a 4c8975b0 mov qword ptr [rbp - 0x50], r14
0x32317e 4c8b7340 mov r14, qword ptr [rbx + 0x40]
0x323182 4c897db8 mov qword ptr [rbp - 0x48], r15
0x323186 4d85f6 test r14, r14
0x323189 747d je 0x323208
0x32318b 4c89f7 mov rdi, r14
0x32318e e87dffffff call 0x323110
0x323193 4939de cmp r14, rbx
0x323196 0f84ac000000 je 0x323248
0x32319c 8b8bfc000000 mov ecx, dword ptr [rbx + 0xfc]
0x3231a2 4d63aef8000000 movsxd r13, dword ptr [r14 + 0xf8]
0x3231a9 4d8ba6f0000000 mov r12, qword ptr [r14 + 0xf0]
0x3231b0 488d83fc000000 lea rax, [rbx + 0xfc]
0x3231b7 89ca mov edx, ecx
0x3231b9 4489abf8000000 mov dword ptr [rbx + 0xf8], r13d
0x3231c0 4409ea or edx, r13d
0x3231c3 747d je 0x323242
0x3231c5 4c8db3f0000000 lea r14, [rbx + 0xf0]
0x3231cc bf10000000 mov edi, 0x10
0x3231d1 be08000000 mov esi, 8
0x3231d6 4489ea mov edx, r13d
0x3231d9 4589e9 mov r9d, r13d
0x3231dc 48890424 mov qword ptr [rsp], rax
0x3231e0 4d89f0 mov r8, r14
0x3231e3 e8e843d0ff call 0x275d0
0x3231e8 4585ed test r13d, r13d
0x3231eb 745b je 0x323248
0x3231ed 498b3e mov rdi, qword ptr [r14]
0x3231f0 49c1e504 shl r13, 4
0x3231f4 4c89e6 mov rsi, r12
0x3231f7 4c89ea mov rdx, r13
0x3231fa e851ec1407 call 0x7471e50
0x3231ff 448bb3f8000000 mov r14d, dword ptr [rbx + 0xf8]
0x323206 eb4c jmp 0x323254
0x323208 c783f800000000000000 mov dword ptr [rbx + 0xf8], 0
0x323212 83bbfc00000000 cmp dword ptr [rbx + 0xfc], 0
0x323219 7436 je 0x323251
0x32321b 4c8d8bfc000000 lea r9, [rbx + 0xfc]
0x323222 488d8bf0000000 lea rcx, [rbx + 0xf0]
0x323229 bf10000000 mov edi, 0x10
0x32322e be08000000 mov esi, 8
0x323233 4531f6 xor r14d, r14d
0x323236 31d2 xor edx, edx
0x323238 4531c0 xor r8d, r8d
0x32323b e8e02bd0ff call 0x25e20
0x323240 eb12 jmp 0x323254
0x323242 c70000000000 mov dword ptr [rax], 0
0x323248 448bb3f8000000 mov r14d, dword ptr [rbx + 0xf8]
0x32324f eb03 jmp 0x323254
0x323251 4531f6 xor r14d, r14d
0x323254 4489b3cc000000 mov dword ptr [rbx + 0xcc], r14d
0x32325b c5f857c0 vxorps xmm0, xmm0, xmm0
0x32325f c5f81145c0 vmovups xmmword ptr [rbp - 0x40], xmm0
0x323264 4885db test rbx, rbx
0x323267 0f84b2010000 je 0x32341f
0x32326d 4c8b6b50 mov r13, qword ptr [rbx + 0x50]
0x323271 4d85ed test r13, r13
0x323274 0f84c6000000 je 0x323340
0x32327a 4c8d65cc lea r12, [rbp - 0x34]
0x32327e 6690 nop 
0x323280 498b4508 mov rax, qword ptr [r13 + 8]
0x323284 f6401001 test byte ptr [rax + 0x10], 1
0x323288 750e jne 0x323298
0x32328a 4d8b6d18 mov r13, qword ptr [r13 + 0x18]
0x32328e 4d85ed test r13, r13
0x323291 75ed jne 0x323280
0x323293 e99f000000 jmp 0x323337
0x323298 4c8d7dc0 lea r15, [rbp - 0x40]
0x32329c eb09 jmp 0x3232a7
0x32329e 4d85ed test r13, r13
0x3232a1 0f8490000000 je 0x323337
0x3232a7 498b4508 mov rax, qword ptr [r13 + 8]
0x3232ab f6401180 test byte ptr [rax + 0x11], 0x80
0x3232af 745f je 0x323310
0x3232b1 41f6453820 test byte ptr [r13 + 0x38], 0x20
0x3232b6 7458 je 0x323310
0x3232b8 4d8d7510 lea r14, [r13 + 0x10]
0x3232bc e8ef280a00 call 0x3c5bb0
0x3232c1 4c89f7 mov rdi, r14
0x3232c4 4889c6 mov rsi, rax
0x3232c7 e8640b0400 call 0x363e30
0x3232cc 84c0 test al, al
0x3232ce 740e je 0x3232de
0x3232d0 498b06 mov rax, qword ptr [r14]
0x3232d3 4883e0fe and rax, 0xfffffffffffffffe
0x3232d7 4839d8 cmp rax, rbx
0x3232da 7534 jne 0x323310
0x3232dc eb07 jmp 0x3232e5
0x3232de 31c0 xor eax, eax
0x3232e0 4839d8 cmp rax, rbx
0x3232e3 752b jne 0x323310
0x3232e5 8b45c8 mov eax, dword ptr [rbp - 0x38]
0x3232e8 3b45cc cmp eax, dword ptr [rbp - 0x34]
0x3232eb 7510 jne 0x3232fd
0x3232ed bf08080000 mov edi, 0x808
0x3232f2 4c89fe mov rsi, r15
0x3232f5 4c89e2 mov rdx, r12
0x3232f8 e8a365d0ff call 0x298a0
0x3232fd 8d4801 lea ecx, [rax + 1]
0x323300 4898 cdqe 
0x323302 894dc8 mov dword ptr [rbp - 0x38], ecx
0x323305 488b4dc0 mov rcx, qword ptr [rbp - 0x40]
0x323309 4c892cc1 mov qword ptr [rcx + rax*8], r13
0x32330d 0f1f00 nop dword ptr [rax]
0x323310 4d8b6d18 mov r13, qword ptr [r13 + 0x18]
0x323314 4885db test rbx, rbx
0x323317 7485 je 0x32329e
0x323319 4d85ed test r13, r13
0x32331c 7422 je 0x323340
0x32331e 6690 nop 
0x323320 498b4508 mov rax, qword ptr [r13 + 8]
0x323324 f6401001 test byte ptr [rax + 0x10], 1
0x323328 0f8570ffffff jne 0x32329e
0x32332e 4d8b6d18 mov r13, qword ptr [r13 + 0x18]
0x323332 4d85ed test r13, r13
0x323335 75e9 jne 0x323320
0x323337 4885db test rbx, rbx
0x32333a 0f84df000000 je 0x32341f
0x323340 4c8b7348 mov r14, qword ptr [rbx + 0x48]
0x323344 4d85f6 test r14, r14
0x323347 0f84d2000000 je 0x32341f
0x32334d 0f1f00 nop dword ptr [rax]
0x323350 498b4610 mov rax, qword ptr [r14 + 0x10]
0x323354 f680d800000001 test byte ptr [rax + 0xd8], 1
0x32335b 750e jne 0x32336b
0x32335d 4d8b7628 mov r14, qword ptr [r14 + 0x28]
0x323361 4d85f6 test r14, r14
0x323364 75ea jne 0x323350
0x323366 e9b4000000 jmp 0x32341f
0x32336b 4c8d2587560309 lea r12, [rip + 0x9035687]
0x323372 eb09 jmp 0x32337d
0x323374 4d85f6 test r14, r14
0x323377 0f84a2000000 je 0x32341f
0x32337d e82e510000 call 0x3284b0
0x323382 498b4e10 mov rcx, qword ptr [r14 + 0x10]
0x323386 48635038 movsxd rdx, dword ptr [rax + 0x38]
0x32338a 3b5138 cmp edx, dword ptr [rcx + 0x38]
0x32338d 7f61 jg 0x3233f0
0x32338f 488b4930 mov rcx, qword ptr [rcx + 0x30]
0x323393 4883c030 add rax, 0x30
0x323397 483904d1 cmp qword ptr [rcx + rdx*8], rax
0x32339b 7553 jne 0x3233f0
0x32339d 41803c2401 cmp byte ptr [r12], 1
0x3233a2 7504 jne 0x3233a8
0x3233a4 418b4608 mov eax, dword ptr [r14 + 8]
0x3233a8 41f686b000000040 test byte ptr [r14 + 0xb0], 0x40
0x3233b0 743e je 0x3233f0
0x3233b2 49837e4000 cmp qword ptr [r14 + 0x40], 0
0x3233b7 7537 jne 0x3233f0
0x3233b9 8b8308010000 mov eax, dword ptr [rbx + 0x108]
0x3233bf 3b830c010000 cmp eax, dword ptr [rbx + 0x10c]
0x3233c5 7512 jne 0x3233d9
0x3233c7 488b75b0 mov rsi, qword ptr [rbp - 0x50]
0x3233cb 488b55b8 mov rdx, qword ptr [rbp - 0x48]
0x3233cf bf08080000 mov edi, 0x808
0x3233d4 e8c764d0ff call 0x298a0
0x3233d9 8d4801 lea ecx, [rax + 1]
0x3233dc 4898 cdqe 
0x3233de 898b08010000 mov dword ptr [rbx + 0x108], ecx
0x3233e4 488b8b00010000 mov rcx, qword ptr [rbx + 0x100]
0x3233eb 4c8934c1 mov qword ptr [rcx + rax*8], r14
0x3233ef 90 nop 
0x3233f0 4d8b7628 mov r14, qword ptr [r14 + 0x28]
0x3233f4 4885db test rbx, rbx
0x3233f7 0f8477ffffff je 0x323374
0x3233fd 0f1f00 nop dword ptr [rax]
0x323400 4d85f6 test r14, r14
0x323403 741a je 0x32341f
0x323405 498b4610 mov rax, qword ptr [r14 + 0x10]
0x323409 f680d800000001 test byte ptr [rax + 0xd8], 1
0x323410 0f855effffff jne 0x323374
0x323416 4d8b7628 mov r14, qword ptr [r14 + 0x28]
0x32341a 4d85f6 test r14, r14
0x32341d 75e6 jne 0x323405
0x32341f f683d400000080 test byte ptr [rbx + 0xd4], 0x80
0x323426 8b75c8 mov esi, dword ptr [rbp - 0x38]
0x323429 750c jne 0x323437
0x32342b 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0x32342f e87cd40000 call 0x3308b0
0x323434 8b75c8 mov esi, dword ptr [rbp - 0x38]
0x323437 448b83f8000000 mov r8d, dword ptr [rbx + 0xf8]
0x32343e 89f2 mov edx, esi
0x323440 4401c2 add edx, r8d
0x323443 0f8858010000 js 0x3235a1
0x323449 488d83f0000000 lea rax, [rbx + 0xf0]
0x323450 4c8dabfc000000 lea r13, [rbx + 0xfc]
0x323457 488945a8 mov qword ptr [rbp - 0x58], rax
0x32345b 3993fc000000 cmp dword ptr [rbx + 0xfc], edx
0x323461 7d19 jge 0x32347c
0x323463 488b4da8 mov rcx, qword ptr [rbp - 0x58]
0x323467 bf10000000 mov edi, 0x10
0x32346c be08000000 mov esi, 8
0x323471 4d89e9 mov r9, r13
0x323474 e8a729d0ff call 0x25e20
0x323479 8b75c8 mov esi, dword ptr [rbp - 0x38]
0x32347c 85f6 test esi, esi
0x32347e 0f8e9d000000 jle 0x323521
0x323484 8b83f8000000 mov eax, dword ptr [rbx + 0xf8]
0x32348a 488b4dc0 mov rcx, qword ptr [rbp - 0x40]
0x32348e 4531f6 xor r14d, r14d
0x323491 eb1b jmp 0x3234ae
0x323493 666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3234a0 8b75c8 mov esi, dword ptr [rbp - 0x38]
0x3234a3 49ffc6 inc r14
0x3234a6 4863d6 movsxd rdx, esi
0x3234a9 4939d6 cmp r14, rdx
0x3234ac 7d73 jge 0x323521
0x3234ae 4a8b14f1 mov rdx, qword ptr [rcx + r14*8]
0x3234b2 66894240 mov word ptr [rdx + 0x40], ax
0x3234b6 837a2c00 cmp dword ptr [rdx + 0x2c], 0
0x3234ba 7ee7 jle 0x3234a3
0x3234bc 4e8d24f1 lea r12, [rcx + r14*8]
0x3234c0 4531ff xor r15d, r15d
0x3234c3 eb3b jmp 0x323500
0x3234c5 66662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3234d0 4863f0 movsxd rsi, eax
0x3234d3 ffc0 inc eax
0x3234d5 4e8d24f1 lea r12, [rcx + r14*8]
0x3234d9 8983f8000000 mov dword ptr [rbx + 0xf8], eax
0x3234df 48c1e604 shl rsi, 4
0x3234e3 488bbbf0000000 mov rdi, qword ptr [rbx + 0xf0]
0x3234ea 48891437 mov qword ptr [rdi + rsi], rdx
0x3234ee 44897c3708 mov dword ptr [rdi + rsi + 8], r15d
0x3234f3 41ffc7 inc r15d
0x3234f6 4a8b14f1 mov rdx, qword ptr [rcx + r14*8]
0x3234fa 443b7a2c cmp r15d, dword ptr [rdx + 0x2c]
0x3234fe 7da0 jge 0x3234a0
0x323500 413b4500 cmp eax, dword ptr [r13]
0x323504 75ca jne 0x3234d0
0x323506 488b75a8 mov rsi, qword ptr [rbp - 0x58]
0x32350a bf10080000 mov edi, 0x810
0x32350f 4c89ea mov rdx, r13
0x323512 e88963d0ff call 0x298a0
0x323517 498b1424 mov rdx, qword ptr [r12]
0x32351b 488b4dc0 mov rcx, qword ptr [rbp - 0x40]
0x32351f ebaf jmp 0x3234d0
0x323521 8b9308010000 mov edx, dword ptr [rbx + 0x108]
0x323527 8bb30c010000 mov esi, dword ptr [rbx + 0x10c]
0x32352d 39d6 cmp esi, edx
0x32352f 7420 je 0x323551
0x323531 488b4db0 mov rcx, qword ptr [rbp - 0x50]
0x323535 4c8b4db8 mov r9, qword ptr [rbp - 0x48]
0x323539 bf08000000 mov edi, 8
0x32353e be08000000 mov esi, 8
0x323543 4189d0 mov r8d, edx
0x323546 e8d528d0ff call 0x25e20
0x32354b 8bb308010000 mov esi, dword ptr [rbx + 0x108]
0x323551 488bbb00010000 mov rdi, qword ptr [rbx + 0x100]
0x323558 488d15d16bd0ff lea rdx, [rip - 0x2f942f]
0x32355f 31c9 xor ecx, ecx
0x323561 e85ae20000 call 0x3317c0
0x323566 808bd500000008 or byte ptr [rbx + 0xd5], 8
0x32356d 4c8b3594e9e008 mov r14, qword ptr [rip + 0x8e0e994]
0x323574 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0x323578 4885ff test rdi, rdi
0x32357b 7405 je 0x323582
0x32357d e80e45d9ff call 0xb7a90
0x323582 498b06 mov rax, qword ptr [r14]
0x323585 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x323589 750f jne 0x32359a
0x32358b 4883c438 add rsp, 0x38
0x32358f 5b pop rbx
0x323590 415c pop r12
0x323592 415d pop r13
0x323594 415e pop r14
0x323596 415f pop r15
0x323598 5d pop rbp
0x323599 c3 ret 
0x32359a e8c1e81407 call 0x7471e60
0x32359f 0f0b ud2 
0x3235a1 89d7 mov edi, edx
0x3235a3 e858fbd3ff call 0x63100
0x3235a8 0f0b ud2 
0x3235aa cc int3 
0x3235ab cc int3 
0x3235ac cc int3 
0x3235ad cc int3 
0x3235ae cc int3 
0x3235af cc int3 
0x3235b0 55 push rbp
0x3235b1 4889e5 mov rbp, rsp
0x3235b4 488b07 mov rax, qword ptr [rdi]
0x3235b7 ff9000040000 call qword ptr [rax + 0x400]
0x3235bd 5d pop rbp
0x3235be c3 ret 
0x3235bf cc int3 
0x3235c0 55 push rbp
0x3235c1 4889e5 mov rbp, rsp
0x3235c4 4156 push r14
0x3235c6 53 push rbx
0x3235c7 4889f3 mov rbx, rsi
0x3235ca 4989fe mov r14, rdi
0x3235cd 4885f6 test rsi, rsi
0x3235d0 7414 je 0x3235e6
0x3235d2 488d0583b80709 lea rax, [rip + 0x907b883]
0x3235d9 803800 cmp byte ptr [rax], 0
0x3235dc 7408 je 0x3235e6
0x3235de 4889df mov rdi, rbx
0x3235e1 e8da4c0400 call 0x3682c0
0x3235e6 49899e10010000 mov qword ptr [r14 + 0x110], rbx
0x3235ed 5b pop rbx
0x3235ee 415e pop r14
0x3235f0 5d pop rbp
0x3235f1 c3 ret 
0x3235f2 cc int3 
0x3235f3 cc int3 
0x3235f4 cc int3 
0x3235f5 cc int3 
0x3235f6 cc int3 
0x3235f7 cc int3 
0x3235f8 cc int3 
0x3235f9 cc int3 
0x3235fa cc int3 
0x3235fb cc int3 
0x3235fc cc int3 
0x3235fd cc int3 
0x3235fe cc int3 
0x3235ff cc int3 

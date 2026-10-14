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

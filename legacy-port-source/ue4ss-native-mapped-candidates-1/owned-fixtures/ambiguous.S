# Owned byte fixture: never executed, linked PIC with genuine CFI metadata.
.text
.hidden owned_fname_ctor, owned_construct, owned_load_modules, owned_tick
.globl owned_fname_ctor, owned_construct, owned_load_modules, owned_tick
.type owned_fname_ctor,@function
owned_fname_ctor:
.cfi_startproc
ret
.cfi_endproc
.size owned_fname_ctor,.-owned_fname_ctor
.type owned_construct,@function
owned_construct:
.cfi_startproc
ret
.cfi_endproc
.size owned_construct,.-owned_construct
.type owned_load_modules,@function
owned_load_modules:
.cfi_startproc
mov $1,%edx
call owned_fname_ctor
lea engine+2(%rip),%rsi
lea renderer+2(%rip),%rsi
lea anim+2(%rip),%rsi
lea landscape+2(%rip),%rsi
lea rendercore+2(%rip),%rsi
ret
.cfi_endproc
.size owned_load_modules,.-owned_load_modules
.type owned_tick,@function
owned_tick:
.cfi_startproc
lea tick_string(%rip),%rdi
ret
.cfi_endproc
.size owned_tick,.-owned_tick
# Actual upstream signature encodings as owned scanner controls.
.byte 0x74,0,0x48,0x8d,0x0d
.long owned_object_slot-.-4
.byte 0xc6,0x05
.long 0
.byte 1,0xe8
.long 0
.byte 0xc6,0x05
.long 0
.byte 1,0xc3
.byte 0x48,0x85,0xc9,0x74,0,0x4c,0x8b,0x05
.long owned_allocator_slot-.-4
.byte 0x4d,0x85,0xc0,0x0f,0x84
.long 0
.byte 0xc3
.byte 0xe8
.long owned_construct-.-4
.byte 0x48,0x8b,0xd8,0x48,0x39,0x75,0x30,0x74,0x15,0xc3
.section .rodata
version:
.short 43,43,85,69,53,43,82,101,108,101,97,115,101,45,53,46,55,0
engine:
.short 0,69,110,103,105,110,101,0,0
renderer:
.short 0,82,101,110,100,101,114,101,114,0,0
anim:
.short 0,65,110,105,109,71,114,97,112,104,82,117,110,116,105,109,101,0,0
landscape:
.short 0,76,97,110,100,115,99,97,112,101,0,0
rendercore:
.short 0,82,101,110,100,101,114,67,111,114,101,0,0
tick_string:
.short 99,97,117,115,101,101,118,101,110,116,61,0,0
.bss
.balign 8
.hidden owned_object_slot,owned_allocator_slot
.globl owned_object_slot,owned_allocator_slot
owned_object_slot: .zero 256
owned_allocator_slot: .zero 8
.section .note.GNU-stack,"",@progbits
.text
# Independent competing genuine GU encoding; no injected matching result.
.byte 0x74,0,0x48,0x8d,0x0d
.long owned_allocator_slot-.-4
.byte 0xc6,0x05
.long 0
.byte 1,0xe8
.long 0
.byte 0xc6,0x05
.long 0
.byte 1,0xc3

// fmemory_malloc @ Ghidra 000b7a20 (file vaddr 0xb7a20, mapping delta 0x0)

void CORROB_fmemory_malloc(undefined8 param_1,undefined4 param_2)

{
  int iVar1;
  
  if (((plRam0000000009350950 == (long *)0x0) && (DAT_0932c048 == '\0')) &&
     (iVar1 = func_0x07471eb0(&DAT_0932c048), iVar1 != 0)) {
    func_0x000beba0();
    func_0x07471ec0(&DAT_0932c048);
  }
  (**(code **)(*plRam0000000009350950 + 0x30))(plRam0000000009350950,param_1,param_2);
  return;
}



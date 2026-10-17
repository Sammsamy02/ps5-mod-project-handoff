// fname_ctor @ Ghidra 0023d590 (file vaddr 0x23d590, mapping delta 0x0)

void CORROB_fname_ctor(undefined8 *param_1,ushort *param_2)

{
  ushort uVar1;
  undefined8 uVar2;
  uint uVar3;
  ushort *puVar4;
  ulong uVar5;
  
  if (param_2 == (ushort *)0x0) {
    uVar5 = 0;
    puVar4 = (ushort *)0x0;
  }
  else {
    uVar1 = *param_2;
    uVar5 = 0;
    puVar4 = param_2;
    if (uVar1 != 0) {
      do {
        uVar3 = (uint)uVar5 | (uint)uVar1;
        uVar5 = (ulong)uVar3;
        uVar1 = puVar4[1];
        puVar4 = puVar4 + 1;
      } while (uVar1 != 0);
      uVar5 = (ulong)(0x7f < uVar3) << 0x20;
    }
  }
  uVar2 = CORROB_find_or_store
                    (param_2,(ulong)((long)puVar4 - (long)param_2) >> 1 & 0xffffffff | uVar5);
  *param_1 = uVar2;
  return;
}



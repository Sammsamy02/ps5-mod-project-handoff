// reserve @ Ghidra 00246090 (file vaddr 0x246090, mapping delta 0x0)

ulong CORROB_reserve(long param_1,undefined8 param_2,undefined8 param_3)

{
  ushort *puVar1;
  undefined8 uVar2;
  uint uVar3;
  ulong uVar4;
  uint uVar5;
  uint uVar6;
  
  func_0x074728c0();
  uVar3 = *(uint *)(param_1 + 8);
  uVar4 = (ulong)uVar3;
  uVar5 = *(uint *)(param_1 + 0xc);
  uVar6 = ((int)param_3 << ((byte)((ulong)param_3 >> 0x20) & 1)) + 3U & 0xfffffffe;
  if (0x20000 - uVar5 <= uVar6 - 1) {
    if (uVar5 + 2 < 0x20001) {
      puVar1 = (ushort *)(*(long *)(param_1 + 0x10 + uVar4 * 8) + (ulong)uVar5);
      *puVar1 = *puVar1 & 0x3f;
      uVar3 = *(uint *)(param_1 + 8);
    }
    uVar4 = (ulong)(uVar3 + 1);
    *(uint *)(param_1 + 8) = uVar3 + 1;
    *(undefined4 *)(param_1 + 0xc) = 0;
    if (*(long *)(param_1 + 0x10 + uVar4 * 8) == 0) {
      uVar2 = CORROB_fmemory_malloc(0x20000,2);
      uVar4 = (ulong)*(uint *)(param_1 + 8);
      *(undefined8 *)(param_1 + 0x10 + uVar4 * 8) = uVar2;
    }
    uVar5 = *(uint *)(param_1 + 0xc);
    if (0x20000 - uVar5 < uVar6) {
      uVar6 = 0;
    }
  }
  *(uint *)(param_1 + 0xc) = uVar6 + uVar5;
  func_0x074728d0();
  return (ulong)(uVar5 >> 1) << 0x20 | uVar4;
}



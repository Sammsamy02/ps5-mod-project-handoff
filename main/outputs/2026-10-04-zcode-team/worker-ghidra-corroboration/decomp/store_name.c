// store_name @ Ghidra 0023c0b0 (file vaddr 0x23c0b0, mapping delta 0x0)

uint CORROB_store_name(long param_1,undefined8 param_2,ulong param_3)

{
  undefined *puVar1;
  long lVar2;
  uint uVar3;
  int iVar4;
  ulong uVar5;
  ulong uVar6;
  short sVar7;
  uint uVar8;
  uint uVar9;
  long lVar10;
  undefined1 auVar11 [16];
  uint *puStack_40;
  
  if ((param_3 >> 0x20 & 1) == 0) {
    auVar11 = func_0x00239b00();
  }
  else {
    auVar11 = func_0x0023a2c0(param_2,param_3 & 0xffffffff);
  }
  lVar10 = (auVar11._0_8_ & 0xffffffff) * 0x40;
  puVar1 = &UNK_00010040 + lVar10 + param_1;
  func_0x074728c0();
  uVar8 = *(uint *)(&UNK_0001004c + lVar10 + param_1);
  lVar10 = *(long *)(&UNK_00010050 + lVar10 + param_1);
  uVar5 = (ulong)(auVar11._4_4_ & uVar8);
  uVar3 = *(uint *)(lVar10 + uVar5 * 4);
  uVar9 = auVar11._8_4_;
  sVar7 = auVar11._12_2_;
  if (uVar3 != 0) {
    uVar6 = auVar11._8_8_ >> 0x26 & 0x3ff;
    if ((param_3 >> 0x20 & 1) == 0) {
      do {
        if ((uVar3 & 0xe0000000) == uVar9) {
          lVar2 = *(long *)(*(long *)(puVar1 + 0x18) + 0x10 + (ulong)(uVar3 >> 0x10 & 0x1fff) * 8);
          if (*(short *)(lVar2 + (ulong)(uVar3 & 0xffff) * 2) == sVar7) {
            iVar4 = func_0x0008cdb0(lVar2 + (ulong)(uVar3 & 0xffff) * 2 + 2,param_2,uVar6);
            if (iVar4 == 0) goto LAB_0023c311;
            lVar10 = *(long *)(puVar1 + 0x10);
          }
        }
        uVar5 = (ulong)((int)uVar5 + 1U & uVar8);
        uVar3 = *(uint *)(lVar10 + uVar5 * 4);
      } while (uVar3 != 0);
    }
    else {
      do {
        if ((uVar3 & 0xe0000000) == uVar9) {
          lVar2 = *(long *)(*(long *)(puVar1 + 0x18) + 0x10 + (ulong)(uVar3 >> 0x10 & 0x1fff) * 8);
          if (*(short *)(lVar2 + (ulong)(uVar3 & 0xffff) * 2) == sVar7) {
            iVar4 = func_0x0008ce20(lVar2 + (ulong)(uVar3 & 0xffff) * 2 + 2,param_2,uVar6);
            if (iVar4 == 0) goto LAB_0023c311;
            lVar10 = *(long *)(puVar1 + 0x10);
          }
        }
        uVar5 = (ulong)((int)uVar5 + 1U & uVar8);
        uVar3 = *(uint *)(lVar10 + uVar5 * 4);
      } while (uVar3 != 0);
    }
  }
  puStack_40 = (uint *)(lVar10 + uVar5 * 4);
LAB_0023c249:
  lVar10 = *(long *)(puVar1 + 0x18);
  uVar5 = CORROB_reserve(lVar10,param_2,param_3);
  lVar10 = *(long *)(lVar10 + 0x10 + (uVar5 & 0xffffffff) * 8);
  uVar8 = (uint)(uVar5 >> 0x20);
  uVar6 = (ulong)(uVar8 & 0x7fffffff);
  *(short *)(lVar10 + uVar6 * 2) = sVar7;
  uVar3 = (uint)(param_3 >> 0x20);
  func_0x07471e50(lVar10 + 2 + uVar6 * 2,param_2,(long)((int)param_3 << (uVar3 & 0x1f)));
  uVar8 = (int)uVar5 << 0x10 | uVar8;
  *puStack_40 = uVar9 | uVar8;
  iVar4 = *(int *)(puVar1 + 8);
  *(int *)(puVar1 + 8) = iVar4 + 1;
  if (*(int *)(puVar1 + 0xc) * 9 + 9U < (uint)((iVar4 * 2 + 2) * 5)) {
    func_0x00243850(puVar1,*(int *)(puVar1 + 0xc) * 2 + 2);
  }
  LOCK();
  *(int *)(puVar1 + 0x20) = *(int *)(puVar1 + 0x20) + 1;
  UNLOCK();
  LOCK();
  *(uint *)(puVar1 + 0x24) = *(int *)(puVar1 + 0x24) + (uVar3 & 0xff);
  UNLOCK();
LAB_0023c2f8:
  func_0x074728d0(puVar1);
  return uVar8;
LAB_0023c311:
  uVar8 = *(uint *)(lVar10 + uVar5 * 4);
  if (uVar8 != 0) {
    uVar8 = uVar8 & 0x1fffffff;
    goto LAB_0023c2f8;
  }
  puStack_40 = (uint *)(lVar10 + uVar5 * 4);
  goto LAB_0023c249;
}



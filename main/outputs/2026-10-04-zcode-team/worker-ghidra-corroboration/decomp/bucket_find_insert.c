// bucket_find_insert @ Ghidra 0023c340 (file vaddr 0x23c340, mapping delta 0x0)

uint CORROB_bucket_find_insert(long param_1,undefined8 *param_2)

{
  ushort uVar1;
  uint uVar2;
  long lVar3;
  int iVar4;
  ulong uVar5;
  long lVar6;
  uint uVar7;
  
  func_0x07472960();
  uVar7 = *(uint *)(param_1 + 0xc);
  lVar6 = *(long *)(param_1 + 0x10);
  uVar5 = (ulong)(*(uint *)((long)param_2 + 0x14) & uVar7);
  uVar2 = *(uint *)(lVar6 + uVar5 * 4);
  do {
    if (uVar2 == 0) {
      uVar7 = 0;
LAB_0023c3ef:
      func_0x074728d0(param_1);
      return uVar7;
    }
    if ((uVar2 & 0xe0000000) == *(uint *)(param_2 + 3)) {
      lVar3 = *(long *)(*(long *)(param_1 + 0x18) + 0x10 + (ulong)(uVar2 >> 0x10 & 0x1fff) * 8);
      uVar1 = *(ushort *)(lVar3 + (ulong)(uVar2 & 0xffff) * 2);
      if (uVar1 == *(ushort *)((long)param_2 + 0x1c)) {
        if ((*(byte *)((long)param_2 + 0xc) & 1) == 0) {
          iVar4 = func_0x0008cdb0();
        }
        else {
          iVar4 = func_0x0008ce20(lVar3 + (ulong)(uVar2 & 0xffff) * 2 + 2,*param_2,uVar1 >> 6);
        }
        if (iVar4 == 0) {
          uVar7 = *(uint *)(lVar6 + uVar5 * 4) & 0x1fffffff;
          goto LAB_0023c3ef;
        }
        lVar6 = *(long *)(param_1 + 0x10);
      }
    }
    uVar5 = (ulong)((int)uVar5 + 1U & uVar7);
    uVar2 = *(uint *)(lVar6 + uVar5 * 4);
  } while( true );
}



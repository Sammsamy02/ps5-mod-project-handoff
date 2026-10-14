// fname_tostring @ Ghidra 0023f640 (file vaddr 0x23f640, mapping delta 0x0)

void CORROB_fname_tostring(uint *param_1,long *param_2)

{
  ushort *puVar1;
  long lVar2;
  long lVar3;
  undefined1 auVar4 [16];
  ushort uVar5;
  long lVar6;
  undefined1 auVar7 [32];
  undefined1 auVar8 [32];
  undefined1 auVar9 [32];
  undefined1 auVar10 [32];
  ushort uVar11;
  uint uVar12;
  ulong uVar13;
  undefined2 *puVar14;
  ulong uVar15;
  ulong uVar16;
  long lVar17;
  long lVar18;
  long lVar19;
  
  uVar12 = *param_1;
  if (DAT_09365ea0 == '\0') {
    CORROB_pool_init(0x9365ec0);
    DAT_09365ea0 = '\x01';
  }
  lVar6 = *(long *)((ulong)(uVar12 >> 0x10) * 8 + 0x9365ed0);
  uVar16 = (ulong)(uVar12 & 0xffff);
  puVar1 = (ushort *)(lVar6 + uVar16 * 2);
  lVar19 = param_2[1];
  uVar16 = (ulong)(*(ushort *)(lVar6 + uVar16 * 2) >> 6);
  if ((ulong)param_2[2] <= lVar19 + uVar16 * 2) {
    func_0x00204700(param_2,uVar16);
    lVar19 = param_2[1];
  }
  lVar6 = *param_2;
  param_2[1] = lVar19 + uVar16 * 2;
  uVar5 = *puVar1;
  lVar19 = (long)(int)((ulong)(lVar19 - lVar6) >> 1);
  uVar16 = lVar6 + lVar19 * 2;
  if ((uVar5 & 1) != 0) {
    func_0x07471e50(uVar16,puVar1 + 1,uVar5 >> 5 & 0xfffe);
    goto LAB_0023f73b;
  }
  func_0x07471e50(uVar16,puVar1 + 1,uVar5 >> 6);
  uVar5 = *puVar1;
  if (uVar5 < 0x40) goto LAB_0023f73b;
  uVar11 = uVar5 >> 6;
  uVar15 = (ulong)uVar11;
  if (0x1ff < uVar5) {
    lVar2 = lVar19 + uVar15;
    uVar13 = lVar6 + -2 + lVar2 * 2;
    if ((((uVar13 - (uint)uVar11 * 2) + 2 <= uVar13) && (uVar16 <= (uVar15 - 1) + uVar16)) &&
       ((uVar16 + uVar15 <= uVar16 || ((ulong)(lVar6 + lVar2 * 2) <= uVar16)))) {
      lVar19 = lVar19 * 2;
      uVar12 = (uint)uVar11;
      if (uVar5 < 0x1000) {
        uVar13 = 0;
      }
      else {
        lVar18 = 0;
        lVar17 = lVar6 + -0x10 + uVar15 + lVar19;
        lVar3 = lVar6 + -0x20 + (ulong)((uint)uVar11 * 2) + lVar19;
        uVar13 = (ulong)(uVar12 & 0xffffffc0);
        do {
          auVar7 = vpmovsxbw_avx2(*(undefined1 (*) [16])(lVar17 + lVar18));
          auVar8 = vpmovsxbw_avx2(*(undefined1 (*) [16])(lVar17 + -0x10 + lVar18));
          auVar9 = vpmovsxbw_avx2(*(undefined1 (*) [16])(lVar17 + -0x20 + lVar18));
          auVar10 = vpmovsxbw_avx2(*(undefined1 (*) [16])(lVar17 + -0x30 + lVar18));
          *(undefined1 (*) [32])(lVar3 + lVar18 * 2) = auVar7;
          *(undefined1 (*) [32])(lVar3 + -0x20 + lVar18 * 2) = auVar8;
          *(undefined1 (*) [32])(lVar3 + -0x40 + lVar18 * 2) = auVar9;
          *(undefined1 (*) [32])(lVar3 + -0x60 + lVar18 * 2) = auVar10;
          lVar18 = lVar18 + -0x40;
        } while (-lVar18 != (ulong)(uVar5 >> 0xc) * 0x40);
        if ((uVar12 & 0xffffffc0) == uVar12) goto LAB_0023f73b;
        if ((uVar11 & 0x38) == 0) {
          uVar15 = (ulong)(uVar12 & 0x3f);
          goto LAB_0023f710;
        }
      }
      lVar19 = lVar19 + uVar15;
      lVar17 = -uVar13;
      uVar15 = (ulong)(uVar12 & 7);
      do {
        auVar4._8_8_ = 0;
        auVar4._0_8_ = *(ulong *)(lVar6 + -8 + lVar19 + lVar17);
        auVar4 = vpmovsxbw_avx(auVar4);
        *(undefined1 (*) [16])(lVar2 * 2 + -0x10 + lVar6 + lVar17 * 2) = auVar4;
        lVar17 = lVar17 + -8;
      } while (-lVar17 != (ulong)(uVar5 >> 9) * 8);
      if ((uVar12 & 0xfffffff8) == uVar12) goto LAB_0023f73b;
    }
  }
LAB_0023f710:
  do {
    uVar13 = uVar15 - 1;
    *(short *)((uVar16 - 2) + uVar15 * 2) = (short)*(char *)((uVar16 - 1) + uVar15);
    uVar15 = uVar13;
  } while (uVar13 != 0);
LAB_0023f73b:
  uVar12 = param_1[1];
  if (uVar12 != 0) {
    puVar14 = (undefined2 *)param_2[1];
    if ((undefined2 *)param_2[2] <= puVar14 + 1) {
      func_0x00204700(param_2,1);
      puVar14 = (undefined2 *)param_2[1];
    }
    param_2[1] = (long)(puVar14 + 1);
    *puVar14 = 0x5f;
    func_0x00204ce0(param_2,&UNK_0791fe36,uVar12 - 1);
  }
  return;
}



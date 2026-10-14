// find_or_store @ Ghidra 0023d600 (file vaddr 0x23d600, mapping delta 0x0)

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

ulong CORROB_find_or_store(short *param_1,ulong param_2)

{
  code *pcVar1;
  undefined1 auVar2 [16];
  undefined1 auVar3 [16];
  undefined1 auVar4 [16];
  undefined1 auVar5 [32];
  long lVar6;
  uint uVar7;
  short *psVar8;
  ulong uVar9;
  uint uVar10;
  uint uVar11;
  ulong uVar12;
  ulong uVar13;
  ulong uVar14;
  long lVar15;
  long lVar16;
  ulong uVar17;
  undefined1 auVar18 [32];
  short asStack_428 [8];
  undefined1 auStack_418 [16];
  undefined1 auStack_408 [16];
  undefined1 auStack_3f8 [984];
  long lStack_20;
  
  lStack_20 = _f7uOxY9mM1U_5_J;
  uVar11 = (uint)param_2;
  if (uVar11 == 0) {
    uVar9 = 0;
    goto LAB_0023d848;
  }
  psVar8 = param_1 + (long)(int)uVar11 + -1;
  if (psVar8 < param_1) {
    uVar9 = param_2 & 0xffffffff;
    uVar17 = 0;
joined_r0x0023d824:
    if ((param_2 >> 0x20 & 1) == 0) {
      uVar11 = (uint)uVar9;
      if (0 < (int)uVar11) {
        uVar13 = 0x400;
        if (uVar11 < 0x400) {
          uVar13 = uVar9;
        }
        if (uVar11 < 8) {
          uVar12 = 0;
        }
        else {
          uVar10 = (uint)uVar13;
          if (uVar11 < 0x40) {
            uVar14 = 0;
          }
          else {
            auVar18._8_2_ = 0xff;
            auVar18._0_8_ = 0xff00ff00ff00ff;
            auVar18._10_2_ = 0xff;
            auVar18._12_2_ = 0xff;
            auVar18._14_2_ = 0xff;
            auVar18._16_2_ = 0xff;
            auVar18._18_2_ = 0xff;
            auVar18._20_2_ = 0xff;
            auVar18._22_2_ = 0xff;
            auVar18._24_2_ = 0xff;
            auVar18._26_2_ = 0xff;
            auVar18._28_2_ = 0xff;
            auVar18._30_2_ = 0xff;
            uVar14 = 0;
            uVar12 = (ulong)(uVar10 & 0x7c0);
            do {
              auVar5 = vpand_avx2(auVar18,*(undefined1 (*) [32])(param_1 + uVar14));
              auVar4 = vpackuswb_avx(auVar5._0_16_,auVar5._16_16_);
              auVar5 = vpand_avx2(auVar18,*(undefined1 (*) [32])(param_1 + uVar14 + 0x10));
              auVar2 = vpackuswb_avx(auVar5._0_16_,auVar5._16_16_);
              auVar5 = vpand_avx2(auVar18,*(undefined1 (*) [32])(param_1 + uVar14 + 0x20));
              auVar3 = vpackuswb_avx(auVar5._0_16_,auVar5._16_16_);
              auVar5 = vpand_avx2(auVar18,*(undefined1 (*) [32])(param_1 + uVar14 + 0x30));
              *(undefined1 (*) [16])((long)asStack_428 + uVar14) = auVar4;
              *(undefined1 (*) [16])(auStack_418 + uVar14) = auVar2;
              *(undefined1 (*) [16])(auStack_408 + uVar14) = auVar3;
              auVar4 = vpackuswb_avx(auVar5._0_16_,auVar5._16_16_);
              *(undefined1 (*) [16])(auStack_3f8 + uVar14) = auVar4;
              uVar14 = uVar14 + 0x40;
            } while (uVar12 != uVar14);
            if ((uVar10 & 0x7c0) == uVar10) goto LAB_0023d805;
            uVar14 = uVar12;
            if ((uVar13 & 0x38) == 0) goto LAB_0023d7f0;
          }
          uVar12 = (ulong)(uVar10 & 0x7f8);
          do {
            auVar4 = vpshufb_avx(*(undefined1 (*) [16])(param_1 + uVar14),ZEXT816(0xe0c0a0806040200)
                                );
            *(long *)((long)asStack_428 + uVar14) = auVar4._0_8_;
            uVar14 = uVar14 + 8;
          } while (uVar12 != uVar14);
          if ((uVar10 & 0x7f8) == uVar10) goto LAB_0023d805;
        }
LAB_0023d7f0:
        do {
          *(char *)((long)asStack_428 + uVar12) = (char)param_1[uVar12];
          uVar12 = uVar12 + 1;
        } while (uVar13 != uVar12);
      }
LAB_0023d805:
      param_1 = asStack_428;
    }
    else {
      uVar9 = uVar9 | 0x100000000;
    }
    uVar7 = CORROB_pool_core(param_1,uVar9);
  }
  else {
    uVar10 = 0;
    do {
      if (9 < (ushort)(*psVar8 - 0x30U)) break;
      uVar10 = uVar10 + 1;
      psVar8 = psVar8 + -1;
    } while (param_1 <= psVar8);
    uVar7 = 0;
    if ((uVar10 == 0 || (int)uVar11 <= (int)uVar10) || (10 < uVar10)) {
joined_r0x0023d81b:
      uVar17 = 0;
      uVar9 = param_2 & 0xffffffff;
    }
    else {
      uVar9 = (ulong)uVar10;
      lVar16 = -uVar9;
      if (param_1[(long)(int)uVar11 + (-1 - uVar9)] != 0x5f) goto joined_r0x0023d81b;
      if (uVar10 != 1) {
        if (param_1[(long)(int)uVar11 - uVar9] != 0x30) {
          lVar15 = 0;
          do {
            lVar6 = (int)uVar11 + lVar16;
            lVar16 = lVar16 + 1;
            lVar15 = ((ulong)(ushort)param_1[lVar6] - 0x30) + lVar15 * 10;
          } while (lVar16 != 0);
          if (lVar15 < 0x7fffffff) goto LAB_0023d6df;
        }
        goto joined_r0x0023d81b;
      }
      lVar15 = (ulong)(ushort)param_1[(long)(int)uVar11 - uVar9] - 0x30;
LAB_0023d6df:
      uVar11 = ~uVar10 + uVar11;
      uVar9 = (ulong)uVar11;
      uVar17 = (ulong)((int)lVar15 + 1);
    }
    if (uVar11 != 0) goto joined_r0x0023d824;
    uVar17 = 0;
  }
  uVar9 = (ulong)uVar7 | uVar17 << 0x20;
LAB_0023d848:
  if (_f7uOxY9mM1U_5_J == lStack_20) {
    return uVar9;
  }
  func_0x07471e60();
                    /* WARNING: Does not return */
  pcVar1 = (code *)invalidInstructionException();
  (*pcVar1)();
}



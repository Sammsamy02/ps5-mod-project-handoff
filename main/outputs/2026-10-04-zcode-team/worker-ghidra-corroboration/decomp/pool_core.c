// pool_core @ Ghidra 00242470 (file vaddr 0x242470, mapping delta 0x0)

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

ulong CORROB_pool_core(undefined8 param_1,ulong param_2,int param_3)

{
  code *pcVar1;
  undefined1 auVar2 [14];
  ulong uVar3;
  ulong uVar4;
  undefined1 auVar5 [16];
  undefined8 uStack_50;
  ulong uStack_48;
  ulong uStack_40;
  undefined4 uStack_38;
  undefined2 uStack_34;
  long lStack_30;
  
  lStack_30 = _f7uOxY9mM1U_5_J;
  if ((param_2 & 0xfffffc00) == 0) {
    if (DAT_09365ea0 == '\0') {
      CORROB_pool_init(0x9365ec0);
      DAT_09365ea0 = '\x01';
    }
    if (param_3 == 1) {
      uVar3 = CORROB_store_name(0x9365ec0,param_1,param_2);
    }
    else {
      uStack_50 = param_1;
      uStack_48 = param_2;
      if ((param_2 >> 0x20 & 1) == 0) {
        auVar5 = func_0x00239b00();
        auVar2 = auVar5._0_14_;
      }
      else {
        auVar5 = func_0x0023a2c0(param_1,param_2 & 0xffffffff);
        auVar2 = auVar5._0_14_;
      }
      uStack_40 = auVar2._0_8_;
      uStack_38 = auVar2._8_4_;
      uStack_34 = auVar2._12_2_;
      uVar3 = CORROB_bucket_find_insert((uStack_40 & 0xffffffff) * 0x40 + 0x9375f00,&uStack_50);
    }
    uVar4 = uVar3 & 0xffffffff;
  }
  else if (param_3 == 0) {
    uVar3 = 0;
    uVar4 = 0;
  }
  else {
    uVar3 = CORROB_pool_core(&UNK_07acb255,0x18,1);
    uVar4 = uVar3 >> 0x20;
  }
  if (_f7uOxY9mM1U_5_J == lStack_30) {
    return uVar3 & 0xffffffff | uVar4 << 0x20;
  }
  func_0x07471e60();
                    /* WARNING: Does not return */
  pcVar1 = (code *)invalidInstructionException();
  (*pcVar1)();
}



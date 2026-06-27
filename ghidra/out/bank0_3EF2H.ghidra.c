/* Ghidra decompiler output for 3EF2H.bin
 * Language: 8048:LE:16:default
 * Image base: CODE:0000
 * Functions: 35
 */


void RESET(void)

{
  nop();
  FUN_CODE_0005();
  return;
}



undefined1 EXTIRQ(undefined1 param_1)

{
  byte bVar1;
  byte bVar2;
  undefined1 uVar3;
  undefined *puVar4;
  char cVar5;
  undefined1 uVar6;
  
  setBank(1);
  DAT_EXTMEM_21 = P2;
  DAT_EXTMEM_1f = param_1;
  do {
    while( true ) {
      while( true ) {
        while( true ) {
          P2 = 0xaf;
          bVar1 = P6;
          bVar2 = P7;
          if ((bool)((bVar2 & 4) >> 2)) {
            do {
              bVar1 = P2;
              P2 = bVar1 & 0x7f;
            } while( true );
          }
          if (!(bool)(bVar2 & 1)) break;
          FUN_CODE_068f();
          FUN_CODE_068b();
          DAT_EXTMEM_17 = 0;
        }
        if (!(bool)((bVar2 & 2) >> 1)) break;
        FUN_CODE_068f();
        FUN_CODE_068b();
      }
      if ((bool)((bVar2 & 8) >> 3)) break;
      if ((bool)((bVar1 & 0xf) >> 1 & 1)) {
LAB_CODE_054f:
        cVar5 = '\0';
        do {
          cVar5 = cVar5 + -1;
        } while (cVar5 != '\0');
        uVar3 = 0x1d;
        DAT_EXTMEM_1d = 0;
        FUN_CODE_068f();
        bVar1 = P1;
        P1 = bVar1 | 0x20;
        P2 = 0xef;
        bVar1 = P5;
        P5 = bVar1 | 4;
        FUN_CODE_0367();
        FUN_CODE_0637(uVar3);
        bVar1 = P1;
        P1 = bVar1 & 0xdf;
      }
      else if ((bool)((bVar1 & 0xf) >> 2 & 1)) {
        FUN_CODE_068f();
        DAT_EXTMEM_16 = 0x1e;
        P2 = 0xef;
        bVar1 = P5;
        P5 = bVar1 | 1;
      }
      else {
        if (!(bool)((bVar1 & 0xf) >> 3)) {
          P2 = DAT_EXTMEM_21;
          setBank(0);
          return DAT_EXTMEM_1f;
        }
        FUN_CODE_068f();
        bVar1 = P1;
        P1 = bVar1 | 0x20;
        FUN_CODE_0635();
        FUN_CODE_0684();
        DAT_EXTMEM_1a = 1;
      }
    }
    DAT_EXTMEM_1d = 0;
    DAT_EXTMEM_1c = 0;
    FUN_CODE_068f();
    bVar1 = P1;
    P1 = bVar1 | 0x20;
    FUN_CODE_0684();
    puVar4 = &BANK1_R2;
    if ((bool)(DAT_EXTMEM_1a & 1)) {
      uVar3 = 2;
LAB_CODE_0601:
      uVar6 = 0;
      FUN_CODE_0678();
    }
    else {
      if ((bool)(DAT_EXTMEM_1a >> 1 & 1)) {
        uVar3 = 4;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 2 & 1)) {
        uVar3 = 8;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 3 & 1)) {
        uVar3 = 0x10;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 4 & 1)) {
        uVar3 = 0x60;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 5 & 1)) {
        if ((bool)(DAT_EXTMEM_1a >> 6 & 1)) {
          uVar3 = 0x20;
          goto LAB_CODE_0601;
        }
        uVar3 = 0xc0;
LAB_CODE_0610:
        uVar6 = 1;
        FUN_CODE_0678();
      }
      else {
        if ((bool)(DAT_EXTMEM_1a >> 6 & 1)) {
          if ((bool)(DAT_EXTMEM_1a >> 7)) {
            uVar3 = 0x40;
            goto LAB_CODE_0610;
          }
          uVar3 = 0x80;
        }
        else {
          if (!(bool)(DAT_EXTMEM_1a >> 7)) {
            DAT_EXTMEM_1a = 1;
            goto LAB_CODE_054f;
          }
          uVar3 = 0;
        }
        uVar6 = 1;
        FUN_CODE_0678();
      }
    }
    *puVar4 = uVar3;
    if ((bool)uVar6) {
      FUN_CODE_06bd();
    }
    else {
      FUN_CODE_069c();
    }
    FUN_CODE_06c7();
    FUN_CODE_0665();
    FUN_CODE_067e();
    bVar1 = P1;
    P1 = bVar1 & 0xdf;
  } while( true );
}



void FUN_CODE_0005(void)

{
  bool bVar1;
  
  bVar1 = (bool)getT0();
  if (bVar1) {
    FUN_CODE_002f();
  }
  FUN_CODE_000d();
  FUN_CODE_006f();
  return;
}



void TIMIRQ(void)

{
  FUN_CODE_002f();
  FUN_CODE_000d();
  FUN_CODE_006f();
  return;
}



byte FUN_CODE_000d(void)

{
  byte bVar1;
  
  P2 = 0xff;
  bVar1 = P7;
  P7 = bVar1 | 0xf;
  bVar1 = P6;
  P6 = bVar1 | 0xf;
  bVar1 = P5;
  P5 = bVar1 | 0xf;
  P7 = 0;
  P6 = 0;
  FUN_CODE_0029(0x8f);
  FUN_CODE_0029(0x9f);
  FUN_CODE_0029(0xaf);
  P2 = 0xff;
  bVar1 = P4;
  return bVar1 & 0xf;
}



byte FUN_CODE_0029(undefined1 param_1)

{
  undefined1 uVar1;
  byte bVar2;
  
  P2 = param_1;
  uVar1 = P4;
  uVar1 = P5;
  uVar1 = P6;
  bVar2 = P7;
  return bVar2 & 0xf;
}



void FUN_CODE_002f(void)

{
  byte bVar1;
  undefined1 *puVar2;
  char cVar3;
  
  puVar2 = (undefined1 *)0xff;
  cVar3 = -1;
  do {
    *puVar2 = 0;
    puVar2 = puVar2 + -1;
    cVar3 = cVar3 + -1;
  } while (cVar3 != '\0');
  DAT_EXTMEM_15 = 0xfe;
  DAT_EXTMEM_f8 = 0xff;
  bVar1 = P1;
  P1 = bVar1 | 0x20;
  FUN_CODE_0067();
  FUN_CODE_0067();
  FUN_CODE_0067();
  FUN_CODE_0067();
  FUN_CODE_0067();
  FUN_CODE_0067();
  DAT_EXTMEM_6d = 0x60;
  DAT_EXTMEM_f4 = 0x20;
  return;
}



void FUN_CODE_0067(void)

{
  undefined1 *in_R0;
  
  *in_R0 = 0x80;
  in_R0['\x01'] = 1;
  return;
}



void FUN_CODE_006f(void)

{
  byte bVar1;
  undefined1 in_F1;
  
  FUN_CODE_0635();
  DAT_EXTMEM_1a = 1;
  DAT_EXTMEM_1d = '\a';
  DAT_EXTMEM_1e = '\x03';
  DAT_EXTMEM_1c = 0;
  do {
    if (DAT_EXTMEM_1c == 0) {
      FUN_CODE_0117();
      FUN_CODE_014b();
      FUN_CODE_06e5();
    }
    else {
      if ((bool)(DAT_EXTMEM_1c & 1)) {
        DAT_EXTMEM_1b = 0x38;
      }
      else if ((bool)(DAT_EXTMEM_1c >> 1 & 1)) {
        DAT_EXTMEM_1b = 0x3d;
      }
      else if ((bool)(DAT_EXTMEM_1c >> 2 & 1)) {
        DAT_EXTMEM_1b = 0x42;
      }
      else if ((bool)(DAT_EXTMEM_1c >> 3 & 1)) {
        DAT_EXTMEM_1b = 0x47;
      }
      else if ((bool)(DAT_EXTMEM_1c >> 4 & 1)) {
        DAT_EXTMEM_1b = 0x4c;
      }
      else {
        if (!(bool)(DAT_EXTMEM_1c >> 5 & 1)) {
LAB_CODE_0110:
          FUN_CODE_0117();
          FUN_CODE_0224();
          return;
        }
        DAT_EXTMEM_1b = 0x51;
      }
      FUN_CODE_0117();
      while( true ) {
        FUN_CODE_014b();
        if ((bool)in_F1) goto LAB_CODE_0110;
        FUN_CODE_0175();
        FUN_CODE_0121();
        if (DAT_EXTMEM_1e == '\0') break;
        P2 = 0xff;
        bVar1 = P5;
        P5 = bVar1 | 1;
      }
      DAT_EXTMEM_1e = '\x03';
      if (!(bool)in_F1) {
        FUN_CODE_06e5(DAT_EXTMEM_1b);
      }
    }
    in_F1 = 0;
    if ((DAT_EXTMEM_1d == '\0') || (DAT_EXTMEM_1d = DAT_EXTMEM_1d + -1, DAT_EXTMEM_1d == '\0'))
    goto LAB_CODE_0110;
  } while( true );
}



void FUN_CODE_0117(void)

{
  byte in_R2;
  undefined1 in_R3;
  
  P2 = 0xff;
  P5 = in_R2 & 0xf;
  DAT_EXTMEM_1c = in_R3;
  return;
}



void FUN_CODE_0121(void)

{
  if ((((((bool)(bEXTMEM02 >> 7)) || ((bool)(bEXTMEM02 >> 6 & 1))) || ((bool)(bEXTMEM02 >> 5 & 1)))
      || (((bool)(bEXTMEM02 >> 4 & 1) || ((bool)(bEXTMEM02 >> 3 & 1))))) ||
     (((bool)(bEXTMEM02 >> 2 & 1) ||
      ((((bool)(bEXTMEM02 >> 1 & 1) || ((bool)(bEXTMEM02 & 1))) && ((bool)(bEXTMEM01 >> 7))))))) {
    DAT_EXTMEM_1e = DAT_EXTMEM_1e + -1;
  }
  else {
    DAT_EXTMEM_1e = '\0';
  }
  return;
}



void FUN_CODE_014b(void)

{
  byte bVar1;
  undefined1 in_F1;
  
  enableExtInt();
  disableExtInt();
  FUN_CODE_01eb();
  if (!(bool)in_F1) {
    enableExtInt();
    disableExtInt();
    FUN_CODE_06e5();
    P2 = 0xff;
    bVar1 = P5;
    P5 = bVar1 & 0xe;
    FUN_CODE_01eb();
    if (!(bool)in_F1) {
      func_0x099c();
      FUN_CODE_06e5();
    }
  }
  enableExtInt();
  disableExtInt();
  return;
}



void FUN_CODE_0175(void)

{
  func_0x099c();
  func_0x0800();
  DAT_EXTMEM_0a = 0;
  DAT_EXTMEM_09 = 0x98;
  DAT_EXTMEM_08 = 0x58;
  DAT_EXTMEM_07 = 2;
  DAT_EXTMEM_06 = 0;
  func_0x0916();
  FUN_CODE_06e5();
  uEXTMEM00 = uEXTMEM01;
  uEXTMEM01 = 0;
  func_0x0916();
  FUN_CODE_06d5();
  DAT_EXTMEM_0a = 0x25;
  func_0x0916();
  FUN_CODE_06e5();
  FUN_CODE_06d5();
  DAT_EXTMEM_0a = 0x75;
  func_0x0916();
  func_0x099f();
  return;
}



/* WARNING: Removing unreachable block (CODE,0x0200) */
/* WARNING: Removing unreachable block (CODE,0x0203) */
/* WARNING: Removing unreachable block (CODE,0x0205) */
/* WARNING: Removing unreachable block (CODE,0x0208) */
/* WARNING: Removing unreachable block (CODE,0x0210) */
/* WARNING: Removing unreachable block (CODE,0x020e) */
/* WARNING: Removing unreachable block (CODE,0x0217) */
/* WARNING: Removing unreachable block (CODE,0x0219) */
/* WARNING: Removing unreachable block (CODE,0x0214) */

void FUN_CODE_01eb(void)

{
  byte bVar1;
  char cVar2;
  char cVar3;
  char in_R5;
  byte in_R6;
  undefined1 in_F1;
  
  if (DAT_EXTMEM_1d == '\0') {
    return;
  }
  bVar1 = P1;
  P1 = bVar1 | 0x20;
  setBank(1);
  setTmr(0);
  cVar2 = '\0';
  do {
    cVar2 = cVar2 + -1;
  } while (cVar2 != '\0');
  cVar3 = -1;
  bVar1 = P7;
  bVar1 = bVar1 & 0xf;
  nop();
  nop();
  nop();
  nop();
  cVar2 = cVar3;
  if (in_R6 != 0) {
    if ((bool)(in_R6 & 1)) {
      FUN_CODE_0367();
      FUN_CODE_037c();
      cVar2 = cVar3;
      if ((bool)in_F1) {
        FUN_CODE_0358();
        FUN_CODE_03b4();
        cVar2 = cVar3;
        if ((bool)in_F1) {
          for (; in_R5 != '\0'; in_R5 = in_R5 + -1) {
            cVar2 = cVar2 + -1;
          }
        }
      }
    }
    else {
      DAT_EXTMEM_28 = '\f';
      cVar2 = DAT_EXTMEM_28;
    }
  }
  for (; DAT_EXTMEM_28 = cVar2, DAT_EXTMEM_29 = cVar3, bVar1 != 0; bVar1 = bVar1 - 1) {
    cVar3 = DAT_EXTMEM_29 + '\x01';
    cVar2 = DAT_EXTMEM_28;
  }
  func_0x0bc4();
  FUN_CODE_06e5();
  func_0x0bc4(DAT_EXTMEM_28);
  FUN_CODE_06e5();
  FUN_CODE_0402();
  return;
}



void FUN_CODE_0224(void)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  byte bVar4;
  byte bVar5;
  byte *pbVar6;
  undefined1 in_F1;
  
  if (DAT_EXTMEM_16 == '\0') {
    P2 = 0xef;
    bVar2 = P5;
    P5 = bVar2 & 0xe;
    P2 = 0xaf;
    bVar3 = P5;
    bVar2 = P4;
    if ((((((bool)(bVar2 & 1) == false && (bVar3 & 0xf) == 0) || ((bool)(bVar3 & 1))) ||
         ((bool)((bVar3 & 2) >> 1))) || (((bool)((bVar3 & 4) >> 2) || ((bool)((bVar3 & 8) >> 3)))))
       || ((bool)(bVar2 & 1))) {
      FUN_CODE_06d5();
      FUN_CODE_06e5();
      P2 = 0xff;
      bVar2 = P4;
      bVar3 = bVar2 & 0xf;
      if ((((bool)(bVar2 & 1)) || ((bool)(bVar3 >> 1 & 1))) ||
         (((bool)(bVar3 >> 2 & 1) || ((bool)(bVar3 >> 3))))) {
        FUN_CODE_06d5();
        FUN_CODE_06e5();
        P2 = 0x9f;
        bVar2 = P4;
        bVar2 = bVar2 & 0xf;
        bVar3 = P5;
        bVar4 = bVar3 & 0xf | 0x10;
        bVar3 = P6;
        bVar5 = P7;
        bVar5 = bVar5 & 0xf;
        nop();
        nop();
        nop();
        nop();
        bVar1 = bVar4;
        if ((bVar3 & 3) != 0) {
          if ((bool)(bVar3 & 1)) {
            FUN_CODE_0367();
            FUN_CODE_037c();
            bVar1 = bVar4;
            if ((bool)in_F1) {
              FUN_CODE_0358();
              FUN_CODE_03b4();
              bVar1 = bVar4;
              if ((bool)in_F1) {
                for (; bVar2 != 0; bVar2 = bVar2 - 1) {
                  bVar1 = bVar1 - 1;
                }
              }
            }
          }
          else {
            DAT_EXTMEM_28 = 0xc;
            bVar1 = DAT_EXTMEM_28;
          }
        }
        for (; DAT_EXTMEM_28 = bVar1, DAT_EXTMEM_29 = bVar4, bVar5 != 0; bVar5 = bVar5 - 1) {
          bVar4 = DAT_EXTMEM_29 + 1;
          bVar1 = DAT_EXTMEM_28;
        }
        func_0x0bc4();
        FUN_CODE_06e5();
        func_0x0bc4(DAT_EXTMEM_28);
        FUN_CODE_06e5();
      }
    }
    FUN_CODE_0402();
    return;
  }
  P2 = 0xef;
  func_0x099c(0xef);
  FUN_CODE_06d5();
  func_0x099c();
  if ((bool)in_F1) {
    DAT_EXTMEM_16 = DAT_EXTMEM_16 + -1;
    if (DAT_EXTMEM_16 != '\0') {
      FUN_CODE_06d5();
      func_0x099c();
      if (!(bool)in_F1) {
        pbVar6 = &DAT_EXTMEM_15;
        P7 = DAT_EXTMEM_15 & 0xf | 8;
        bVar2 = func_0x0cff();
        *pbVar6 = bVar2 & 0xfb;
        P7 = bVar2 & 0xb;
        P2 = 0xef;
        bVar2 = P5;
        P5 = bVar2 | 1;
        goto LAB_CODE_0281;
      }
    }
    P7 = (DAT_EXTMEM_15 | 0xfc) & 0xf;
  }
  bVar2 = P5;
  P5 = bVar2 & 0xe;
LAB_CODE_0281:
  P2 = 0xdf;
  P6 = 0;
  enableExtInt();
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}



byte FUN_CODE_0358(void)

{
  byte bVar1;
  byte bVar2;
  
  bVar1 = P1;
  P1 = bVar1 | 0xc0;
  bVar1 = P1;
  bVar2 = P1;
  P1 = bVar2 & 0xbf;
  bVar2 = P1;
  return bVar2 & 0xf | (bVar1 & 3) << 4;
}



char FUN_CODE_0367(void)

{
  byte bVar1;
  byte bVar2;
  
  bVar1 = P1;
  P1 = bVar1 & 0x3f;
  bVar1 = P1;
  P1 = bVar1 | 0x40;
  bVar1 = P1;
  bVar2 = P1;
  P1 = bVar2 | 0x80;
  bVar2 = P1;
  return (bVar2 >> 1 & 6 | bVar1 >> 3 & 1) + 1;
}



void FUN_CODE_037c(void)

{
  undefined1 uVar1;
  char in_R0;
  
  P2 = 0x8f;
  uVar1 = P6;
  uVar1 = P7;
  do {
    in_R0 = in_R0 + -1;
  } while (in_R0 != '\0');
  return;
}



void FUN_CODE_03b4(void)

{
  undefined1 uVar1;
  
  P2 = 0x8f;
  uVar1 = P4;
  uVar1 = P5;
  return;
}



/* WARNING: Control flow encountered bad instruction data */

void FUN_CODE_0402(void)

{
  undefined1 in_F1;
  undefined1 uVar1;
  
  if ((bool)(DAT_EXTMEM_34 >> 7)) goto LAB_CODE_04f6;
  FUN_CODE_06d5();
  func_0x099c();
  if ((bool)in_F1) goto LAB_CODE_04f6;
  func_0x099c();
  if ((bool)in_F1) {
    func_0x099c();
    FUN_CODE_06d5();
    func_0x099c();
    uVar1 = 0;
    if (!(bool)in_F1) goto LAB_CODE_0461;
LAB_CODE_043d:
    P2 = 0xef;
    P7 = DAT_EXTMEM_15 & 0xd;
    DAT_EXTMEM_15 = DAT_EXTMEM_15 & 0xfd;
    goto LAB_CODE_04b3;
  }
  func_0x099c();
  FUN_CODE_06d5();
  func_0x099c();
  uVar1 = 1;
  if (!(bool)in_F1) goto LAB_CODE_043d;
LAB_CODE_0461:
  P2 = 0xef;
  P7 = DAT_EXTMEM_15 & 0xf | 2;
  DAT_EXTMEM_15 = DAT_EXTMEM_15 | 2;
  func_0x099c();
  if ((bool)(DAT_EXTMEM_f7 & 1)) {
    FUN_CODE_06d5();
    func_0x099f();
    if ((bool)uVar1) goto LAB_CODE_04ac;
  }
  else {
    FUN_CODE_06d5();
    func_0x099c();
    if ((bool)uVar1) {
LAB_CODE_04ac:
      uVar1 = 1;
      if (!(bool)(DAT_EXTMEM_f7 >> 4 & 1)) {
        func_0x099c();
        if ((bool)(DAT_EXTMEM_f7 & 1)) {
          FUN_CODE_06d5();
          func_0x099c();
        }
        else {
          FUN_CODE_06d5();
          func_0x099c();
        }
        if (!(bool)uVar1) goto LAB_CODE_049a;
      }
LAB_CODE_04b3:
      P2 = 0xef;
      P7 = DAT_EXTMEM_15 & 0xf | 1;
      DAT_EXTMEM_f7 = DAT_EXTMEM_f7 & 0xfe;
      DAT_EXTMEM_15 = DAT_EXTMEM_15 | 1;
      goto LAB_CODE_04f6;
    }
  }
LAB_CODE_049a:
  P2 = 0xef;
  P7 = DAT_EXTMEM_15 & 0xe;
  DAT_EXTMEM_f7 = DAT_EXTMEM_f7 | 1;
  DAT_EXTMEM_15 = DAT_EXTMEM_15 & 0xfe;
LAB_CODE_04f6:
  FUN_CODE_06e5();
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



undefined1 FUN_CODE_04ff(undefined1 param_1)

{
  byte bVar1;
  byte bVar2;
  undefined1 uVar3;
  undefined *puVar4;
  char cVar5;
  undefined1 uVar6;
  
  setBank(1);
  DAT_EXTMEM_21 = P2;
  DAT_EXTMEM_1f = param_1;
  do {
    while( true ) {
      while( true ) {
        while( true ) {
          P2 = 0xaf;
          bVar1 = P6;
          bVar2 = P7;
          if ((bool)((bVar2 & 4) >> 2)) {
            do {
              bVar1 = P2;
              P2 = bVar1 & 0x7f;
            } while( true );
          }
          if (!(bool)(bVar2 & 1)) break;
          FUN_CODE_068f();
          FUN_CODE_068b();
          DAT_EXTMEM_17 = 0;
        }
        if (!(bool)((bVar2 & 2) >> 1)) break;
        FUN_CODE_068f();
        FUN_CODE_068b();
      }
      if ((bool)((bVar2 & 8) >> 3)) break;
      if ((bool)((bVar1 & 0xf) >> 1 & 1)) {
LAB_CODE_054f:
        cVar5 = '\0';
        do {
          cVar5 = cVar5 + -1;
        } while (cVar5 != '\0');
        uVar3 = 0x1d;
        DAT_EXTMEM_1d = 0;
        FUN_CODE_068f();
        bVar1 = P1;
        P1 = bVar1 | 0x20;
        P2 = 0xef;
        bVar1 = P5;
        P5 = bVar1 | 4;
        FUN_CODE_0367();
        FUN_CODE_0637(uVar3);
        bVar1 = P1;
        P1 = bVar1 & 0xdf;
      }
      else if ((bool)((bVar1 & 0xf) >> 2 & 1)) {
        FUN_CODE_068f();
        DAT_EXTMEM_16 = 0x1e;
        P2 = 0xef;
        bVar1 = P5;
        P5 = bVar1 | 1;
      }
      else {
        if (!(bool)((bVar1 & 0xf) >> 3)) {
          P2 = DAT_EXTMEM_21;
          setBank(0);
          return DAT_EXTMEM_1f;
        }
        FUN_CODE_068f();
        bVar1 = P1;
        P1 = bVar1 | 0x20;
        FUN_CODE_0635();
        FUN_CODE_0684();
        DAT_EXTMEM_1a = 1;
      }
    }
    DAT_EXTMEM_1d = 0;
    DAT_EXTMEM_1c = 0;
    FUN_CODE_068f();
    bVar1 = P1;
    P1 = bVar1 | 0x20;
    FUN_CODE_0684();
    puVar4 = &BANK1_R2;
    if ((bool)(DAT_EXTMEM_1a & 1)) {
      uVar3 = 2;
LAB_CODE_0601:
      uVar6 = 0;
      FUN_CODE_0678();
    }
    else {
      if ((bool)(DAT_EXTMEM_1a >> 1 & 1)) {
        uVar3 = 4;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 2 & 1)) {
        uVar3 = 8;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 3 & 1)) {
        uVar3 = 0x10;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 4 & 1)) {
        uVar3 = 0x60;
        goto LAB_CODE_0601;
      }
      if ((bool)(DAT_EXTMEM_1a >> 5 & 1)) {
        if ((bool)(DAT_EXTMEM_1a >> 6 & 1)) {
          uVar3 = 0x20;
          goto LAB_CODE_0601;
        }
        uVar3 = 0xc0;
LAB_CODE_0610:
        uVar6 = 1;
        FUN_CODE_0678();
      }
      else {
        if ((bool)(DAT_EXTMEM_1a >> 6 & 1)) {
          if ((bool)(DAT_EXTMEM_1a >> 7)) {
            uVar3 = 0x40;
            goto LAB_CODE_0610;
          }
          uVar3 = 0x80;
        }
        else {
          if (!(bool)(DAT_EXTMEM_1a >> 7)) {
            DAT_EXTMEM_1a = 1;
            goto LAB_CODE_054f;
          }
          uVar3 = 0;
        }
        uVar6 = 1;
        FUN_CODE_0678();
      }
    }
    *puVar4 = uVar3;
    if ((bool)uVar6) {
      FUN_CODE_06bd();
    }
    else {
      FUN_CODE_069c();
    }
    FUN_CODE_06c7();
    FUN_CODE_0665();
    FUN_CODE_067e();
    bVar1 = P1;
    P1 = bVar1 & 0xdf;
  } while( true );
}



void FUN_CODE_0635(void)

{
  FUN_CODE_067e();
  FUN_CODE_0678();
  FUN_CODE_0645();
  FUN_CODE_0665();
  FUN_CODE_06c7();
  return;
}



void FUN_CODE_0637(void)

{
  FUN_CODE_067e();
  FUN_CODE_0678();
  FUN_CODE_0645();
  FUN_CODE_0665();
  FUN_CODE_06c7();
  return;
}



byte FUN_CODE_0645(void)

{
  byte bVar1;
  undefined1 uVar2;
  
  bVar1 = P1;
  P1 = bVar1 & 0x3f;
  uVar2 = P1;
  bVar1 = P1;
  P1 = bVar1 | 0x40;
  uVar2 = P1;
  bVar1 = P1;
  P1 = bVar1 & 0x3f;
  bVar1 = P1;
  P1 = bVar1 | 0x80;
  uVar2 = P1;
  bVar1 = P1;
  P1 = bVar1 | 0xc0;
  bVar1 = P1;
  return bVar1 & 3 | 0xf0;
}



void FUN_CODE_0665(void)

{
  byte in_R5;
  byte in_R6;
  byte in_R7;
  
  P2 = 0xcf;
  P5 = in_R7 & 0xf;
  P4 = in_R7 >> 4;
  P7 = in_R6 & 0xf;
  P6 = in_R6 >> 4;
  P2 = 0xdf;
  P5 = in_R5 & 0xf;
  P4 = in_R5 >> 4;
  return;
}



void FUN_CODE_0678(void)

{
  byte in_R4;
  
  P2 = 0xef;
  P4 = in_R4 & 0xf;
  return;
}



void FUN_CODE_067e(void)

{
  byte in_R3;
  
  P2 = 0xdf;
  P7 = in_R3 & 0xf;
  return;
}



void FUN_CODE_0684(void)

{
  byte bVar1;
  
  P2 = 0xef;
  bVar1 = P5;
  P5 = bVar1 & 0xb;
  return;
}



void FUN_CODE_068b(void)

{
  char *in_R0;
  
  *in_R0 = *in_R0 + '\x01';
  return;
}



void FUN_CODE_068f(void)

{
  byte bVar1;
  byte in_R1;
  
  P2 = 0xff;
  bVar1 = P7;
  P7 = bVar1 | in_R1 & 0xf;
  bVar1 = P7;
  P7 = bVar1 & ~in_R1 & 0xf;
  bVar1 = P6;
  P6 = bVar1 | in_R1 >> 4;
  bVar1 = P6;
  P6 = bVar1 & ~(in_R1 >> 4) & 0xf;
  return;
}



void FUN_CODE_069c(void)

{
  return;
}



undefined1 FUN_CODE_06bd(void)

{
  char in_R1;
  
  return *(undefined1 *)(in_R1 + -2);
}



void FUN_CODE_06c7(void)

{
  byte bVar1;
  bool in_F1;
  
  P2 = 0xef;
  if (in_F1) {
    bVar1 = P5;
    P5 = bVar1 | 2;
  }
  else {
    bVar1 = P5;
    P5 = bVar1 & 0xd;
  }
  return;
}



void FUN_CODE_06d5(void)

{
  undefined1 in_R4;
  undefined1 in_R5;
  
  DAT_EXTMEM_0a = 0;
  DAT_EXTMEM_09 = in_R5;
  DAT_EXTMEM_08 = in_R4;
  DAT_EXTMEM_07 = 0;
  DAT_EXTMEM_06 = 0;
  return;
}



void FUN_CODE_06e5(void)

{
  undefined1 *in_R0;
  undefined1 *in_R1;
  char cVar1;
  
  cVar1 = '\x05';
  do {
    *in_R1 = *in_R0;
    in_R0 = in_R0 + -1;
    in_R1 = in_R1 + -1;
    cVar1 = cVar1 + -1;
  } while (cVar1 != '\0');
  return;
}



/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* 6502 core - 6502 emulator core - David Alan Gilbert 16/10/94 */
/* Mike Wyatt 7/6/97 - Added undocumented instructions */
/* Copied for 65C02 Tube core - 13/04/01 */

#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>

#include "6502core.h"
#include "main.h"
#include "beebmem.h"
#include "tube.h"

#ifdef WIN32
#include <windows.h>
#define INLINE inline
#else
#define INLINE
#endif

// Some interrupt set macros
#define SETTUBEINT(a) TubeintStatus|=1<<a
#define RESETTUBEINT(a) TubeintStatus&=~(1<<a)

static unsigned int InstrCount;
unsigned char TubeRam[65536];
extern int DumpAfterEach;
unsigned char TubeEnabled,EnableTube;

CycleCountT TotalTubeCycles=0;

int TubeProgramCounter;
static int Accumulator,XReg,YReg;
static unsigned char StackReg,PSR;

unsigned char TubeintStatus=0; /* bit set (nums in IRQ_Nums) if interrupt being caused */
unsigned char TubeNMIStatus=0; /* bit set (nums in NMI_Nums) if NMI being caused */
static unsigned int NMILock=0; /* Well I think NMI's are maskable - to stop repeated NMI's - the lock is released when an RTI is done */

typedef int int16;
/* Stats */
static int Stats[256];

enum PSRFlags {
  FlagC=1,
  FlagZ=2,
  FlagI=4,
  FlagD=8,
  FlagB=16,
  FlagV=64,
  FlagN=128
};

/* Note how GETCFLAG is special since being bit 0 we don't need to test it to get a clean 0/1 */
#define GETCFLAG ((PSR & FlagC))
#define GETZFLAG ((PSR & FlagZ)>0)
#define GETIFLAG ((PSR & FlagI)>0)
#define GETDFLAG ((PSR & FlagD)>0)
#define GETBFLAG ((PSR & FlagB)>0)
#define GETVFLAG ((PSR & FlagV)>0)
#define GETNFLAG ((PSR & FlagN)>0)

/* Types for internal function arrays */
typedef void (*InstrHandlerFuncType)(int16 Operand);
typedef int16 (*AddrModeHandlerFuncType)(int WantsAddr);

static int TubeCyclesTable[]={
  7,6,0,0,0,3,5,5,3,2,2,0,0,4,6,0, /* 0 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 1 */
  6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0, /* 2 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 3 */
  6,6,0,0,0,3,5,0,3,2,2,2,3,4,6,0, /* 4 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 5 */
  6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0, /* 6 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 7 */
  2,6,0,0,3,3,3,3,2,0,2,0,4,4,4,0, /* 8 */
  2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0, /* 9 */
  2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0, /* a */
  2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0, /* b */
  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* c */
  2,5,0,0,0,4,6,0,2,4,0,0,4,4,7,0, /* d */
  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* e */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0  /* f */
}; /* TubeCyclesTable */

/* The number of TubeCycles to be used by the current instruction - exported to
   allow fernangling by memory subsystem */
unsigned int TubeCycles;

static unsigned char Branched,Carried;
// Branched - 1 if the instruction branched
// Carried - 1 if the instruction carried over to high byte in index calculation
static unsigned char FirstCycle;
// 1 if first cycle happened

/* A macro to speed up writes - uses a local variable called 'tmpaddr' */
#define TUBEREADMEM_FAST(a) ((a<0xfef7)?TubeRam[a]:TubeReadMem(a))
#define TUBEREADMEM_FASTINC(a) ((a<0xfef7)?TubeRam[a++]:TubeReadMem(a++))
#define TUBEWRITEMEM_FAST(Address, Value) if (Address<0xfef7) TubeRam[Address]=Value; else TubeWriteMem(Address,Value);
#define TUBEWRITEMEM_DIRECT(Address, Value) TubeRam[Address]=Value;
#define TUBEFASTWRITE(addr,val) tmpaddr=addr; if (tmpaddr<0xfef7) TUBEWRITEMEM_DIRECT(tmpaddr,val) else TubeWriteMem(tmpaddr,val);

// Tube memory/io handling functions

unsigned char R1PHData[25];
unsigned char R1PHPtr;
unsigned char R1HPData;

unsigned char ReadTubeFromHostSide(unsigned char IOAddr) {
    unsigned char TmpData,TmpCntr;
    if (!EnableTube)
        return(0xfe+MachineType);
    else {
        if ((IOAddr==1) && (R1PHPtr>0)) {
            //R1 Data, Host side
            TmpData=R1PHData[0];
            for (TmpCntr=1;TmpCntr<24;TmpCntr++) R1PHData[TmpCntr-1]=R1PHData[TmpCntr]; // Shift FIFO Buffer
            R1PHPtr--; // Shift FIFO Pointer
            RESETTUBEINT(R1);
        }
    }
}

void WriteTubeFromHostSide(unsigned char IOAddr,unsigned char IOData) {
    // Write Tube register
    if ((IOAddr==1) && (R1PHPtr<24)) {
        // R1 Data, Parasite side
        R1PHData[++R1PHPtr]=IOData;
        SETTUBEINT(R1);
    }
}

unsigned char ReadTubeFromParasiteSide(unsigned char IOAddr) {
    // Read Tube register
    return(0);
}

void WriteTubeFromParasiteSide(unsigned char IOAddr,unsigned char IOData) {
    // Write Tube register
}

void TubeWriteMem(unsigned int IOAddr,unsigned char IOData) {
    if (IOAddr>0xff00) TubeRam[IOAddr]=IOData;
    else
    WriteTubeFromParasiteSide(IOAddr-0xfef8,IOData);
}

unsigned char TubeReadMem(unsigned int IOAddr) {
    if (IOAddr>0xff00) return(TubeRam[IOAddr]);
    else
    return(ReadTubeFromParasiteSide(IOAddr-0xfef8));
}

/* Get a two byte address from the program counter, and then post inc the program counter */
#define GETTWOBYTEFROMPC(var) \
  var=TubeRam[TubeProgramCounter]; \
  var|=(TubeRam[TubeProgramCounter+1]<<8); \
  TubeProgramCounter+=2;

/*----------------------------------------------------------------------------*/
INLINE int SignExtendByte(signed char in) {
  /*if (in & 0x80) return(in | 0xffffff00); else return(in); */
  /* I think this should sign extend by virtue of the casts - gcc does anyway - the code
  above will definitly do the trick */
  return((int)in);
} /* SignExtendByte */

/*----------------------------------------------------------------------------*/
/* Set the Z flag if 'in' is 0, and N if bit 7 is set - leave all other bits  */
/* untouched.                                                                 */
INLINE static void SetPSRZN(const unsigned char in) {
  PSR&=~(FlagZ | FlagN);
  PSR|=((in==0)<<1) | (in & 128);
}; /* SetPSRZN */

/*----------------------------------------------------------------------------*/
/* Note: n is 128 for true - not 1                                            */
INLINE static void SetPSR(int mask,int c,int z,int i,int d,int b, int v, int n) {
  PSR&=~mask;
  PSR|=c | (z<<1) | (i<<2) | (d<<3) | (b<<4) | (v<<6) | n;
} /* SetPSR */

/*----------------------------------------------------------------------------*/
/* NOTE!!!!! n is 128 or 0 - not 1 or 0                                       */
INLINE static void SetPSRCZN(int c,int z, int n) {
  PSR&=~(FlagC | FlagZ | FlagN);
  PSR|=c | (z<<1) | n;
} /* SetPSRCZN */

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
INLINE static void Push(unsigned char ToPush) {
  TUBEWRITEMEM_DIRECT(0x100+StackReg,ToPush);
  StackReg--;
} /* Push */

/*----------------------------------------------------------------------------*/
INLINE static unsigned char Pop(void) {
  StackReg++;
  return(TubeRam[0x100+StackReg]);
} /* Pop */

/*----------------------------------------------------------------------------*/
INLINE static void PushWord(int16 topush) {
  Push((topush>>8) & 255);
  Push(topush & 255);
} /* PushWord */

/*----------------------------------------------------------------------------*/
INLINE static int16 PopWord() {
  int16 RetValue;

  RetValue=Pop();
  RetValue|=(Pop()<<8);
  return(RetValue);
} /* PopWord */

/*-------------------------------------------------------------------------*/
/* Relative addressing mode handler                                        */
INLINE static int16 RelAddrModeHandler_Data(void) {
  int EffectiveAddress;

  /* For branches - is this correct - i.e. is the program counter incremented
     at the correct time? */
  EffectiveAddress=SignExtendByte((signed char)TubeRam[TubeProgramCounter++]);
  EffectiveAddress+=TubeProgramCounter;

  return(EffectiveAddress);
} /* RelAddrModeHandler */

/*----------------------------------------------------------------------------*/
INLINE static void ADCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  int TmpResultV,TmpResultC;
  if (!GETDFLAG) {
    TmpResultC=Accumulator+operand+GETCFLAG;
    TmpResultV=(signed char)Accumulator+(signed char)operand+GETCFLAG;
    Accumulator=TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, (TmpResultC & 256)>0,Accumulator==0,0,0,0,((Accumulator & 128)>0) ^ (TmpResultV<0),(Accumulator & 128));
  } else {
    int ZFlag=0,NFlag=0,CFlag=0,VFlag=0;
    int TmpResult,TmpCarry=0;
    int ln,hn;

    /* Z flag determined from 2's compl result, not BCD result! */
    TmpResult=Accumulator+operand+GETCFLAG;
    ZFlag=((TmpResult & 0xff)==0);

    ln=(Accumulator & 0xf)+(operand & 0xf)+GETCFLAG;
    if (ln>9) {
      ln += 6;
      ln &= 0xf;
      TmpCarry=0x10;
    }
    hn=(Accumulator & 0xf0)+(operand & 0xf0)+TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    NFlag=hn & 128;
    VFlag=((hn & 128)==0) ^ ((Accumulator & 128)==0);
    if (hn>0x90) {
      hn += 0x60;
      hn &= 0xf0;
      CFlag=1;
    }
    Accumulator=hn|ln;
    SetPSR(FlagC | FlagZ | FlagV | FlagN,CFlag,ZFlag,0,0,0,VFlag,NFlag);
  }
  if (GETDFLAG) TubeCycles++; // Add 1 cycle if in decimal mode
} /* ADCInstrHandler */

/*----------------------------------------------------------------------------*/
INLINE static void ANDInstrHandler(int16 operand) {
  Accumulator=Accumulator & operand;
  PSR&=~(FlagZ | FlagN);
  PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
} /* ANDInstrHandler */

INLINE static void ASLInstrHandler(int16 address) {
  unsigned char oldVal,newVal;
  oldVal=TUBEREADMEM_FAST(address);
  newVal=(((unsigned int)oldVal)<<1);
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler */

INLINE static void TRBInstrHandler(int16 address) {
    unsigned char oldVal,newVal;
    oldVal=TUBEREADMEM_FAST(address);
    newVal=(Accumulator ^ 255) & oldVal;
    TUBEWRITEMEM_FAST(address,newVal);
    PSR&=253;
    PSR|=((Accumulator & oldVal)==0) ? 2 : 0;
} // TRBInstrHandler

INLINE static void TSBInstrHandler(int16 address) {
    unsigned char oldVal,newVal;
    oldVal=TUBEREADMEM_FAST(address);
    newVal=Accumulator | oldVal;
    TUBEWRITEMEM_FAST(address,newVal);
    PSR&=253;
    PSR|=((Accumulator & oldVal)==0) ? 2 : 0;
} // TSBInstrHandler

INLINE static void ASLInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;
  /* Accumulator */
  oldVal=Accumulator;
  Accumulator=newVal=(((unsigned int)Accumulator)<<1);
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler_Acc */

INLINE static void BCCInstrHandler(void) {
  if (!GETCFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BCCInstrHandler */

INLINE static void BCSInstrHandler(void) {
  if (GETCFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BCSInstrHandler */

INLINE static void BEQInstrHandler(void) {
  if (GETZFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BEQInstrHandler */

INLINE static void BITInstrHandler(int16 operand) {
  PSR&=~(FlagZ | FlagN | FlagV);
  /* z if result 0, and NV to top bits of operand */
  PSR|=(((Accumulator & operand)==0)<<1) | (operand & 192);
} /* BITInstrHandler */

INLINE static void BMIInstrHandler(void) {
  if (GETNFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BMIInstrHandler */

INLINE static void BNEInstrHandler(void) {
  if (!GETZFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BNEInstrHandler */

INLINE static void BPLInstrHandler(void) {
  if (!GETNFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
}; /* BPLInstrHandler */

INLINE static void BRKInstrHandler(void) {
  PushWord(TubeProgramCounter+1);
  SetPSR(FlagB,0,0,0,0,1,0,0); /* Set B before pushing */
  Push(PSR);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Set I after pushing - see Birnbaum */
  TubeProgramCounter=TubeReadMem(0xfffe) | (TubeReadMem(0xffff)<<8);
} /* BRKInstrHandler */

INLINE static void BVCInstrHandler(void) {
  if (!GETVFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BVCInstrHandler */

INLINE static void BVSInstrHandler(void) {
  if (GETVFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BVSInstrHandler */

INLINE static void BRAInstrHandler(void) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
} /* BRAnstrHandler */

INLINE static void CMPInstrHandler(int16 operand) {
  /* NOTE! Should we consult D flag ? */
  unsigned char result=Accumulator-operand;
  SetPSRCZN(Accumulator>=operand,Accumulator==operand,result & 128);
} /* CMPInstrHandler */

INLINE static void CPXInstrHandler(int16 operand) {
  unsigned char result=(XReg-operand);
  SetPSRCZN(XReg>=operand,XReg==operand,result & 128);
} /* CPXInstrHandler */

INLINE static void CPYInstrHandler(int16 operand) {
  unsigned char result=(YReg-operand);
  SetPSRCZN(YReg>=operand,YReg==operand,result & 128);
} /* CPYInstrHandler */

INLINE static void DECInstrHandler(int16 address) {
  unsigned char val;

  val=TUBEREADMEM_FAST(address);

  val=(val-1);

  TUBEWRITEMEM_FAST(address,val);
  SetPSRZN(val);
} /* DECInstrHandler */

INLINE static void DEXInstrHandler(void) {
  XReg=(XReg-1) & 255;
  SetPSRZN(XReg);
} /* DEXInstrHandler */

INLINE static void DEAInstrHandler(void) {
  Accumulator=(Accumulator-1) & 255;
  SetPSRZN(Accumulator);
} /* DEAInstrHandler */

INLINE static void EORInstrHandler(int16 operand) {
  Accumulator^=operand;
  SetPSRZN(Accumulator);
} /* EORInstrHandler */

INLINE static void INCInstrHandler(int16 address) {
  unsigned char val;

  val=TUBEREADMEM_FAST(address);

  val=(val+1) & 255;

  TUBEWRITEMEM_FAST(address,val);
  SetPSRZN(val);
} /* INCInstrHandler */

INLINE static void INXInstrHandler(void) {
  XReg+=1;
  XReg&=255;
  SetPSRZN(XReg);
} /* INXInstrHandler */

INLINE static void INAInstrHandler(void) {
  Accumulator+=1;
  Accumulator&=255;
  SetPSRZN(Accumulator);
} /* INAInstrHandler */

INLINE static void JSRInstrHandler(int16 address) {
  PushWord(TubeProgramCounter-1);
  TubeProgramCounter=address;
} /* JSRInstrHandler */

INLINE static void LDAInstrHandler(int16 operand) {
  Accumulator=operand;
  SetPSRZN(Accumulator);
} /* LDAInstrHandler */

INLINE static void LDXInstrHandler(int16 operand) {
  XReg=operand;
  SetPSRZN(XReg);
} /* LDXInstrHandler */

INLINE static void LDYInstrHandler(int16 operand) {
  YReg=operand;
  SetPSRZN(YReg);
} /* LDYInstrHandler */

INLINE static void LSRInstrHandler(int16 address) {
  unsigned char oldVal,newVal;
  oldVal=TUBEREADMEM_FAST(address);
  newVal=(((unsigned int)oldVal)>>1);
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 1)>0, newVal==0,0);
} /* LSRInstrHandler */

INLINE static void LSRInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;
  /* Accumulator */
  oldVal=Accumulator;
  Accumulator=newVal=(((unsigned int)Accumulator)>>1) & 255;
  SetPSRCZN((oldVal & 1)>0, newVal==0,0);
} /* LSRInstrHandler_Acc */

INLINE static void ORAInstrHandler(int16 operand) {
  Accumulator=Accumulator | operand;
  SetPSRZN(Accumulator);
} /* ORAInstrHandler */

INLINE static void ROLInstrHandler(int16 address) {
  unsigned char oldVal,newVal;

  oldVal=TUBEREADMEM_FAST(address);
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 128)>0,newVal==0,newVal & 128);
} /* ROLInstrHandler */

INLINE static void ROLInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;

  oldVal=Accumulator;
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  Accumulator=newVal;
  SetPSRCZN((oldVal & 128)>0,newVal==0,newVal & 128);
} /* ROLInstrHandler_Acc */

INLINE static void RORInstrHandler(int16 address) {
  unsigned char oldVal,newVal;

  oldVal=TUBEREADMEM_FAST(address);
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN(oldVal & 1,newVal==0,newVal & 128);
} /* RORInstrHandler */

INLINE static void RORInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;

  oldVal=Accumulator;
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  Accumulator=newVal;
  SetPSRCZN(oldVal & 1,newVal==0,newVal & 128);
} /* RORInstrHandler_Acc */

INLINE static void SBCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  int TmpResultV,TmpResultC;
  if (!GETDFLAG) {
    TmpResultV=(signed char)Accumulator-(signed char)operand-(1-GETCFLAG);
    TmpResultC=Accumulator-operand-(1-GETCFLAG);
    Accumulator=TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, TmpResultC>=0,Accumulator==0,0,0,0,
      ((Accumulator & 128)>0) ^ ((TmpResultV & 256)!=0),(Accumulator & 128));
  } else {
    int ZFlag=0,NFlag=0,CFlag=1,VFlag=0;
    int TmpResult,TmpCarry=0;
    int ln,hn;

    /* Z flag determined from 2's compl result, not BCD result! */
    TmpResult=Accumulator-operand-(1-GETCFLAG);
    ZFlag=((TmpResult & 0xff)==0);

    ln=(Accumulator & 0xf)-(operand & 0xf)-(1-GETCFLAG);
    if (ln<0) {
      ln-=6;
      ln&=0xf;
      TmpCarry=0x10;
    }
    hn=(Accumulator & 0xf0)-(operand & 0xf0)-TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    NFlag=hn & 128;
    VFlag=((hn & 128)==0) ^ ((Accumulator & 128)==0);
    if (hn<0) {
      hn-=0x60;
      hn&=0xf0;
      CFlag=0;
    }
    Accumulator=hn|ln;
    SetPSR(FlagC | FlagZ | FlagV | FlagN,CFlag,ZFlag,0,0,0,VFlag,NFlag);
  }
  if (GETDFLAG) TubeCycles++; // Add 1 cycle if in decimal mode
} /* SBCInstrHandler */

INLINE static void STXInstrHandler(int16 address) {
  TUBEWRITEMEM_FAST(address,XReg);
} /* STXInstrHandler */

INLINE static void STYInstrHandler(int16 address) {
  TUBEWRITEMEM_FAST(address,YReg);
} /* STYInstrHandler */

INLINE static void BadInstrHandler(int opcode) {
    if (!IgnoreIllegalInstructions)
    {
#ifdef WIN32
        char errstr[250];
        sprintf(errstr,"Unsupported 65C02 instruction 0x%02X at 0x%04X\n"
            "  OK - instruction will be skipped\n"
            "  Cancel - dump memory and exit",opcode,TubeProgramCounter-1);
        if (MessageBox(GETHWND,errstr,"BBC Emulator",MB_OKCANCEL|MB_ICONERROR) == IDCANCEL)
        {
            exit(0);
        }
#else
        fprintf(stderr,"Bad instruction handler called:\n");
        fprintf(stderr,"Dumping main memory\n");
        beebmem_dumpstate();
        // abort();
#endif
    }

    /* Do not know what the instruction does but can guess if it is 1,2 or 3 bytes */
    switch (opcode & 0xf)
    {
    /* One byte instructions */
    case 0xa:
        break;

    /* Two byte instructions */
    case 0x0:
    case 0x2:  /* Inst 0xf2 causes the 6502 to hang! Try it on your BBC Micro */
    case 0x3:
    case 0x4:
    case 0x7:
    case 0x9:
    case 0xb:
        TubeProgramCounter++;
        break;

    /* Three byte instructions */
    case 0xc:
    case 0xe:
    case 0xf:
        TubeProgramCounter+=2;
        break;
    }
} /* BadInstrHandler */

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Data(void) {
  int FullAddress;

  /* Get the address from after the instruction */

  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(TUBEREADMEM_FAST(FullAddress));
} /* AbsAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Address(void) {
  int FullAddress;

  /* Get the address from after the instruction */
  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(FullAddress);
} /* AbsAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page addressing mode handler                                       */
INLINE static int16 ZeroPgAddrModeHandler_Address(void) {
  return(TubeRam[TubeProgramCounter++]);
} /* ZeroPgAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Data(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;

  EffectiveAddress=TubeRam[ZeroPageAddress] | (TubeRam[ZeroPageAddress+1]<<8);
  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* IndXAddrModeHandler_Data */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Address(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;

  EffectiveAddress=TubeRam[ZeroPageAddress] | (TubeRam[ZeroPageAddress+1]<<8);
  return(EffectiveAddress);
} /* IndXAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried=1;
  EffectiveAddress+=(TubeRam[ZPAddr+1]<<8);

  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried=1;
  EffectiveAddress+=(TubeRam[ZPAddr+1]<<8);

  return(EffectiveAddress);
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;
  return(TubeRam[EffectiveAddress]);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with X offset addressing mode handler                          */
INLINE static int16 AbsXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress);
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+XReg) & 0xff00)) Carried=1;
  EffectiveAddress+=XReg;
  EffectiveAddress&=0xffff;

  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* AbsXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with X offset addressing mode handler                          */
INLINE static int16 AbsXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+XReg) & 0xff00)) Carried=1;
  EffectiveAddress+=XReg;
  EffectiveAddress&=0xffff;

  return(EffectiveAddress);
} /* AbsXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with Y offset addressing mode handler                          */
INLINE static int16 AbsYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress);
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+YReg) & 0xff00)) Carried=1;
  EffectiveAddress+=YReg;

  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* AbsYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with Y offset addressing mode handler                          */
INLINE static int16 AbsYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+YReg) & 0xff00)) Carried=1;
  EffectiveAddress+=YReg;

  return(EffectiveAddress);
} /* AbsYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indirect addressing mode handler                                        */
INLINE static int16 IndAddrModeHandler_Address(void) {
  /* For jump indirect only */
  int VectorLocation;
  int EffectiveAddress;

  GETTWOBYTEFROMPC(VectorLocation)

  /* Ok kiddies, deliberate bug time.
  According to my BBC Master Reference Manual Part 2
  the 6502 has a bug concerning this addressing mode and VectorLocation==xxFF
  so, we're going to emulate that bug -- Richard Gellman */
  if ((VectorLocation & 0xff)!=0xff || MachineType==1) {
   EffectiveAddress=TUBEREADMEM_FAST(VectorLocation);
   EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  else {
   EffectiveAddress=TUBEREADMEM_FAST(VectorLocation);
   EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation-255) << 8;
  }
  return(EffectiveAddress);
} /* IndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page Indirect addressing mode handler                                        */
INLINE static int16 ZPIndAddrModeHandler_Address(void) {
  int VectorLocation;
  int EffectiveAddress;

  VectorLocation=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[VectorLocation]+(TubeRam[VectorLocation+1]<<8);

   // EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  return(EffectiveAddress);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page Indirect addressing mode handler                                        */
INLINE static int16 ZPIndAddrModeHandler_Data(void) {
  int VectorLocation;
  int EffectiveAddress;

  VectorLocation=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[VectorLocation]+(TubeRam[VectorLocation+1]<<8);

   // EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  return(TubeRam[EffectiveAddress]);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Pre-indexed absolute Indirect addressing mode handler                                        */
INLINE static int16 IndAddrXModeHandler_Address(void) {
  /* For jump indirect only */
  int VectorLocation;
  int EffectiveAddress;

  GETTWOBYTEFROMPC(VectorLocation)
  EffectiveAddress=TUBEREADMEM_FAST(VectorLocation+XReg);
  EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1+XReg) << 8;

   // EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  return(EffectiveAddress);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+YReg) & 255;
  return(TubeRam[EffectiveAddress]);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+YReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Initialise 6502core                                                     */
void Init65C02core(void) {
  FILE *TubeRom;
  char TRN[256];
  char *TubeRomName=TRN;
  TubeProgramCounter=TubeReadMem(0xfffc) | (TubeReadMem(0xfffd)<<8);
  Accumulator=XReg=YReg=0; /* For consistancy of execution */
  StackReg=0xff; /* Initial value ? */
  PSR=FlagI; /* Interrupts off for starters */

  TubeintStatus=0;
  TubeNMIStatus=0;
  NMILock=0;
  //The fun part, the tube OS is copied from ROM to tube RAM before the processor starts processing
  //This makes the OS "ROM" writable in effect, but must be restored on each reset.
  strcpy(TubeRomName,RomPath); strcat(TubeRomName,"/beebfile/6502Tube.rom");
  TubeRom=fopen(TubeRomName,"rb");
  if (TubeRom!=NULL) {
      fread(TubeRam+0xf800,1,2048,TubeRom);
      fclose(TubeRom);
  }
} /* Init6502core */

#include "via.h"

/*-------------------------------------------------------------------------*/
void DoTubeInterrupt(void) {
  PushWord(TubeProgramCounter);
  Push(PSR & ~FlagB);
  TubeProgramCounter=TubeReadMem(0xfffe) | (TubeReadMem(0xffff)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0);
} /* DoInterrupt */

/*-------------------------------------------------------------------------*/
void DoTubeNMI(void) {
  /*cerr << "Doing NMI\n"; */
  NMILock=1;
  PushWord(TubeProgramCounter);
  Push(PSR);
  TubeProgramCounter=TubeReadMem(0xfffa) | (TubeReadMem(0xfffb)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Normal interrupts should be disabled during NMI ? */
} /* DoNMI */

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec65C02Instruction(void) {
  static int CurrentInstruction;
  static int tmpaddr;
  static int OldTubeNMIStatus;
  int OldPC;
  int loop;
  for(loop=0;loop<512;loop++) {
  // For the Master, check Shadow Ram Presence
  // Note, this has to be done BEFORE reading an instruction due to Bit E and the PC
  /* Read an instruction and post inc program couter */
  CurrentInstruction=TubeRam[TubeProgramCounter++];
  // cout << "Fetch at " << hex << (TubeProgramCounter-1) << " giving 0x" << CurrentInstruction << dec << "\n";
  TubeCycles=TubeCyclesTable[CurrentInstruction];
  /*Stats[CurrentInstruction]++; */
  OldPC=TubeProgramCounter;
  Carried=0; Branched=0;
  switch (CurrentInstruction) {
    case 0x00:
      BRKInstrHandler();
      break;
    case 0x01:
      ORAInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x04:
      if (MachineType==1) TSBInstrHandler(ZeroPgAddrModeHandler_Address()); else TubeProgramCounter+=1;
      break;
    case 0x05:
      ORAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x06:
      ASLInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x08:
      Push(PSR); /* PHP */
      break;
    case 0x09:
      ORAInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x0a:
      ASLInstrHandler_Acc();
      break;
    case 0x0c:
      if (MachineType==1) TSBInstrHandler(AbsAddrModeHandler_Address()); else TubeProgramCounter+=2;
      break;
    case 0x0d:
      ORAInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x0e:
      ASLInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x10:
      BPLInstrHandler();
      break;
    case 0x30:
      BMIInstrHandler();
      break;
    case 0x50:
      BVCInstrHandler();
      break;
    case 0x70:
      BVSInstrHandler();
      break;
    case 0x80:
      BRAInstrHandler();
      break;
    case 0x90:
      BCCInstrHandler();
      break;
    case 0xb0:
      BCSInstrHandler();
      break;
    case 0xd0:
      BNEInstrHandler();
      break;
    case 0xf0:
      BEQInstrHandler();
      break;
    case 0x11:
      ORAInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x12:
      if (MachineType==1) ORAInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x14:
      if (MachineType==1) TRBInstrHandler(ZeroPgAddrModeHandler_Address()); else TubeProgramCounter+=1;
      break;
    case 0x15:
      ORAInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x16:
      ASLInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x18:
      PSR&=255-FlagC; /* CLC */
      break;
    case 0x19:
      ORAInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x1a:
      if (MachineType==1) INAInstrHandler();
      break;
    case 0x1c:
      if (MachineType==1) TRBInstrHandler(AbsAddrModeHandler_Address()); else TubeProgramCounter+=2;
      break;
    case 0x1d:
      ORAInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x1e:
      ASLInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x20:
      JSRInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x21:
      ANDInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x24:
      BITInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x25:
      ANDInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x26:
      ROLInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x28:
      PSR=Pop(); /* PLP */
      break;
    case 0x29:
      ANDInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x2a:
      ROLInstrHandler_Acc();
      break;
    case 0x2c:
      BITInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x2d:
      ANDInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x2e:
      ROLInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x31:
      ANDInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x32:
      if (MachineType==1) ANDInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x34: /* BIT Absolute,X */
      if (MachineType==1) BITInstrHandler(ZeroPgXAddrModeHandler_Data()); else TubeProgramCounter+=1;
      break;
    case 0x35:
      ANDInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x36:
      ROLInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x38:
      PSR|=FlagC; /* SEC */
      break;
    case 0x39:
      ANDInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x3a:
      if (MachineType==1) DEAInstrHandler();
      break;
    case 0x3c: /* BIT Absolute,X */
      if (MachineType==1) BITInstrHandler(AbsXAddrModeHandler_Data()); else TubeProgramCounter+=2;
      break;
    case 0x3d:
      ANDInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x3e:
      ROLInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x40:
      PSR=Pop(); /* RTI */
      TubeProgramCounter=PopWord();
      NMILock=0;
      break;
    case 0x41:
      EORInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x45:
      EORInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x46:
      LSRInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x48:
      Push(Accumulator); /* PHA */
      break;
    case 0x49:
      EORInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x4a:
      LSRInstrHandler_Acc();
      break;
    case 0x4c:
      TubeProgramCounter=AbsAddrModeHandler_Address(); /* JMP */
      break;
    case 0x4d:
      EORInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x4e:
      LSRInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x51:
      EORInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x52:
      if (MachineType==1) EORInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x55:
      EORInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x56:
      LSRInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x58:
      PSR&=255-FlagI; /* CLI */
      break;
    case 0x59:
      EORInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x5a:
      if (MachineType==1) Push(YReg); /* PHY */
      break;
    case 0x5d:
      EORInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x5e:
      LSRInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x60:
      TubeProgramCounter=PopWord()+1; /* RTS */
      break;
    case 0x61:
      ADCInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x64:
      if (MachineType==1) TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),0); /* STZ Zero Page */
      break;
    case 0x65:
      ADCInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x66:
      RORInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x68:
      Accumulator=Pop(); /* PLA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x69:
      ADCInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x6a:
      RORInstrHandler_Acc();
      break;
    case 0x6c:
      TubeProgramCounter=IndAddrModeHandler_Address(); /* JMP */
      break;
    case 0x6d:
      ADCInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x6e:
      RORInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x71:
      ADCInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x72:
      if (MachineType==1) ADCInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x74:
      if (MachineType==1) { TUBEFASTWRITE(ZeroPgXAddrModeHandler_Address(),0); } else TubeProgramCounter+=1; /* STZ Zpg,X */
      break;
    case 0x75:
      ADCInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x76:
      RORInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x78:
      PSR|=FlagI; /* SEI */
      break;
    case 0x79:
      ADCInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x7a:
        if (MachineType==1) {
            YReg=Pop(); /* PLY */
            PSR&=~(FlagZ | FlagN);
            PSR|=((XReg==0)<<1) | (YReg & 128);
        }
      break;
    case 0x7c:
      if (MachineType==1) TubeProgramCounter=IndAddrXModeHandler_Address(); /* JMP abs,X*/ else TubeProgramCounter+=2;
      break;
    case 0x7d:
      ADCInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x7e:
      RORInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x81:
      TUBEFASTWRITE(IndXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x84:
      TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),YReg);
      break;
    case 0x85:
      TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x86:
      TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),XReg);
      break;
    case 0x88:
      YReg=(YReg-1) & 255; /* DEY */
      PSR&=~(FlagZ | FlagN);
      PSR|=((YReg==0)<<1) | (YReg & 128);
      break;
    case 0x89: /* BIT Immediate */
      if (MachineType==1) BITInstrHandler(TubeRam[TubeProgramCounter++]);
      break;
    case 0x8a:
      Accumulator=XReg; /* TXA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x8c:
      STYInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x8d:
      TUBEFASTWRITE(AbsAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x8e:
      STXInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x91:
      TUBEFASTWRITE(IndYAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x92:
      if (MachineType==1) TUBEFASTWRITE(ZPIndAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x94:
      STYInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x95:
      TUBEFASTWRITE(ZeroPgXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x96:
      STXInstrHandler(ZeroPgYAddrModeHandler_Address());
      break;
    case 0x98:
      Accumulator=YReg; /* TYA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x99:
      TUBEFASTWRITE(AbsYAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x9a:
      StackReg=XReg; /* TXS */
      break;
    case 0x9c:
      TUBEFASTWRITE(AbsAddrModeHandler_Address(),0); /* STZ Absolute */
      /* here's a curiosity, STZ Absolute IS on the 6502 UNOFFICIALLY
      and on the 65C12 OFFICIALLY. Something we should know? - Richard Gellman */
      break;
    case 0x9d:
      TUBEFASTWRITE(AbsXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x9e:
        if (MachineType==1) { TUBEFASTWRITE(AbsXAddrModeHandler_Address(),0); } /* STZ Abs,X */
        else TubeRam[AbsXAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0xa0:
      LDYInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xa1:
      LDAInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xa2:
      LDXInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xa4:
      LDYInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xa5:
      LDAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xa6:
      LDXInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xa8:
      YReg=Accumulator; /* TAY */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0xa9:
      LDAInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xaa:
      XReg=Accumulator; /* TXA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0xac:
      LDYInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xad:
      LDAInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xae:
      LDXInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xb1:
      LDAInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xb2:
      if (MachineType==1) LDAInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0xb4:
      LDYInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xb5:
      LDAInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xb6:
      LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
      break;
    case 0xb8:
      PSR&=255-FlagV; /* CLV */
      break;
    case 0xb9:
      LDAInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xba:
      XReg=StackReg; /* TSX */
      PSR&=~(FlagZ | FlagN);
      PSR|=((XReg==0)<<1) | (XReg & 128);
      break;
    case 0xbc:
      LDYInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xbd:
      LDAInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xbe:
      LDXInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xc0:
      CPYInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xc1:
      CMPInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xc4:
      CPYInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xc5:
      CMPInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xc6:
      DECInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0xc8:
      YReg+=1; /* INY */
      YReg&=255;
      PSR&=~(FlagZ | FlagN);
      PSR|=((YReg==0)<<1) | (YReg & 128);
      break;
    case 0xc9:
      CMPInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xca:
      DEXInstrHandler();
      break;
    case 0xcc:
      CPYInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xcd:
      CMPInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xce:
      DECInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0xd1:
      CMPInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xd2:
      if (MachineType==1) CMPInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0xd5:
      CMPInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xd6:
      DECInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0xd8:
      PSR&=255-FlagD; /* CLD */
      break;
    case 0xd9:
      CMPInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xda:
      if (MachineType==1) Push(XReg); /* PHX */
      break;
    case 0xdd:
      CMPInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xde:
      DECInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0xe0:
      CPXInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xe1:
      SBCInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xe4:
      CPXInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xe5:
      SBCInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xe6:
      INCInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0xe8:
      INXInstrHandler();
      break;
    case 0xe9:
      SBCInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xea:
      /* NOP */
      break;
    case 0xec:
      CPXInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xed:
      SBCInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xee:
      INCInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0xf1:
      SBCInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xf2:
      if (MachineType==1) SBCInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0xf5:
      SBCInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xf6:
      INCInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0xf8:
      PSR|=FlagD; /* SED */
      break;
    case 0xf9:
      SBCInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xfa:
        if (MachineType==1) {
      XReg=Pop(); /* PLX */
      PSR&=~(FlagZ | FlagN);
      PSR|=((XReg==0)<<1) | (XReg & 128);
        }
        break;
    case 0xfd:
      SBCInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xfe:
      INCInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x07: /* Undocumented Instruction: ASL zp and ORA zp */
      {
        int16 zpaddr = ZeroPgAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x03: /* Undocumented Instruction: ASL-ORA (zp,X) */
      {
        int16 zpaddr = IndXAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x13: /* Undocumented Instruction: ASL-ORA (zp),Y */
      {
        int16 zpaddr = IndYAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x0f: /* Undocumented Instruction: ASL-ORA abs */
      {
        int16 zpaddr = AbsAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x17: /* Undocumented Instruction: ASL-ORA zp,X */
      {
        int16 zpaddr = ZeroPgXAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x1b: /* Undocumented Instruction: ASL-ORA abs,Y */
      {
        int16 zpaddr = AbsYAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x1f: /* Undocumented Instruction: ASL-ORA abs,X */
      {
        int16 zpaddr = AbsXAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x23: /* Undocumented Instruction: ROL-AND (zp,X) */
        {
        int16 zpaddr=IndXAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x27: /* Undocumented Instruction: ROL-AND zp */
        {
        int16 zpaddr=ZeroPgAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x2f: /* Undocumented Instruction: ROL-AND abs */
        {
        int16 zpaddr=AbsAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x33: /* Undocumented Instruction: ROL-AND (zp),Y */
        {
        int16 zpaddr=IndYAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x37: /* Undocumented Instruction: ROL-AND zp,X */
        {
        int16 zpaddr=ZeroPgXAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x3b: /* Undocumented Instruction: ROL-AND abs.Y */
        {
        int16 zpaddr=AbsYAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x3f: /* Undocumented Instruction: ROL-AND abs.X */
        {
        int16 zpaddr=AbsXAddrModeHandler_Address();
        ROLInstrHandler(zpaddr);
        ANDInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x43: /* Undocumented Instruction: LSR-EOR (zp,X) */
        {
        int16 zpaddr=IndXAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x47: /* Undocumented Instruction: LSR-EOR zp */
        {
        int16 zpaddr=ZeroPgAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x4f: /* Undocumented Instruction: LSR-EOR abs */
        {
        int16 zpaddr=AbsAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x53: /* Undocumented Instruction: LSR-EOR (zp),Y */
        {
        int16 zpaddr=IndYAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x57: /* Undocumented Instruction: LSR-EOR zp,X */
        {
        int16 zpaddr=ZeroPgXAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x5b: /* Undocumented Instruction: LSR-EOR abs,Y */
        {
        int16 zpaddr=AbsYAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x5f: /* Undocumented Instruction: LSR-EOR abs,X */
        {
        int16 zpaddr=AbsXAddrModeHandler_Address();
        LSRInstrHandler(zpaddr);
        EORInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x44:
    case 0x54:
        TubeProgramCounter+=1;
        break;
    case 0x5c:
        TubeProgramCounter+=2;
        break;
    case 0x63: /* Undocumented Instruction: ROR-ADC (zp,X) */
        {
        int16 zpaddr=IndXAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x67: /* Undocumented Instruction: ROR-ADC zp */
        {
        int16 zpaddr=ZeroPgAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x6f: /* Undocumented Instruction: ROR-ADC abs */
        {
        int16 zpaddr=AbsAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x73: /* Undocumented Instruction: ROR-ADC (zp),Y */
        {
        int16 zpaddr=IndYAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x77: /* Undocumented Instruction: ROR-ADC zp,X */
        {
        int16 zpaddr=ZeroPgXAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x7b: /* Undocumented Instruction: ROR-ADC abs,Y */
        {
        int16 zpaddr=AbsYAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x7f: /* Undocumented Instruction: ROR-ADC abs,X */
        {
        int16 zpaddr=AbsXAddrModeHandler_Address();
        RORInstrHandler(zpaddr);
        ADCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0x0b:
    case 0x2b:
      ANDInstrHandler(TubeRam[TubeProgramCounter++]); /* AND-MVC #n,b7 */
      PSR|=((Accumulator & 128)>>7);
      break;
    case 0x4b: /* Undocumented Instruction: AND imm and LSR A */
      ANDInstrHandler(TubeRam[TubeProgramCounter++]);
      LSRInstrHandler_Acc();
      break;
    case 0x87: /* Undocumented Instruction: SAX zp (i.e. (zp) = A & X) */
      /* This one does not seem to change the processor flags */
      TubeRam[ZeroPgAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x83: /* Undocumented Instruction: SAX (zp,X) */
      TubeRam[IndXAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x8f: /* Undocumented Instruction: SAX abs */
      TubeRam[AbsAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x93: /* Undocumented Instruction: SAX (zp),Y */
      TubeRam[IndYAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x97: /* Undocumented Instruction: SAX zp,Y */
      TubeRam[ZeroPgYAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x9b: /* Undocumented Instruction: SAX abs,Y */
      TubeRam[AbsYAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x9f: /* Undocumented Instruction: SAX abs,X */
      TubeRam[AbsXAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0xab: /* Undocumented Instruction: LAX #n */
      LDAInstrHandler(TubeRam[TubeProgramCounter++]);
      XReg = Accumulator;
      break;
    case 0xa3: /* Undocumented Instruction: LAX (zp,X) */
      LDAInstrHandler(IndXAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xa7: /* Undocumented Instruction: LAX zp */
      LDAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
      XReg = Accumulator;
      break;
    case 0xaf: /* Undocumented Instruction: LAX abs */
      LDAInstrHandler(AbsAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xb3: /* Undocumented Instruction: LAX (zp),Y */
      LDAInstrHandler(IndYAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xb7: /* Undocumented Instruction: LAX zp,Y */
      LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
      Accumulator = XReg;
      break;
    case 0xbb:
    case 0xbf: /* Undocumented Instruction: LAX abs,Y */
      LDAInstrHandler(AbsYAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    // Undocumented DEC-CMP and INC-SBC Instructions
    case 0xc3: // DEC-CMP (zp,X)
        {
        int16 zpaddr=IndXAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xc7: // DEC-CMP zp
        {
        int16 zpaddr=ZeroPgAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xcf: // DEC-CMP abs
        {
        int16 zpaddr=AbsAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xd3: // DEC-CMP (zp),Y
        {
        int16 zpaddr=IndYAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xd7: // DEC-CMP zp,X
        {
        int16 zpaddr=ZeroPgXAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xdb: // DEC-CMP abs,Y
        {
        int16 zpaddr=AbsYAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xdf: // DEC-CMP abs,X
        {
        int16 zpaddr=AbsXAddrModeHandler_Address();
        DECInstrHandler(zpaddr);
        CMPInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xd4:
    case 0xf4:
        TubeProgramCounter+=1;
        break;
    case 0xdc:
    case 0xfc:
        TubeProgramCounter+=2;
        break;
    case 0xe3: // INC-SBC (zp,X)
        {
        int16 zpaddr=IndXAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xe7: // INC-SBC zp
        {
        int16 zpaddr=ZeroPgAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xef: // INC-SBC abs
        {
        int16 zpaddr=AbsAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xf3: // INC-SBC (zp).Y
        {
        int16 zpaddr=IndYAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xf7: // INC-SBC zp,X
        {
        int16 zpaddr=ZeroPgXAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xfb: // INC-SBC abs,Y
        {
        int16 zpaddr=AbsYAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    case 0xff: // INC-SBC abs,X
        {
        int16 zpaddr=AbsXAddrModeHandler_Address();
        INCInstrHandler(zpaddr);
        SBCInstrHandler(TubeRam[zpaddr]);
        }
        break;
    // REALLY Undocumented instructions 6B, 8B and CB
    case 0x6b:
        ANDInstrHandler(TubeRam[TubeProgramCounter++]);
        RORInstrHandler_Acc();
        break;
    case 0x8b:
        Accumulator=XReg; /* TXA */
        PSR&=~(FlagZ | FlagN);
        PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
        ANDInstrHandler(TubeRam[TubeProgramCounter++]);
        break;
    case 0xcb:
        // SBX #n - I dont know if this uses the carry or not, i'm assuming its
        // Subtract #n from X with carry.
        {
            unsigned char TmpAcc=Accumulator;
            Accumulator=XReg;
            SBCInstrHandler(TubeRam[TubeProgramCounter++]);
            XReg=Accumulator;
            Accumulator=TmpAcc; // Fudge so that I dont have to do the whole SBC code again
        }
        break;
    default:
      BadInstrHandler(CurrentInstruction);
      break;
    break;

  }; /* OpCode switch */
    // This block corrects the cycle count for the branch instructions
    if ((CurrentInstruction==0x10) ||
        (CurrentInstruction==0x30) ||
        (CurrentInstruction==0x50) ||
        (CurrentInstruction==0x70) ||
        (CurrentInstruction==0x80) ||
        (CurrentInstruction==0x90) ||
        (CurrentInstruction==0xb0) ||
        (CurrentInstruction==0xd0) ||
        (CurrentInstruction==0xf0)) {
            if (((TubeProgramCounter & 0xff00)!=(OldPC & 0xff00)) && (Branched==1))
                TubeCycles+=2; else TubeCycles++;
    }
    if (((CurrentInstruction & 0xf)==1) ||
        ((CurrentInstruction & 0xf)==9) ||
        ((CurrentInstruction & 0xf)==0xD)) {
        if (((CurrentInstruction &0x10)==0) &&
            ((CurrentInstruction &0xf0)!=0x90) &&
            (Carried==1)) TubeCycles++;
    }
    if (((CurrentInstruction==0xBC) || (CurrentInstruction==0xBE)) && (Carried==1)) TubeCycles++;
    // End of cycle correction
  OldTubeNMIStatus=TubeNMIStatus;
  /* NOTE: Check IRQ status before polling hardware - this is essential for
     Rocket Raid to work since it polls the IFR in the sys via for start of
     frame - but with interrupts enabled.  If you do the interrupt check later
     then the interrupt handler will always be entered and rocket raid will
     never see it */
  if ((TubeintStatus) && (!GETIFLAG)) DoInterrupt();

  TotalTubeCycles+=TubeCycles;
  if (TotalTubeCycles > CycleCountWrap)
  {
    TotalTubeCycles -= CycleCountWrap;
  }

  if ((TubeNMIStatus) && (!OldTubeNMIStatus)) DoNMI();
  };
} /* Exec6502Instruction */

void SyncTubeProcessor (void) {
    // This proc syncronises the two processors on a cycle based timing.
    // i.e. if parasitecycles<hostcycles then execute parasite instructions until
    // parasitecycles>=hostcycles.
    while (TotalTubeCycles<TotalCycles) {
        Exec65C02Instruction();
    }
}
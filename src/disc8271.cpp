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
/* Please report any problems to the author at gilbertd@cs.man.ac.uk        */
/****************************************************************************/
/*--------------------------------------------------------------------------*/
/* 8271 disc emulation - David Alan Gilbert 4/12/94 */
#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "6502core.h"
#include "disc8271.h"


int Disc8271Trigger; /* Cycle based time Disc8271Trigger */
static unsigned char ResultReg;
static unsigned char StatusReg;
static unsigned char DataReadReg;
static unsigned char Internal_Scan_SectorNum;
static unsigned int Internal_Scan_Count; /* Read as two bytes */
static unsigned char Internal_ModeReg;
static unsigned char Internal_CurrentTrack[2]; /* 0/1 for surface number */
static unsigned char Internal_DriveControlOutputPort;
static unsigned char Internal_DriveControlInputPort;
static unsigned char Internal_BadTracks[2][2]; /* 1st subscript is surface 0/1 and second subscript is badtrack 0/1 */

static int ThisCommand;
static int NParamsInThisCommand;
static int PresentParam; /* From 0 */
static unsigned char Params[16]; /* Wildly more than we need */

static int Selects[2]; /* Drive selects */
static int Writeable[2]={1,1}; /* True if the drives are writeable - for the moment let them be */

static int NextInterruptIsErr; /* none 0 causes error and drops this value into result reg */
#define TRACKSPERDRIVE 80

/* Note Head select is done from bit 5 of the drive output register */
#define CURRENTHEAD ((Internal_DriveControlOutputPort>>5) & 1)

/* Note: reads one byte every 80us */
#define TIMEBETWEENBYTES (160)

typedef struct {

  struct {
    unsigned int CylinderNum:7;
    unsigned int RecordNum:5;
    unsigned int HeadNum:1;
    unsigned int PhysRecLength;
  } IDField;

  unsigned int Deleted:1; /* If non-zero the sector is deleted */
  char *Data;
} SectorType;

typedef struct {
  int LogicalSectors; /* Number of sectors stated in format command */
  int NSectors; /* i.e. the number of records we have - not anything physical */
  SectorType *Sectors;
  int Gap1Size,Gap3Size,Gap5Size; /* From format command */
} TrackType;

/* All data on the disc - first param is drive number, then head. then physical track id */
TrackType DiscStore[2][2][TRACKSPERDRIVE];

typedef void (*CommandFunc)(void);

#define UPDATENMISTATUS if (StatusReg & 8) NMIStatus |=1<<nmi_floppy; else NMIStatus &= ~(1<<nmi_floppy);

/*--------------------------------------------------------------------------*/
static struct {
  int TrackAddr;
  int CurrentSector;
  int SectorLength; /* In bytes */
  int SectorsToGo;

  SectorType *CurrentSectorPtr;
  TrackType *CurrentTrackPtr;

  int ByteWithinSector; /* Next byte to be read in sector */
} ReadCommandStatus;

/*--------------------------------------------------------------------------*/
typedef struct  {
  unsigned char CommandNum;
  unsigned char Mask; /* Mask command with this before comparing with CommandNum - allows drive ID to be removed */
  int NParams; /* Number of parameters to follow */
  CommandFunc ToCall; /* Called after all paameters have arrived */
  CommandFunc IntHandler; /* Called when interrupt requested by command is about to happen */
  char *Ident; /* Mainly for debugging */
} PrimaryCommandLookupType;

/*--------------------------------------------------------------------------*/
/* For appropriate commands checks the select bits in the command code and  */
/* selects the appropriate drive.                                           */
static void DoSelects(void) {
  Selects[0]=(ThisCommand & 0x40)!=0;
  Selects[1]=(ThisCommand & 0x80)!=0;
  Internal_DriveControlOutputPort&=0x3f;
  if (Selects[0]) Internal_DriveControlOutputPort|=0x40;
  if (Selects[1]) Internal_DriveControlOutputPort|=0x80;
}; /* DoSelects */

/*--------------------------------------------------------------------------*/
static void NotImp(const char *NotImpCom) {
  cerr << NotImpCom << " has not been implemented in disc8271 - sorry\n";
  exit(0);
}; /* NotImp */

/*--------------------------------------------------------------------------*/
/* Load the head - ignore for the moment                                    */
static void DoLoadHead(void) {
}; /* DoLoadHead */

/*--------------------------------------------------------------------------*/
/* Initialise our disc structures                                           */
static void InitDiscStore(void) {
  int head,track,drive;
  TrackType blank={0,0,NULL,0,0,0};

  for(drive=0;drive<2;drive++)
    for(head=0;head<2;head++)
      for(track=0;track<TRACKSPERDRIVE;track++)
        DiscStore[drive][head][track]=blank;
}; /* InitDiscStore */

/*--------------------------------------------------------------------------*/
/* Given a logical track number accounts for bad tracks                     */
static int SkipBadTracks(int Unit, int trackin) {
  int offset=0;
  if (Internal_BadTracks[Unit][0]<=trackin) offset++;
  if (Internal_BadTracks[Unit][1]<=trackin) offset++;

  return(trackin+offset);
}; /* SkipBadTracks */

/*--------------------------------------------------------------------------*/
/* Returns a pointer to the data structure for a particular track.  You     */
/* pass the logical track number, it takes into account bad tracks and the  */
/* drive select and head select etc.  It always returns a valid ptr - if    */
/* there aren't that many tracks then it uses the last one.                 */
/* The one exception!!!! is that if no drives are selected it returns NULL  */
static TrackType *GetTrackPtr(int LogicalTrackID) {
  int UnitID=-1;

  if (Selects[0]) UnitID=0;
  if (Selects[1]) UnitID=1;

  if (UnitID<0) return(NULL);

  LogicalTrackID=SkipBadTracks(UnitID,LogicalTrackID);

  if (LogicalTrackID>=TRACKSPERDRIVE) LogicalTrackID=TRACKSPERDRIVE-1;

  return(&(DiscStore[UnitID][CURRENTHEAD][LogicalTrackID]));
}; /* GetTrackPtr */

/*--------------------------------------------------------------------------*/
/* Returns a pointer to the data structure for a particular sector. Returns */
/* NULL for Sector not found. Doesn't check cylinder/head ID                */
static SectorType *GetSectorPtr(TrackType *Track, int LogicalSectorID, int FindDeleted) {
  int CurrentSector;

  if (Track->Sectors==NULL) return(NULL);

  for(CurrentSector=0;CurrentSector<Track->NSectors;CurrentSector++)
    if ((Track->Sectors[CurrentSector].IDField.RecordNum==LogicalSectorID) && ((!Track->Sectors[CurrentSector].Deleted) || (!FindDeleted)))
      return(&(Track->Sectors[CurrentSector]));

  return(NULL);
}; /* GetSectorPtr */

/*--------------------------------------------------------------------------*/
/* Returns true if the drive signified by the current selects is ready      */
static int CheckReady(void) {
  if (Selects[0]) return(1);
  if (Selects[1]) return(1);

  return(0);
};  /* CheckReady */

/*--------------------------------------------------------------------------*/
/* Cause an error - pass err num                                            */
static void DoErr(int ErrNum) {
  SetTrigger(50,Disc8271Trigger); /* Give it a bit of time */
  NextInterruptIsErr=ErrNum;
  StatusReg=0x80; /* Command is busy - come back when I have an interrupt */
  UPDATENMISTATUS;
}; /* DoErr */

/*--------------------------------------------------------------------------*/
/* Checks a few things in the sector - returns true if OK                   */
static int ValidateSector(SectorType *SecToVal,int Track,int SecLength) {
  if (SecToVal->IDField.CylinderNum!=Track) return(0);
  if (SecToVal->IDField.PhysRecLength!=SecLength) return(0);

  return(1);
}; /* ValidateSector */
/*--------------------------------------------------------------------------*/
static void DoVarLength_ScanDataCommand(void) {
  DoSelects();

  NotImp("DoVarLength_ScanDataCommand");
};

/*--------------------------------------------------------------------------*/
static void DoVarLength_ScanDataAndDeldCommand(void) {
  DoSelects();
  NotImp("DoVarLength_ScanDataAndDeldCommand");
};

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_WriteDataCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_WriteDataCommand");
};

/*--------------------------------------------------------------------------*/
static void DoVarLength_WriteDataCommand(void) {
  DoSelects();
  NotImp("DoVarLength_WriteDataCommand");
};

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_WriteDeletedDataCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_WriteDeletedDataCommand");
};

/*--------------------------------------------------------------------------*/
static void DoVarLength_WriteDeletedDataCommand(void) {
  DoSelects();
  NotImp("DoVarLength_WriteDeletedDataCommand");
};

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_ReadDataCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_ReadDataCommand");
};

/*--------------------------------------------------------------------------*/
static void DoVarLength_ReadDataCommand(void) {
  int Drive=-1;

  DoSelects();

  DoLoadHead();


  if (!CheckReady()) {
    DoErr(0x10); /* Drive not ready */
    return;
  };

  if (Selects[0]) Drive=0;
  if (Selects[1]) Drive=1;

  Internal_CurrentTrack[Drive]=Params[0];
  if (ReadCommandStatus.CurrentTrackPtr=GetTrackPtr(Params[0]),ReadCommandStatus.CurrentTrackPtr==NULL) {
    DoErr(0x10);
    return;
  };

  if (ReadCommandStatus.CurrentSectorPtr=GetSectorPtr(ReadCommandStatus.CurrentTrackPtr,Params[1],0),ReadCommandStatus.CurrentSectorPtr==NULL) {
    DoErr(0x1e); /* Sector not found */
    return;
  };

  ReadCommandStatus.TrackAddr=Params[0];
  ReadCommandStatus.CurrentSector=Params[1];
  ReadCommandStatus.SectorsToGo=Params[2] & 31;
  ReadCommandStatus.SectorLength=1<<(7+((Params[2] >> 5) & 7));

  if (ValidateSector(ReadCommandStatus.CurrentSectorPtr,ReadCommandStatus.TrackAddr,ReadCommandStatus.SectorLength)) {
    ReadCommandStatus.ByteWithinSector=0;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
    StatusReg=0x80; /* Command busy */
    UPDATENMISTATUS;
    ReadCommandStatus.ByteWithinSector=0;
  } else {
    DoErr(0x1e); /* Sector not found */
  };
};

/*--------------------------------------------------------------------------*/
static void ReadInterrupt(void) {
  extern int DumpAfterEach;
  int LastByte=0;

  if (ReadCommandStatus.SectorsToGo<0) {
    StatusReg=0x18; /* Result and interrupt */
    UPDATENMISTATUS;
    return;
  };

  DataReadReg=ReadCommandStatus.CurrentSectorPtr->Data[ReadCommandStatus.ByteWithinSector++];
  /*cerr << "ReadInterrupt called - DataReadReg=0x" << hex << int(DataReadReg) << dec << "ByteWithinSector=" << ReadCommandStatus.ByteWithinSector << "\n"; */

  /* DumpAfterEach=1; */
  ResultReg=0;
  if (ReadCommandStatus.ByteWithinSector>=ReadCommandStatus.SectorLength) {
    ReadCommandStatus.ByteWithinSector=0;
    /* I don't know if this can cause the thing to step - I presume not for the moment */
    if (--ReadCommandStatus.SectorsToGo) {
      ReadCommandStatus.CurrentSector++;
      if (ReadCommandStatus.CurrentSectorPtr=GetSectorPtr(ReadCommandStatus.CurrentTrackPtr,ReadCommandStatus.CurrentSector,0),
          ReadCommandStatus.CurrentSectorPtr==NULL) {
    DoErr(0x1e); /* Sector not found */
    return;
      }/* else cerr << "all ptr for sector " << ReadCommandStatus.CurrentSector << "\n"*/;
    } else {
      /* Last sector done */
      StatusReg=0x9c;
      UPDATENMISTATUS;
      LastByte=1;
      ReadCommandStatus.SectorsToGo=-1; /* To let us bail out */
      SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger); /* To pick up result */
    };
  };

  if (!LastByte) {
    StatusReg=0x8c; /* Command busy, */
    UPDATENMISTATUS;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
  };
}; /* ReadInterrupt */

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_ReadDataAndDeldCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_ReadDataAndDeldCommand");
};
/*--------------------------------------------------------------------------*/
static void DoVarLength_ReadDataAndDeldCommand(void) {
  DoSelects();
  NotImp("DoVarLength_ReadDataAndDeldCommand");
};
/*--------------------------------------------------------------------------*/
static void DoReadIDCommand(void) {
  DoSelects();
  NotImp("DoReadIDCommand");
};
/*--------------------------------------------------------------------------*/
static void Do128ByteSR_VerifyDataAndDeldCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_VerifyDataAndDeldCommand");
};
/*--------------------------------------------------------------------------*/
static void DoVarLength_VerifyDataAndDeldCommand(void) {
  DoSelects();
  NotImp("DoVarLength_VerifyDataAndDeldCommand");
};
/*--------------------------------------------------------------------------*/
static void DoFormatCommand(void) {
  DoSelects();
  NotImp("DoFormatCommand");
};

/*--------------------------------------------------------------------------*/
static void DoSeekInt(void) {
  StatusReg=0x18; /* Result with interrupt */
  UPDATENMISTATUS;
  ResultReg=0; /* All OK */
};
/*--------------------------------------------------------------------------*/
static void DoSeekCommand(void) {
  int Drive=-1;
  DoSelects();

  DoLoadHead();

  if (Selects[0]) Drive=0;
  if (Selects[1]) Drive=1;

  if (Drive<0) {
    DoErr(0x10);
    return;
  };

  Internal_CurrentTrack[Drive]=Params[0];

  StatusReg=0x80; /* Command busy */
  UPDATENMISTATUS;
  SetTrigger(100,Disc8271Trigger); /* A short delay to causing an interrupt */
};

/*--------------------------------------------------------------------------*/
static void DoReadDriveStatusCommand(void) {
  int Track0,WriteProt;

  DoSelects();

  if (Selects[0]) {
    Track0=(Internal_CurrentTrack[0]==0);
    WriteProt=(!Writeable[0]);
  };

  if (Selects[1]) {
    Track0=(Internal_CurrentTrack[1]==0);
    WriteProt=(!Writeable[1]);
  };

  ResultReg=0x81; /* For the moment!!! */

  ResultReg=0x80 | (Selects[1]?0x40:0) | (Selects[0]?0x4:0) | (Track0?2:0) | (WriteProt?8:0);
  StatusReg|=0x10; /* Result */
  UPDATENMISTATUS;
};
/*--------------------------------------------------------------------------*/
static void DoSpecifyCommand(void) {
  /* Should set stuff up here */
};
/*--------------------------------------------------------------------------*/
static void DoWriteSpecialCommand(void) {
  DoSelects();
  switch(Params[0]) {
    case 0x06:
      Internal_Scan_SectorNum=Params[1];
      break;

    case 0x14:
      Internal_Scan_Count&=0xff;
      Internal_Scan_Count|=Params[1]<<8;
      break;

    case 0x13:
      Internal_Scan_Count&=0xff00;
      Internal_Scan_Count|=Params[1];
      break;

    case 0x12:
      Internal_CurrentTrack[0]=Params[1];
      break;

    case 0x1a:
      Internal_CurrentTrack[1]=Params[1];
      break;

    case 0x17:
      Internal_ModeReg=Params[1];
      break;

    case 0x23:
      Internal_DriveControlOutputPort=Params[1];
      Selects[0]=(Params[1] & 0x40)!=0;
      Selects[1]=(Params[1] & 0x80)!=0;
      break;

    case 0x22:
      Internal_DriveControlInputPort=Params[1];
      break;

    case 0x10:
      Internal_BadTracks[0][0]=Params[1];
      break;

    case 0x11:
      Internal_BadTracks[0][1]=Params[1];
      break;

    case 0x18:
      Internal_BadTracks[1][0]=Params[1];
      break;

    case 0x19:
      Internal_BadTracks[1][1]=Params[1];
      break;

    default:
      cerr << "Write to bad special register\n";
      return;
      break;
  }; /* Special register number switch */

}; /* DoWriteSpecialCommand */

/*--------------------------------------------------------------------------*/
static void DoReadSpecialCommand(void) {
  DoSelects();
  switch(Params[0]) {
    case 0x06:
      ResultReg=Internal_Scan_SectorNum;
      break;

    case 0x14:
      ResultReg=(Internal_Scan_Count>>8) & 255;
      break;

    case 0x13:
      ResultReg=Internal_Scan_Count & 255;
      break;

    case 0x12:
      ResultReg=Internal_CurrentTrack[0];
      break;

    case 0x1a:
      ResultReg=Internal_CurrentTrack[1];
      break;

    case 0x17:
      ResultReg=Internal_ModeReg;
      break;

    case 0x23:
      ResultReg=Internal_DriveControlOutputPort;
      break;

    case 0x22:
      ResultReg=Internal_DriveControlInputPort;
      break;

    case 0x10:
      ResultReg=Internal_BadTracks[0][0];
      break;

    case 0x11:
      ResultReg=Internal_BadTracks[0][1];
      break;

    case 0x18:
      ResultReg=Internal_BadTracks[1][0];
      break;

    case 0x19:
      ResultReg=Internal_BadTracks[1][1];
      break;

    default:
      cerr << "Read of bad special register\n";
      return;
      break;
  }; /* Special register number switch */

  StatusReg |= 16; /* Result reg full */
  UPDATENMISTATUS;
};
/*--------------------------------------------------------------------------*/
static void DoBadCommand(void) {
};

/*--------------------------------------------------------------------------*/
/* The following table is used to parse commands from the command number written into
the command register - it can't distinguish between subcommands selected from the
first parameter */
static PrimaryCommandLookupType PrimaryCommandLookup[]={
  {0x00, 0x3f, 3, DoVarLength_ScanDataCommand, NULL,  "Scan Data (Variable Length/Multi-Record)"},
  {0x04, 0x3f, 3, DoVarLength_ScanDataAndDeldCommand, NULL,  "Scan Data & deleted data (Variable Length/Multi-Record)"},
  {0x0a, 0x3f, 2, Do128ByteSR_WriteDataCommand, NULL, "Write Data (128 byte/single record)"},
  {0x0b, 0x3f, 3, DoVarLength_WriteDataCommand, NULL, "Write Data (Variable Length/Multi-Record)"},
  {0x0e, 0x3f, 2, Do128ByteSR_WriteDeletedDataCommand, NULL, "Write Deleted Data (128 byte/single record)"},
  {0x0f, 0x3f, 3, DoVarLength_WriteDeletedDataCommand, NULL, "Write Deleted Data (Variable Length/Multi-Record)"},
  {0x12, 0x3f, 2, Do128ByteSR_ReadDataCommand, NULL, "Read Data (128 byte/single record)"},
  {0x13, 0x3f, 3, DoVarLength_ReadDataCommand, ReadInterrupt, "Read Data (Variable Length/Multi-Record)"},
  {0x16, 0x3f, 2, Do128ByteSR_ReadDataAndDeldCommand, NULL, "Read Data & deleted data (128 byte/single record)"},
  {0x17, 0x3f, 3, DoVarLength_ReadDataAndDeldCommand, NULL, "Read Data & deleted data (Variable Length/Multi-Record)"},
  {0x1b, 0x3f, 3, DoReadIDCommand, NULL, "ReadID" },
  {0x1e, 0x3f, 2, Do128ByteSR_VerifyDataAndDeldCommand, NULL, "Verify Data and Deleted Data (128 byte/single record)"},
  {0x1f, 0x3f, 3, DoVarLength_VerifyDataAndDeldCommand, NULL, "Verify Data and Deleted Data (Variable Length/Multi-Record)"},
  {0x23, 0x3f, 5, DoFormatCommand, NULL, "Format"},
  {0x29, 0x3f, 1, DoSeekCommand, DoSeekInt,    "Seek"},
  {0x2c, 0x3f, 0, DoReadDriveStatusCommand, NULL, "Read drive status"},
  {0x35, 0xff, 4, DoSpecifyCommand, NULL, "Specify" },
  {0x3a, 0x3f, 2, DoWriteSpecialCommand, NULL, "Write special registers" },
  {0x3d, 0x3f, 1, DoReadSpecialCommand, NULL, "Read special registers" },
  {0,    0,    0, DoBadCommand, NULL, "Unknown command"} /* Terminator due to 0 mask matching all */
}; /* PrimaryCommandLookup */

/*--------------------------------------------------------------------------*/
/* returns a pointer to the data structure for the given command            */
/* If no matching command is given, the pointer points to an entry with a 0 */
/* mask, with a sensible function to call.                                  */
static PrimaryCommandLookupType *CommandPtrFromNumber(int CommandNumber) {
  PrimaryCommandLookupType *presptr=PrimaryCommandLookup;

  for(;presptr->CommandNum!=(presptr->Mask & CommandNumber);presptr++);

  return(presptr);
}; /* CommandPtrFromNumber */

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-7 - with the fe80 etc stripped out */
int Disc8271_read(int Address) {
  switch (Address) {
    case 0:
      /*cerr << "8271 Status register read (0x" << hex << int(StatusReg) << dec << ")\n"; */
      return(StatusReg);
      break;

    case 1:
      /*cerr << "8271 Result register read (0x" << hex << int(ResultReg) << dec << ")\n"; */
      StatusReg &=~18; /* Clear interrupt request  and result reg full flag*/
      UPDATENMISTATUS;
      return(ResultReg);
      break;

    case 4:
      /*cerr << "8271 data register read\n"; */
      StatusReg &= ~0xc; /* Clear interrupt and non-dma request - not stated but DFS never looks at result reg!*/
      UPDATENMISTATUS;
      return(DataReadReg);
      break;

    default:
      cerr << "8271: Read to unknown register address=" << Address << "\n";
      break;
  }; /* Address switch */

  return(0);
}; /* Disc8271_read */

/*--------------------------------------------------------------------------*/
static void CommandRegWrite(int Value) {
  PrimaryCommandLookupType *ptr=CommandPtrFromNumber(Value);
  /*cerr << "8271: Command register write value=0x" << hex << Value << dec << "(Name=" << ptr->Ident << ")\n"; */
  ThisCommand=Value;
  NParamsInThisCommand=ptr->NParams;
  PresentParam=0;

  StatusReg|=0x90; /* Observed on beeb for read special */
  UPDATENMISTATUS;

  /* No parameters then call routine immediatly */
  if (NParamsInThisCommand==0) {
    StatusReg&=0x7e;
    UPDATENMISTATUS;
    ptr->ToCall();
  };

}; /* CommandRegWrite */

/*--------------------------------------------------------------------------*/
static void ParamRegWrite(int Value) {
  if (PresentParam>=NParamsInThisCommand) {
    cerr << "8271: Unwanted parameter register write value=0x" << hex << Value << dec << "\n";
  } else {
    Params[PresentParam++]=Value;

    StatusReg&=0xfe; /* Observed on beeb */
    UPDATENMISTATUS;

    if (PresentParam>=NParamsInThisCommand) {
      int tmp;

      StatusReg&=0x7e; /* Observed on beeb */
      UPDATENMISTATUS;

      PrimaryCommandLookupType *ptr=CommandPtrFromNumber(ThisCommand);
    cerr << "<Disc access>";

    /*  cerr << "8271: All parameters arrived for '" << ptr->Ident;

      for(tmp=0;tmp<PresentParam;tmp++)
        cerr << " 0x" << hex << int(Params[tmp]);

      cerr << dec << "\n"; */

      ptr->ToCall();
    }; /* Got all params yet? */
  } /* Parameter wanted ? */
}; /* ParamRegWrite */

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-7 - with the fe80 etc stripped out */
void Disc8271_write(int Address, int Value) {
  switch (Address) {
    case 0:
      CommandRegWrite(Value);
      break;

    case 1:
      ParamRegWrite(Value);
      break;

    case 2:
      cerr << "8271: Reset register write, value=0x" << hex << Value << dec << "\n";
      /* The caller should write a 1 and then >11 cycles later a 0 - but I'm just going
      to reset on both edges */
      Disc8271_reset();
      break;

    case 4:
      cerr << "8271: data register write, value=0x" << hex << Value << dec << "\n";
      break;

    default:
      cerr << "8271: Write to unknown register address=" << Address << ", value=0x" << hex << Value << dec << "\n";
      break;
  }; /* Address switch */
}; /* Disc8271_write */

/*--------------------------------------------------------------------------*/
void Disc8271_poll_real(void) {
  ClearTrigger(Disc8271Trigger);
  PrimaryCommandLookupType *comptr;
  /* Set the interrupt flag in the status register */
  StatusReg|=8;
  UPDATENMISTATUS;

  if (NextInterruptIsErr) {
    ResultReg=NextInterruptIsErr;
    StatusReg=0x18; /* ResultReg full and interrupt */
    UPDATENMISTATUS;
    NextInterruptIsErr=0;
  } else {
    /* Should only happen while a command is still active */
    comptr=CommandPtrFromNumber(ThisCommand);
    if (comptr->IntHandler!=NULL) comptr->IntHandler();
  };
}; /* Disc8271_poll */

/*--------------------------------------------------------------------------*/
static void LoadSimpleDiscImage(char *FileName, int DriveNum,int HeadNum, int Tracks) {
  FILE *infile=fopen(FileName,"rb");
  int CurrentTrack,CurrentSector;
  SectorType *SecPtr;

  if (!infile) {
    cerr << "Could not open disc file " << FileName << "\n";
    return;
  };

  for(CurrentTrack=0;CurrentTrack<Tracks;CurrentTrack++) {
    DiscStore[DriveNum][HeadNum][CurrentTrack].LogicalSectors=10;
    DiscStore[DriveNum][HeadNum][CurrentTrack].NSectors=10;
    SecPtr=DiscStore[DriveNum][HeadNum][CurrentTrack].Sectors=(SectorType*)calloc(10,sizeof(SectorType));
    DiscStore[DriveNum][HeadNum][CurrentTrack].Gap1Size=0; /* Don't bother for the mo */
    DiscStore[DriveNum][HeadNum][CurrentTrack].Gap3Size=0;
    DiscStore[DriveNum][HeadNum][CurrentTrack].Gap5Size=0;

    for(CurrentSector=0;CurrentSector<10;CurrentSector++) {
      SecPtr[CurrentSector].IDField.CylinderNum=CurrentTrack;
      SecPtr[CurrentSector].IDField.RecordNum=CurrentSector;
      SecPtr[CurrentSector].IDField.HeadNum=HeadNum;
      SecPtr[CurrentSector].IDField.PhysRecLength=256;
      SecPtr[CurrentSector].Deleted=0;

      SecPtr[CurrentSector].Data=(char *)malloc(256);
      fread(SecPtr[CurrentSector].Data,1,256,infile);
    }; /* Sector */
  }; /* Track */

  fclose(infile);
};  /* LoadSimpleDiscImage */
/*--------------------------------------------------------------------------*/
static void LoadSimpleDSDiscImage(char *FileName, int DriveNum,int Tracks) {
  FILE *infile=fopen(FileName,"rb");
  int CurrentTrack,CurrentSector,HeadNum;
  SectorType *SecPtr;

  if (!infile) {
    cerr << "Could not open disc file " << FileName << "\n";
    return;
  };

  for(CurrentTrack=0;CurrentTrack<Tracks;CurrentTrack++) {
    for(HeadNum=0;HeadNum<2;HeadNum++) {
      DiscStore[DriveNum][HeadNum][CurrentTrack].LogicalSectors=10;
      DiscStore[DriveNum][HeadNum][CurrentTrack].NSectors=10;
      SecPtr=DiscStore[DriveNum][HeadNum][CurrentTrack].Sectors=(SectorType *)calloc(10,sizeof(SectorType));
      DiscStore[DriveNum][HeadNum][CurrentTrack].Gap1Size=0; /* Don't bother for the mo */
      DiscStore[DriveNum][HeadNum][CurrentTrack].Gap3Size=0;
      DiscStore[DriveNum][HeadNum][CurrentTrack].Gap5Size=0;

      for(CurrentSector=0;CurrentSector<10;CurrentSector++) {
    SecPtr[CurrentSector].IDField.CylinderNum=CurrentTrack;
    SecPtr[CurrentSector].IDField.RecordNum=CurrentSector;
    SecPtr[CurrentSector].IDField.HeadNum=HeadNum;
    SecPtr[CurrentSector].IDField.PhysRecLength=256;
    SecPtr[CurrentSector].Deleted=0;

    SecPtr[CurrentSector].Data=(char *)malloc(256);
    fread(SecPtr[CurrentSector].Data,1,256,infile);
      }; /* Sector */
    }; /* Head */
  }; /* Track */

  fclose(infile);
};  /* LoadSimpleDSDiscImage */
/*--------------------------------------------------------------------------*/
void Disc8271_reset(void) {
  static onetime_initdisc=0;

  ResultReg=0;
  StatusReg=0;
  UPDATENMISTATUS;
  Internal_Scan_SectorNum=0;
  Internal_Scan_Count=0; /* Read as two bytes */
  Internal_ModeReg=0;
  Internal_CurrentTrack[0]=Internal_CurrentTrack[1]=0; /* 0/1 for surface number */
  Internal_DriveControlOutputPort=0;
  Internal_DriveControlInputPort=0;
  Internal_BadTracks[0][0]=Internal_BadTracks[0][1]=Internal_BadTracks[1][0]=Internal_BadTracks[1][1]=0xff; /* 1st subscript is surface 0/1 and second subscript is badtrack 0/1 */
  ClearTrigger(Disc8271Trigger); /* No Disc8271Triggered events yet */

  ThisCommand=-1;
  NParamsInThisCommand=0;
  PresentParam=0;
  Selects[0]=Selects[1]=0;

  if (!onetime_initdisc) {
    char *DiscString=getenv("BeebDiscLoad");
    onetime_initdisc++;
    InitDiscStore();
    if (DiscString==NULL) {
      LoadSimpleDSDiscImage(".\\discims\\elite", 0,80);
    } else {
      char DoubleSided;
      int Tracks;
      char Name[1024];
      int scanfres;
      if (scanfres=sscanf(DiscString,"%c:%d:%s",&DoubleSided,&Tracks,Name),
          scanfres!=3) {
        cerr << "Incorrect format for BeebDiscLoad - the correct format is\n";
        cerr << "  D|S:tracks:filename\n e.g. D80discims/elite\n";
        cerr << "  for a double sided, 80 track disc image called discims/elite\n";
        LoadSimpleDSDiscImage(".\\discims\\elite", 0,80);
      } else {
        switch (DoubleSided) {
          case 'd':
          case 'D':
            LoadSimpleDSDiscImage(Name,0,Tracks);
            break;

          case 'S':
          case 's':
            LoadSimpleDiscImage(Name,0,0,Tracks);
            break;

          default:
            cerr << "BeebDiscLoad environment variable set wrong - the\n";
            cerr << "first character is either D or S signifying\n";
            cerr << "Single or double sided\n";
            LoadSimpleDSDiscImage(".\\discims\\elite", 0,80);
            break;
        }; /* Switch */
      }; /* Successful parse of env variable */
    }; /* env variable set */
  }; /* One time discload */
}; /* Disc8271_reset */

/*--------------------------------------------------------------------------*/
void disc8271_dumpstate(void) {
  cerr << "8271:\n";
  cerr << "  ResultReg=" << int(ResultReg)<< "\n";
  cerr << "  StatusReg=" << int(StatusReg)<< "\n";
  cerr << "  DataReadReg=" << int(DataReadReg)<< "\n";
  cerr << "  Internal_Scan_SectorNum=" << int(Internal_Scan_SectorNum)<< "\n";
  cerr << "  Internal_Scan_Count=" << Internal_Scan_Count<< "\n";
  cerr << "  Internal_ModeReg=" << int(Internal_ModeReg)<< "\n";
  cerr << "  Internal_CurrentTrack=" << int(Internal_CurrentTrack[0]) << "," << int(Internal_CurrentTrack[1]) << "\n";
  cerr << "  Internal_DriveControlOutputPort=" << int(Internal_DriveControlOutputPort)<< "\n";
  cerr << "  Internal_DriveControlInputPort=" << int(Internal_DriveControlInputPort)<< "\n";
  cerr << "  Internal_BadTracks=" << "(" << int(Internal_BadTracks[0][0]) << "," << int(Internal_BadTracks[0][1]) << ") (";
  cerr <<                                   int(Internal_BadTracks[1][0]) << "," << int(Internal_BadTracks[1][1]) << ")\n";
  cerr << "  Disc8271Trigger=" << Disc8271Trigger << "\n";
  cerr << "  ThisCommand=" << ThisCommand<< "\n";
  cerr << "  NParamsInThisCommand=" << NParamsInThisCommand<< "\n";
  cerr << "  PresentParam=" << PresentParam<< "\n";
  cerr << "  Selects=" << Selects[0] << "," << Selects[1] << "\n";
  cerr << "  NextInterruptIsErr=" << NextInterruptIsErr<< "\n";
};

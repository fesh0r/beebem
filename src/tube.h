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
/* 6502Core - header - David Alan Gilbert 16/10/94 */
/* Copied for 65C02 Tube by Richard Gellman 13/04/01 */
#ifndef TUBE6502_HEADER
#define TUBE6502_HEADER

#include "port.h"

extern unsigned char EnableTube,TubeEnabled;
// EnableTube - Should the tube be enabled on next start - 1=yes
// TubeEnabled - Is the tube enabled by default - 1=yes

typedef enum {
    R1,
    R2,
    R4,
} TubeIRQ;

typedef enum {
    R3,
} TubeNMI;


/*-------------------------------------------------------------------------*/
/* Initialise 6502core                                                     */
void Init65C02core(void);

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec65C02Instruction(void);

void DoTubeNMI(void);
void DoTubeInterrupt(void);
void SyncTubeProcessor(void);
unsigned char ReadTubeFromHostSide(unsigned char IOAddr);
void WriteTubeFromHostSide(unsigned char IOAddr,unsigned char IOData);
extern int TubeProgramCounter;
#endif

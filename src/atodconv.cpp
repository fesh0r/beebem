/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997  Mike Wyatt

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

/* Analogue to digital converter support file for the beeb emulator -
   Mike Wyatt 7/6/97 */

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>

#include "6502core.h"
#include "atodconv.h"
#include "sysvia.h"

int JoystickEnabled = 0;

/* X and Y positions for joystick 1 */
int JoystickX;
int JoystickY;

/* A to D state */
typedef struct AtoDStateT{
    unsigned char datalatch;
    unsigned char status;
    unsigned char high;
    unsigned char low;
} AtoDStateT;

AtoDStateT AtoDState;

int AtoDTrigger;  /* For next A to D conversion completion */

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fec0 stripped out */
void AtoDWrite(int Address, int Value)
{
    if (Address == 0)
    {
        int timetoconvert;
        AtoDState.datalatch = Value & 0xff;
        if (AtoDState.datalatch & 8)
            timetoconvert = 20000; /* 10 bit conversion, 10 ms */
        else
            timetoconvert = 8000; /* 8 bit conversion, 4 ms */
        SetTrigger(timetoconvert,AtoDTrigger);

        AtoDState.status = (AtoDState.datalatch & 0xf) | 0x80; /* busy, not complete */
    }
}

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fec0 stripped out */
int AtoDRead(int Address)
{
    int Value = 0xff;

    switch (Address)
    {
    case 0:
        Value = AtoDState.status;
        break;

    case 1:
        Value = AtoDState.high;
        break;

    case 2:
        Value = AtoDState.low;
        break;
    }

    return(Value);
}

/*--------------------------------------------------------------------------*/
void AtoD_poll_real(void)
{
    int value;

    ClearTrigger(AtoDTrigger);
    AtoDState.status &= 0xf;
    AtoDState.status |= 0x40; /* not busy */
    PulseSysViaCB1();

    switch (AtoDState.status & 3)
    {
    case 0:
        value = JoystickX;
        break;
    case 1:
        value = JoystickY;
        break;
    default:
        value = 0;
        break;
    }

    AtoDState.status |= (value & 0xc000)>>10;
    AtoDState.high = value>>8;
    AtoDState.low = value & 0xf0;
}

/*--------------------------------------------------------------------------*/
void AtoDInit(void)
{
    JoystickEnabled = 1;
    AtoDState.datalatch = 0;
    AtoDState.high = 0;
    AtoDState.low = 0;
    ClearTrigger(AtoDTrigger);

    /* Not busy, conversion complete (OS1.2 will then request another conversion) */
    AtoDState.status = 0x40;
    PulseSysViaCB1();
}

/*--------------------------------------------------------------------------*/
void AtoDReset(void)
{
    JoystickEnabled = 0;
    AtoDState.datalatch = 0;
    AtoDState.status = 0x80; /* busy, conversion not complete */
    AtoDState.high = 0;
    AtoDState.low = 0;
    ClearTrigger(AtoDTrigger);

    /* Move joystick to middle (superpool looks at joystick even when not selected) */
    JoystickX = 32767;
    JoystickY = 32767;
}

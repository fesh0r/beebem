/****************************************************************************/
/*                               Beebem                                     */
/*                               ------                                     */
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
/****************************************************************************/
/* Mike Wyatt and NRM's port to win32 - 7/6/97 */
/* Conveted to use DirectX - Mike Wyatt 11/1/98 */

#include <stdio.h>
#include <windows.h>
#include <initguid.h>
#include "main.h"
#include "beebwin.h"
#include "port.h"
#include "6502core.h"
#include "disc8271.h"
#include "disc1770.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "beebsound.h"
#include "beebmem.h"
#include "beebemrc.h"
#include "atodconv.h"
#include "beebstate.h"
#include "userkybd.h"
#include "cRegistry.h" // David Overton's registry access code.
#include "serial.h"
#include "tube.h"
#include "ext1770.h"
#include "uefstate.h"

// some LED based macros

#define LED_COLOUR_TYPE (LEDByte&4)>>2
#define LED_SHOW_KB (LEDByte&1)
#define LED_SHOW_DISC (LEDByte&2)>>1

// Registry access stuff
cRegistry SysReg;
bool RegRes;
int rbs=1;
int *binsize=&rbs;
// End of registry access stuff

FILE *CMDF2;
unsigned char CMA2;

unsigned char HideMenuEnabled;
bool MenuOn;
int LastTickCount=0;

struct LEDType LEDs;
char DiscLedColour=0; // 0 for red, 1 for green.

// FDC Board extension DLL variables
HMODULE hFDCBoard;

typedef void (*lGetBoardProperties)(struct DriveControlBlock *);
typedef unsigned char (*lSetDriveControl)(unsigned char);
typedef unsigned char (*lGetDriveControl)(unsigned char);


lGetBoardProperties PGetBoardProperties;
lSetDriveControl PSetDriveControl;
lGetDriveControl PGetDriveControl;

bool m_PageFlipping=0;

EDCB ExtBoard={0,0,NULL};
bool DiscLoaded[2]={FALSE,FALSE}; // Set to TRUE when a disc image has been loaded.
char CDiscName[2][256]; // Filename of disc current in drive 0 and 1;
char CDiscType[2]; // Current disc types
bool MustEnableSound=FALSE; // Set to true if the directsound must be unmuted after menu loop exit.
char FDCDLL[256];

static const char *WindowTitle = "BeebEm - BBC Model B/Master 128 Emulator";
static const char *AboutText = "BeebEm\nBBC Micro Model B/Master 128 Emulator\n"
                                "Version 1.41, 19 March 2002";

/* Configuration file strings */
static const char *CFG_FILE_NAME = "BeebEm.ini";

static const char *CFG_VIEW_SECTION = "View";
static const char *CFG_VIEW_WIN_SIZE = "WinSize";
static const char *CFG_VIEW_WIN_XPOS = "WinXPos";
static const char *CFG_VIEW_WIN_YPOS = "WinYPos";
static const char *CFG_VIEW_SHOW_FPS = "ShowFSP";
static const char *CFG_VIEW_DIRECT_ENABLED = "DirectDrawEnabled";
static const char *CFG_VIEW_BUFFER_IN_VIDEO = "BufferInVideoRAM";
static const char *CFG_VIEW_MONITOR = "Monitor";

static const char *CFG_SOUND_SECTION = "Sound";
static const char *CFG_SOUND_SAMPLE_RATE = "SampleRate";
static const char *CFG_SOUND_VOLUME = "Volume";
static const char *CFG_SOUND_ENABLED = "SoundEnabled";
static const char *CFG_SOUND_DIRECT_ENABLED = "DirectSoundEnabled";

static const char *CFG_OPTIONS_SECTION = "Options";
static const char *CFG_OPTIONS_STICKS = "Sticks";
static const char *CFG_OPTIONS_KEY_MAPPING = "KeyMapping";
static const char *CFG_OPTIONS_USER_KEY_MAP = "UserKeyMap";
static const char *CFG_OPTIONS_FREEZEINACTIVE = "FreezeWhenInactive";
static const char *CFG_OPTIONS_HIDE_CURSOR = "HideCursor";

static const char *CFG_SPEED_SECTION = "Speed";
static const char *CFG_SPEED_TIMING = "Timing";

static const char *CFG_AMX_SECTION = "AMX";
static const char *CFG_AMX_ENABLED = "AMXMouseEnabled";
static const char *CFG_AMX_LRFORMIDDLE = "AMXMouseLRForMiddle";
static const char *CFG_AMX_SIZE = "AMXMouseSize";
static const char *CFG_AMX_ADJUST = "AMXMouseAdjust";

static const char *CFG_PRINTER_SECTION = "Printer";
static const char *CFG_PRINTER_ENABLED = "PrinterEnabled";
static const char *CFG_PRINTER_PORT = "PrinterPort";
static const char *CFG_PRINTER_FILE = "PrinterFile";

static const char *CFG_MODEL_SECTION = "Model";
static const char *CFG_MACHINE_TYPE = "MachineType";

/* Prototypes */
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int TranslateKey(int, int *, int *);

// Row,Col
static int transTable1[256][2]={
    0,0,    0,0,    0,0,    0,0,   // 0
    0,0,    0,0,    0,0,    0,0,   // 4
    5,9,    6,0,    0,0,    0,0,   // 8 [BS][TAB]..
    0,0,    4,9,    0,0,    0,0,   // 12 .RET..
    0,0,    0,1,    0,0,    -2,-2,   // 16 .CTRL.BREAK
    4,0,    0,0,    0,0,    0,0,   // 20 CAPS...
    0,0,    0,0,    0,0,    7,0,   // 24 ...ESC
    0,0,    0,0,    0,0,    0,0,   // 28
    6,2,    -3,-3,  -3,-4,  6,9,   // 32 SPACE..[End]      2nd is Page Up 3rd is Page Down
    0,0,    1,9,    3,9,    7,9,   // 36 .[Left][Up][Right]
    2,9,    0,0,    0,0,    0,0,   // 40 [Down]...
    0,0,    0,0,    5,9,    0,0,   // 44 ..[DEL].
    2,7,    3,0,    3,1,    1,1,   // 48 0123
    1,2,    1,3,    3,4,    2,4,   // 52 4567
    1,5,    2,6,    0,0,    0,0,   // 56 89
    0,0,    0,0,    0,0,    0,0,   // 60
    0,0,    4,1,    6,4,    5,2,   // 64.. ABC
    3,2,    2,2,    4,3,    5,3,   // 68  DEFG
    5,4,    2,5,    4,5,    4,6,   // 72  HIJK
    5,6,    6,5,    5,5,    3,6,   // 76  LMNO
    3,7,    1,0,    3,3,    5,1,   // 80  PQRS
    2,3,    3,5,    6,3,    2,1,   // 84  TUVW
    4,2,    4,4,    6,1,    0,0,   // 88  XYZ
    0,0,    6,2,    0,0,    0,0,   // 92  . SPACE ..
    0,0,    0,0,    0,0,    0,0,   // 96
    0,0,    0,0,    0,0,    0,0,   // 100
    0,0,    0,0,    0,0,    0,0,   // 104
    0,0,    0,0,    0,0,    0,0,   // 108
    7,1,    7,2,    7,3,    1,4,   // 112 F1 F2 F3 F4
    7,4,    7,5,    1,6,    7,6,   // 116 F5 F6 F7 F8
    7,7,    2,0,    2,0,    -2,-2,   // 120 F9 F10 F11 F12
    0,0,    0,0,    0,0,    0,0,   // 124
    0,0,    0,0,    0,0,    0,0,   // 128
    0,0,    0,0,    0,0,    0,0,   // 132
    0,0,    0,0,    0,0,    0,0,   // 136
    0,0,    0,0,    0,0,    0,0,   // 140
    0,0,    0,0,    0,0,    0,0,   // 144
    0,0,    0,0,    0,0,    0,0,   // 148
    0,0,    0,0,    0,0,    0,0,   // 152
    0,0,    0,0,    0,0,    0,0,   // 156
    0,0,    0,0,    0,0,    0,0,   // 160
    0,0,    0,0,    0,0,    0,0,   // 164
    0,0,    0,0,    0,0,    0,0,   // 168
    0,0,    0,0,    0,0,    0,0,   // 172
    0,0,    0,0,    0,0,    0,0,   // 176
    0,0,    0,0,    0,0,    0,0,   // 180
    0,0,    0,0,    5,7,    1,8,   // 184  ..;=
    6,6,    1,7,    6,7,    6,8,   // 188  ,-./
    4,8,    0,0,    0,0,    0,0,   // 192  @ ...
    0,0,    0,0,    0,0,    0,0,   // 196
    0,0,    0,0,    0,0,    0,0,   // 200
    0,0,    0,0,    0,0,    0,0,   // 204
    0,0,    0,0,    0,0,    0,0,   // 208
    0,0,    0,0,    0,0,    0,0,   // 212
    0,0,    0,0,    0,0,    3,8,   // 216 ...[
    7,8,    5,8,    2,8,    4,7,   // 220 \]#`
    0,0,    0,0,    0,0,    0,0,   // 224
    0,0,    0,0,    0,0,    0,0,   // 228
    0,0,    0,0,    0,0,    0,0,   // 232
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0
};

/* This table is better for games:
    A & S are mapped to Caps Lock and Ctrl
    F1-F9 are mapped to f0-f8 */
static int transTable2[256][2]={
    0,0,    0,0,    0,0,    0,0,   // 0
    0,0,    0,0,    0,0,    0,0,   // 4
    5,9,    6,0,    0,0,    0,0,   // 8 [BS][TAB]..
    0,0,    4,9,    0,0,    0,0,   // 12 .RET..
    0,0,    0,1,    0,0,    -2,-2,   // 16 .CTRL.BREAK
    4,0,    0,0,    0,0,    0,0,   // 20 CAPS...
    0,0,    0,0,    0,0,    7,0,   // 24 ...ESC
    0,0,    0,0,    0,0,    0,0,   // 28
    6,2,    0,0,    0,0,    6,9,   // 32 SPACE..[End]
    0,0,    1,9,    3,9,    7,9,   // 36 .[Left][Up][Right]
    2,9,    0,0,    0,0,    -3,-3,   // 40 [Down]...
    0,0,    0,0,    5,9,    0,0,   // 44 ..[DEL].
    2,7,    3,0,    3,1,    1,1,   // 48 0123
    1,2,    1,3,    3,4,    2,4,   // 52 4567
    1,5,    2,6,    0,0,    0,0,   // 56 89
    0,0,    0,0,    0,0,    0,0,   // 60
    0,0,    4,0,    6,4,    5,2,   // 64  . CAPS B C
    3,2,    2,2,    4,3,    5,3,   // 68  DEFG
    5,4,    2,5,    4,5,    4,6,   // 72  HIJK
    5,6,    6,5,    5,5,    3,6,   // 76  LMNO
    3,7,    1,0,    3,3,    0,1,   // 80  PQR CTRL
    2,3,    3,5,    6,3,    2,1,   // 84  TUVW
    4,2,    4,4,    6,1,    6,2,   // 88  XYZ SPACE
    6,2,    6,2,    0,0,    0,0,   // 92  SPACE SPACE ..
    0,0,    0,0,    0,0,    0,0,   // 96
    0,0,    0,0,    0,0,    0,0,   // 100
    0,0,    0,0,    0,0,    0,0,   // 104
    0,0,    0,0,    0,0,    0,0,   // 108
    2,0,    7,1,    7,2,    7,3,   // 112 F1 F2 F3 F4      - f0 f1 f2 f3
    1,4,    7,4,    7,5,    1,6,   // 116 F5 F6 F7 F8      - f4 f5 f6 f7
    7,6,    2,0,    7,7,    -2,-2,   // 120 F9 F10 F11 F12 - f8 .  f9 BREAK
    0,0,    0,0,    0,0,    0,0,   // 124
    0,0,    0,0,    0,0,    0,0,   // 128
    0,0,    0,0,    0,0,    0,0,   // 132
    0,0,    0,0,    0,0,    0,0,   // 136
    0,0,    0,0,    0,0,    0,0,   // 140
    0,0,    0,0,    0,0,    0,0,   // 144
    0,0,    0,0,    0,0,    0,0,   // 148
    0,0,    0,0,    0,0,    0,0,   // 152
    0,0,    0,0,    0,0,    0,0,   // 156
    0,0,    0,0,    0,0,    0,0,   // 160
    0,0,    0,0,    0,0,    0,0,   // 164
    0,0,    0,0,    0,0,    0,0,   // 168
    0,0,    0,0,    0,0,    0,0,   // 172
    0,0,    0,0,    0,0,    0,0,   // 176
    0,0,    0,0,    0,0,    0,0,   // 180
    0,0,    0,0,    5,7,    1,8,   // 184  ..;=
    6,6,    1,7,    6,7,    6,8,   // 188  ,-./
    4,8,    0,0,    0,0,    0,0,   // 192  @ ...
    0,0,    0,0,    0,0,    0,0,   // 196
    0,0,    0,0,    0,0,    0,0,   // 200
    0,0,    0,0,    0,0,    0,0,   // 204
    0,0,    0,0,    0,0,    0,0,   // 208
    0,0,    0,0,    0,0,    0,0,   // 212
    0,0,    0,0,    0,0,    3,8,   // 216 ...[
    7,8,    5,8,    2,8,    4,7,   // 220 \]#`
    0,0,    0,0,    0,0,    0,0,   // 224
    0,0,    0,0,    0,0,    0,0,   // 228
    0,0,    0,0,    0,0,    0,0,   // 232
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0,
    0,0,    0,0,    0,0,    0,0
};

/* Currently selected translation table */
static int (*transTable)[2] = transTable1;
long WindowPos[4];

/****************************************************************************/
BeebWin::BeebWin()
{
}

/****************************************************************************/
void BeebWin::Initialise()
{
    char CfgName[256];
    char CfgValue[256];
    char DefValue[256];
    int row, col;
    int LEDByte=0;
    LARGE_INTEGER Dummy_Var;
    WINDOWPLACEMENT tWindowPlacement;

    m_DXInit = FALSE;

    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_DIRECT_ENABLED, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_DirectDrawEnabled = atoi(CfgValue);

    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_BUFFER_IN_VIDEO, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_DDS2InVideoRAM = atoi(CfgValue);

    sprintf(DefValue, "%d", IDM_320X256);
    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_WIN_SIZE, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdWinSize = atoi(CfgValue);

  if (m_MenuIdWinSize == IDM_FULLSCREEN) {
    // Handle old configuration
    m_isFullScreen = true;
    m_MenuIdWinSize = IDM_320X256;
  }
  else
    m_isFullScreen   = GetPrivateProfileInt(CFG_VIEW_SECTION, "isFullScreen", 0, CFG_FILE_NAME);

  m_DDFullScreenMode = GetPrivateProfileInt(CFG_VIEW_SECTION, "DDFullScreenMode", ID_VIEW_DD_640X480, CFG_FILE_NAME);
  m_isDD32           = GetPrivateProfileInt(CFG_VIEW_SECTION, "isDD32", 0, CFG_FILE_NAME);

    TranslateWindowSize();

    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_SHOW_FPS, "1",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_ShowSpeedAndFPS = atoi(CfgValue);

    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_MONITOR, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    palette_type = PaletteType(atoi(CfgValue));

    sprintf(DefValue, "%d", IDM_REALTIME);
    GetPrivateProfileString(CFG_SPEED_SECTION, CFG_SPEED_TIMING, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdTiming = atoi(CfgValue);
    TranslateTiming();

    sprintf(DefValue, "%d", IDM_22050KHZ);
    GetPrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_SAMPLE_RATE, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdSampleRate = atoi(CfgValue);
    TranslateSampleRate();

    sprintf(DefValue, "%d", IDM_MEDIUMVOLUME);
    GetPrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_VOLUME, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdVolume = atoi(CfgValue);
    TranslateVolume();

    GetPrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_DIRECT_ENABLED, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    DirectSoundEnabled = atoi(CfgValue);

    GetPrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_STICKS, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdSticks = atoi(CfgValue);

    GetPrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_FREEZEINACTIVE, "1",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_FreezeWhenInactive = atoi(CfgValue);

    GetPrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_HIDE_CURSOR, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_HideCursor = atoi(CfgValue);

    sprintf(DefValue, "%d", IDM_KEYBOARDMAPPING1);
    GetPrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_KEY_MAPPING, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdKeyMapping = atoi(CfgValue);
    TranslateKeyMapping();

    GetPrivateProfileString(CFG_AMX_SECTION, CFG_AMX_ENABLED, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    AMXMouseEnabled = atoi(CfgValue);

    GetPrivateProfileString(CFG_AMX_SECTION, CFG_AMX_LRFORMIDDLE, "1",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    AMXLRForMiddle = atoi(CfgValue);

    sprintf(DefValue, "%d", IDM_AMX_320X256);
    GetPrivateProfileString(CFG_AMX_SECTION, CFG_AMX_SIZE, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdAMXSize = atoi(CfgValue);

    sprintf(DefValue, "%d", IDM_AMX_ADJUSTP30);
    GetPrivateProfileString(CFG_AMX_SECTION, CFG_AMX_ADJUST, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdAMXAdjust = atoi(CfgValue);
    TranslateAMX();

    GetPrivateProfileString(CFG_PRINTER_SECTION, CFG_PRINTER_ENABLED, "0",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    PrinterEnabled = atoi(CfgValue);

    sprintf(DefValue, "%d", IDM_PRINTER_LPT1);
    GetPrivateProfileString(CFG_PRINTER_SECTION, CFG_PRINTER_PORT, DefValue,
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    m_MenuIdPrinterPort = atoi(CfgValue);

    GetPrivateProfileString(CFG_PRINTER_SECTION, CFG_PRINTER_FILE, "",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    strcpy(m_PrinterFileName, CfgValue);
    TranslatePrinterPort();

    for (int key=0; key<256; ++key)
    {
        sprintf(CfgName, "%s%d", CFG_OPTIONS_USER_KEY_MAP, key);
        GetPrivateProfileString(CFG_OPTIONS_SECTION, CfgName, "-1 -1",
                CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
        sscanf(CfgValue, "%d %d", &row, &col);
        if (row != -1 && col != -1)
        {
            UserKeymap[key][0] = row;
            UserKeymap[key][1] = col;
        }
    }

    // insert registry code here
    *binsize=1;
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","MachineType",&MachineType,binsize);
    if (!RegRes) {
        MachineType=0;
        SysReg.CreateKey(HKEY_CURRENT_USER,"Software\\BeebEm");
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","MachineType",&MachineType,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","RelaySoundEnabled",&RelaySoundEnabled,binsize);
    if (!RegRes) {
        RelaySoundEnabled=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","RelaySoundEnabled",&RelaySoundEnabled,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","TubeEnabled",&TubeEnabled,binsize);
    if (!RegRes) {
        TubeEnabled=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","TubeEnabled",&TubeEnabled,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","HideMenuEnabled",&HideMenuEnabled,binsize);
    if (!RegRes) {
        HideMenuEnabled=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","HideMenuEnabled",&HideMenuEnabled,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SerialPortEnabled",&SerialPortEnabled,binsize);
    if (!RegRes) {
        SerialPortEnabled=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SerialPortEnabled",&SerialPortEnabled,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SerialPort",&SerialPort,binsize);
    if (!RegRes) {
        SerialPort=2;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SerialPort",&SerialPort,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","LED Information",&LEDByte,binsize);
    if (!RegRes) {
        LEDByte=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","LED Information",&LEDByte,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","TapeSoundEnabled",&TapeSoundEnabled,binsize);
    if (!RegRes) {
        TapeSoundEnabled=0; // BBC Tapes always were a bit harsh on the ears....
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","TapeSoundEnabled",&TapeSoundEnabled,binsize);
    }
    *binsize=2;
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Tape Clock Speed",&TapeClockSpeed,binsize);
    if (!RegRes) {
        TapeClockSpeed=5600;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Tape Clock Speed",&TapeClockSpeed,binsize);
    }
    *binsize=1;
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","UseHostClock",&UseHostClock,binsize);
    if (!RegRes) {
        UseHostClock=1;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","UseHostClock",&UseHostClock,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","OpCodes",&OpCodes,binsize);
    if (!RegRes) {
        OpCodes=2;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","OpCodes",&OpCodes,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Basic Hardware",&BHardware,binsize);
    if (!RegRes) {
        BHardware=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Basic Hardware",&BHardware,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Teletext Half Mode",&THalfMode,binsize);
    if (!RegRes) {
        THalfMode=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Teletext Half Mode",&THalfMode,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","UsePrimaryBuffer",&UsePrimaryBuffer,binsize);
    if (!RegRes) {
        UsePrimaryBuffer=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","UsePrimaryBuffer",&UsePrimaryBuffer,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Part Samples",&PartSamples,binsize);
    if (!RegRes) {
        PartSamples=1;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Part Samples",&PartSamples,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SoundEnabled",&SoundDefault,binsize);
    if (!RegRes) {
        SoundDefault=1;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SoundEnabled",&SoundDefault,binsize);
    }
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SoundBlockSize",&SBSize,binsize);
    if (!RegRes) {
        SBSize=0;
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SoundBlockSize",&SBSize,binsize);
    }

    DiscLedColour=LED_COLOUR_TYPE;
    LEDs.ShowDisc=LED_SHOW_DISC;
    LEDs.ShowKB=LED_SHOW_KB;

    IgnoreIllegalInstructions = 1;

    m_WriteProtectDisc[0] = !IsDiscWritable(0);
    m_WriteProtectDisc[1] = !IsDiscWritable(1);

    m_hBitmap = m_hOldObj = m_hDCBitmap = NULL;
    m_ScreenRefreshCount = 0;
    m_RelativeSpeed = 1;
    m_FramesPerSecond = 50;
    strcpy(m_szTitle, WindowTitle);

    for(int i=0;i<12;i++)
        cols[i] = i;

    InitClass();
    CreateBeebWindow();
    CreateBitmap();

  m_hMenu = GetMenu(m_hWnd);

  InitMenu();

    m_hDC = GetDC(m_hWnd);

    if (m_DirectDrawEnabled)
        InitDirectX();

//  if (SoundEnabled)
//      SoundInit();

    /* Initialise printer */
    if (PrinterEnabled)
        PrinterEnable(m_PrinterDevice);
    else
        PrinterDisable();

    /* Joysticks can only be initialised after the window is created (needs hwnd) */
    if (m_MenuIdSticks == IDM_JOYSTICK)
        InitJoystick();

    /* Get the applications path - used for disc and state operations */
    // ... and ROMS! - Richard Gellman
    char app_path[_MAX_PATH];
    char app_drive[_MAX_DRIVE];
    char app_dir[_MAX_DIR];
    GetModuleFileName(NULL, app_path, _MAX_PATH);
    _splitpath(app_path, app_drive, app_dir, NULL, NULL);
    _makepath(m_AppPath, app_drive, app_dir, NULL, NULL);

    m_frozen = FALSE;
    strcpy(RomPath, m_AppPath);
    UpdateModelType();
    UpdateSFXMenu();
    UpdateLEDMenu(m_hMenu);
    CheckMenuItem(m_hMenu,ID_TAPESOUND,(TapeSoundEnabled)?MF_CHECKED:MF_UNCHECKED);
    MenuOn=TRUE;
    RegRes=SysReg.GetStringValue(HKEY_CURRENT_USER,"Software\\BeebEm","FDCDLL",FDCDLL);
    if (!RegRes) {
        strcpy(FDCDLL,"None");
        SysReg.SetStringValue(HKEY_CURRENT_USER,"Software\\BeebEm","FDCDLL",FDCDLL);
    }
    NativeFDC=TRUE;
    if (strcmp(FDCDLL,"None")!=0) LoadFDC(FDCDLL);
    if (NativeFDC) {
        CheckMenuItem(m_hMenu,ID_8271,MF_CHECKED);
        CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_UNCHECKED);
    } else {
        CheckMenuItem(m_hMenu,ID_8271,MF_UNCHECKED);
        CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_CHECKED);
    }
    DisplayCycles=7000000;
    if ((NativeFDC) || (MachineType==1)) DisplayCycles=0;
    SetTapeSpeedMenu();
    BOOL pfr;
    pfr=QueryPerformanceFrequency(&Dummy_Var);
    if (!pfr) {
        UseHostClock=0;
        EnableMenuItem(m_hMenu,ID_HOSTCLOCK,MF_GRAYED);
    }
    CheckMenuItem(m_hMenu,ID_HOSTCLOCK,(UseHostClock)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_PSAMPLES,(PartSamples)?MF_CHECKED:MF_UNCHECKED);
    UpdateOptiMenu();
    *binsize=sizeof(tWindowPlacement);
    RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","WindowPos",&tWindowPlacement,binsize);
    if (!RegRes) {
        tWindowPlacement.length=*binsize;
        GetWindowPlacement(GETHWND,&tWindowPlacement);
        SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","WindowPos",&tWindowPlacement,binsize);
    } else {
        if (!m_isFullScreen)
        {
            tWindowPlacement.length=sizeof(tWindowPlacement);
            tWindowPlacement.showCmd=SW_RESTORE;
            SetWindowPlacement(GETHWND,&tWindowPlacement);
        }
    }
}

void BeebWin::LoadFDC(char *DLLName) {
    // Load External DLL(s)
    if (hFDCBoard!=NULL) FreeLibrary(hFDCBoard);
    hFDCBoard=NULL; NativeFDC=TRUE;
    hFDCBoard=LoadLibrary(DLLName);
    if (hFDCBoard==NULL) {
        MessageBox(GETHWND,"Unable to load FDD Extension Board DLL\nReverting to native 8271\n",WindowTitle,MB_OK|MB_ICONERROR);
    }
    if (hFDCBoard!=NULL) {
        PGetBoardProperties=(lGetBoardProperties) GetProcAddress(hFDCBoard,"GetBoardProperties");
        PSetDriveControl=(lSetDriveControl) GetProcAddress(hFDCBoard,"SetDriveControl");
        PGetDriveControl=(lGetDriveControl) GetProcAddress(hFDCBoard,"GetDriveControl");
        if ((PGetBoardProperties==NULL) || (PSetDriveControl==NULL) || (PGetDriveControl==NULL)) {
            MessageBox(GETHWND,"Invalid FDD Extension Board DLL\nReverting to native 8271\n",WindowTitle,MB_OK|MB_ICONERROR); }
        else {
        PGetBoardProperties(&ExtBoard);
        EFDCAddr=ExtBoard.FDCAddress;
        EDCAddr=ExtBoard.DCAddress;
        NativeFDC=FALSE; // at last, a working DLL!
        InvertTR00=ExtBoard.TR00_ActiveHigh;
        SysReg.SetStringValue(HKEY_CURRENT_USER,"Software\\BeebEm","FDCDLL",DLLName); // Write back DLL to registry
        }
    }
    // Set menu options
    if (NativeFDC) {
        CheckMenuItem(m_hMenu,ID_8271,MF_CHECKED);
        CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_UNCHECKED);
    } else {
        CheckMenuItem(m_hMenu,ID_8271,MF_UNCHECKED);
        CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_CHECKED);
    }
    DisplayCycles=7000000;
    if ((NativeFDC) || (MachineType==1)) DisplayCycles=0;
}

void BeebWin::KillDLLs(void) {
    if (hFDCBoard!=NULL) FreeLibrary(hFDCBoard); hFDCBoard=NULL;
}

void BeebWin::SetDriveControl(unsigned char value) {
    // This takes a value from the mem/io decoder, as written by the cpu, runs it through the
    // DLL's translator, then sends it on to the 1770 FDC in master 128 form.
    WriteFDCControlReg(PSetDriveControl(value));
}

unsigned char BeebWin::GetDriveControl(void) {
    // Same as above, but in reverse, i.e. reading
    unsigned char temp,temp2;
    temp=ReadFDCControlReg();
    temp2=PGetDriveControl(temp);
    return(temp2);
}
/****************************************************************************/
BeebWin::~BeebWin()
{
    if (m_DirectDrawEnabled)
    {
        ResetSurfaces();
        m_DD2->Release();
        m_DD->Release();
    }

    if (SoundEnabled)
        SoundReset();

    ReleaseDC(m_hWnd, m_hDC);

    if (m_hOldObj != NULL)
        SelectObject(m_hDCBitmap, m_hOldObj);
    if (m_hBitmap != NULL)
        DeleteObject(m_hBitmap);
    if (m_hDCBitmap != NULL)
        DeleteDC(m_hDCBitmap);
    // Write back position of window for reloading
}

void SaveWindowPos(void) {
    WINDOWPLACEMENT tWindowPlacement;
    tWindowPlacement.length=sizeof(tWindowPlacement);
    GetWindowPlacement(GETHWND,&tWindowPlacement);
    *binsize=sizeof(tWindowPlacement);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","WindowPos",&tWindowPlacement,binsize);
}
/****************************************************************************/
void BeebWin::ResetBeebSystem(unsigned char NewModelType,unsigned char TubeStatus,unsigned char LoadRoms)
{
    SwitchOnCycles=0; // Reset delay
    SoundChipReset();
    SwitchOnSound();
    EnableTube=TubeStatus;
    memset(WholeRam,0,0x8000);
    MachineType=NewModelType;
    if (MachineType==1) {
        memset(FSRam,0,0x2000);
        memset(ShadowRAM,0,0x8000);
        memset(PrivateRAM,0,0x1000);
        ACCCON=0;
        UseShadow=0;
        PRAM=FRAM=Sh_Display=Sh_CPUE=Sh_CPUX=0;
    }
    PagedRomReg=0xf;
    BeebMemInit(LoadRoms);
    Init6502core();
    if (EnableTube) Init65C02core();
    SysVIAReset();
    UserVIAReset();
    VideoInit();
    Disc8271_reset();
    Reset1770();
    AtoDReset();
    SetRomMenu();
    FreeDiscImage(0);
    // Keep the disc images loaded
    FreeDiscImage(1);
    Close1770Disc(0);
    Close1770Disc(1);
    if (MachineType==1) InvertTR00=FALSE;
    if ((MachineType==0) && (NativeFDC)) {
        // 8271 disc
        if ((DiscLoaded[0]) && (CDiscType[0]==0)) LoadSimpleDiscImage(CDiscName[0],0,0,80);
        if ((DiscLoaded[0]) && (CDiscType[0]==1)) LoadSimpleDSDiscImage(CDiscName[0],0,80);
        if ((DiscLoaded[1]) && (CDiscType[1]==0)) LoadSimpleDiscImage(CDiscName[1],0,0,80);
        if ((DiscLoaded[1]) && (CDiscType[1]==1)) LoadSimpleDSDiscImage(CDiscName[1],0,80);
    }
    if (((MachineType==0) && (!NativeFDC)) || (MachineType==1)) {
        // 1770 Disc
        if (DiscLoaded[0]) Load1770DiscImage(CDiscName[0],0,CDiscType[0],m_hMenu);
        if (DiscLoaded[1]) Load1770DiscImage(CDiscName[1],1,CDiscType[1],m_hMenu);
    }
    if ((MachineType==0) && (!NativeFDC)) {
        RegRes=SysReg.GetStringValue(HKEY_CURRENT_USER,"Software\\BeebEm","FDCDLL",FDCDLL);
        if (!RegRes) {
            strcpy(FDCDLL,"None");
            SysReg.SetStringValue(HKEY_CURRENT_USER,"Software\\BeebEm","FDCDLL",FDCDLL);
        }
        NativeFDC=TRUE;
        if (strcmp(FDCDLL,"None")!=0) LoadFDC(FDCDLL);
    }
}
/****************************************************************************/
void BeebWin::CreateBitmap()
{
    if (m_hBitmap != NULL)
        DeleteObject(m_hBitmap);
    if (m_hDCBitmap != NULL)
        DeleteDC(m_hDCBitmap);

    m_hDCBitmap = CreateCompatibleDC(NULL);

    m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bmi.bmiHeader.biWidth = 800;
    m_bmi.bmiHeader.biHeight = -512;
    m_bmi.bmiHeader.biPlanes = 1;
    m_bmi.bmiHeader.biBitCount = 8;
    m_bmi.bmiHeader.biCompression = BI_RGB;
    m_bmi.bmiHeader.biSizeImage = 800*512;
    m_bmi.bmiHeader.biClrUsed = 12;
    m_bmi.bmiHeader.biClrImportant = 12;

#ifdef USE_PALETTE
    __int16 *pInts = (__int16 *)&m_bmi.bmiColors[0];

    for(int i=0; i<12; i++)
        pInts[i] = i;

    m_hBitmap = CreateDIBSection(m_hDCBitmap, (BITMAPINFO *)&m_bmi, DIB_PAL_COLORS,
                            (void**)&m_screen, NULL,0);
#else
    for (int i = 0; i < 9; ++i)
    {
    float r,g,b;
    r = i & 1;
    g = (i & 2) >> 1;
    b = (i & 4) >> 2;

    if (palette_type != RGB) {
      r = g = b = 0.299 * r + 0.587 * g + 0.114 * b;
      switch (palette_type) {
      case AMBER:
        r *= 1.0;
        g *= 0.8;
        b *= 0.1;
        break;
      case GREEN:
        r *= 0.2;
        g *= 0.9;
        b *= 0.1;
        break;
      }
    }

        m_bmi.bmiColors[i].rgbRed   = r * 255;
        m_bmi.bmiColors[i].rgbGreen = g * 255;
        m_bmi.bmiColors[i].rgbBlue  = b * 255;
        m_bmi.bmiColors[i].rgbReserved = 0;
    }

    // Red Leds - left is dark, right is lit.
    m_bmi.bmiColors[8].rgbRed=80;       m_bmi.bmiColors[9].rgbRed=255;
    m_bmi.bmiColors[8].rgbGreen=0;      m_bmi.bmiColors[9].rgbGreen=0;
    m_bmi.bmiColors[8].rgbBlue=0;       m_bmi.bmiColors[9].rgbBlue=0;
    m_bmi.bmiColors[8].rgbReserved=0;   m_bmi.bmiColors[9].rgbReserved=0;
    // Green Leds - left is dark, right is lit.
    m_bmi.bmiColors[10].rgbRed=0;       m_bmi.bmiColors[11].rgbRed=0;
    m_bmi.bmiColors[10].rgbGreen=80;        m_bmi.bmiColors[11].rgbGreen=255;
    m_bmi.bmiColors[10].rgbBlue=0;      m_bmi.bmiColors[11].rgbBlue=0;
    m_bmi.bmiColors[10].rgbReserved=0;  m_bmi.bmiColors[11].rgbReserved=0;

    m_hBitmap = CreateDIBSection(m_hDCBitmap, (BITMAPINFO *)&m_bmi, DIB_RGB_COLORS,
                            (void**)&m_screen, NULL,0);
#endif

    m_hOldObj = SelectObject(m_hDCBitmap, m_hBitmap);
    if(m_hOldObj == NULL)
        MessageBox(m_hWnd,"Cannot select the screen bitmap\n"
                    "Try running in a 256 colour mode",WindowTitle,MB_OK|MB_ICONERROR);
}

/****************************************************************************/
BOOL BeebWin::InitClass(void)
{
    WNDCLASS  wc;

    // Fill in window class structure with parameters that describe the
    // main window.

    wc.style         = CS_HREDRAW | CS_VREDRAW;// Class style(s).
    wc.lpfnWndProc   = (WNDPROC)WndProc;       // Window Procedure
    wc.cbClsExtra    = 0;                      // No per-class extra data.
    wc.cbWndExtra    = 0;                      // No per-window extra data.
    wc.hInstance     = hInst;                  // Owner of this class
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BEEBEM));
    wc.hCursor       = LoadCursor(hInst, IDC_ARROW);
    wc.hbrBackground = NULL; //(HBRUSH)(COLOR_WINDOW+1);// Default color
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU); // Menu from .RC
    wc.lpszClassName = "BEEBWIN"; //szAppName;              // Name to register as

    // Register the window class and return success/failure code.
    return (RegisterClass(&wc));
}

/****************************************************************************/
void BeebWin::CreateBeebWindow(void)
{
    DWORD style;
    int x,y;
    char CfgValue[256];

    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_WIN_XPOS, "-1",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    x=atoi(CfgValue);
    m_XWinPos = x;
    GetPrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_WIN_YPOS, "-1",
            CfgValue, sizeof(CfgValue), CFG_FILE_NAME);
    y=atoi(CfgValue);
    m_YWinPos = y;
    if (x == -1 || y == -1)
    {
        x = CW_USEDEFAULT;
        y = 0;
        m_XWinPos = 0;
        m_YWinPos = 0;
    }

    if (!m_DirectDrawEnabled && m_isFullScreen)
    {
        RECT scrrect;
        SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
        x = scrrect.left;
        y = scrrect.top;
    }

    if (m_DirectDrawEnabled && m_isFullScreen)
    {
        style = WS_POPUP;
    }
    else
    {
        style = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    }

    m_hWnd = CreateWindow(
                "BEEBWIN",              // See RegisterClass() call.
                m_szTitle,      // Text for window title bar.
                style,
                x, y,
                m_XWinSize + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
                m_YWinSize + GetSystemMetrics(SM_CYFIXEDFRAME) * 2
                    + GetSystemMetrics(SM_CYMENUSIZE)
                    + GetSystemMetrics(SM_CYCAPTION)
                    + 1,
                NULL,                   // Overlapped windows have no parent.
                NULL,                // Use the window class menu.
                hInst,           // This instance owns this window.
                NULL                 // We don't use any data in our WM_CREATE
        );

    ShowWindow(m_hWnd, SW_SHOW); // Show the window
    UpdateWindow(m_hWnd);         // Sends WM_PAINT message
}

void BeebWin::ShowMenu(bool on) {
 if (on!=MenuOn) {
  if (on)
    SetMenu(m_hWnd, m_hMenu);
  else
    SetMenu(m_hWnd, NULL);
 }
  MenuOn=on;
}

void BeebWin::TrackPopupMenu(int x, int y) {
  ::TrackPopupMenu(m_hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                   x, y,
                   0,
                   m_hWnd,
                   NULL);
}

/****************************************************************************/
void BeebWin::InitMenu(void)
{
    char menu_string[256];
    HMENU hMenu = m_hMenu;

    CheckMenuItem(hMenu, IDM_SPEEDANDFPS, m_ShowSpeedAndFPS ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, m_MenuIdWinSize, MF_CHECKED);
  CheckMenuItem(hMenu, IDM_FULLSCREEN, m_isFullScreen ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenu, IDM_DD32ONOFF, m_isDD32 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_SOUNDONOFF, SoundEnabled ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_CHECKED);
    CheckMenuItem(hMenu, m_MenuIdVolume, MF_CHECKED);
    CheckMenuItem(hMenu, IDM_ALLOWALLROMWRITES, WritableRoms ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, m_MenuIdTiming, MF_CHECKED);
    if (m_MenuIdSticks != 0)
        CheckMenuItem(hMenu, m_MenuIdSticks, MF_CHECKED);
    CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, m_FreezeWhenInactive ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_HIDECURSOR, m_HideCursor ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS,
                    IgnoreIllegalInstructions ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_CHECKED);
    CheckMenuItem(hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_AMXONOFF, AMXMouseEnabled ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, AMXLRForMiddle ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_CHECKED);
    if (m_MenuIdAMXAdjust != 0)
        CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_CHECKED);
    CheckMenuItem(hMenu, IDM_PRINTERONOFF, PrinterEnabled ? MF_CHECKED : MF_UNCHECKED);
    strcpy(menu_string, "File: ");
    strcat(menu_string, m_PrinterFileName);
    ModifyMenu(hMenu, IDM_PRINTER_FILE, MF_BYCOMMAND, IDM_PRINTER_FILE, menu_string);
    CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
    CheckMenuItem(hMenu, IDM_DDRAWONOFF, m_DirectDrawEnabled ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_DDINVIDEORAM, m_DDS2InVideoRAM ? MF_CHECKED : MF_UNCHECKED);

  CheckMenuItem(hMenu, m_DDFullScreenMode, MF_CHECKED);

  CheckMenuItem(hMenu, ID_SERIAL, SerialPortEnabled ? MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_COM1, (SerialPort==1)? MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_COM2, (SerialPort==2)? MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_COM3, (SerialPort==3)? MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_COM4, (SerialPort==4)? MF_CHECKED:MF_UNCHECKED);

  CheckMenuItem(hMenu, ID_HIDEMENU, HideMenuEnabled ? MF_CHECKED:MF_UNCHECKED);

  UpdateMonitorMenu();

    /* Initialise the ROM Menu. */
    SetRomMenu();
}

void BeebWin::UpdateMonitorMenu() {
    HMENU hMenu = m_hMenu;
  CheckMenuItem(hMenu, ID_MONITOR_RGB, (palette_type == RGB) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_MONITOR_BW , (palette_type == BW) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_MONITOR_GREEN , (palette_type == GREEN) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_MONITOR_AMBER , (palette_type == AMBER) ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::UpdateModelType() {
    HMENU hMenu= m_hMenu;
    CheckMenuItem(hMenu, ID_MODELB, (MachineType == 0) ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_MASTER128, (MachineType == 1) ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::UpdateSFXMenu() {
    HMENU hMenu = m_hMenu;
    CheckMenuItem(hMenu,ID_SFX_RELAY,RelaySoundEnabled?MF_CHECKED:MF_UNCHECKED);
}

/****************************************************************************/
void BeebWin::InitDirectX(void)
{
    HRESULT ddrval;

    ddrval = DirectDrawCreate( NULL, &m_DD, NULL );
    if( ddrval == DD_OK )
    {
        ddrval = m_DD->QueryInterface(IID_IDirectDraw2, (LPVOID *)&m_DD2);
    }
    if( ddrval == DD_OK )
    {
        ddrval = InitSurfaces();
    }
    if( ddrval != DD_OK )
    {
        char  errstr[200];
        sprintf(errstr,"DirectX initialisation failed\nFailure code %X",ddrval);
        MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
    }
}

/****************************************************************************/
HRESULT BeebWin::InitSurfaces(void)
{
    DDSURFACEDESC ddsd;
    DDSCAPS ddscaps2;
    HRESULT ddrval;

    if (m_isFullScreen)
        ddrval = m_DD2->SetCooperativeLevel( m_hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
    else
        ddrval = m_DD2->SetCooperativeLevel( m_hWnd, DDSCL_NORMAL );
    if( ddrval == DD_OK )
    {
    if (m_isFullScreen) {
      if (m_isDD32)
        ddrval = m_DD2->SetDisplayMode(m_XWinSize, m_YWinSize, 32, 0, 0);
      else
          ddrval = m_DD2->SetDisplayMode(m_XWinSize, m_YWinSize, 8, 0, 0);
    }
    }
    if( ddrval == DD_OK )
    {
        ddsd.dwSize = sizeof( ddsd );
        ddsd.dwFlags = DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        if (m_PageFlipping) {
            ddsd.dwFlags|=DDSD_BACKBUFFERCOUNT;
            ddsd.ddsCaps.dwCaps|=DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_VIDEOMEMORY;
            ddsd.dwBackBufferCount=1;
        }

        ddrval = m_DD2->CreateSurface( &ddsd, &m_DDSPrimary, NULL );
    }

    if (m_PageFlipping) {
        ddscaps2.dwCaps=DDSCAPS_BACKBUFFER;
        if (ddrval==DD_OK)
            ddrval=m_DDSPrimary->GetAttachedSurface(&ddscaps2,&m_BackBuffer);
        if (ddrval==DD_OK)
            ddrval=m_BackBuffer->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_BackBuffer2);
    }

    if( ddrval == DD_OK )
        ddrval = m_DDSPrimary->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2Primary);
    if( ddrval == DD_OK )
        ddrval = m_DD2->CreateClipper( 0, &m_Clipper, NULL );
    if( ddrval == DD_OK )
        ddrval = m_Clipper->SetHWnd( 0, m_hWnd );
    if( ddrval == DD_OK )
        ddrval = m_DDS2Primary->SetClipper( m_Clipper );
    if( ddrval == DD_OK )
        {
        ZeroMemory(&ddsd, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        if (m_DDS2InVideoRAM)
            ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
        else
            ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
        ddsd.dwWidth = 800;
        ddsd.dwHeight = 512;
        ddrval = m_DD2->CreateSurface(&ddsd, &m_DDSOne, NULL);
        }
    if( ddrval == DD_OK )
        {
        ddrval = m_DDSOne->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2One);
        }

    m_DXInit = TRUE;

    return ddrval;
}

/****************************************************************************/
void BeebWin::ResetSurfaces(void)
{
    m_Clipper->Release();
    m_DDS2One->Release();
    m_DDSOne->Release();
    m_DDS2Primary->Release();
    m_DDSPrimary->Release();

    m_DXInit = FALSE;
}

/****************************************************************************/
void BeebWin::SetRomMenu(void)
{
    HMENU hMenu = m_hMenu;

    // Set the ROM Titles in the ROM/RAM menu.
    CHAR Title[19];

    int  i;

    for( i=0; i<16; i++ )
    {
        Title[0] = '&';
        _itoa( i, &Title[1], 16 );
        Title[2] = ' ';

        // Get the Rom Title.
        ReadRomTitle( i, &Title[3], sizeof( Title )-4);

        if ( Title[3]== '\0' )
            strcpy( &Title[3], "Empty" );

        ModifyMenu( hMenu,  // handle of menu
                    IDM_ALLOWWRITES_ROM0 + i,
                    MF_BYCOMMAND,   // menu item to modify
                //  MF_STRING,  // menu item flags
                    IDM_ALLOWWRITES_ROM0 + i,   // menu item identifier or pop-up menu handle
                    Title       // menu item content
                    );

    /* LRW Now uncheck the Roms which are NOT writable, that have already been loaded. */
    CheckMenuItem(hMenu, IDM_ALLOWWRITES_ROM0 + i, RomWritable[i] ? MF_CHECKED : MF_UNCHECKED );
  }
}

/****************************************************************************/
void BeebWin::GetRomMenu(void)
{
    HMENU hMenu = m_hMenu;

  for (int i=0; i<16; ++i)
    /* LRW Now uncheck the Roms as NOT writable, that have already been loaded. */
      RomWritable[i] = ( GetMenuState(hMenu, IDM_ALLOWWRITES_ROM0 + i, MF_BYCOMMAND ) & MF_CHECKED );
}

/****************************************************************************/
void BeebWin::GreyRomMenu(BOOL SetToGrey)
{
    HMENU hMenu = m_hMenu;

  for (int i=1; i<16; ++i)
    EnableMenuItem(hMenu, IDM_ALLOWWRITES_ROM0 + i, SetToGrey ? MF_GRAYED : MF_ENABLED );
}

/****************************************************************************/
void BeebWin::InitJoystick(void)
{
    MMRESULT mmresult;

    /* Get joystick updates 10 times a second */
    mmresult = joySetCapture(m_hWnd, JOYSTICKID1, 100, FALSE);
    if (mmresult == JOYERR_NOERROR)
        mmresult = joyGetDevCaps(JOYSTICKID1, &m_JoystickCaps, sizeof(JOYCAPS));

    if (mmresult == JOYERR_NOERROR)
    {
        AtoDInit();
    }
    else if (mmresult == JOYERR_UNPLUGGED)
    {
        MessageBox(m_hWnd, "Joystick is not plugged in",
                    WindowTitle, MB_OK|MB_ICONERROR);
    }
    else
    {
        MessageBox(m_hWnd, "Failed to initialise the joystick",
                    WindowTitle, MB_OK|MB_ICONERROR);
    }
}

/****************************************************************************/
void BeebWin::ScaleJoystick(unsigned int x, unsigned int y)
{
    /* Scale and reverse the readings */
    JoystickX = (int)((double)(m_JoystickCaps.wXmax - x) * 65536.0 /
                        (double)(m_JoystickCaps.wXmax - m_JoystickCaps.wXmin));
    JoystickY = (int)((double)(m_JoystickCaps.wYmax - y) * 65536.0 /
                        (double)(m_JoystickCaps.wYmax - m_JoystickCaps.wYmin));
}

/****************************************************************************/
void BeebWin::ResetJoystick(void)
{
    joyReleaseCapture(JOYSTICKID1);
    AtoDReset();
}

/****************************************************************************/
void BeebWin::SetMousestickButton(int button)
{
    if (m_MenuIdSticks == IDM_MOUSESTICK)
        JoystickButton = button;
}

/****************************************************************************/
void BeebWin::ScaleMousestick(unsigned int x, unsigned int y)
{
    if (m_MenuIdSticks == IDM_MOUSESTICK)
    {
        JoystickX = (m_XWinSize - x - 1) * 65536 / m_XWinSize;
        JoystickY = (m_YWinSize - y - 1) * 65536 / m_YWinSize;
    }

    if (m_HideCursor)
        SetCursor(NULL);
}

/****************************************************************************/
void BeebWin::SetAMXPosition(unsigned int x, unsigned int y)
{
    if (AMXMouseEnabled)
    {
        // Scale the window coords to the beeb screen coords
        AMXTargetX = x * m_AMXXSize * (100 + m_AMXAdjust) / 100 / m_XWinSize;
        AMXTargetY = y * m_AMXYSize * (100 + m_AMXAdjust) / 100 / m_YWinSize;

        AMXMouseMovement();
    }
}

/****************************************************************************/
LRESULT CALLBACK WndProc(
                HWND hWnd,         // window handle
                UINT message,      // type of message
                WPARAM uParam,     // additional information
                LPARAM lParam)     // additional information
{
    int wmId, wmEvent;
    HDC hdc;
    int row, col;

    switch (message)
    {
        case WM_COMMAND:  // message: command from application menu
            wmId    = LOWORD(uParam);
            wmEvent = HIWORD(uParam);
            if (mainWin)
                mainWin->HandleCommand(wmId);
            break;

        case WM_PALETTECHANGED:
            if(!mainWin)
                break;
            if ((HWND)uParam == hWnd)
                break;

            // fall through to WM_QUERYNEWPALETTE
        case WM_QUERYNEWPALETTE:
            if(!mainWin)
                break;

            hdc = GetDC(hWnd);
            mainWin->RealizePalette(hdc);
            ReleaseDC(hWnd,hdc);
            return TRUE;
            break;

        case WM_PAINT:
            if(mainWin != NULL) {
                PAINTSTRUCT ps;
                HDC         hDC;

                hDC = BeginPaint(hWnd, &ps);
                mainWin->RealizePalette(hDC);
                mainWin->updateLines(hDC, 0, (TeletextEnabled)?500:256);
                EndPaint(hWnd, &ps);
            }
            break;

        case WM_KEYDOWN:
            if(TranslateKey(uParam, &row, &col)>=0)
                BeebKeyDown(row, col);
            break;

        case WM_KEYUP:
            if(TranslateKey(uParam,&row, &col)>=0)
                BeebKeyUp(row, col);
            else if(row==-2)
            { // Must do a reset!
                Init6502core();
                Disc8271_reset();
                Reset1770();
                //SoundChipReset();
            }
            else if(row==-3)
            {
                if (col==-3) SoundTuning+=0.1; // Page Up
                if (col==-4) SoundTuning-=0.1; // Page Down
            }
            break;

        case WM_SETFOCUS:
            if (mainWin)
                mainWin->Focus(TRUE);
            break;

        case WM_KILLFOCUS:
            BeebReleaseAllKeys();
            if (mainWin)
                mainWin->Focus(FALSE);
            break;

        case MM_JOY1MOVE:
            if (mainWin)
                mainWin->ScaleJoystick(LOWORD(lParam), HIWORD(lParam));
            break;

        case MM_JOY1BUTTONDOWN:
        case MM_JOY1BUTTONUP:
            JoystickButton = ((UINT)uParam & (JOY_BUTTON1 | JOY_BUTTON2)) ? 1 : 0;
            break;

        case WM_MOUSEMOVE:
            if (mainWin)
            {
                mainWin->ScaleMousestick(LOWORD(lParam), HIWORD(lParam));
                mainWin->SetAMXPosition(LOWORD(lParam), HIWORD(lParam));
// Experiment: show menu in full screen when cursor moved to top of window
                    if (HideMenuEnabled) {
                        if (HIWORD(lParam) <= 2)
                            mainWin->ShowMenu(true);
                        else
                            mainWin->ShowMenu(false);
                    }
            }
            break;

        case WM_LBUTTONDOWN:
            if (mainWin)
                mainWin->SetMousestickButton(((UINT)uParam & MK_LBUTTON) ? TRUE : FALSE);
            AMXButtons |= AMX_LEFT_BUTTON;
            break;
        case WM_LBUTTONUP:
            if (mainWin)
                mainWin->SetMousestickButton(((UINT)uParam & MK_LBUTTON) ? TRUE : FALSE);
            AMXButtons &= ~AMX_LEFT_BUTTON;
            break;

        case WM_MBUTTONDOWN:
            AMXButtons |= AMX_MIDDLE_BUTTON;
            break;
        case WM_MBUTTONUP:
            AMXButtons &= ~AMX_MIDDLE_BUTTON;
            break;

        case WM_RBUTTONDOWN:
            AMXButtons |= AMX_RIGHT_BUTTON;
            break;
        case WM_RBUTTONUP:
// Experiment: tried popping up the main menu on right click in full screen,
// however, it doesn't display corrently.
//      if (mainWin && mainWin->IsFullScreen() && (!AMXMouseEnabled || (uParam & MK_SHIFT)))
//        mainWin->TrackPopupMenu(LOWORD(lParam), HIWORD(lParam));
            AMXButtons &= ~AMX_RIGHT_BUTTON;
            break;

        case WM_DESTROY:  // message: window being destroyed
            PostQuitMessage(0);
            break;

        case WM_ENTERMENULOOP: // entering menu, must mute directsound
            SetSound(MUTED);
            break;

        case WM_EXITMENULOOP:
            SetSound(UNMUTED);
            break;

        default:          // Passes it on if unproccessed
            return (DefWindowProc(hWnd, message, uParam, lParam));
        }
    return (0);
}

/****************************************************************************/
int TranslateKey(int vkey, int *row, int *col)
{
    row[0] = transTable[vkey][0];
    col[0] = transTable[vkey][1];

    return(row[0]);
}

/****************************************************************************/
int BeebWin::StartOfFrame(void)
{
    int FrameNum = 1;

    if (UpdateTiming()) {
        FrameNum = 0;
        // Blank screen on frame 0
        memset(m_screen,0,800*((TeletextEnabled)?500:256));
    }

    return FrameNum;
}

void BeebWin::doLED(int sx,bool on) {
    int tsy; char colbase;
    colbase=(DiscLedColour*2)+8; // colour will be 0 for red, 1 for green.
    if (sx<100) colbase=8; // Red leds for keyboard always
    if (TeletextEnabled) tsy=496; else tsy=254;
    doUHorizLine(mainWin->cols[((on)?1:0)+colbase],tsy,sx,8);
    doUHorizLine(mainWin->cols[((on)?1:0)+colbase],tsy,sx,8);
};

/****************************************************************************/
void BeebWin::updateLines(HDC hDC, int starty, int nlines)
{
    WINDOWPLACEMENT wndpl;
    HRESULT ddrval;
    HDC hdc;
    int TTLines=0;
    int TextStart=236;

    if (TeletextEnabled) TextStart=(480/TeletextStyle)-((TeletextStyle==2)?12:0);

    ++m_ScreenRefreshCount;
    TTLines=500/TeletextStyle;

    if (!m_DirectDrawEnabled)
    {
        if ((m_MenuIdWinSize == IDM_640X256) && (!TeletextEnabled))
        {
            BitBlt(hDC, 0, starty, 800, (TeletextEnabled)?TTLines:256, m_hDCBitmap, 0, starty, SRCCOPY);
        }
        else
        {
            int win_starty = starty * m_YWinSize / 256;
            int win_nlines = 256 * m_YWinSize / 256;
            if ((DisplayCycles>0) && (hFDCBoard!=NULL)) {
                SetBkMode(hDC,TRANSPARENT);
                SetTextColor(hDC,0x808080);
                TextOut(hDC,0,TextStart,ExtBoard.BoardName,strlen(ExtBoard.BoardName));
            }
            StretchBlt(hDC, 0, win_starty, m_XWinSize, win_nlines,
                m_hDCBitmap, 0, starty, (TeletextEnabled)?552:ActualScreenWidth, (TeletextEnabled==1)?TTLines:256, SRCCOPY);
        }
    }
    else
    {
        if (m_DXInit == FALSE)
            return;

        wndpl.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(m_hWnd, &wndpl))
        {
            if (wndpl.showCmd == SW_SHOWMINIMIZED)
                return;
        }

        // Blit the beeb bitbap onto the secondary buffer
        ddrval = m_DDS2One->GetDC(&hdc);
        if (ddrval == DDERR_SURFACELOST)
        {
            ddrval = m_DDS2One->Restore();
            if (ddrval == DD_OK)
                ddrval = m_DDS2One->GetDC(&hdc);
        }
        if (ddrval == DD_OK)
        {
            BitBlt(hdc, 0, starty, 800, nlines, m_hDCBitmap, 0, starty, SRCCOPY);

            if ((DisplayCycles>0) && (hFDCBoard!=NULL)) {
                SetBkMode(hdc,TRANSPARENT);
                SetTextColor(hdc,0x808080);
                TextOut(hdc,0,TextStart,ExtBoard.BoardName,strlen(ExtBoard.BoardName));
            }

            if (m_ShowSpeedAndFPS && m_isFullScreen)
            {
                char fps[50];
                sprintf(fps, "%2.2f %2d %04x", m_RelativeSpeed, (int)m_FramesPerSecond,ProgramCounter);
                SetBkMode(hdc,TRANSPARENT);
                SetTextColor(hdc,0x808080);
                TextOut(hdc,400,TextStart,fps,strlen(fps));
            }

            m_DDS2One->ReleaseDC(hdc);

            // Work out where on screen to blit image
            RECT destRect;
            RECT srcRect;
            POINT pt;
            GetClientRect( m_hWnd, &destRect );
            pt.x = pt.y = 0;
            ClientToScreen( m_hWnd, &pt );
            OffsetRect(&destRect, pt.x, pt.y);

            // Blit the whole of the secondary buffer onto the screen
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = (TeletextEnabled)?552:ActualScreenWidth;
            srcRect.bottom = (TeletextEnabled)?TTLines:256;

            if (!m_PageFlipping) {
                ddrval = m_DDS2Primary->Blt( &destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL);
                if (ddrval == DDERR_SURFACELOST)
                {
                    ddrval = m_DDS2Primary->Restore();
                    if (ddrval == DD_OK)
                        ddrval = m_DDS2Primary->Blt( &destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL );
                }
            } else {
                ddrval = m_BackBuffer2->Blt( &destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL);
                if (ddrval == DDERR_SURFACELOST)
                {
                    ddrval = m_BackBuffer2->Restore();
                    if (ddrval == DD_OK)
                        ddrval = m_BackBuffer2->Blt( &destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL );
                }
                if (ddrval == DD_OK)
                    m_DDSPrimary->Flip(NULL,NULL);
            }
        }

        if (ddrval != DD_OK && ddrval != DDERR_WASSTILLDRAWING)
        {
            char  errstr[200];
            sprintf(errstr,"DirectX failure while updating screen\nFailure code %X",ddrval);
            MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
        }
    }
}

/****************************************************************************/
BOOL BeebWin::UpdateTiming(void)
{
    int Ticks;
    bool UpdateScreen=FALSE;
    int CycleTarget=0;
    int CT,NT; // Ticks for waiting a millisecond
    double TCycles;

    TCycles=(m_RealTimeTarget)?m_RealTimeTarget:1;

    Ticks=GetTickCount() % 1000;
    if (Ticks<LastTickCount) {
        if (SecCycles>(2000000*TCycles)) SecCycles-=(2000000*TCycles);
        else SecCycles=0;
        m_FramesPerSecond=m_ScreenRefreshCount;
        m_ScreenRefreshCount=0;
        if (!m_isFullScreen) DisplayTiming();
    }
    LastTickCount=Ticks;
    // Now we work out if BeebEm is running too fast or not
    CycleTarget=(Ticks*2000)*TCycles;
    if (m_RealTimeTarget>0) {
        UpdateScreen=FALSE;
        if (SecCycles>=CycleTarget) {
            UpdateScreen=TRUE;
            CT=GetTickCount();
            NT=(SecCycles-CycleTarget)/(2000*m_RealTimeTarget);
            do { } while (GetTickCount()<CT+NT);
        }
    }
    if (m_FPSTarget>0) {
        UpdateScreen=TRUE;
        if (((2000000/m_FPSTarget)*m_ScreenRefreshCount)>(Ticks*2000)) UpdateScreen=FALSE;
        if (m_ScreenRefreshCount<1) UpdateScreen=TRUE;
    }
    m_RelativeSpeed=(double)((float)SecCycles/(float)(CycleTarget/TCycles));
    return UpdateScreen;
}

/****************************************************************************/
void BeebWin::DisplayTiming(void)
{
    if (m_ShowSpeedAndFPS && /*(!m_DirectDrawEnabled ||*/ !m_isFullScreen) //)
    {
        sprintf(m_szTitle, "%s  Speed: %2.2f  fps: %2d",
                WindowTitle, m_RelativeSpeed, (int)m_FramesPerSecond);
        SetWindowText(m_hWnd, m_szTitle);
    }
}

/****************************************************************************/
void BeebWin::TranslateWindowSize(void)
{
  if (m_isFullScreen) {
        if (m_DirectDrawEnabled)
        {
      switch (m_DDFullScreenMode) {
      case ID_VIEW_DD_640X480:
              m_XWinSize = 640;
              m_YWinSize = 480;
        break;
      case ID_VIEW_DD_1024X768:
              m_XWinSize = 1024;
              m_YWinSize = 768;
        break;
      case ID_VIEW_DD_1280X1024:
              m_XWinSize = 1280;
              m_YWinSize = 1024;
        break;
      }
//        m_YWinSize -= GetSystemMetrics(SM_CYMENUSIZE);
        }
        else
        {
            RECT scrrect;
            SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
            m_XWinSize = scrrect.right - scrrect.left - GetSystemMetrics(SM_CXFIXEDFRAME) * 2;
            m_YWinSize = scrrect.bottom - scrrect.top - GetSystemMetrics(SM_CYFIXEDFRAME) * 2
                    - GetSystemMetrics(SM_CYMENUSIZE) - GetSystemMetrics(SM_CYCAPTION);
        }
  }
  else switch (m_MenuIdWinSize)
      {
      case IDM_160X128:
          m_XWinSize = 160;
          m_YWinSize = 128;
          break;

      case IDM_240X192:
          m_XWinSize = 240;
          m_YWinSize = 192;
          break;

      case IDM_640X256:
          m_XWinSize = 640;
          m_YWinSize = 256;
          break;

      default:
      case IDM_320X256:
          m_XWinSize = 320;
          m_YWinSize = 256;
          break;

      case IDM_640X512:
          m_XWinSize = 640;
          m_YWinSize = 512;
          break;

      case IDM_800X600:
          m_XWinSize = 800;
          m_YWinSize = 600;
          break;

      case IDM_1024X768:
          m_XWinSize = 1024;
          m_YWinSize = 768;
          break;

      case IDM_1024X512:
          m_XWinSize = 1024;
          m_YWinSize = 512;
          break;
      }
}

/****************************************************************************/
void BeebWin::TranslateSampleRate(void)
{
    switch (m_MenuIdSampleRate)
    {
    case IDM_44100KHZ:
        SoundSampleRate = 44100;
        break;

    default:
    case IDM_22050KHZ:
        SoundSampleRate = 22050;
        break;

    case IDM_11025KHZ:
        SoundSampleRate = 11025;
        break;
    }
}

/****************************************************************************/
void BeebWin::TranslateVolume(void)
{
    switch (m_MenuIdVolume)
    {
    case IDM_FULLVOLUME:
        SoundVolume = 1;
        break;

    case IDM_HIGHVOLUME:
        SoundVolume = 2;
        break;

    default:
    case IDM_MEDIUMVOLUME:
        SoundVolume = 3;
        break;

    case IDM_LOWVOLUME:
        SoundVolume = 4;
        break;
    }
}

/****************************************************************************/
void BeebWin::TranslateTiming(void)
{
    m_FPSTarget = 0;
    m_RealTimeTarget = 1.0;

    switch (m_MenuIdTiming)
    {
    default:
    case IDM_REALTIME:
        if (m_RealTimeTarget==0) SecCycles=(GetTickCount() % 1000)*2000;
        m_RealTimeTarget = 1.0;
        m_FPSTarget = 0;
        break;

    case IDM_3QSPEED:
        if (m_RealTimeTarget==0) SecCycles=(GetTickCount() % 1000)*2000;
        m_RealTimeTarget = 0.75;
        m_FPSTarget = 0;
        break;

    case IDM_HALFSPEED:
        if (m_RealTimeTarget==0 )SecCycles=(GetTickCount() % 1000)*2000;
        m_RealTimeTarget = 0.5;
        m_FPSTarget = 0;
        break;

    case IDM_50FPS:
        m_FPSTarget = 50;
        m_RealTimeTarget = 0;
        break;

    case IDM_25FPS:
        m_FPSTarget = 25;
        m_RealTimeTarget = 0;
        break;

    case IDM_10FPS:
        m_FPSTarget = 10;
        m_RealTimeTarget = 0;
        break;

    case IDM_5FPS:
        m_FPSTarget = 5;
        m_RealTimeTarget = 0;
        break;

    case IDM_1FPS:
        m_FPSTarget = 1;
        m_RealTimeTarget = 0;
        break;
    }
}

/****************************************************************************/
void BeebWin::TranslateKeyMapping(void)
{
    switch (m_MenuIdKeyMapping )
    {
    default:
    case IDM_KEYBOARDMAPPING1:
        transTable = transTable1;
        break;

    case IDM_KEYBOARDMAPPING2:
        transTable = transTable2;
        break;

    case IDM_USERKYBDMAPPING:
        transTable = UserKeymap;
        break;
    }
}

void BeebWin::SetImageName(char *DiscName,char Drive,char DType) {
    strcpy(CDiscName[Drive],DiscName);
    CDiscType[Drive]=DType;
    DiscLoaded[Drive]=TRUE;
}

/****************************************************************************/
void BeebWin::ReadDisc(int Drive,HMENU dmenu)
{
    char StartPath[_MAX_PATH], DefaultPath[_MAX_PATH];
    char FileName[256];
    OPENFILENAME ofn;

    strcpy(DefaultPath, m_AppPath);
    strcat(DefaultPath, "discims");

  GetProfileString("BeebEm", "DiscsPath", DefaultPath, StartPath, _MAX_PATH);
  ofn.nFilterIndex = GetProfileInt("BeebEm", "LoadDiscFilter", 1);

    FileName[0] = '\0';

    /* Hmm, what do I put in all these fields! */
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Auto (*.ssd;*.dsd;*.ad*;*.img)\0*.ssd;*.dsd;*.adl;*.adf;*.img\0"
                    "ADFS Disc (*.adl *.adf)\0*.adl;*.adf\0"
                    "Single Sided Disc (*.ssd)\0*.ssd\0"
                    "Double Sided Disc (*.dsd)\0*.dsd\0"
                    "Single Sided Disc (*.*)\0*.*\0"
                    "Double Sided Disc (*.*)\0*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = StartPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetOpenFileName(&ofn))
    {
    unsigned PathLength = strrchr(FileName, '\\') - FileName;
    strncpy(DefaultPath, FileName, PathLength);
    DefaultPath[PathLength] = 0;
    WriteProfileString("BeebEm", "DiscsPath", DefaultPath);
    WriteProfileString("BeebEm", "LoadDiscFilter", itoa(ofn.nFilterIndex, DefaultPath, _MAX_PATH));

    bool dsd = false;
    bool adfs = false;
    switch (ofn.nFilterIndex) {
    case 1:
      {
        char *ext = strrchr(FileName, '.');
        if (ext != NULL)
          if (stricmp(ext+1, "dsd") == 0)
            dsd = true;
          if (stricmp(ext+1, "adl") == 0)
            adfs = true;
          if (stricmp(ext+1, "adf") == 0)
            adfs = true;
        break;
      }
    case 2:
        adfs=true;
        break;
    case 4:
    case 6:
      dsd = true;
    }
    // Another Master 128 Update, brought to you by Richard Gellman
    if (MachineType==0) {
        if (dsd) {
            if (NativeFDC) LoadSimpleDSDiscImage(FileName, Drive, 80);
            else Load1770DiscImage(FileName,Drive,1,dmenu); } // 1 = dsd
        if ((!dsd) && (!adfs)) {
            if (NativeFDC) LoadSimpleDiscImage(FileName, Drive, 0, 80);
            else Load1770DiscImage(FileName,Drive,0,dmenu); } // 0 = ssd
        if (adfs) {
            if (NativeFDC) MessageBox(GETHWND,"The native 8271 FDC cannot read ADFS discs\n","BeebEm",MB_OK|MB_ICONERROR);
            else Load1770DiscImage(FileName,Drive,2,dmenu); } // 2 = adfs
    }

    if (MachineType==1) {
        if (dsd)
            Load1770DiscImage(FileName,Drive,1,dmenu); // 0 = ssd
        if (!dsd && !adfs)                       // Here we go a transposing...
            Load1770DiscImage(FileName,Drive,0,dmenu); // 1 = dsd
        if (adfs)
            Load1770DiscImage(FileName,Drive,2,dmenu); // ADFS OO La La!
    }

        /* Write protect the disc */
        if (!m_WriteProtectDisc[Drive])
            ToggleWriteProtect(Drive);
    }
    strcpy(DefaultPath, m_AppPath);
}

/****************************************************************************/
void BeebWin::LoadTape(void)
{
    char DefaultPath[_MAX_PATH];
    char FileName[256];
    OPENFILENAME ofn;

    strcpy(DefaultPath, m_AppPath);
    strcat(DefaultPath, "tapes");

//  GetProfileString("BeebEm", "DiscsPath", DefaultPath, StartPath, _MAX_PATH);
//  ofn.nFilterIndex = GetProfileInt("BeebEm", "LoadDiscFilter", 1);
    ofn.nFilterIndex = 1;
    FileName[0] = '\0';

    /* Hmm, what do I put in all these fields! */
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "UEF Tape File (*.uef)\0*.uef\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = DefaultPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetOpenFileName(&ofn))
    {
    unsigned PathLength = strrchr(FileName, '\\') - FileName;
    strncpy(DefaultPath, FileName, PathLength);
    DefaultPath[PathLength] = 0;
//    WriteProfileString("BeebEm", "DiscsPath", DefaultPath);
//    WriteProfileString("BeebEm", "LoadDiscFilter", itoa(ofn.nFilterIndex, DefaultPath, _MAX_PATH));
    LoadUEF(FileName);
    }
    bool dsd = false;
    bool adfs = false;

    strcpy(DefaultPath, m_AppPath);
}

void BeebWin::SelectFDC(void)
{
    char DefaultPath[_MAX_PATH];
    char FileName[256];
    OPENFILENAME ofn;

    strcpy(DefaultPath, m_AppPath);
    strcat(DefaultPath, "Hardware");

//  GetProfileString("BeebEm", "DiscsPath", DefaultPath, StartPath, _MAX_PATH);
//  ofn.nFilterIndex = GetProfileInt("BeebEm", "LoadDiscFilter", 1);
    ofn.nFilterIndex = 1;
    FileName[0] = '\0';

    /* Hmm, what do I put in all these fields! */
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "FDC Extension Board Plugin DLL (*.dll)\0*.dll\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = DefaultPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetOpenFileName(&ofn))
    {
    unsigned PathLength = strrchr(FileName, '\\') - FileName;
    strncpy(DefaultPath, FileName, PathLength);
    DefaultPath[PathLength] = 0;
//    WriteProfileString("BeebEm", "DiscsPath", DefaultPath);
//    WriteProfileString("BeebEm", "LoadDiscFilter", itoa(ofn.nFilterIndex, DefaultPath, _MAX_PATH));
    LoadFDC(FileName);
    }
    bool dsd = false;
    bool adfs = false;

    strcpy(DefaultPath, m_AppPath);
}

/****************************************************************************/
void BeebWin::NewDiscImage(int Drive)
{
    char StartPath[_MAX_PATH], DefaultPath[_MAX_PATH];
    char FileName[256];
    OPENFILENAME ofn;

    strcpy(DefaultPath, m_AppPath);
    strcat(DefaultPath, "discims");

  GetProfileString("BeebEm", "DiscsPath", DefaultPath, StartPath, _MAX_PATH);
  ofn.nFilterIndex = GetProfileInt("BeebEm", "NewDiscFilter", 1);

    FileName[0] = '\0';

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Single Sided Disc (*.ssd)\0*.ssd\0"
                    "Double Sided Disc (*.dsd)\0*.dsd\0"
                    "Single Sided Disc\0*.*\0"
                    "Double Sided Disc\0*.*\0"
                    "ADFS M ( 80 Track) Disc\0*.adf\0"
                    "ADFS L (160 Track) Disc\0*.adl\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = StartPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetSaveFileName(&ofn))
    {
    unsigned PathLength = strrchr(FileName, '\\') - FileName;
    strncpy(DefaultPath, FileName, PathLength);
    DefaultPath[PathLength] = 0;
    WriteProfileString("BeebEm", "DiscsPath", DefaultPath);
    WriteProfileString("BeebEm", "NewDiscFilter", itoa(ofn.nFilterIndex, DefaultPath, _MAX_PATH));

        /* Add a file extension if the user did not specify one */
        if (strchr(FileName, '.') == NULL)
        {
            if (ofn.nFilterIndex == 1 ||
          ofn.nFilterIndex == 3)
                strcat(FileName, ".ssd");
            if (ofn.nFilterIndex == 2 ||
          ofn.nFilterIndex == 4)
                strcat(FileName, ".dsd");
            if (ofn.nFilterIndex==5)
                strcat(FileName, ".adf");
            if (ofn.nFilterIndex==6)
                strcat(FileName, ".adl");
        }

        if (ofn.nFilterIndex == 1 ||
        ofn.nFilterIndex == 3)
        {
            CreateDiscImage(FileName, Drive, 1, 80);
        }
        if (ofn.nFilterIndex == 2 ||
        ofn.nFilterIndex == 4)
        {
            CreateDiscImage(FileName, Drive, 2, 80);
        }
        if (ofn.nFilterIndex == 5) CreateADFSImage(FileName,Drive,80,m_hMenu);
        if (ofn.nFilterIndex == 6) CreateADFSImage(FileName,Drive,160,m_hMenu);

        /* Allow disc writes */
        if (m_WriteProtectDisc[Drive])
            ToggleWriteProtect(Drive);
        DWriteable[Drive]=1;
        DiscLoaded[Drive]=TRUE;
        strcpy(CDiscName[1],FileName);
    }
strcpy(DefaultPath, m_AppPath);
}

/****************************************************************************/
void BeebWin::SaveState()
{
    char StartPath[_MAX_PATH], DefaultPath[_MAX_PATH];
    char FileName[260];
    bool uef;
    OPENFILENAME ofn;

    strcpy(DefaultPath, m_AppPath);
    strcat(DefaultPath, "beebstate");

  GetProfileString("BeebEm", "StatesPath", DefaultPath, StartPath, _MAX_PATH);

    FileName[0] = '\0';

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "UEF State File\0*.uef\0"
                      "Beeb State File\0*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = StartPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetSaveFileName(&ofn))
    {
    unsigned PathLength = strrchr(FileName, '\\') - FileName;
    strncpy(DefaultPath, FileName, PathLength);
    DefaultPath[PathLength] = 0;
    WriteProfileString("BeebEm", "StatesPath", DefaultPath);

    uef=FALSE;
    // Add UEF extension if not already set and is UEF
    if (ofn.nFilterIndex==1) {
        if ((strcmp(FileName+(strlen(FileName)-4),".UEF")!=0) &&
            (strcmp(FileName+(strlen(FileName)-4),".uef")!=0)) {
            strcat(FileName,".uef");
        }
        uef=TRUE;
    }
    if (!uef) BeebSaveState(FileName); else SaveUEFState(FileName);
    }

strcpy(DefaultPath, m_AppPath);
}

/****************************************************************************/
void BeebWin::RestoreState()
{
    char StartPath[_MAX_PATH], DefaultPath[_MAX_PATH];
    char FileName[256];
    OPENFILENAME ofn;

    strcpy(DefaultPath, m_AppPath);
    strcat(DefaultPath, "beebstate");

  GetProfileString("BeebEm", "StatesPath", DefaultPath, StartPath, _MAX_PATH);

    FileName[0] = '\0';

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "UEF State File\0*.uef\0"
                      "Beeb State File\0*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = StartPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetOpenFileName(&ofn))
    {
    unsigned PathLength = strrchr(FileName, '\\') - FileName;
    strncpy(DefaultPath, FileName, PathLength);
    DefaultPath[PathLength] = 0;
    WriteProfileString("BeebEm", "StatesPath", DefaultPath);

        if (ofn.nFilterIndex==2) BeebRestoreState(FileName); else LoadUEFState(FileName);
    }
strcpy(DefaultPath, m_AppPath);
}

/****************************************************************************/
void BeebWin::ToggleWriteProtect(int Drive)
{
    HMENU hMenu = m_hMenu;
  if (MachineType==0) {
    if (m_WriteProtectDisc[Drive])
    {
        m_WriteProtectDisc[Drive] = 0;
        DiscWriteEnable(Drive, 1);
    }
    else
    {
        m_WriteProtectDisc[Drive] = 1;
        DiscWriteEnable(Drive, 0);
    }

    if (Drive == 0)
        CheckMenuItem(hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
    else
        CheckMenuItem(hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
  }
  if (MachineType==1) {
      DWriteable[Drive]=1-DWriteable[Drive];
    if (Drive == 0)
        CheckMenuItem(hMenu, IDM_WPDISC0, DWriteable[0] ? MF_UNCHECKED : MF_CHECKED);
    else
        CheckMenuItem(hMenu, IDM_WPDISC1, DWriteable[1] ? MF_UNCHECKED : MF_CHECKED);
  }
}

/****************************************************************************/
BOOL BeebWin::PrinterFile()
{
    char StartPath[_MAX_PATH];
    char FileName[256];
    OPENFILENAME ofn;
    BOOL changed;

    if (strlen(m_PrinterFileName) == 0)
    {
        strcpy(StartPath, m_AppPath);
        FileName[0] = '\0';
    }
    else
    {
        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        char fname[_MAX_FNAME];
        char ext[_MAX_EXT];
        _splitpath(m_PrinterFileName, drive, dir, fname, ext);
        _makepath(StartPath, drive, dir, NULL, NULL);
        _makepath(FileName, NULL, NULL, fname, ext);
    }

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Printer Output\0*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 0;
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = sizeof(FileName);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = StartPath;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    changed = GetSaveFileName(&ofn);
    if (changed)
    {
        strcpy(m_PrinterFileName, FileName);
    }

    return(changed);
}

/****************************************************************************/
void BeebWin::TogglePrinter()
{
    BOOL FileOK = TRUE;
    HMENU hMenu = m_hMenu;

    if (PrinterEnabled)
    {
        PrinterDisable();
    }
    else
    {
        if (m_MenuIdPrinterPort == IDM_PRINTER_FILE)
        {
            if (strlen(m_PrinterFileName) == 0)
                PrinterFile();
            if (strlen(m_PrinterFileName) != 0)
            {
                /* First check if file already exists */
                FILE *outfile;
                outfile=fopen(m_PrinterFileName,"rb");
                if (outfile != NULL)
                {
                    fclose(outfile);
                    char errstr[200];
                    sprintf(errstr, "File already exists:\n  %s\n\nOverwrite file?", m_PrinterFileName);
                    if (MessageBox(m_hWnd,errstr,WindowTitle,MB_YESNO|MB_ICONQUESTION) != IDYES)
                        FileOK = FALSE;
                }
                if (FileOK == TRUE)
                    PrinterEnable(m_PrinterFileName);
            }
        }
        else
        {
            PrinterEnable(m_PrinterDevice);
        }
    }

    CheckMenuItem(hMenu, IDM_PRINTERONOFF, PrinterEnabled ? MF_CHECKED : MF_UNCHECKED);
}

/****************************************************************************/
void BeebWin::TranslatePrinterPort()
{
    switch (m_MenuIdPrinterPort)
    {
    case IDM_PRINTER_FILE:
        strcpy(m_PrinterDevice, m_PrinterFileName);
        break;
    default:
    case IDM_PRINTER_LPT1:
        strcpy(m_PrinterDevice, "LPT1");
        break;
    case IDM_PRINTER_LPT2:
        strcpy(m_PrinterDevice, "LPT2");
        break;
    case IDM_PRINTER_LPT3:
        strcpy(m_PrinterDevice, "LPT3");
        break;
    case IDM_PRINTER_LPT4:
        strcpy(m_PrinterDevice, "LPT4");
        break;
    }
}

/****************************************************************************/
void BeebWin::SavePreferences()
{
    char CfgValue[256];
    char CfgName[256];
    RECT wndrect;
    int LEDByte;

    sprintf(CfgValue, "%d", m_DirectDrawEnabled);
    WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_DIRECT_ENABLED,
      CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_DDS2InVideoRAM);
    WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_BUFFER_IN_VIDEO,
            CfgValue, CFG_FILE_NAME);

  sprintf(CfgValue, "%d", m_DDFullScreenMode);
  WritePrivateProfileString(CFG_VIEW_SECTION, "DDFullScreenMode", CfgValue, CFG_FILE_NAME);

  sprintf(CfgValue, "%d", m_isFullScreen);
  WritePrivateProfileString(CFG_VIEW_SECTION, "isFullScreen", CfgValue, CFG_FILE_NAME);

  sprintf(CfgValue, "%d", m_isDD32);
  WritePrivateProfileString(CFG_VIEW_SECTION, "isDD32", CfgValue, CFG_FILE_NAME);

  sprintf(CfgValue, "%d", m_MenuIdWinSize);
    WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_WIN_SIZE,
            CfgValue, CFG_FILE_NAME);

    if (m_isFullScreen)
    {
        GetWindowRect(m_hWnd, &wndrect);
        sprintf(CfgValue, "%d", wndrect.left);
        WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_WIN_XPOS,
                CfgValue, CFG_FILE_NAME);
        sprintf(CfgValue, "%d", wndrect.top);
        WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_WIN_YPOS,
                CfgValue, CFG_FILE_NAME);
    }

    sprintf(CfgValue, "%d", m_ShowSpeedAndFPS);
    WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_SHOW_FPS,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", SoundEnabled);
    WritePrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_ENABLED,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", DirectSoundEnabled);
    WritePrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_DIRECT_ENABLED,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdSampleRate);
    WritePrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_SAMPLE_RATE,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdVolume);
    WritePrivateProfileString(CFG_SOUND_SECTION, CFG_SOUND_VOLUME,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdTiming);
    WritePrivateProfileString(CFG_SPEED_SECTION, CFG_SPEED_TIMING,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdKeyMapping);
    WritePrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_KEY_MAPPING,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdSticks);
    WritePrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_STICKS,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_FreezeWhenInactive);
    WritePrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_FREEZEINACTIVE,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_HideCursor);
    WritePrivateProfileString(CFG_OPTIONS_SECTION, CFG_OPTIONS_HIDE_CURSOR,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", AMXMouseEnabled);
    WritePrivateProfileString(CFG_AMX_SECTION, CFG_AMX_ENABLED,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdAMXSize);
    WritePrivateProfileString(CFG_AMX_SECTION, CFG_AMX_SIZE,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", m_MenuIdAMXAdjust);
    WritePrivateProfileString(CFG_AMX_SECTION, CFG_AMX_ADJUST,
            CfgValue, CFG_FILE_NAME);

    sprintf(CfgValue, "%d", PrinterEnabled);
    WritePrivateProfileString(CFG_PRINTER_SECTION, CFG_PRINTER_ENABLED,
            CfgValue, CFG_FILE_NAME);
    sprintf(CfgValue, "%d", m_MenuIdPrinterPort);
    WritePrivateProfileString(CFG_PRINTER_SECTION, CFG_PRINTER_PORT,
            CfgValue, CFG_FILE_NAME);
    WritePrivateProfileString(CFG_PRINTER_SECTION, CFG_PRINTER_FILE,
            m_PrinterFileName, CFG_FILE_NAME);

    for (int key=0; key<256; ++key)
    {
        if (UserKeymap[key][0] != 0 || UserKeymap[key][1] != 0)
        {
            sprintf(CfgName, "%s%d", CFG_OPTIONS_USER_KEY_MAP, key);
            sprintf(CfgValue, "%d %d", UserKeymap[key][0], UserKeymap[key][1]);
            WritePrivateProfileString(CFG_OPTIONS_SECTION, CfgName,
                CfgValue, CFG_FILE_NAME);
        }
    }

    sprintf(CfgValue, "%d", palette_type);
    WritePrivateProfileString(CFG_VIEW_SECTION, CFG_VIEW_MONITOR,
            CfgValue, CFG_FILE_NAME);
    *binsize=1;
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","MachineType",&MachineType,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","RelaySoundEnabled",&RelaySoundEnabled,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","TubeEnabled",&TubeEnabled,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","HideMenuEnabled",&HideMenuEnabled,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SerialPortEnabled",&SerialPortEnabled,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SerialPort",&SerialPort,binsize);
    LEDByte=(DiscLedColour<<2)|((LEDs.ShowDisc?1:0)<<1)|(LEDs.ShowKB?1:0);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","LED Information",&LEDByte,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","TapeSoundEnabled",&TapeSoundEnabled,binsize);
    *binsize=2;
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Tape Clock Speed",&TapeClockSpeed,binsize);
    *binsize=1;
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","UseHostClock",&UseHostClock,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","OpCodes",&OpCodes,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Basic Hardware",&BHardware,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Teletext Half Mode",&THalfMode,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","UsePrimaryBuffer",&UsePrimaryBuffer,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","Part Samples",&PartSamples,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SoundEnabled",&SoundDefault,binsize);
    SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","SoundBlockSize",&SBSize,binsize);
}

void BeebWin::SetTapeSpeedMenu(void) {
    CheckMenuItem(m_hMenu,ID_TAPE_FAST,(TapeClockSpeed==750)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_TAPE_MFAST,(TapeClockSpeed==1600)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_TAPE_MSLOW,(TapeClockSpeed==3200)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_TAPE_NORMAL,(TapeClockSpeed==5600)?MF_CHECKED:MF_UNCHECKED);
}

/****************************************************************************/
void BeebWin::SetWindowAttributes(bool wasFullScreen)
{
    HRESULT ddrval;
    RECT wndrect;
    RECT scrrect;
    long style;

    if (m_isFullScreen)
    {
        if (!wasFullScreen)
        {
            GetWindowRect(m_hWnd, &wndrect);
            m_XWinPos = wndrect.left;
            m_YWinPos = wndrect.top;
        }

        if (m_DirectDrawEnabled)
        {
            style = GetWindowLong(m_hWnd, GWL_STYLE);
            style &= ~(WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
            style |= WS_POPUP;
            SetWindowLong(m_hWnd, GWL_STYLE, style);

            if (m_DXInit == TRUE)
            {
                ResetSurfaces();
                ddrval = InitSurfaces();
                if( ddrval != DD_OK )
                {
                    char  errstr[200];
                    sprintf(errstr,"DirectX failure changing screen size\nFailure code %X",ddrval);
                    MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
                }
            }
        }
        else
        {
            style = GetWindowLong(m_hWnd, GWL_STYLE);
            style &= ~WS_POPUP;
            style |= WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
            SetWindowLong(m_hWnd, GWL_STYLE, style);

            SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
            SetWindowPos(m_hWnd, HWND_TOP, scrrect.left, scrrect.top,
                m_XWinSize + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
                m_YWinSize + GetSystemMetrics(SM_CYFIXEDFRAME) * 2
                    + GetSystemMetrics(SM_CYMENUSIZE)
                    + GetSystemMetrics(SM_CYCAPTION)
                    + 1,
                0);
        }

// Experiment: hide menu in full screen
    if (HideMenuEnabled) ShowMenu(false);
    }
    else
    {
        if (m_DirectDrawEnabled && m_DXInit == TRUE)
        {
            ResetSurfaces();
            ddrval = InitSurfaces();
            if( ddrval != DD_OK )
            {
                char  errstr[200];
                sprintf(errstr,"DirectX failure changing screen size\nFailure code %X",ddrval);
                MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
            }
        }

        style = GetWindowLong(m_hWnd, GWL_STYLE);
        style &= ~WS_POPUP;
        style |= WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
        SetWindowLong(m_hWnd, GWL_STYLE, style);

        SetWindowPos(m_hWnd, HWND_TOP, m_XWinPos, m_YWinPos,
            m_XWinSize + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
            m_YWinSize + GetSystemMetrics(SM_CYFIXEDFRAME) * 2
                + GetSystemMetrics(SM_CYMENUSIZE) * (m_MenuIdWinSize == IDM_160X128 ? 2:1)
                + GetSystemMetrics(SM_CYCAPTION)
                + 1,
            !wasFullScreen ? SWP_NOMOVE : 0);

// Experiment: hide menu in full screen
    if (HideMenuEnabled) ShowMenu(true);
    }
}

/****************************************************************************/
void BeebWin::TranslateAMX(void)
{
    switch (m_MenuIdAMXSize)
    {
    case IDM_AMX_160X256:
        m_AMXXSize = 160;
        m_AMXYSize = 256;
        break;
    default:
    case IDM_AMX_320X256:
        m_AMXXSize = 320;
        m_AMXYSize = 256;
        break;
    case IDM_AMX_640X256:
        m_AMXXSize = 640;
        m_AMXYSize = 256;
        break;
    }

    switch (m_MenuIdAMXAdjust)
    {
    case 0:
        m_AMXAdjust = 0;
        break;
    case IDM_AMX_ADJUSTP50:
        m_AMXAdjust = 50;
        break;
    default:
    case IDM_AMX_ADJUSTP30:
        m_AMXAdjust = 30;
        break;
    case IDM_AMX_ADJUSTP10:
        m_AMXAdjust = 10;
        break;
    case IDM_AMX_ADJUSTM10:
        m_AMXAdjust = -10;
        break;
    case IDM_AMX_ADJUSTM30:
        m_AMXAdjust = -30;
        break;
    case IDM_AMX_ADJUSTM50:
        m_AMXAdjust = -50;
        break;
    }
}

void BeebWin::UpdateSerialMenu(HMENU hMenu) {
    CheckMenuItem(hMenu, ID_SERIAL, (SerialPortEnabled)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_COM1, (SerialPort==1)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_COM2, (SerialPort==2)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_COM3, (SerialPort==3)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_COM4, (SerialPort==4)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::UpdateLEDMenu(HMENU hMenu) {
    // Update the LED Menu
    CheckMenuItem(hMenu,ID_RED_LEDS,(DiscLedColour>0)?MF_UNCHECKED:MF_CHECKED);
    CheckMenuItem(hMenu,ID_GREEN_LEDS,(DiscLedColour>0)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(hMenu,ID_SHOW_KBLEDS,(LEDs.ShowKB)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(hMenu,ID_SHOW_DISCLEDS,(LEDs.ShowDisc)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::UpdateOptiMenu(void) {
    CheckMenuItem(m_hMenu,ID_DOCONLY,(OpCodes==1)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_EXTRAS ,(OpCodes==2)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_FULLSET,(OpCodes==3)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_BHARDWARE,(BHardware==1)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_TSTYLE,(THalfMode==1)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_1773,(SBSize==1)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_443,(SBSize==0)?MF_CHECKED:MF_UNCHECKED);
    CheckMenuItem(m_hMenu,ID_PSAMPLES,(PartSamples)?MF_CHECKED:MF_UNCHECKED);
}
/***************************************************************************/
void BeebWin::HandleCommand(int MenuId)
{
    char TmpPath[256];
    BOOL b;
    HRESULT ddrval;
    HMENU hMenu = m_hMenu;
  int prev_palette_type = palette_type;
    SetSound(MUTED);
    switch (MenuId)
    {
    case IDM_LOADSTATE:
        RestoreState();
        MustEnableSound=TRUE;
        break;
    case IDM_SAVESTATE:
        SaveState();
        MustEnableSound=TRUE;
        break;

    case IDM_QUICKLOAD:
    case IDM_QUICKSAVE:
        {
            char FileName[_MAX_PATH];
            strcpy(FileName, m_AppPath);
            strcat(FileName, "beebstate\\quicksave.uef");
            if (MenuId == IDM_QUICKLOAD)
                SaveUEFState(FileName);
            else
                LoadUEFState(FileName);
        }
        break;

    case IDM_LOADDISC0:
        ReadDisc(0,hMenu);
        MustEnableSound=TRUE;
        break;
    case IDM_LOADDISC1:
        ReadDisc(1,hMenu);
        MustEnableSound=TRUE;
        break;

    case ID_LOADTAPE:
        LoadTape();
        MustEnableSound=TRUE;
        break;

    case IDM_NEWDISC0:
        NewDiscImage(0);
        MustEnableSound=TRUE;
        break;
    case IDM_NEWDISC1:
        NewDiscImage(1);
        MustEnableSound=TRUE;
        break;

    case IDM_WPDISC0:
        ToggleWriteProtect(0);
        break;
    case IDM_WPDISC1:
        ToggleWriteProtect(1);
        break;

    case IDM_PRINTER_FILE:
        if (PrinterFile())
        {
            /* If printer is enabled then need to
                disable it before changing file */
            if (PrinterEnabled)
                TogglePrinter();

            /* Add file name to menu */
            char menu_string[256];
            strcpy(menu_string, "File: ");
            strcat(menu_string, m_PrinterFileName);
            ModifyMenu(hMenu, IDM_PRINTER_FILE,
                MF_BYCOMMAND, IDM_PRINTER_FILE,
                menu_string);

            if (MenuId != m_MenuIdPrinterPort)
            {
                CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_UNCHECKED);
                m_MenuIdPrinterPort = MenuId;
                CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
            }
            TranslatePrinterPort();
        }
        MustEnableSound=TRUE;
        break;
    case IDM_PRINTER_LPT1:
    case IDM_PRINTER_LPT2:
    case IDM_PRINTER_LPT3:
    case IDM_PRINTER_LPT4:
        if (MenuId != m_MenuIdPrinterPort)
        {
            /* If printer is enabled then need to
                disable it before changing file */
            if (PrinterEnabled)
                TogglePrinter();

            CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_UNCHECKED);
            m_MenuIdPrinterPort = MenuId;
            CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
            TranslatePrinterPort();
        }
        break;
    case IDM_PRINTERONOFF:
        TogglePrinter();
        break;

    case ID_SERIAL:
        SerialPortEnabled=1-SerialPortEnabled;
        bSerialStateChanged=TRUE;
        UpdateSerialMenu(hMenu);
        break;
    case ID_COM1:
        SerialPort=1; UpdateSerialMenu(hMenu); bSerialStateChanged=TRUE; break;
    case ID_COM2:
        SerialPort=2; UpdateSerialMenu(hMenu); bSerialStateChanged=TRUE; break;
    case ID_COM3:
        SerialPort=3; UpdateSerialMenu(hMenu); bSerialStateChanged=TRUE; break;
    case ID_COM4:
        SerialPort=4; UpdateSerialMenu(hMenu); bSerialStateChanged=TRUE; break;

    case IDM_DDRAWONOFF:
    {
      int enabled;
          if (m_DirectDrawEnabled == TRUE)
          {
              m_DirectDrawEnabled = FALSE;
              ResetSurfaces();
              m_DD2->Release();
              m_DD->Release();
              TranslateWindowSize();
              SetWindowAttributes(m_isFullScreen);
          enabled = MF_GRAYED;
          }
          else
          {
              m_DirectDrawEnabled = TRUE;
              TranslateWindowSize();
              SetWindowAttributes(m_isFullScreen);
              InitDirectX();
          enabled = MF_ENABLED;
          }
          CheckMenuItem(hMenu, IDM_DDRAWONOFF, m_DirectDrawEnabled ? MF_CHECKED : MF_UNCHECKED);
        EnableMenuItem(hMenu, ID_VIEW_DD_640X480, enabled);
        EnableMenuItem(hMenu, ID_VIEW_DD_1024X768, enabled);
        EnableMenuItem(hMenu, ID_VIEW_DD_1280X1024, enabled);
        EnableMenuItem(hMenu, IDM_DDINVIDEORAM, enabled);
        EnableMenuItem(hMenu, IDM_DD32ONOFF, enabled);
    }
        break;

  case IDM_DD32ONOFF:
    m_isDD32 = !m_isDD32;
        if (m_DirectDrawEnabled == TRUE)
        {
            ResetSurfaces();
            ddrval = InitSurfaces();
            if( ddrval != DD_OK )
            {
                char  errstr[200];
                sprintf(errstr,"DirectX failure changing buffer RAM\nFailure code %X",ddrval);
                MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
        m_isDD32 = !m_isDD32;
            }
            else
                CheckMenuItem(hMenu, IDM_DD32ONOFF, m_isDD32 ? MF_CHECKED : MF_UNCHECKED);
        }
    break;

    case IDM_DDINVIDEORAM:
        m_DDS2InVideoRAM = !m_DDS2InVideoRAM;
        if (m_DirectDrawEnabled == TRUE)
        {
            ResetSurfaces();
            ddrval = InitSurfaces();
            if( ddrval != DD_OK )
            {
                char  errstr[200];
                sprintf(errstr,"DirectX failure changing buffer RAM\nFailure code %X",ddrval);
                MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
                m_DDS2InVideoRAM = !m_DDS2InVideoRAM;
            }
            else
                CheckMenuItem(hMenu, IDM_DDINVIDEORAM, m_DDS2InVideoRAM ? MF_CHECKED : MF_UNCHECKED);
        }
        break;

    case IDM_160X128:
    case IDM_240X192:
    case IDM_640X256:
    case IDM_320X256:
    case IDM_640X512:
    case IDM_800X600:
    case IDM_1024X768:
    case IDM_1024X512:
        {
      if (m_isFullScreen)
        HandleCommand(IDM_FULLSCREEN);
            CheckMenuItem(hMenu, m_MenuIdWinSize, MF_UNCHECKED);
            m_MenuIdWinSize = MenuId;
            CheckMenuItem(hMenu, m_MenuIdWinSize, MF_CHECKED);
            TranslateWindowSize();
            SetWindowAttributes(m_isFullScreen);
    }
        break;

  case ID_VIEW_DD_640X480:
  case ID_VIEW_DD_1024X768:
  case ID_VIEW_DD_1280X1024:
        {
      if (!m_isFullScreen)
        HandleCommand(IDM_FULLSCREEN);
      if (!m_DirectDrawEnabled)
        // Should not happen since the items are grayed out, but anyway...
        HandleCommand(IDM_DDRAWONOFF);
            CheckMenuItem(hMenu, m_DDFullScreenMode, MF_UNCHECKED);
            m_DDFullScreenMode = MenuId;
            CheckMenuItem(hMenu, m_DDFullScreenMode, MF_CHECKED);
            TranslateWindowSize();
            SetWindowAttributes(m_isFullScreen);
    }
        break;

    case IDM_FULLSCREEN:
        WINDOWPLACEMENT tWindowPlacement;
        if (!m_isFullScreen) {
            tWindowPlacement.length=*binsize;
            GetWindowPlacement(GETHWND,&tWindowPlacement);
            SysReg.SetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","WindowPos",&tWindowPlacement,binsize);
        }
        m_isFullScreen = !m_isFullScreen;
        CheckMenuItem(hMenu, IDM_FULLSCREEN, m_isFullScreen ? MF_CHECKED : MF_UNCHECKED);
        TranslateWindowSize();
        SetWindowAttributes(!m_isFullScreen);
        if (!m_isFullScreen) {
            RegRes=SysReg.GetBinaryValue(HKEY_CURRENT_USER,"Software\\BeebEm","WindowPos",&tWindowPlacement,binsize);
            tWindowPlacement.length=sizeof(tWindowPlacement);
            tWindowPlacement.showCmd=SW_RESTORE;
            SetWindowPlacement(GETHWND,&tWindowPlacement);
        }
        break;

    case IDM_SPEEDANDFPS:
        if (m_ShowSpeedAndFPS)
        {
            m_ShowSpeedAndFPS = FALSE;
            CheckMenuItem(hMenu, IDM_SPEEDANDFPS, MF_UNCHECKED);
            SetWindowText(m_hWnd, WindowTitle);
        }
        else
        {
            m_ShowSpeedAndFPS = TRUE;
            CheckMenuItem(hMenu, IDM_SPEEDANDFPS, MF_CHECKED);
        }
        break;

    case IDM_SOUNDONOFF:
        if (SoundDefault)
        {
            CheckMenuItem(hMenu, IDM_SOUNDONOFF, MF_UNCHECKED);
            SoundReset();
            SoundDefault=0;
        }
        else
        {
            SoundInit();
            if (SoundEnabled) {
                CheckMenuItem(hMenu, IDM_SOUNDONOFF, MF_CHECKED);
                SoundDefault=1;
            }
        }
        break;

    case ID_SFX_RELAY:
        RelaySoundEnabled=1-RelaySoundEnabled;
        CheckMenuItem(hMenu,ID_SFX_RELAY,RelaySoundEnabled?MF_CHECKED:MF_UNCHECKED);
        break;

    case IDM_44100KHZ:
    case IDM_22050KHZ:
    case IDM_11025KHZ:
        if (MenuId != m_MenuIdSampleRate)
        {
            CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_UNCHECKED);
            m_MenuIdSampleRate = MenuId;
            CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_CHECKED);
            TranslateSampleRate();

            if (SoundEnabled)
            {
                SoundReset();
                SoundInit();
            }
        }
        break;

    case IDM_FULLVOLUME:
    case IDM_HIGHVOLUME:
    case IDM_MEDIUMVOLUME:
    case IDM_LOWVOLUME:
        if (MenuId != m_MenuIdVolume)
        {
            CheckMenuItem(hMenu, m_MenuIdVolume, MF_UNCHECKED);
            m_MenuIdVolume = MenuId;
            CheckMenuItem(hMenu, m_MenuIdVolume, MF_CHECKED);
            TranslateVolume();
        }
        break;

    case IDM_ALLOWALLROMWRITES:
        if (WritableRoms)
        {
            WritableRoms = FALSE;
            CheckMenuItem(hMenu, IDM_ALLOWALLROMWRITES, MF_UNCHECKED);
            GreyRomMenu( FALSE );
        }
        else
        {
            WritableRoms = TRUE;
            CheckMenuItem(hMenu, IDM_ALLOWALLROMWRITES, MF_CHECKED);
            GreyRomMenu( TRUE );
        }
        break;

    /* LRW Added switch individual ROMS Writable ON/OFF */
    case IDM_ALLOWWRITES_ROM0:
    case IDM_ALLOWWRITES_ROM1:
    case IDM_ALLOWWRITES_ROM2:
    case IDM_ALLOWWRITES_ROM3:
    case IDM_ALLOWWRITES_ROM4:
    case IDM_ALLOWWRITES_ROM5:
    case IDM_ALLOWWRITES_ROM6:
    case IDM_ALLOWWRITES_ROM7:
    case IDM_ALLOWWRITES_ROM8:
    case IDM_ALLOWWRITES_ROM9:
    case IDM_ALLOWWRITES_ROMA:
    case IDM_ALLOWWRITES_ROMB:
    case IDM_ALLOWWRITES_ROMC:
    case IDM_ALLOWWRITES_ROMD:
    case IDM_ALLOWWRITES_ROME:
    case IDM_ALLOWWRITES_ROMF:

        CheckMenuItem(hMenu,  MenuId, RomWritable[( MenuId-IDM_ALLOWWRITES_ROM0)] ? MF_UNCHECKED : MF_CHECKED );
        GetRomMenu();   // Update the Rom/Ram state for all the roms.
        break;

    case IDM_REALTIME:
    case IDM_3QSPEED:
    case IDM_HALFSPEED:
    case IDM_50FPS:
    case IDM_25FPS:
    case IDM_10FPS:
    case IDM_5FPS:
    case IDM_1FPS:
        if (MenuId != m_MenuIdTiming)
        {
            CheckMenuItem(hMenu, m_MenuIdTiming, MF_UNCHECKED);
            m_MenuIdTiming = MenuId;
            CheckMenuItem(hMenu, m_MenuIdTiming, MF_CHECKED);
            TranslateTiming();
        }
        break;

    case IDM_JOYSTICK:
    case IDM_MOUSESTICK:
        /* Disable current selection */
        if (m_MenuIdSticks != 0)
        {
            CheckMenuItem(hMenu, m_MenuIdSticks, MF_UNCHECKED);
            if (m_MenuIdSticks == IDM_JOYSTICK)
            {
                ResetJoystick();
            }
            else /* mousestick */
            {
                AtoDReset();
            }
        }

        if (MenuId == m_MenuIdSticks)
        {
            /* Joysticks switched off completely */
            m_MenuIdSticks = 0;
        }
        else
        {
            /* Initialise new selection */
            m_MenuIdSticks = MenuId;
            if (m_MenuIdSticks == IDM_JOYSTICK)
            {
                InitJoystick();
            }
            else /* mousestick */
            {
                AtoDInit();
            }
            if (JoystickEnabled)
                CheckMenuItem(hMenu, m_MenuIdSticks, MF_CHECKED);
            else
                m_MenuIdSticks = 0;
        }
        break;

    case IDM_FREEZEINACTIVE:
        if (m_FreezeWhenInactive)
        {
            m_FreezeWhenInactive = FALSE;
            CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, MF_UNCHECKED);
        }
        else
        {
            m_FreezeWhenInactive = TRUE;
            CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, MF_CHECKED);
        }
        break;

    case IDM_HIDECURSOR:
        if (m_HideCursor)
        {
            m_HideCursor = FALSE;
            CheckMenuItem(hMenu, IDM_HIDECURSOR, MF_UNCHECKED);
        }
        else
        {
            m_HideCursor = TRUE;
            CheckMenuItem(hMenu, IDM_HIDECURSOR, MF_CHECKED);
        }
        break;

    case IDM_IGNOREILLEGALOPS:
        if (IgnoreIllegalInstructions)
        {
            IgnoreIllegalInstructions = FALSE;
            CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS, MF_UNCHECKED);
        }
        else
        {
            IgnoreIllegalInstructions = TRUE;
            CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS, MF_CHECKED);
        }
        break;

    case IDM_DEFINEKEYMAP:
        UserKeyboardDialog( m_hWnd );
        MustEnableSound=TRUE;
        break;

    case IDM_USERKYBDMAPPING:
    case IDM_KEYBOARDMAPPING1:
    case IDM_KEYBOARDMAPPING2:
        if (MenuId != m_MenuIdKeyMapping)
        {
            CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_UNCHECKED);
            m_MenuIdKeyMapping = MenuId;
            CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_CHECKED);
            TranslateKeyMapping();
        }
        break;

    case IDM_ABOUT:
        MessageBox(m_hWnd, AboutText, WindowTitle, MB_OK);
        break;

    case IDM_EXIT:
        // write out cmos ram first
        strcpy(TmpPath,RomPath); strcat(TmpPath,"/beebstate/cmos.ram");
        CMDF2=fopen(TmpPath,"wb");
        for(CMA2=0xe;CMA2<64;CMA2++) fputc(CMOSRAM[CMA2],CMDF2);
        fclose(CMDF2);
        if (!m_isFullScreen) SaveWindowPos(); // Save window co-ordinates
        PostMessage(m_hWnd, WM_CLOSE, 0, 0L);
        break;

    case IDM_SAVE_PREFS:
        SavePreferences();
        break;

    case IDM_AMXONOFF:
        if (AMXMouseEnabled)
        {
            CheckMenuItem(hMenu, IDM_AMXONOFF, MF_UNCHECKED);
            AMXMouseEnabled = FALSE;
        }
        else
        {
            CheckMenuItem(hMenu, IDM_AMXONOFF, MF_CHECKED);
            AMXMouseEnabled = TRUE;
        }
        break;

    case IDM_AMX_LRFORMIDDLE:
        if (AMXLRForMiddle)
        {
            CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, MF_UNCHECKED);
            AMXLRForMiddle = FALSE;
        }
        else
        {
            CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, MF_CHECKED);
            AMXLRForMiddle = TRUE;
        }
        break;

    case IDM_AMX_160X256:
    case IDM_AMX_320X256:
    case IDM_AMX_640X256:
        if (MenuId != m_MenuIdAMXSize)
        {
            CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_UNCHECKED);
            m_MenuIdAMXSize = MenuId;
            CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_CHECKED);
        }
        TranslateAMX();
        break;

    case IDM_AMX_ADJUSTP50:
    case IDM_AMX_ADJUSTP30:
    case IDM_AMX_ADJUSTP10:
    case IDM_AMX_ADJUSTM10:
    case IDM_AMX_ADJUSTM30:
    case IDM_AMX_ADJUSTM50:
        if (m_MenuIdAMXAdjust != 0)
        {
            CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_UNCHECKED);
        }

        if (MenuId != m_MenuIdAMXAdjust)
        {
            m_MenuIdAMXAdjust = MenuId;
            CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_CHECKED);
        }
        else
        {
            m_MenuIdAMXAdjust = 0;
        }
        TranslateAMX();
        break;

  case ID_MONITOR_RGB:
    palette_type = RGB;
    CreateBitmap();
    break;
  case ID_MONITOR_BW:
    palette_type = BW;
    CreateBitmap();
    break;
  case ID_MONITOR_GREEN:
    palette_type = GREEN;
    CreateBitmap();
    break;
  case ID_MONITOR_AMBER:
    palette_type = AMBER;
    break;
/*  case IDM_TUBE:
    TubeEnabled=1-TubeEnabled;
    CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED); */
  case ID_FILE_RESET:
    ResetBeebSystem(MachineType,TubeEnabled,0);
    break;
/*  case IDM_TUBERESET:
      ResetBeebSystem(MachineType,1,0);
    break; */
  case ID_MODELB:
      if (MachineType==1) {
        ResetBeebSystem(0,EnableTube,1);
        UpdateModelType();
      }
    break;
  case ID_MASTER128:
      if (MachineType==0) {
        ResetBeebSystem(1,EnableTube,1);
        UpdateModelType();
        //BeginDump=1;
      }
      break;
  case ID_REWINDTAPE:
      RewindTape();
      break;
  case ID_HIDEMENU:
      HideMenuEnabled=1-HideMenuEnabled;
      CheckMenuItem(hMenu, ID_HIDEMENU, (HideMenuEnabled)?MF_CHECKED:MF_UNCHECKED);
      break;
   case ID_RED_LEDS:
       DiscLedColour=0;
       UpdateLEDMenu(hMenu);
       break;
   case ID_GREEN_LEDS:
       DiscLedColour=1;
       UpdateLEDMenu(hMenu);
       break;
   case ID_SHOW_KBLEDS:
       LEDs.ShowKB=!LEDs.ShowKB;
       UpdateLEDMenu(hMenu);
       break;
   case ID_SHOW_DISCLEDS:
       LEDs.ShowDisc=!LEDs.ShowDisc;
       UpdateLEDMenu(hMenu);
       break;
   case ID_FDC_DLL:
       SelectFDC();
       MustEnableSound=TRUE;
       break;
   case ID_8271:
       KillDLLs();
       NativeFDC=TRUE;
       CheckMenuItem(m_hMenu,ID_8271,MF_CHECKED);
       CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_UNCHECKED);
       SysReg.SetStringValue(HKEY_CURRENT_USER,"Software\\BeebEm","FDCDLL","None"); // Write back DLL to registry
       break;
   case ID_TAPE_FAST:
       TapeClockSpeed=750;
       SetTapeSpeedMenu();
       break;
   case ID_TAPE_MFAST:
       TapeClockSpeed=1600;
       SetTapeSpeedMenu();
       break;
   case ID_TAPE_MSLOW:
       TapeClockSpeed=3200;
       SetTapeSpeedMenu();
       break;
   case ID_TAPE_NORMAL:
       TapeClockSpeed=5600;
       SetTapeSpeedMenu();
       break;
   case ID_TAPESOUND:
       TapeSoundEnabled=!TapeSoundEnabled;
       CheckMenuItem(m_hMenu,ID_TAPESOUND,(TapeSoundEnabled)?MF_CHECKED:MF_UNCHECKED);
       break;
   case ID_HOSTCLOCK:
       UseHostClock=1-UseHostClock;
       CheckMenuItem(m_hMenu,ID_HOSTCLOCK,(UseHostClock)?MF_CHECKED:MF_UNCHECKED);
       SoundReset();
       if (SoundDefault) SoundInit();
       break;
   case ID_PBUFF:
       UsePrimaryBuffer=1-UsePrimaryBuffer;
       SoundReset();
       if (SoundDefault) SoundInit();
       break;
   case ID_TSTYLE:
       THalfMode=1-THalfMode;
       UpdateOptiMenu();
       break;
   case ID_DOCONLY:
       OpCodes=1;
       UpdateOptiMenu();
       break;
   case ID_EXTRAS:
       OpCodes=2;
       UpdateOptiMenu();
       break;
   case ID_FULLSET:
       OpCodes=3;
       UpdateOptiMenu();
       break;
   case ID_BHARDWARE:
       BHardware=1-BHardware;
       UpdateOptiMenu();
       break;
   case ID_PSAMPLES:
       PartSamples=1-PartSamples;
       UpdateOptiMenu();
       break;
   case ID_1773:
       SBSize=1;
       UpdateOptiMenu();
       break;
   case ID_443:
       SBSize=0;
       UpdateOptiMenu();
       break;
   }
  SetSound(UNMUTED);
  if (palette_type != prev_palette_type) {
    CreateBitmap();
    UpdateMonitorMenu();
  }
}

void BeebWin::SetPBuff(void) {
    CheckMenuItem(m_hMenu,ID_PBUFF,(UsePrimaryBuffer)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::SetSoundMenu(void) {
    CheckMenuItem(m_hMenu,IDM_SOUNDONOFF,(SoundEnabled)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::Focus(BOOL gotit)
{
    if (gotit)
        m_frozen = FALSE;
    else
        if (m_FreezeWhenInactive)
            m_frozen = TRUE;
}

BOOL BeebWin::IsFrozen(void)
{
    return m_frozen;
}

void SaveEmuUEF(FILE *SUEF) {
    char EmuName[16];
    fput16(0x046C,SUEF);
    fput32(16,SUEF);
    // BeebEm Title Block
    strcpy(EmuName,"BeebEm");
    EmuName[14]=1;
    EmuName[15]=4; // Version, 1.4
    fwrite(EmuName,16,1,SUEF);
    //
    fput16(0x046a,SUEF);
    fput32(16,SUEF);
    // Emulator Specifics
    // Note about this block: It should only be handled by beebem from uefstate.cpp if
    // the UEF has been determined to be from BeebEm (Block 046C)
    fputc(MachineType,SUEF);
    fputc((NativeFDC)?0:1,SUEF);
    fputc(TubeEnabled,SUEF);
    fputc(0,SUEF); // Monitor type, reserved
    fputc(0,SUEF); // Speed Setting, reserved
    fput32(0,SUEF);
    fput32(0,SUEF);
    fput16(0,SUEF);
    fputc(0,SUEF);
}

void LoadEmuUEF(FILE *SUEF) {
    MachineType=fgetc(SUEF);
    NativeFDC=(fgetc(SUEF)==0)?TRUE:FALSE;
    TubeEnabled=fgetc(SUEF);
    mainWin->ResetBeebSystem(MachineType,TubeEnabled,1);
    mainWin->UpdateModelType();
}
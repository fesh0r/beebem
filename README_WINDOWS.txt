Beebem for Microsoft Windows (5 Aug 1997)
============================

Beebem has been ported to MS Windows. The following people have contributed
to the Windows port:

  Nigel Magnay (magnayn@cs.man.ac.uk) - original port of V0.4
  Robert Schmidt (rsc@vingmed.no) - new mode 7 font, fixed backspace key
  Mike Wyatt (mikebuk@aol.com) - V0.7 features

Note however that the Copyright for the Emulator resides with David Gilbert,
the original author, as described in the Readme.1st text file.


Compiling
---------

This distribution of Beebem contains Win32 executables but if you want to
compile it yourself then all the source files are included as well.  There
are also a couple of files that can be used with MS Visual C++ V4.2:

  BeebEm.mdp  - Developer Studio Workspace
  BeebEm.mak  - MS VC++ makefile

You should be able to compile the sources using other compilers (such as
Borland or Watcom) but you may need to remove some of the App Studio stuff
from the resources file (beebem.rc).  You will also need to edit port.h to
change the EightByteType from __int64 (which is Microsoft specific) to a
structure of two long words.


Executables
-----------

There are two executables in this distribution, in the 'intelbin' directory:

  BeebEm.exe  - Win32 version for all Intel processors
  BeebEmP.exe - Win32 version optimised for a Pentium processor

The executables run fine on Windows 95 and NT 4.0.  They should run on NT
3.51 and Win 3.1 with Win32s but I have not tried them.  The executables
were not compiled with explicit Win32s support so they may not work quite as
smoothly as on Win95 or NT4.

NOTE: The V0.4 distribution contained a WinG version.  This version does not
contain the WinG code as the GDI in Win95 and NT4 appears to give exactly
the same performance as the WinG library (perhaps its been built in?).


Running BeebEm
--------------

You will need to get some ROM images from a BBC Micro to run BeebEm.  You
will need OS1.2, Basic and DFS ROM images.  You will also need some disc
images to use with BeebEm.  See the ReadMe.1st file for some more info on
ROMs and disc images.

You may also find Robert Schmidt's Beeb web page useful.  It contains
details of various ways to transfer ROM images and discs from a Beeb to a
PC.  See http://www.nvg.unit.no/bbc/bbc.html

There is also an example disc image in the discims directory in this
distribution.  It contains some simple programs to test the emulator.

Once you have ROM and disc images you need to create directories to keep
them in.  First create a directory to keep the emulator in, e.g. C:\Beeb,
and copy the executable into it.  Then create the following subdirectories
in C:\Beeb:

  beebfile  - put the ROM images in here
  discims   - put the disc images in here
  beebstate - Beeb State files go in here (leave this empty for now)

The OS ROM should have a file name of 'os12'.  It does not matter what the
file names for the other ROMs are.

Once all the files and directories are set up, run the emulator.  It should
work in any graphics mode but it will run fastest in a 256 colour mode.

When BeebEm starts up you should see the normal 'BBC Computer 32K' message
followed by the start up messages for all the ROMs (i.e. the DFS and BASIC).
Load a disc using the File menu (remember to select the correct file type)
and press Shift+F12 to boot it.

If a disc does not boot using Shift+F12 then use '*CAT' and 'CH."filename"'
commands to load the programs from it.


New Features in V0.7 and V0.71
------------------------------

Dynamic disc selection - select and load disc images while the emulator is
  running.  Two discs can be loaded, one each for drive 0 and 1.  Note that
  the BeebDiscLoad environment variable can still be used to specify the
  disc that is loaded on startup (see ReadMe.1st).  The windows version of
  Beebem will not try to load discims/elite by default.

Beeb State save and restore - saves and restore the state of the emulator.
  Useful given that disc writes are not implemented yet.  For the
  programmers out there the Beeb State file format is documented in the
  beebstate.h file.

Various Window sizes - select from 8 different window sizes.  The bigger
  they are the slower it gets!

Real time and fixed frame rate modes - run the emulator at exactly the same
  speed as a BBC Micro, run it slower (useful for tricky games) or reduce
  the frame rate to run it faster.  The speed relative to a BBC Micro and
  the frames per second is displayed in the window header.

Sound support - if you have a sound card then switch on the sound
  generation.  Three sample rates are available so you can balance quality
  with speed (you will need a Pentium 100MHz or faster to get good sound
  output).  Note also that sound only really works properly when running in
  real time mode.

Joystick support - use your PC analogue joystick as a beeb joystick.
  Calibrate the PC joystick using the control panel joystick option.

Mousestick support - Beebem can map the mouse cursor position to the beeb
  joystick position.  The mouse cursor can also be hidden so it does not
  obscure the window.

ROM write protection - write protect the ROMs. Some ROMs write to themselves
  to check that they are not being run in sideways RAM.

Multiple ROM initialisation - Beebem will read all the ROMs from the
  beebfile directory (up to 16 of them) when it starts up (8K and 16K ROMs).

Games keyboard mapping - there are two keyboard mappings, the normal one and
  a games one.  See below.

Full menu control - there are a load of window menus to control everything.

Pentium optimised version - runs a bit faster than the generic Intel
  executable.

Undocumented 6502 instructions - many of these have now been implemented
  (Zalaga and Dare Devil Denis now work).

Screen cursor - the beeb cursor is now displayed.

Sideways RAM support - ROM slot 0 is treated as RAM and any writes to memory
  between 0x8000 and 0xBFFF will get written to the RAM (this means Exile
  can be played full screen).  The Beeb State file has also been extended to
  save the RAM if its in use.


Keyboard Mappings
-----------------

Most keys are the same on the Beeb and PC.  These are the ones that are not:

   PC (mapping 1)   Beeb

   F11              f0
   F1-F9            f1-f9
   F12              Break
   -_               -=
   =+               ^~
   `                @
   #~               _�
   ;:               ;+
   '@               :*
   End              Copy

Key mapping 2 is the same as 1 except A and S are mapped to Caps Lock and
Ctrl and F1-F9 are mapped to f0-f8.

Also, on a Windows 95 keyboard the menu key is mapped to space bar (which is
good for Rocket Raid and Thrust).


Menu Options
------------

File
  Load Disc 0  - Load a disc image into drive 0 or 1.  Use the File Type
  Load Disc 1    field to indicate if the disc image is a single or double
                 sided one.

  Load State   - Load or save the state of Beebem.  ROM and disc data is not
  Save State     saved so if a ROM or disc was being used when the state was
                 saved you need to make sure the same ROM or disc is loaded
                 when the state is restored.  State files are put in the
                 beebstate directory by default.

  Quick Load   - Load and save Beebem state without having to specify the
  Quick Save     filename.  State is saved and loaded from the
                 beebstate/quicksave file.

  Exit         - Exit Beebem

View
  Speed and Frames per Second  - Show or hide the relative speed and the
                                 number of frames per second.

  160x128                      - Set the window size.
  240x192
  320x256
  640x256
  640x512
  800x600
  1024x512
  1024x768

Speed
  Real Time    - Run Beebem at a constant speed, relative to a real BBC
  3/4 Speed      Micro.  The frame rate is varied in order to achieve the
  1/2 Speed      correct speed.  It may take a second or two to adjust it.

  50 FPS       - Run Beebem at a constant frame rate.  The slower the frame
  25 FPS         rate the faster Beebem runs relative to a BBC Micro.
  10 FPS
  5 FPS
  1 FPS

Sound
  On/Off         - Switch sound on or off.

  44.1 kHz       - Sets the sound sample rate.  The higher it is the better
  22.05 kHz        the sound quality but the slower Beebem runs.
  11.025 kHz

  Full Volume    - Set the sound volume.
  High Volume
  Medium Volume
  Low Volume

Options
  Allow ROM writes    - Enable/disable ROM writes.

  Ignore Illegal Instructions  - When disabled a dialog appears detailing
                                 the opcode and program counter.

  Joystick            - Switch on or off PC/Beeb analogue joystick support.
                        Calibrate the joystick through the control panel.

  Mousestick          - Switch on or off the mapping of Mouse position to
                        Beeb joystick.

  Hide Cursor         - Show or hide the mouse cursor while it is over the
                        Beebem window (useful when using the Mousestick).

  Keyboard Mapping 1  - Select either of the two keyboard mappings.
  Keyboard Mapping 2

Help
  About BBC Emulator  - Show version number and date of Beebem.


Finally
-------

Thanks to Dave Gilbert, Nigel Magnay and Robert Schmidt for writing the
emulator in the first place and distributing the source code with it - its
great!

If there are any other features you would like to see in the Windows version
(or the core emulator code) then send me some email.  I may be able to add
them.

Mike Wyatt
mikebuk@aol.com

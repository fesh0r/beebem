BeebEm for Microsoft Windows (27 Mar 1998)
============================

Beebem has been ported to MS Windows. The following people have contributed
to the Windows port:

  Nigel Magnay (magnayn@cs.man.ac.uk) - original port of V0.4
  Robert Schmidt (rsc@vingmed.no) - new mode 7 font, fixed backspace key
  Mike Wyatt (mikebuk@aol.com) - V0.7 to V1.0 features
  Laurie Whiffen - user keyboard mapping and SW Ram features

Note however that the Copyright for the Emulator resides with David Gilbert,
the original author, as described in the Readme.1st text file.


Compiling
---------

This distribution of Beebem contains Win32 executables but if you want to
compile it yourself then all the source files are included as well.  There
are also a couple of files that can be used with MS Visual C++ V5.0:

  BeebEm.dsw  - Developer Studio Workspace
  BeebEm.dsp  - VC++ Project file

You should be able to compile the sources using other compilers (such as
Borland or Watcom) but you may need to remove some of the App Studio stuff
from the resources file (beebem.rc).  You will also need to edit port.h to
change the EightByteType from __int64 (which is Microsoft specific) to a
structure of two long words.


Executable
----------

There is an executable in this distribution, in the 'intelbin' directory:

  BeebEm.exe  - Win32 version for all Intel processors

The executables will run on Windows 95 and NT 4.0.  You will also need to
install the Microsoft DirectX drivers if you have not already got them.
DirectX version 5 is available from the MS web page www.microsoft.com.


Running BeebEm
--------------

You will need to get some ROM images from a BBC Micro to run BeebEm.  You
will need OS1.2, Basic and DFS ROM images.  You will also need some disc
images to use with BeebEm.

You will find Robert Schmidt's 'BBC Lives!' web page useful.  His page is:

  http://www.nvg.unit.no/bbc/bbc.html

It contains DFS ROM images and disc images.  It also contains details of how
to get hold of the OS1.2 and Basic ROM images.  Take a look at the 'ROM
Images' section of the Archive page.  You will find the 'StuffIt Expander'
route the easiest (you may also need WinZip to expand the 'hqx' file to a
'sit' file) but if this does not work follow the 'Fools Guide to getting
ROMs'.

For the DFS I recommend you use the Watford DFS, you can find it here:

  ftp://ftp.nvg.unit.no/pub/bbc/rom/Watford/fs/DFS-1.44.rom

There is an example disc image in the discims directory in this
distribution.  It contains some simple programs to test the emulator.

Once you have ROM and disc images you need to create directories to keep
them in.  First create a directory to keep the emulator in, e.g. C:\Beeb,
and copy the executable into it.  Then create the following sub-directories
in C:\Beeb:

  beebfile  - put the ROM images in here
  discims   - put the disc images in here
  beebstate - Beeb State files go in here (leave this empty for now)

The OS ROM should have a file name of 'os12' (note: no file extension).  It
does not matter what the file names for the other ROMs are.  Beebem will
read all the ROMs from the beebfile directory (up to 16 of them) when it
starts up (8K and 16K ROMs).

Once all the files and directories are set up, run the emulator.  It should
work in any graphics mode but it will run fastest in a 256 colour mode.  If
you get an error message saying DirectX initialisation has failed then check
your DirectX installation (or install it if you have not already).

When BeebEm starts up you should see the normal 'BBC Computer 32K' message
followed by the start up messages for all the ROMs (i.e. the DFS and BASIC).
Load a disc using the File menu (remember to select the correct file type)
and press Shift+F12 to boot it.

If a disc does not boot using Shift+F12 then use '*CAT' and 'CH."filename"'
commands to load the programs from it.


New Features in V1.03 (Robert Schmidt)
--------------------------------------

Monochrome monitor/B&W TV emulation - In the "View" menu, open the "Monitor"
    submenu, then select either RGB, B&W, Amber or Green

(Files modified: beebwin.h, beebwin.cpp, beebemrc.h, beebem.rc, userkeybd.cpp)



New Features in V1.0
--------------------

Printer support - You can print to a file or any of the LPT or COM ports.

Switchable DirectX - DirectDraw screen updates and DirectSound output are
  not always faster than conventional display and sound output.  You can
  switch DirectX on and off to achieve the fastest emulation.


Disc Writing
------------

Disc writes and formatting works by writing data straight back to the image
files on your PC disc.  A disc image file will get extended/enlarged as
required by BeebEm.

WARNING - most disc images you will find on the Internet or that you have
created yourself will have an invalid catalogue (they contain just enough
information for disc reads to work but not writes).

When you write enable a disc BeebEm checks the disc catalogues and it will
display a warning message if either are invalid.  If you get a warning
message then do NOT write to the disc image, you will loose data.

To fix an invalid catalogue do the following:

  1. Start BeebEm and load the invalid image into drive 1.

  2. Create a new disc image file in drive 0.

  3. If the invalid image uses a Watford DFS 62 file catalogue then format
     the new disc image with a 62 file catalogue (you need a Watford DFS!),
       e.g. *ENABLE
            *FORM80 0

  4. Copy all the files from the invalid disc to the new one and set the
     shift break option and title,
       e.g. *COPY 1 0 *.*
            *OPT4,3
            *TITLE Games

The new image will now have a valid catalogue.  If you are using double
sided images then remember to format and copy both sides.


Some of the double sided disc images you find on the Internet are actually
distributed as two separate files (usually an 'a' and 'b' file, one for each
side).  You can now use BeebEm to create a single interleaved double sided
disc image from the two files.  This is how:

  1. Start BeebEm and create a new double sided disc image file in drive 0.

  2. If the disc images use a Watford DFS 62 file catalogue then format
     the new disc image with 62 file catalogues (you need a Watford DFS!),
       e.g. *ENABLE
            *FORM80 0
            *ENABLE
            *FORM80 2

  3. Load the side 1 file into drive 1 (remember to set the file type to
     single sided) and copy the files across,
       e.g. *COPY 1 0 *.*

  4. Load the side 2 file into drive 1 and copy the files across,
       e.g. *COPY 1 2 *.*

  5. Set the shift-break and titles on the new disc,
       e.g. *DRIVE 0
            *OPT4,3
            *TITLE Games
            *DRIVE 2
            *OPT4,3
            *TITLE Games

The new image will now contain both the original disc images.


Keyboard Mappings
-----------------

Most keys are the same on the Beeb and PC (UK keyboard).  These are the ones
that are not:

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

If you do not use a UK keyboard then you will probably want to set up your
own mapping.  Use the 'user keyboard' mapping options in BeebEm to do this
and remember to save it using the 'save preferences' option.


Menu Options
------------

File
  Load Disc 0  - Load a disc image into drive 0 or 1.  Use the File Type
  Load Disc 1    field to indicate if the disc image is a single or double
                 sided one.  Discs are write protected when loaded to
                 prevent any accidental data loss.

                 Note that the BeebDiscLoadX environment variables can still
                 be used to specify the discs that are loaded on start-up
                 (see ReadMe.1st).

  New Disc 0   - Creates a new disc image in drive 0 or 1.  Use the File
  New Disc 1     Type field to create a single or double sided image.  New
                 disc images are write enabled when created.  The images
                 have a standard 31 file catalogue by default.  If you want
                 a 62 file catalogue (Watford DFS) then format the disc.

  Write Protect 0 - Toggles write protection for drive 0 or 1.  Keep discs
  Write Protect 1   write protected unless you intend to write to them.
                    Also see WARNING above.

  Load State   - Load or save the state of Beebem.  ROM and disc data is not
  Save State     saved so if a ROM or disc was being used when the state was
                 saved you need to make sure the same ROM or disc is loaded
                 when the state is restored.  SW RAM data is saved however
                 if it is in use at the time.  State files are put in the
                 beebstate directory by default.

  Quick Load   - Load and save Beebem state without having to specify the
  Quick Save     filename.  State is saved and loaded from the
                 beebstate/quicksave file.

  Printer Destination - Select where to send the printer output.
                        WARNING - if you direct printer output to an LPT or
                        COM port that is not attached to anything BeebEm may
                        hang.

  Printer On/Off      - Switches printer capture on or off.  To start and
                        stop printing within BeebEm use the VDU2 and VDU3
                        commands or Ctrl B and Ctrl C.

  Exit         - Exit Beebem

View
  DirectDraw On/Off   - Switches DirectDraw screen output on or off.
                        DirectDraw should be faster than GDI output but this
                        is not always the case.  Choose whatever gives the
                        most frames per second.

  Buffer In Video RAM - When using DirectDraw each frame can be prepared in
                        either video or system RAM.  This option switches
                        between the two.  Choose whatever gives the most
                        frames per second.

  Speed and FPS On/Off - Show or hide the relative speed and the number of
                         frames per second.

  160x128              - Set the window size.  The bigger the window the
  240x192                slower it gets!  Full screen with DirectDraw
  320x256                switched on will be reasonably quick though.
  640x256
  640x512
  800x600
  1024x512
  1024x768
  Full Screen

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
  Sound On/Off   - Switch sound on or off.  Note that sound only really
                   works properly when running in real time mode.

  DirectSound On/Off - Switches DirectSound output on or off.  DirectSound
                       should be faster than MM output but this may not be
                       the case on your PC.  Choose whatever gives the most
                       frames per second while sound is being generated.

  44.1 kHz       - Sets the sound sample rate.  The higher it is the better
  22.05 kHz        the sound quality but the slower Beebem runs.
  11.025 kHz

  Full Volume    - Set the sound volume.
  High Volume
  Medium Volume
  Low Volume

AMX
  On/Off          - Switch AMX mouse on or off.  Note that you may find the
                    AMX mouse easier to use if you reduce the Beebem speed
                    to 3/4 or 1/2 speed.  It may also be useful to hide the
                    Windows cursor as well (see the Options menu).

  L+R for Middle  - Simulates a middle button press when you press the left
                    and right buttons together.

  Map to 160x256  - Coordinate range to map the Windows mouse position to.
  Map to 320x256    Pick the one that gives AMX mouse movements nearest to
  Map to 640x256    your Windows mouse movements.

  Adjust +50%     - Percentage to increase or decrease the AMX map sizes.
  Adjust +30%       Pick the one that gives AMX mouse movements that are
  Adjust +10%       slightly greater than the corresponding Windows mouse
  Adjust -10%       movements.  You can then match up the AMX and Windows
  Adjust -30%       pointer positions by moving the Windows pointer to the
  Adjust -50%       edges of the BeebEm Window.  This is easiest to do in
                    full screen mode.

Options
  Allow ROM writes    - Enable/disable ROM writes for each ROM slot.
                        ROMs read at start-up are write protected by
                        default.  All unused slots are set up as RAM.

  Ignore Illegal Instructions - When disabled a dialog appears detailing
                                the opcode and program counter.

  Joystick            - Switch on or off PC/Beeb analogue joystick support.
                        Calibrate the joystick through the control panel.

  Mousestick          - Switch on or off the mapping of Mouse position to
                        Beeb joystick.

  Hide Cursor         - Show or hide the mouse cursor while it is over the
                        Beebem window (useful when using the Mousestick or
                        the AMX mouse).

  Define User Keyboard - Allows you to define your own keyboard mapping.
                         Click on one of the BBC Micro keys and then press
                         the key you want mapped to it (most will already
                         be mapped to the correct keys).  Once you have
                         defined the keys you want select the 'user mapping'
                         and remember to save it using 'save prefs' option.

  User Defined Mapping - Select the user defined mapping or either of the
  Keyboard Mapping 1     two fixed keyboard mappings.
  Keyboard Mapping 2

  Save Preferences     - Saves the BeebEm start-up options.  This includes
                         the window size and position, speed, sound,
                         joystick and keyboard mappings.  If you want to
                         return all BeebEm settings to their defaults then
                         delete the BEEBEM.INI file from your WINDOWS
                         directory.

Help
  About BeebEm         - Show version number and date of BeebEm.


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

Beebem for Microsoft Windows (1 Sep 1997)
============================

Beebem has been ported to MS Windows. The following people have contributed
to the Windows port:

  Nigel Magnay (magnayn@cs.man.ac.uk) - original port of V0.4
  Robert Schmidt (rsc@vingmed.no) - new mode 7 font, fixed backspace key
  Mike Wyatt (mikebuk@aol.com) - V0.7 and V0.8 features
  Laurie Whiffen - user keyboard mapping and SW Ram features

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

The executables run fine on Windows 95.  They should run on NT 4.0, 3.51 and
Win 3.1 with Win32s but I have not tried them.  The executables were not
compiled with explicit Win32s support so they may not work quite as smoothly
on 3.1 and NT3.51 as on Win95 or NT4.

NOTE: The V0.4 distribution contained a WinG version.  This version does not
contain the WinG code as the GDI in Win95 appears to give exactly the same
performance as the WinG library (perhaps its been built in?).


Running BeebEm
--------------

You will need to get some ROM images from a BBC Micro to run BeebEm.  You
will need OS1.2, Basic and DFS ROM images.  You will also need some disc
images to use with BeebEm.

You may find Robert Schmidt's Beeb web page useful.  It contains images of
ROMS and discs and contains details of various ways to transfer ROM images
and discs from a Beeb to a PC.  See http://www.nvg.unit.no/bbc/bbc.html

I recommend you use the Watford DFS, you can find it here:
  ftp://ftp.nvg.unit.no/pub/bbc/rom/Watford/fs/DFS-1.44.rom

There is also an example disc image in the discims directory in this
distribution.  It contains some simple programs to test the emulator.

Once you have ROM and disc images you need to create directories to keep
them in.  First create a directory to keep the emulator in, e.g. C:\Beeb,
and copy the executable into it.  Then create the following sub-directories
in C:\Beeb:

  beebfile  - put the ROM images in here
  discims   - put the disc images in here
  beebstate - Beeb State files go in here (leave this empty for now)

The OS ROM should have a file name of 'os12'.  It does not matter what the
file names for the other ROMs are.  Beebem will read all the ROMs from the
beebfile directory (up to 16 of them) when it starts up (8K and 16K ROMs).

Once all the files and directories are set up, run the emulator.  It should
work in any graphics mode but it will run fastest in a 256 colour mode.

When BeebEm starts up you should see the normal 'BBC Computer 32K' message
followed by the start up messages for all the ROMs (i.e. the DFS and BASIC).
Load a disc using the File menu (remember to select the correct file type)
and press Shift+F12 to boot it.

If a disc does not boot using Shift+F12 then use '*CAT' and 'CH."filename"'
commands to load the programs from it.


New Features in V0.8
--------------------

Disc write and format support - any changes you make to files within BeebEm
  will automatically get written back to the image file.  BeebEm also checks
  that the disc catalogue is valid (see below).

Warnings for incorrect disc selection - BeebEm will warn you if it thinks
  you have selected the wrong disc type when loading a disc image (it is
  vital to get it right if you are going to write to the disc).

User defined keyboard mapping - the mapping for the whole keyboard can be
  changed using a graphical representation of the BBC Micro keyboard.

Individual sideways ROM/RAM selection - ROM slots can be changed between ROM
  and RAM while BeebEm is running.

User preferences - preferences for BeebEm start-up can be saved.  This
  includes the user defined key mapping.


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

  Exit         - Exit Beebem

View
  Speed and Frames per Second  - Show or hide the relative speed and the
                                 number of frames per second.

  160x128                      - Set the window size.  The bigger the window
  240x192                        the slower it gets!
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
  On/Off         - Switch sound on or off.  Note that sound only really
                   works properly when running in real time mode.

  44.1 kHz       - Sets the sound sample rate.  The higher it is the better
  22.05 kHz        the sound quality but the slower Beebem runs.
  11.025 kHz

  Full Volume    - Set the sound volume.
  High Volume
  Medium Volume
  Low Volume

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
                        Beebem window (useful when using the Mousestick).

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

Readme.1st for V0.9 of Beebem (see 'changes' file for history)

/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994/1995                   */
/*              -----------------------------------------                   */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed COMPLETE,with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warranties, or guarantees etc. - you use it at   */
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

This directory contains 'Beebem' a program which attempts to emulate
the behavior of the British Broadcasting Corporation Model B computer system
produced in the early 1980's by Acorn Computers Ltd of Cambridge, England.
It should be noted at this stage that the author has no connection with
either the BBC or Acorn and this product has not been approved or
sanctioned by either of them.  The author recognizes all trademarks etc.
I don't want to tread on anyones toes - if there is something else I should
say then tell me - I'll add it.

The copyright on everything in Beebem is held by the author, David Alan Gilbert.

<OK thats the formal stuff out of the way.....>

MICROSOFT WINDOWS
-----------------

Beebem has been ported to Microsoft Windows.  For details of the Windows
version see the ReadMe.Win file.  Most of this readme refers to the X
version for UNIX systems.

0) PLEASE
---------
Send me suggestions for improvments - both in the emulator and the way I have
coded it.  I'm fairly new to C++ and X (although I have a lot of C experience)
so any lessons on what I am doing wrong are welcome.
If you find a bug, don't just curse at it - tell me about it and I may be able
to fix it.

Dave Gilbert - beebem@treblig.org

1) This program is designed to run under the X windows system on Unix
workstations.  It has been tested on a Linux PC,Sun and HP workstations.

2) You need an 8 bit deep display on a server which supports the MIT
shared memory extensions - I've not made any provision for use without shared
memory - this also means that you have to run the program on the workstation
that the display is on.

3) The program requires a C++ compiler which supports templates.  I haven't
made much use of this yet.  I have used gcc 2.58,2.6.3 and 2.7.0 - I strongly
recommend 2.6.3 over 2.5.8 since 2.5.8 has a nasty habit of missing some
errors. It actually needs to be able to handle nested templates - I think the
Silicon Graphics compilers have problems with that.

4) It also requires the C++ iostream libraries.  The names of these vary from
OS to OS and compiler version etc.  The Linux config file now uses -lstdc++
which gcc 2.7.0 seems to use (or is it the latest Linux libraries???) - if
you have earlier versions you will need to change this to -liostream .

5) You will need images of the Beeb D(N)FS, OS 1.2 and BASIC and disc images
of programs you wish to run under the beeb - more details below.

Installation
------------
You've presumably unpacked this tar - good!

Next thing is to construct a makefile (or three!).  First have a look
if there is a 'config.' file for your machine - e.g. config.linux.  If not
create one based on one of them.  Edit the file until it matches your
environment (includes etc.).

Now type 'configure machinename' (e.g. configure linux) - that
will build all the makefiles.  Now edit port.h - if your
using gcc you probably don't need to do anything.
Finally you may have to change the keyboard map.
There is a line in beebwin.cc:

#include "keytable_2"

Thats a keyboard table which suits PC style keyboards (and Sun 4, and some
Sun 5) - you can change that to point at another keytable file.  If you
want to create your own see the description below.

If you want sound you need to add -DSOUNDSUPPORT onto the CFLAGS in the top
level makefile.  Sound has only been tested (and probably will only work) on
Linux.  You need a lot of processing speed for it (but see the
BeebVideoRefreshFreq environment variable below).

Now type 'make' - and hope for the best! When it doesn't work look at the
bottom of this file for some hints.

Now you have to get three disc images from a beeb.  One of an 8271 dfs (I
used DNFS).  Place that in beebfile/dnfs (even if its a dfs).  Put the basic
rom in beebfile/basic and the OS in beebfile/os12.  These images are just 16KB
binary files. I CAN'T SEND YOU COPIES OF THESE FILES!!!!

If you are planning on reading a disc (a good idea!) you'll have to figure out
how to make a disc image.  The present disc images are simple sector images -
the format of which is described below.  I produced these using an
Acorn Archimedes with a 5.25" drive - I suspect you can do the same with a PC
with a suitable drive (which I do not have).

If you can't generate a disc image then you can use the test one supplied
in the discims directory.

By default 'beebem' looks in discims/test.ssd for the discimage (which is an
80 track single sided image).  An environment variable can be used to change
this - see below.

OK - now type 'beebem'.  It should come up in mode 7 with the standard beeb
start up messages.

Use:
----
The only thing you really have to get used to is the keyboard mapping which
isn't quite right yet.  Break is mapped to F12 and Break and Pause.  One of
those should work for you.  If not try L2 on a sun.  On my Linux box shift or
Ctrl F12 doesn't work but Shift+F2 seems to do the trick - I don't understand
why. F0 is mapped to F10.  On HP's the four unmarked keys above the keypad
correspond to F9-F12.

The rest of the keyboard is also a bit strange.  Each key on the keyboard
corresponds directly to a key on the beeb's keyboard - that is one key will
have the effect of the beeb's ;+ key even though the workstations keyboard
doesn't have it.   The mapping is defined in the 'keymaps_2' file which works
well for PC, Sun 4 and some Sun 5 keyboards, details of how to change it are
described below.

BUT! z,x and / are the same and the default map uses the key with the ' on the
bottom for the beebs :* key - that should let you play most games!

Environment variables
---------------------
1) BeebDiscLoad0 and BeebDiscLoad1

Default value for BeebDiscLoad0 is "S:80:discims/test.ssd".

These variables allow you to load disc images into drives 0 and 1.
Values should take the form "D|S:tracks:filename", D for double sided image
and S for single sided.

E.g. doing the following in SH loads a (D)ouble sided, 80 track disc image
     called 'discims/games' into drive 0.

BeebDiscLoad0="D:80:discims/games"
export BeebDiscLoad0

The format of the disc image files is described below.

2) BeebVideoRefreshFreq (default 1)

This is a speed hack.  If set to 'n' then only every n'th field is rendered
and thus the emulation is faster (but not 'n' times!!).

If you have enabled the sound support but all you get is noise when you run
BeebEm then try setting BeebVideoRefreshFreq to a larger value.  Keep
increasing the value until sound generation works.  On a Pentium 100 try
values between 5 and 8.  On a P200 it should run with a value of 1 (it will
depend on your graphics card speed however).

2) BeebDiscWrites

Defining this variable will enable disc writes for both drives.  Any changes
you make to the files in the disc will automatically get written back to the
image file.

WARNING - most disc images you will find on the internet or that you have
created yourself will have an invalid catalogue (they contain just enough
information for disc reads to work but not writes).

When beebem starts up with disc writes enabled it checks the disc catalogues
and it will display a warning message if either are invalid.  If you see a
warning message then do NOT write to the disc image, you will loose data.

To fix an invalid catalogue do the following:

  1. Create a new disc image file,
       e.g. touch discims/newdisc.ssd

  2. Load the old and new images into BeebEm with writes enabled,
       e.g. BeebDiscLoad0="S:80:discims/newdisc.ssd"
            BeebDiscLoad1="S:80:discims/olddisc.ssd"
            BeebDiscWrites=1
            export BeebDiscLoad0 BeebDiscLoad1 BeebDiscWrites
            beebem

  3. Format the new disc image - you will need a DFS with a format command
     to do this (such as the Watford DFS),
       e.g. *ENABLE
            *FORM80 0     (make sure you use the correct drive!)

  4. Copy all the files from the old disc to the new one and set the shift
     break option and title,
       e.g. *COPY 1 0 *.*
            *OPT4,3
            *TITLE Games

The new image will now have a valid catalogue.  If you are using double
sided images then remember to format and copy both sides.


Disc image formats
------------------

Two forms of disc image are presently supported.  The first is single sided
format.  This consists of a raw disc image of a beeb, 10 sector per track, 256
byte per sector disc side.  So the image is ordered
  track 0,sector 0,
  track 0,sector 1,
         .
  track 0,sector 9,
  track 1,sector 0

  etc.

The double sided format uses interleaved tracks:
  track 0,head 0, sector 0,
            .
  track 0,head 0, sector 9,
  track 0,head 1, sector 0,
            .
  track 0,head 1, sector 9,
  track 1,head 0, sector 0,
            etc

This double sided format can be produced using the !Zap editor on the Acorn
Archimedes with a 5 1/4" drive.  The author intends to write a utility for
Linux to do the same thing.

Keymaps
-------
Mapping the beeb keyboard to that of a workstation is a nightmare.  Due to the
way in which the beeb keyboard handles shifts, it is difficult to reliably map
the symbols printed on the workstation keys to the beeb.  For this reason what
'beebem' does is to map one workstation key exactly onto a beeb key.  For
example on the standard keymap the '6^' key on my PC or workstation actually
produces 6& just like the beeb.

You may have to modify the keytable (keytable_2) if you have a wierd keyboard.
It consists of a table of X keysyms and the corresponding row/column positions
on the beebs key matrix.  The keysyms in this file should be the first keysym
associated with akey - i.e. what you get without any modifiers (shifts/control
etc.) - so only XK_t is used NOT Xk_T.

Please remember the terminator on the end of the list.

One problem is that lock keys, like the Caps Lock key get locked before the
emulator gets to them, so to release a capslock on the beeb you have to press
it twice.  I've got some ideas for how to cure this, but in the mean time if
you have a game which uses capslock (e.g. positron) you could always use the
keytable file to map it to a different key.

If you have good working keytables I'd be happy to include them in the
distribution.

Emulation Limitations
---------------------
1) Hold and Release graphics in Mode 7

These two command codes aren't emulated in Mode 7- if someone can explain to
me what they are supposed to do, I'll add them, but so far I've completely
failed to find a decent explanation!

2) Incomplete 8271 emulation

A subset of the 8271 is supported.  Enough to allow disc reads, writes and
formatting of 'normal' BBC format discs, i.e. 40 or 80 track, single or
double sided, 256 byte sectors, 10 sectors per track.

3) Elite - sticky explosions (bug)

The explosions don't go away in Elite - I suspect I know why.

4) Revs - screen display corrupted (bug)

Revs relies on precise timing information and is difficult to emulate without
hacking.  I intend to cure this in later releases.

5) Undefined opcodes

Many of the undocumented 6502 instructions have been implemented in V0.7 so
games such as Zalaga and Dare Devel Denis now work.  There are quite a few
opcodes that are still not implemented though.  The emulator treats all of
these instructions as NOPs.

6) Timing

The timing of instructions is not precise - well most are but the special
cases (i.e. if you skip over a page boundary it takes one more cycle) are not
accounted for.

7) Missing hardware

Serial, ADC, Printer port, keyboard LEDs, tube, light pen, and
econet are not yet emulated.  Econet holds a lot of possiblities - I
quite like the idea of doing a socket based daemon type thing.

8) Hangs after reset

Sometimes if you've played a game and hit reset the emulated beeb hangs - I
think thats due to not resetting VIAs correctly.

9) Junk at bottom/side of screen

In mode 7 you sometimes get a bit of junk at the bottom of the screen.  This
also happens in screen modes which only use part of the display area.

10) Crashes if you resize window on X

Yeh - I know; I must fix that....

Problems
--------
1) olvwm
On suns olvwm never passes any input to beebem.
On the olvwm I have with Linux, beebem never gets focus in and out events
which it needs to handle auto repeat properly.
The solution in both cases is to use a different window manager - such as
Fvwm.

2) Can't find stdc++ during link
Change the '-lstdc++' to '-liostream'; this depends on the compiler version.

3) Lockups
It hasn't done it for a while - but I had it happen on an older version of the
X server under Linux - if it happens to you try hitting a key.  If you've got
Linux try and get the latest version of the server and see if that helps.

4) Errors

The emulator doesn't handle X, memory or file missing problems nicely - it
normally just core's.

Other things
------------
1) Other signals
Sending a SIGUSR1 causes beebem to dump the register state after each
instruction.
Sending a SIGUSR2 causes beebem to dump the state of all IO registers once.

2) Adding ROMS
In beebmem.cc in the routine BeebMemInit there are a number of lines of the
form :

ReadRom("basic",0xf);

You can add extra lines to read into other rom banks.

Todo
----

1) Fix bugs
2) Add extra hardware support
3) Speed it up!!!! (although it works fine on my Pentium-90 or an HP9000/700!)
4) Snazzy X windows thing for setting options, key maps, selecting disc images
and roms.
5) Make it portable - its portable on the basis that it will probably work
under GCC on any platform - that really isn't as portable as it should be.

Thanks
------

Thanks to the members of the Beeb emulator mailing list for giving me the mad
idea of writing this thing and suggesting fixes for some problems.  Thanks for
various friends and members of the department for trying the program (at very
early and touchy stages!) and lending me discs to try on it!

PLEASE
------
Send me suggestions for improvments - both in the emulator and the way I have
coded it.  I'm fairly new to C++ and X (although I have a lot of C experience)
so any lessons on what I am doing wrong are welcome.
If you find a bug, don't just curse at it - tell me about it and I may be able
to fix it.

Dave Gilbert - beebem@treblig.org

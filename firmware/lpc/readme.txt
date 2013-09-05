How to compile it on Windows
============================

Prerequisites: Python 2.7, directory in PATH

- install GCC ARM: https://launchpad.net/gcc-arm-embedded/+download (I've used 
"4.7-2013-q2-update release from the 4.7 series released 2013-06-28")

- run the installer: gcc-arm-none-eabi-4_7-2013q2-20130614-win32.exe Looks like 
the build script has problems with spaces in path names, so enter "c:\gcc-arm" 
as the destination directory

- clone my mbed project: https://github.com/FrankBuss/mbed
(it is the original project, except one pull-request is pending, a missing 
define for the LPC11U24_301 target)

- copy "settings.py" in "C:\Users\Frank\Documents\GitHub\mbed\workspace_tools" 
(replace "Frank" with your Windows user name) to "private_settings.py" and edit 
it: GCC_ARM_PATH = "c:\\gcc-arm\\bin" 

- edit tests.py, add this at the end of the TESTS list:
    {
        "id": "FPGALink", "description": "FPGALink",
        "source_dir": "C:\\Users\\Frank\\Documents\\GitHub\\libfpgalink\\firmware\\lpc",
        "dependencies": [MBED_LIBRARIES, USB_LIBRARIES],
    },
(replace the "source_dir" content with the path where the sources are located on 
your system)

- on a DOS-prompt, build the libraries:
cd C:\Users\Frank\Documents\GitHub\mbed
python workspace_tools\build.py -m LPC11U24_301 -t GCC_ARM -u

- build the FPGALink program:
python workspace_tools\make.py -m LPC11U24_301 -t GCC_ARM -c -n FPGALink

The result is at 
C:\Users\Frank\Documents\GitHub\mbed\build\test\LPC11U24_301\GCC_ARM\FPGALink\lpc.bin
and can be copied to the microcontroller via USB in mass storage programming 
mode.


FPGA communication
==================

The firmware on the microcontroller uses SPI to communicate with the FPGA. This 
is a sample project on the FPGA side:

https://github.com/FrankBuss/comm_fpga/tree/master/spi

Currently there is no data throttling support in the microcontroller firmware 
with h2fReady_in and no implementation of f2hValid_in in the VHDL code.

The sample code implements six 32 bit registers, which can be accessed with 
channel 0..5.


Testing
=======

The firmware can be tested with the flcli application (latest version in the 
dev-branch). For example to set the GPIO P0.19 to output and low level (use "+" 
for high level):

win.x86/dbg/flcli.exe -v 1D50:602B -d "A19-"

"A" means port 0 and "B" means port 1.

JTAG scanning:

win.x86/dbg/flcli.exe -v 1D50:602B -q A1A2A3A4

(the ports are hardcoded in portMethods.cpp, so "A1A2A3A4" is ignored)

Programming a FPGA over JTAG:

win.x86/dbg/flcli.exe -v 1D50:602B -p J:A1A2A3A4:CrazyCartridge.svf

Testing the FPGA communication, writing to some registers:

win.x86/dbg/flcli.exe -v 1D50:602B -a "w0 cafebabe"
win.x86/dbg/flcli.exe -v 1D50:602B -a "w1 42abcdef"

Reading back the registers:

Frank@64bit$ win.x86/dbg/flcli.exe -v 1D50:602B -a "r0 4"
Attempting to open connection to FPGALink device 1D50:602B...
Executing CommFPGA actions on FPGALink device 1D50:602B...
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 CA FE BA BE                                     ....
Frank@64bit$ win.x86/dbg/flcli.exe -v 1D50:602B -a "r1 4"
Attempting to open connection to FPGALink device 1D50:602B...
Executing CommFPGA actions on FPGALink device 1D50:602B...
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 42 AB CD EF                                     B...


How to port it to other targets
===============================

Use e.g. "LPC1347" instead of "LPC11U24_301" for the steps in "How to compile it 
on Windows".

The source code should be target independant. All hardware related 
functionality, like SPI transfer and GPIO functions, are in portMethods.cpp and 
can be changed for your target, e.g. with "#if defined (TARGET_LPC1347)".

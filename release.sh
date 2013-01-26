#
# Copyright (C) 2009-2012 Chris McClelland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Helper script for building the binary distribution. It's unlikely you'll need
# this unless you're forking the project.
#
# After calling blinx.sh and building the MacOSX and Windows distributions, call
# this script to package up all the binaries and upload the datestamped
# distribution to the web.
#
#!/bin/bash
export LIB=libfpgalink
export DATE=$(date +%Y%m%d)
rm -rf ${LIB}-${DATE}
mkdir ${LIB}-${DATE}

# Linux x64 binaries
mkdir -p ${LIB}-${DATE}/lin.x64/rel
cp -rp lin.x64/rel/*.{so,txt} xsvf2csvf/lin.x64/rel/xsvf2csvf dump/lin.x64/rel/dump ${LIB}-${DATE}/lin.x64/rel/
mkdir -p ${LIB}-${DATE}/lin.x64/dbg
cp -rp lin.x64/dbg/*.{so,txt} ${LIB}-${DATE}/lin.x64/dbg/
cp -rp lin.x64/incs.txt ${LIB}-${DATE}/lin.x64/

# Linux x86 binaries
mkdir -p ${LIB}-${DATE}/lin.x86/rel
cp -rp lin.x86/rel/*.{so,txt} xsvf2csvf/lin.x86/rel/xsvf2csvf dump/lin.x86/rel/dump ${LIB}-${DATE}/lin.x86/rel/
mkdir -p ${LIB}-${DATE}/lin.x86/dbg
cp -rp lin.x86/dbg/*.{so,txt} ${LIB}-${DATE}/lin.x86/dbg/
cp -rp lin.x86/incs.txt ${LIB}-${DATE}/lin.x86/

# Linux armel binaries
mkdir -p ${LIB}-${DATE}/lin.armel/rel
cp -rp lin.armel/rel/*.{so,txt} xsvf2csvf/lin.armel/rel/xsvf2csvf dump/lin.armel/rel/dump ${LIB}-${DATE}/lin.armel/rel/
mkdir -p ${LIB}-${DATE}/lin.armel/dbg
cp -rp lin.armel/dbg/*.{so,txt} ${LIB}-${DATE}/lin.armel/dbg/
cp -rp lin.armel/incs.txt ${LIB}-${DATE}/lin.armel/

# Linux armhf binaries
mkdir -p ${LIB}-${DATE}/lin.armhf/rel
cp -rp lin.armhf/rel/*.{so,txt} xsvf2csvf/lin.armhf/rel/xsvf2csvf dump/lin.armhf/rel/dump ${LIB}-${DATE}/lin.armhf/rel/
mkdir -p ${LIB}-${DATE}/lin.armhf/dbg
cp -rp lin.armhf/dbg/*.{so,txt} ${LIB}-${DATE}/lin.armhf/dbg/
cp -rp lin.armhf/incs.txt ${LIB}-${DATE}/lin.armhf/

# Linux ppc binaries
mkdir -p ${LIB}-${DATE}/lin.ppc/rel
cp -rp lin.ppc/rel/*.{so,txt} xsvf2csvf/lin.ppc/rel/xsvf2csvf dump/lin.ppc/rel/dump ${LIB}-${DATE}/lin.ppc/rel/
mkdir -p ${LIB}-${DATE}/lin.ppc/dbg
cp -rp lin.ppc/dbg/*.{so,txt} ${LIB}-${DATE}/lin.ppc/dbg/
cp -rp lin.ppc/incs.txt ${LIB}-${DATE}/lin.ppc/

# MacOSX x86/x64 binaries
mkdir -p ${LIB}-${DATE}/osx/rel
cp -rp osx/rel/*.{dylib,txt} xsvf2csvf/osx/rel/xsvf2csvf dump/osx/rel/dump ${LIB}-${DATE}/osx/rel/
mkdir -p ${LIB}-${DATE}/osx/dbg
cp -rp osx/dbg/*.{dylib,txt} ${LIB}-${DATE}/osx/dbg/
cp -rp osx/incs.txt ${LIB}-${DATE}/osx/

# Windows x64 binaries
mkdir -p ${LIB}-${DATE}/win.x64/rel
cp -rp win.x64/rel/*.{dll,txt} win.x64/rel/*.lib win.x64/rel/*.pdb xsvf2csvf/win.x64/rel/xsvf2csvf.exe dump/win.x64/rel/dump.exe ${LIB}-${DATE}/win.x64/rel/
mkdir -p ${LIB}-${DATE}/win.x64/dbg
cp -rp win.x64/dbg/*.{dll,txt} win.x64/dbg/*.lib win.x64/dbg/*.pdb ${LIB}-${DATE}/win.x64/dbg/
cp -rp win.x64/incs.txt ${LIB}-${DATE}/win.x64/

# Windows x86 binaries
mkdir -p ${LIB}-${DATE}/win.x86/rel
cp -rp win.x86/rel/*.{dll,txt} win.x86/rel/*.lib win.x86/rel/*.pdb xsvf2csvf/win.x86/rel/xsvf2csvf.exe dump/win.x86/rel/dump.exe ${LIB}-${DATE}/win.x86/rel/
mkdir -p ${LIB}-${DATE}/win.x86/dbg
cp -rp win.x86/dbg/*.{dll,txt} win.x86/dbg/*.lib win.x86/dbg/*.pdb ${LIB}-${DATE}/win.x86/dbg/
cp -rp win.x86/incs.txt ${LIB}-${DATE}/win.x86/

# FLCLI binaries
export FLCLI=../../apps/flcli
cp -rp ${FLCLI}/lin.x64/rel/flcli ${FLCLI}/lin.x64/rel/libargtable2.so ${LIB}-${DATE}/lin.x64/rel/
cp -rp ${FLCLI}/lin.x64/dbg/flcli ${FLCLI}/lin.x64/dbg/libargtable2.so ${LIB}-${DATE}/lin.x64/dbg/
cp -rp ${FLCLI}/lin.x86/rel/flcli ${FLCLI}/lin.x86/rel/libargtable2.so ${LIB}-${DATE}/lin.x86/rel/
cp -rp ${FLCLI}/lin.x86/dbg/flcli ${FLCLI}/lin.x86/dbg/libargtable2.so ${LIB}-${DATE}/lin.x86/dbg/
cp -rp ${FLCLI}/lin.armel/rel/flcli ${FLCLI}/lin.armel/rel/libargtable2.so ${LIB}-${DATE}/lin.armel/rel/
cp -rp ${FLCLI}/lin.armel/dbg/flcli ${FLCLI}/lin.armel/dbg/libargtable2.so ${LIB}-${DATE}/lin.armel/dbg/
cp -rp ${FLCLI}/lin.armhf/rel/flcli ${FLCLI}/lin.armhf/rel/libargtable2.so ${LIB}-${DATE}/lin.armhf/rel/
cp -rp ${FLCLI}/lin.armhf/dbg/flcli ${FLCLI}/lin.armhf/dbg/libargtable2.so ${LIB}-${DATE}/lin.armhf/dbg/
cp -rp ${FLCLI}/lin.ppc/rel/flcli ${FLCLI}/lin.ppc/rel/libargtable2.so ${LIB}-${DATE}/lin.ppc/rel/
cp -rp ${FLCLI}/lin.ppc/dbg/flcli ${FLCLI}/lin.ppc/dbg/libargtable2.so ${LIB}-${DATE}/lin.ppc/dbg/
cp -rp ${FLCLI}/osx/rel/flcli ${FLCLI}/osx/rel/libargtable2.dylib ${LIB}-${DATE}/osx/rel/
cp -rp ${FLCLI}/osx/dbg/flcli ${FLCLI}/osx/dbg/libargtable2.dylib ${LIB}-${DATE}/osx/dbg/
cp -rp ${FLCLI}/win.x64/rel/flcli.exe ${FLCLI}/win.x64/rel/libargtable2.dll ${FLCLI}/win.x64/rel/libreadline.dll ${LIB}-${DATE}/win.x64/rel/
cp -rp ${FLCLI}/win.x64/dbg/flcli.exe ${FLCLI}/win.x64/dbg/libargtable2.dll ${FLCLI}/win.x64/dbg/libreadline.dll ${LIB}-${DATE}/win.x64/dbg/
cp -rp ${FLCLI}/win.x86/rel/flcli.exe ${FLCLI}/win.x86/rel/libargtable2.dll ${FLCLI}/win.x86/rel/libreadline.dll ${LIB}-${DATE}/win.x86/rel/
cp -rp ${FLCLI}/win.x86/dbg/flcli.exe ${FLCLI}/win.x86/dbg/libargtable2.dll ${FLCLI}/win.x86/dbg/libreadline.dll ${LIB}-${DATE}/win.x86/dbg/

# Headers
cp -rp ../../common/makestuff.h ${LIB}-${DATE}/
cp -rp ${LIB}.h ${LIB}-${DATE}/

# HDL files
#cp -rp hdl ${LIB}-${DATE}/

# Examples
rm -rf 2to3
wget -O - http://www.swaton.ukfsn.org/bin/2to3.tar.gz | tar zxf -
cd examples/python
cp fpgalink2.py fpgalink3.py
../../2to3/2to3 fpgalink3.py | patch fpgalink3.py
cd ../..
cp -rp examples ${LIB}-${DATE}/
sed -i s/fpgalink/fpgalink-${DATE}/g ${LIB}-${DATE}/examples/c/Makefile

# User Manual
#cp -p docs/*.pdf /mnt/ukfsn/docs/fpgalink/

# AVR firmware
mkdir -p ${LIB}-${DATE}/firmware
cat > ${LIB}-${DATE}/firmware/README <<EOF
This is the Atmel AVR firmware. It is suitable for loading into an AT90USB162 (e.g Minimus) or
ATMEGA32U2 (Minimus32) with Atmel FLIP or dfu-programmer.

There is no Cypress FX2LP firmware here because it is embedded into the FPGALink library binary.
EOF
cd firmware/avr
make clean
make MCU=atmega32u2
cp firmware.hex ../../${LIB}-${DATE}/firmware/atmega32u2.hex
make clean
make MCU=at90usb162
cp firmware.hex ../../${LIB}-${DATE}/firmware/at90usb162.hex
make clean
cd ../..

# Licenses
cp -p COPYING ${LIB}-${DATE}/
cp -p COPYING.LESSER ${LIB}-${DATE}/
cat > ${LIB}-${DATE}/README <<EOF
FPGALink Binary Distribution

Consult the User Manual:
  VHDL Paper Edition: http://www.swaton.ukfsn.org/bin/fpgalink-${DATE}/vhdl_paper.pdf
  VHDL Kindle Edition: http://www.swaton.ukfsn.org/bin/fpgalink-${DATE}/vhdl_mobile.pdf
  Verilog Paper Edition: http://www.swaton.ukfsn.org/bin/fpgalink-${DATE}/verilog_paper.pdf
  Verilog Kindle Edition: http://www.swaton.ukfsn.org/bin/fpgalink-${DATE}/verilog_mobile.pdf

FPGALink is a library for JTAG-programming and subsequently interacting with an FPGA over USB using
a microcontroller (Cypress FX2LP USB Atmel AVR8s).

It allows you to:
   * Load and save Cypress FX2LP firmware
   * Communicate with the FPGA using HiSpeed USB (~25Mbyte/s)
   * Reprogram the FPGA using JTAG over USB
   * Bootstrap an FPGA design standalone using minimal components

Supported platforms:
   * Linux x64, x86, armel, armhf, ppc
   * MacOSX x64, x86
   * Windows x64, x86

FPGALink (except flcli described below) is licensed under the LGPLv3.

Overview here: http://bit.ly/fpgalnk-blog
Source code here: https://github.com/makestuff/libfpgalink
API docs here: http://bit.ly/fpgalnk-api
Example code here: http://bit.ly/fpgalnk-ex

There is a command-line utility called "flcli", which offers many of the library's features, which
is useful for testing, etc. Unlike the rest of FPGALink, flcli is GPLv3-licensed.

chris@x64$ flcli --help
FPGALink Command-Line Interface Copyright (C) 2012 Chris McClelland

Usage: flcli [-psch] [-i <VID:PID>] -v <VID:PID> [-j <portSpec>] [-x <fileName>] [-a <actionString>]

Interact with an FPGALink device.

  -i, --ivp=<VID:PID>          vendor ID and product ID (e.g 04B4:8613)
  -v, --vp=<VID:PID>           vendor ID and product ID (e.g 04B4:8613)
  -j, --jtag=<portSpec>        JTAG port config (e.g D0234)
  -x, --xsvf=<fileName>        SVF, XSVF or CSVF file to load
  -p, --power                  FPGA is powered from USB (Nexys2 only!)
  -s, --scan                   scan the JTAG chain
  -a, --action=<actionString>  a series of CommFPGA actions
  -c, --cli                    start up an interactive CommFPGA session
  -h, --help                   print this help and exit

So assuming you're using a 1200K Digilent Nexys2 and an x64 Linux machine, you can load one of the
supplied example FPGA designs.

chris@x64$ # First make sure the udev rules are set up so normal users can access USB device 1443:0005:
chris@x64$ groups
users
chris@x64$ cat /etc/udev/rules.d/10-local.rules
ACTION=="add", SUBSYSTEM=="usb", ATTR{idVendor}=="1443", ATTR{idProduct}=="0005", GROUP="users", MODE="0660"

chris@x64$ # Now connect the Nexys2 and load the appropriate pre-built CSVF file:
chris@x64$ ./lin.x64/rel/flcli -i 1443:0005 -v 1443:0005 -x hdl/apps/makestuff/swled/cksum/vhdl/csvf/fx2all-nexys2-1200.csvf -p -s
Attempting to open connection to FPGALink device 1443:0005...
Loading firmware into 1443:0005...
Awaiting renumeration............
Attempting to open connection to FPGLink device 1443:0005 again...
Connecting USB power to FPGA...
The FPGALink device at 1443:0005 scanned its JTAG chain, yielding:
  0x21C2E093
  0xF5046093
Playing "hdl/apps/makestuff/swled/cksum/vhdl/csvf/fx2all-nexys2-1200.csvf" into the JTAG chain on FPGALink device 1443:0005...

chris@x64$ # You can then connect to the device with a simple command-line interface:

chris@x64$ dd if=/dev/urandom of=input.dat bs=1024 count=64
64+0 records in
64+0 records out
65536 bytes (66 kB) copied, 0.056143 s, 1.2 MB/s
chris@x64$ ./lin.x64/rel/flcli -v 1443:0005 -c
Attempting to open connection to FPGALink device 1443:0005...

Entering CommFPGA command-line mode:
> w1 12;w2 34;w3 56
> r1;r2;r3
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 12 34 56                                        .4V
> r1 4;r2 4;r3 4
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 12 12 12 12 34 34 34 34 56 56 56 56             ....4444VVVV
> r0 10
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 B7 ................
> r0 10 "output.dat"
> w0 123456
> w0 "input.dat"
> q
chris@x64$ od -t x1 output.dat 
0000000 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7
0000020
chris@x64$

The syntax of the read ("r") command is as follows:

  r<channel> [<count> [<fileName>]]

Where:

  channel:  The FPGA channel, 0-7f.
  count:    How many bytes to read from the FPGA channel, default 1.
  fileName: A binary file (in "quotes") to write the FPGA's data to.

If you don't specify a fileName, the FPGA's data is printed to stdout as a hex dump.

The syntax of the write ("w") command is as follows:

  w<channel> <byteSeq | fileName>

Where:

  channel:  The FPGA channel, 0-7f.
  byteSeq:  A sequence of bytes to be written to the FPGA channel, e.g 0123456789abcdef
  fileName: An existing binary file (in "quotes") to dump into the FPGA.

All numbers are in hexadecimal. Since a byte is two hex digits, the byteSeq must have an even number
of digits. Filenames must be quoted using double-quotes (""). You may put several read and/or write
commands on one line, separated by semicolons (";"). If you don't want an interactive session, you
can specify a command sequence on the command-line:

chris@x64$ ./lin.x64/rel/flcli -v 1443:0005 -a 'w0 "input.dat";r1;r2 4'
Attempting to open connection to FPGALink device 1443:0005...
Executing CommFPGA actions on FPGALink device 1443:0005...
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 12 34 34 34 34                                  .4444
chris@x64$

If you want to incorporate the FPGALink dynamic-link library into your own software, I have supplied
example code for how to do this from C, Python and Excel/VBA.

On the FPGA side, there is a simple module provided in both VHDL and Verilog for you to instantiate
in your FPGA designs. You are free to choose the actual implementation of the channels themselves
(Perhaps simple registers? Perhaps FIFOs?). I have supplied a couple of examples of how to
instantiate the module in your VHDL designs:

  hdl/apps/makestuff/swled/cksum/vhdl
  hdl/apps/makestuff/swled/fifo/vhdl

or if you prefer Verilog:

  hdl/apps/makestuff/swled/cksum/verilog
  hdl/apps/makestuff/swled/fifo/verilog
EOF

# Package it up
tar zcf fpgalink-bin.tar.gz ${LIB}-${DATE}
rm -rf ${LIB}-${DATE}
mkdir -p /mnt/ukfsn/bin/fpgalink-${DATE}
cp -p fpgalink-bin.tar.gz /mnt/ukfsn/bin/fpgalink-${DATE}/

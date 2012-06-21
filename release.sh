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

# Linux x86_64 binaries
mkdir -p ${LIB}-${DATE}/linux.x86_64/rel
cp -rp linux.x86_64/rel/*.so xsvf2csvf/linux.x86_64/rel/xsvf2csvf dump/linux.x86_64/rel/dump ${LIB}-${DATE}/linux.x86_64/rel/
mkdir -p ${LIB}-${DATE}/linux.x86_64/dbg
cp -rp linux.x86_64/dbg/*.so ${LIB}-${DATE}/linux.x86_64/dbg/

# Linux i686 binaries
mkdir -p ${LIB}-${DATE}/linux.i686/rel
cp -rp linux.i686/rel/*.so xsvf2csvf/linux.i686/rel/xsvf2csvf dump/linux.i686/rel/dump ${LIB}-${DATE}/linux.i686/rel/
mkdir -p ${LIB}-${DATE}/linux.i686/dbg
cp -rp linux.i686/dbg/*.so ${LIB}-${DATE}/linux.i686/dbg/

# Linux armel binaries
mkdir -p ${LIB}-${DATE}/linux.armel/rel
cp -rp linux.armel/rel/*.so xsvf2csvf/linux.armel/rel/xsvf2csvf dump/linux.armel/rel/dump ${LIB}-${DATE}/linux.armel/rel/
mkdir -p ${LIB}-${DATE}/linux.armel/dbg
cp -rp linux.armel/dbg/*.so ${LIB}-${DATE}/linux.armel/dbg/

# Linux ppc binaries
mkdir -p ${LIB}-${DATE}/linux.ppc/rel
cp -rp linux.ppc/rel/*.so xsvf2csvf/linux.ppc/rel/xsvf2csvf dump/linux.ppc/rel/dump ${LIB}-${DATE}/linux.ppc/rel/
mkdir -p ${LIB}-${DATE}/linux.ppc/dbg
cp -rp linux.ppc/dbg/*.so ${LIB}-${DATE}/linux.ppc/dbg/

# MacOSX binaries
mkdir -p ${LIB}-${DATE}/darwin/rel
cp -rp darwin/rel/*.dylib xsvf2csvf/darwin/rel/xsvf2csvf dump/darwin/rel/dump ${LIB}-${DATE}/darwin/rel/
mkdir -p ${LIB}-${DATE}/darwin/dbg
cp -rp darwin/dbg/*.dylib ${LIB}-${DATE}/darwin/dbg/

# Windows binaries
mkdir -p ${LIB}-${DATE}/win32/rel
cp -rp win32/rel/*.dll win32/rel/*.lib win32/rel/*.pdb xsvf2csvf/win32/rel/xsvf2csvf.exe dump/win32/rel/dump.exe ${LIB}-${DATE}/win32/rel/
mkdir -p ${LIB}-${DATE}/win32/dbg
cp -rp win32/dbg/*.dll win32/dbg/*.lib win32/dbg/*.pdb ${LIB}-${DATE}/win32/dbg/

# FLCLI binaries
export FLCLI=../../apps/flcli
cp -rp ${FLCLI}/linux.x86_64/rel/flcli ${FLCLI}/linux.x86_64/rel/libargtable2.so ${LIB}-${DATE}/linux.x86_64/rel/
cp -rp ${FLCLI}/linux.x86_64/dbg/flcli ${FLCLI}/linux.x86_64/dbg/libargtable2.so ${LIB}-${DATE}/linux.x86_64/dbg/
cp -rp ${FLCLI}/linux.i686/rel/flcli ${FLCLI}/linux.i686/rel/libargtable2.so ${LIB}-${DATE}/linux.i686/rel/
cp -rp ${FLCLI}/linux.i686/dbg/flcli ${FLCLI}/linux.i686/dbg/libargtable2.so ${LIB}-${DATE}/linux.i686/dbg/
cp -rp ${FLCLI}/linux.armel/rel/flcli ${FLCLI}/linux.armel/rel/libargtable2.so ${LIB}-${DATE}/linux.armel/rel/
cp -rp ${FLCLI}/linux.armel/dbg/flcli ${FLCLI}/linux.armel/dbg/libargtable2.so ${LIB}-${DATE}/linux.armel/dbg/
cp -rp ${FLCLI}/linux.ppc/rel/flcli ${FLCLI}/linux.ppc/rel/libargtable2.so ${LIB}-${DATE}/linux.ppc/rel/
cp -rp ${FLCLI}/linux.ppc/dbg/flcli ${FLCLI}/linux.ppc/dbg/libargtable2.so ${LIB}-${DATE}/linux.ppc/dbg/
cp -rp ${FLCLI}/darwin/rel/flcli ${FLCLI}/darwin/rel/libargtable2.dylib ${LIB}-${DATE}/darwin/rel/
cp -rp ${FLCLI}/darwin/dbg/flcli ${FLCLI}/darwin/dbg/libargtable2.dylib ${LIB}-${DATE}/darwin/dbg/
cp -rp ${FLCLI}/win32/rel/flcli.exe ${FLCLI}/win32/rel/libargtable2.dll ${FLCLI}/win32/rel/libreadline.dll ${LIB}-${DATE}/win32/rel/
cp -rp ${FLCLI}/win32/dbg/flcli.exe ${FLCLI}/win32/dbg/libargtable2.dll ${FLCLI}/win32/dbg/libreadline.dll ${LIB}-${DATE}/win32/dbg/

# Headers
cp -rp ../../common/makestuff.h ${LIB}-${DATE}/
cp -rp ${LIB}.h ${LIB}-${DATE}/

# HDL files
cp -rp hdl ${LIB}-${DATE}/

# CSVF files
cp -rp gen_csvf ${LIB}-${DATE}/

# Examples
rm -rf 2to3
wget -O - http://www.swaton.ukfsn.org/bin/2to3.tar.gz | tar zxf -
cd examples/python
cp fpgalink2.py fpgalink3.py
../../2to3/2to3 fpgalink3.py | patch fpgalink3.py
cd ../..
cp -rp examples ${LIB}-${DATE}/
rm -f ${LIB}-${DATE}/examples/c/Makefile

# User Manual
cp -p docs/*.pdf /mnt/ukfsn/docs/fpgalink/

# AVR firmware
mkdir -p ${LIB}-${DATE}/firmware
cat > ${LIB}-${DATE}/firmware/README <<EOF
This is the Atmel AVR firmware. It is suitable for loading into an AT90USB162 with Atmel FLIP or
dfu-programmer. The Cypress FX2LP firmware is embedded into the FPGALink library.
EOF
cp -rp firmware/avr/firmware.hex ${LIB}-${DATE}/firmware/at90usb162.hex

# Licenses
cp -p COPYING ${LIB}-${DATE}/
cp -p COPYING.LESSER ${LIB}-${DATE}/
cat > ${LIB}-${DATE}/README <<EOF
FPGALink Binary Distribution

Consult the User Manual:
  VHDL Paper Edition: http://www.swaton.ukfsn.org/docs/fpgalink/vhdl_paper.pdf
  VHDL Kindle Edition: http://www.swaton.ukfsn.org/docs/fpgalink/vhdl_mobile.pdf
  Verilog Paper Edition: http://www.swaton.ukfsn.org/docs/fpgalink/verilog_paper.pdf
  Verilog Kindle Edition: http://www.swaton.ukfsn.org/docs/fpgalink/verilog_mobile.pdf

FPGALink is a library for JTAG-programming and subsequently interacting with an FPGA over USB using
a microcontroller (Cypress FX2LP USB Atmel AVR8s).

It allows you to:
   * Load and save Cypress FX2LP firmware
   * Communicate with the FPGA using HiSpeed USB (~25Mbyte/s)
   * Reprogram the FPGA using JTAG over USB
   * Bootstrap an FPGA design standalone using minimal components

Supported platforms:
   * Linux x86_64, i686, arm, ppc
   * MacOSX x86_64, i386
   * Windows i386

FPGALink (except flcli described below) is licensed under the LGPLv3.

Overview here: http://bit.ly/fpgalnk-blog
Source code here: https://github.com/makestuff/libfpgalink
API docs here: http://bit.ly/fpgalnk-api
Example code here: http://bit.ly/fpgalnk-ex

There is a command-line utility called "flcli", which offers many of the library's features, which
is useful for testing, etc. Unlike the rest of FPGALink, flcli is GPLv3-licensed.

chris@armel$ flcli --help
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

So assuming you're using a Digilent Nexys2 connected to an ARM Linux machine, you can load one of
the supplied example FPGA designs:

chris@armel$ sudo linux.armel/rel/flcli -i 1443:0005 -v 1443:0005 -x gen_csvf/ex_cksum_nexys2-1200_fx2_vhdl.csvf -p -s
Attempting to open connection to FPGALink device 1443:0005...
Loading firmware into 1443:0005...
Awaiting renumeration............
Attempting to open connection to FPGLink device 1443:0005 again...
Connecting USB power to FPGA...
The FPGALink device at 1443:0005 scanned its JTAG chain, yielding:
  0x21C2E093
  0xF5046093
Playing "gen_csvf/ex_cksum_nexys2-1200_fx2_vhdl.csvf" into the JTAG chain on FPGALink device 1443:0005...
chris@armel$

You can then connect to the device with a simple command-line interface:

chris@armel$ dd if=/dev/urandom of=input.dat bs=1024 count=64
64+0 records in
64+0 records out
65536 bytes (66 kB) copied, 0.056143 s, 1.2 MB/s
chris@armel$ sudo ./linux.armel/rel/flcli -v 1443:0005 -c
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
chris@armel$ od -t x1 output.dat 
0000000 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7 b7
0000020
chris@armel$

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

chris@armel$ sudo ./linux.armel/rel/flcli -v 1443:0005 -a 'w0 "input.dat";r1;r2 4'
Attempting to open connection to FPGALink device 1443:0005...
Executing CommFPGA actions on FPGALink device 1443:0005...
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
00000000 12 34 34 34 34                                  .4444
chris@armel$

If you want to incorporate the FPGALink dynamic-link library into your own software, I have supplied
example code for how to do this from C, Python and Excel/VBA.

On the FPGA side, there is a simple module provided in both VHDL and Verilog for you to instantiate
in your FPGA designs. You are free to choose the actual implementation of the channels themselves
(Perhaps simple registers? Perhaps FIFOs?). I have supplied a couple of examples of how to
instantiate the module in your FPGA designs:

  hdl/sync/vhdl/ex_cksum
  hdl/sync/vhdl/ex_fifo

or if you prefer Verilog:

  hdl/sync/verilog/ex_cksum
  hdl/sync/verilog/ex_fifo
EOF

# Package it up
tar zcf ${LIB}-${DATE}.tar.gz ${LIB}-${DATE}
rm -rf ${LIB}-${DATE}
cp -p ${LIB}-${DATE}.tar.gz /mnt/ukfsn/bin/

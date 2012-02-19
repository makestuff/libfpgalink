# Helper script for building the binary distribution. It's unlikely you'll need this unless you're
# forking the project.
#
# After calling blinx.sh and building the MacOSX and Windows distributions, call this script to
# package up all the binaries and upload the datestamped distribution to the web.
#
#!/bin/bash
export LIB=libfpgalink
export DATE=$(date +%Y%m%d)
rm -rf ${LIB}-${DATE}
mkdir ${LIB}-${DATE}

# Linux x86_64 binaries
mkdir -p ${LIB}-${DATE}/linux.x86_64/rel
cp -rp linux.x86_64/rel/*.so xsvf2csvf/linux.x86_64/rel/xsvf2csvf ${LIB}-${DATE}/linux.x86_64/rel/
mkdir -p ${LIB}-${DATE}/linux.x86_64/dbg
cp -rp linux.x86_64/dbg/*.so ${LIB}-${DATE}/linux.x86_64/dbg/

# Linux i686 binaries
mkdir -p ${LIB}-${DATE}/linux.i686/rel
cp -rp linux.i686/rel/*.so xsvf2csvf/linux.i686/rel/xsvf2csvf ${LIB}-${DATE}/linux.i686/rel/
mkdir -p ${LIB}-${DATE}/linux.i686/dbg
cp -rp linux.i686/dbg/*.so ${LIB}-${DATE}/linux.i686/dbg/

# Linux armel binaries
mkdir -p ${LIB}-${DATE}/linux.armel/rel
cp -rp linux.armel/rel/*.so xsvf2csvf/linux.armel/rel/xsvf2csvf ${LIB}-${DATE}/linux.armel/rel/
mkdir -p ${LIB}-${DATE}/linux.armel/dbg
cp -rp linux.armel/dbg/*.so ${LIB}-${DATE}/linux.armel/dbg/

# Linux ppc binaries
mkdir -p ${LIB}-${DATE}/linux.ppc/rel
cp -rp linux.ppc/rel/*.so xsvf2csvf/linux.ppc/rel/xsvf2csvf ${LIB}-${DATE}/linux.ppc/rel/
mkdir -p ${LIB}-${DATE}/linux.ppc/dbg
cp -rp linux.ppc/dbg/*.so ${LIB}-${DATE}/linux.ppc/dbg/

# MacOSX binaries
mkdir -p ${LIB}-${DATE}/darwin/rel
cp -rp darwin/rel/*.dylib xsvf2csvf/darwin/rel/xsvf2csvf ${LIB}-${DATE}/darwin/rel/
mkdir -p ${LIB}-${DATE}/darwin/dbg
cp -rp darwin/dbg/*.dylib ${LIB}-${DATE}/darwin/dbg/

# Windows binaries
mkdir -p ${LIB}-${DATE}/win32/rel
cp -rp win32/rel/*.dll win32/rel/*.lib win32/rel/*.pdb xsvf2csvf/win32/rel/xsvf2csvf.exe ${LIB}-${DATE}/win32/rel/
mkdir -p ${LIB}-${DATE}/win32/dbg
cp -rp win32/dbg/*.dll win32/dbg/*.lib win32/dbg/*.pdb ${LIB}-${DATE}/win32/dbg/

# Headers
cp -rp ../../common/makestuff.h ${LIB}-${DATE}/
cp -rp ${LIB}.h ${LIB}-${DATE}/

# VHDL files
cp -rp vhdl ${LIB}-${DATE}/

# CSVF files
cp -rp gen_csvf ${LIB}-${DATE}/

# Examples
cp -rp examples ${LIB}-${DATE}/
rm -f ${LIB}-${DATE}/examples/c/Makefile

cp -p LICENSE.txt ${LIB}-${DATE}/
cat > ${LIB}-${DATE}/README <<EOF
FPGALink Binary Distribution

FPGALink is a library for JTAG-programming and subsequently interacting with an FPGA over USB using
a microcontroller (primarily the Cypress FX2LP, but with limited support for the USB AVR8s with
suitable firmware: http://bit.ly/nero-avr).

It allows you to:
   * Load and save Cypress FX2LP firmware
   * Communicate with the FPGA using HiSpeed USB (~25Mbyte/s)
   * Reprogram the FPGA using JTAG over USB
   * Bootstrap an FPGA design standalone using minimal components

Supported platforms:
   * Linux x86_64, i686, arm, ppc
   * MacOSX x86_64, i386
   * Windows i386

Overview here: http://bit.ly/fpgalnk-blog
Source code here: https://github.com/makestuff/libfpgalink
API docs here: http://bit.ly/fpgalnk-api
Example code here: http://bit.ly/fpgalnk-ex
EOF

# Package it up
tar zcf ${LIB}-${DATE}.tar.gz ${LIB}-${DATE}
rm -rf ${LIB}-${DATE}
cp -p ${LIB}-${DATE}.tar.gz /mnt/ukfsn/bin/

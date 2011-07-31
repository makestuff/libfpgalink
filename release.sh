#!/bin/bash
export LIB=libfpgalink
export DATE=$(date +%Y%m%d)
rm -rf ${LIB}-${DATE}
mkdir ${LIB}-${DATE}

# Linux binaries
mkdir -p ${LIB}-${DATE}/linux/rel
cp -rp linux/rel/*.so ${LIB}-${DATE}/linux/rel/
mkdir -p ${LIB}-${DATE}/linux/dbg
cp -rp linux/dbg/*.so ${LIB}-${DATE}/linux/dbg/

# MacOS binaries
mkdir -p ${LIB}-${DATE}/darwin/rel
cp -rp darwin/rel/*.dylib ${LIB}-${DATE}/darwin/rel/
mkdir -p ${LIB}-${DATE}/darwin/dbg
cp -rp darwin/dbg/*.dylib ${LIB}-${DATE}/darwin/dbg/

# Windows binaries
mkdir -p ${LIB}-${DATE}/win32/rel
cp -rp win32/rel/*.dll ${LIB}-${DATE}/win32/rel/
cp -rp win32/rel/*.lib ${LIB}-${DATE}/win32/rel/
cp -rp win32/rel/*.pdb ${LIB}-${DATE}/win32/rel/
mkdir -p ${LIB}-${DATE}/win32/dbg
cp -rp win32/dbg/*.dll ${LIB}-${DATE}/win32/dbg/
cp -rp win32/dbg/*.lib ${LIB}-${DATE}/win32/dbg/
cp -rp win32/dbg/*.pdb ${LIB}-${DATE}/win32/dbg/

# Headers
cp -rp ../../common/makestuff.h ${LIB}-${DATE}/
cp -rp ${LIB}.h ${LIB}-${DATE}/

# XSVF files
cp -rp gen_xsvf ${LIB}-${DATE}/

# Examples
cp -rp examples ${LIB}-${DATE}/
rm -f ${LIB}-${DATE}/examples/c/Makefile

cp -p LICENSE.txt ${LIB}-${DATE}/
cat > ${LIB}-${DATE}/README <<EOF
FPGALink Binary Distribution

FPGALink is a library for JTAG-programming and subsequently interacting with an FPGA over USB using
a microcontroller (primarily the Cypress FX2LP). It allows you to:

   * Load and save Cypress FX2LP firmware
   * Communicate with the FPGA using HiSpeed USB (~25Mbyte/s)
   * Reprogram the FPGA using JTAG over USB
   * Bootstrap an FPGA design standalone using minimal components

Overview here: http://www.makestuff.eu/wordpress/?page_id=1400
Source code here: https://github.com/makestuff/libfpgalink
API docs here: http://bit.ly/fpgalnk-api
Example code here: http://bit.ly/fpgalnk-ex
EOF

# Package it up
tar zcf ${LIB}-${DATE}.tar.gz ${LIB}-${DATE}
rm -rf ${LIB}-${DATE}
cp -p ${LIB}-${DATE}.tar.gz /mnt/ukfsn/bin/

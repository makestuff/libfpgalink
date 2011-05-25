#!/bin/bash
export DATE=$(date +%Y%m%d)
rm -rf libfpgalink-${DATE}
mkdir libfpgalink-${DATE}

# Linux binaries
mkdir -p libfpgalink-${DATE}/linux/rel
cp -rp linux/rel/*.so libfpgalink-${DATE}/linux/rel/
mkdir -p libfpgalink-${DATE}/linux/dbg
cp -rp linux/dbg/*.so libfpgalink-${DATE}/linux/dbg/

# Windows binaries
mkdir -p libfpgalink-${DATE}/win32/rel
cp -rp win32/rel/*.dll libfpgalink-${DATE}/win32/rel/
cp -rp win32/rel/*.lib libfpgalink-${DATE}/win32/rel/
cp -rp win32/rel/*.pdb libfpgalink-${DATE}/win32/rel/
mkdir -p libfpgalink-${DATE}/win32/dbg
cp -rp win32/dbg/*.dll libfpgalink-${DATE}/win32/dbg/
cp -rp win32/dbg/*.lib libfpgalink-${DATE}/win32/dbg/
cp -rp win32/dbg/*.pdb libfpgalink-${DATE}/win32/dbg/

# Headers
cp -rp ../../common/makestuff.h libfpgalink-${DATE}/
cp -rp libfpgalink.h libfpgalink-${DATE}/

# XSVF files
cp -rp gen_xsvf libfpgalink-${DATE}/

# Examples
cp -rp examples libfpgalink-${DATE}/
rm -f libfpgalink-${DATE}/examples/c/Makefile

cp -p LICENSE.txt libfpgalink-${DATE}/
cat > libfpgalink-${DATE}/README <<EOF
FPGALink Binary Distribution

FPGALink is a library for JTAG-programming and subsequently interacting with an FPGA over USB using
a microcontroller (primarily the Cypress FX2LP). It allows you to:

   * Load and save Cypress FX2LP firmware
   * Communicate with the FPGA using HiSpeed USB (~25Mbyte/s)
   * Reprogram the FPGA using JTAG over USB
   * Bootstrap an FPGA design standalone using minimal components

Overview here: http://www.makestuff.eu/wordpress/?page_id=1400
Source code here: https://github.com/makestuff/libfpgalink
API docs here: http://www.swaton.ukfsn.org/apidocs/libfpgalink_8h.html
EOF

# Package it up
tar zcf libfpgalink-${DATE}.tar.gz libfpgalink-${DATE}
zip -r libfpgalink-${DATE}.zip libfpgalink-${DATE}
rm -rf libfpgalink-${DATE}

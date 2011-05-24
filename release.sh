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

# Package it up
tar zcf libfpgalink-${DATE}.tar.gz libfpgalink-${DATE}
zip -r libfpgalink-${DATE}.zip libfpgalink-${DATE}
rm -rf libfpgalink-${DATE}

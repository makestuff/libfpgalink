#!/bin/sh

cd $(dirname $0)
DATE=$(date +%Y%m%d)
if [ $# -eq 1 ]; then
  DATE=$1
fi
HEADER="/*
 * THIS FILE IS MACHINE-GENERATED! DO NOT EDIT IT!
 *
 * $(sdcc -v | head -1)
 * FIRMWARE VERSION: ${DATE}
 *
 */"

# Fist the RAM firmware
make clean
make DATE=${DATE}
echo "${HEADER}" > ../../src/ramFirmware.c
../../../build/bin/mkfw firmware.hex ram bix >> ../../src/ramFirmware.c

# Then the EEPROM firmware
make clean
make DATE=${DATE} FLAGS="-DEEPROM"
echo "${HEADER}" > ../../src/eepromNoBootFirmware.c
../../../build/bin/mkfw firmware.hex eepromNoBoot iic >> ../../src/eepromNoBootFirmware.c

# Finally, clean up
make clean

#!/bin/bash

echo
echo "WARNING: this will erase any firmware currently in your FX2FPGA EEPROM!!!"
echo
echo "1) Power up the S3BOARD+FX2FPGA without the EEPROM jumper"
echo "2) Connect the EEPROM jumper"
echo "3) Press any key"
read -n 1 -s
echo
echo "Writing firmware to EEPROM now..."
sudo LD_LIBRARY_PATH=../../linux.x86_64/rel python <<EOF
from fpgalink import *
flLoadStandardFirmware("04B4:8613", "04B4:8613")
flAwaitDevice("04B4:8613", 600)
handle = flOpen("04B4:8613")
flAppendWriteRegisterCommand(handle, 0x00, 0x10)
flAppendWriteRegisterCommand(handle, 0x00, 0x10)
flAppendWriteRegisterCommand(handle, 0x00, 0x10)
flFlashStandardFirmware(handle, "04B4:8613", 512, "../../gen_xsvf/s3board.xsvf")
flClose(handle)
quit()
EOF
echo "...done"
echo

echo "4) Now power-cycle the S3BOARD+FX2FPGA"
echo "5) Wait for the display to show 0030"
echo "6) Press any key"
read -n 1 -s

sudo LD_LIBRARY_PATH=../../linux.x86_64/rel python <<EOF
from fpgalink import *
handle = flOpen("04B4:8613")
flWriteRegister(handle, 1000, 0x00, 0x10)
flWriteRegister(handle, 1000, 0x00, 0x10)
flWriteRegister(handle, 1000, 0x00, 0x10)
flWriteRegister(handle, 1000, 0x00, 0xaa)
flClose(handle)
quit()
EOF

echo
echo "The display should now read 010A"

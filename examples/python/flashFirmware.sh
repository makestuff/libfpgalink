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
#!/bin/bash
export PYTHON_VERSION=2

# S3BOARD + FX2FPGA:
export JTAG_PORT=D0234
export CSVF_FILE=../../hdl/apps/makestuff/swled/cksum/vhdl/csvf/fx2all-s3board.csvf

# MakeStuff LX9
#export JTAG_PORT=A7031
#export CSVF_FILE=../../hdl/apps/makestuff/swled/cksum/vhdl/csvf/fx2min-lx9.csvf

echo
echo "WARNING: this will erase any firmware currently in your FX2FPGA EEPROM!!!"
echo
echo "1) Power up the S3BOARD+FX2FPGA without the EEPROM jumper"
echo "2) Connect the EEPROM jumper"
echo "3) Press any key"
read -n 1 -s
echo
echo "Writing firmware to EEPROM now..."
LD_LIBRARY_PATH=../../lin.x64/rel python <<EOF
from fpgalink${PYTHON_VERSION} import *
flLoadStandardFirmware("04B4:8613", "1D50:602B:0002", "${JTAG_PORT}")
flAwaitDevice("1D50:602B:0002", 600)
handle = flOpen("1D50:602B:0002")
flAppendWriteChannelCommand(handle, 0x00, 0x10)
flAppendWriteChannelCommand(handle, 0x00, 0x10)
flAppendWriteChannelCommand(handle, 0x00, 0x10)
flFlashStandardFirmware(handle, "1D50:602B:0002", "${JTAG_PORT}", 512, "${CSVF_FILE}")
flClose(handle)
quit()
EOF
echo "...done"
echo

echo "4) Now power-cycle the S3BOARD+FX2FPGA"
echo "5) Wait for the display to show 0030"
echo "6) Press any key"
read -n 1 -s

LD_LIBRARY_PATH=../../lin.x64/rel python <<EOF
from fpgalink${PYTHON_VERSION} import *
handle = flOpen("1D50:602B:0002")
flWriteChannel(handle, 1000, 0x00, 0x10)
flWriteChannel(handle, 1000, 0x00, 0x10)
flWriteChannel(handle, 1000, 0x00, 0x10)
flWriteChannel(handle, 1000, 0x00, 0xaa)
flClose(handle)
quit()
EOF

echo
echo "The display should now read 010A"

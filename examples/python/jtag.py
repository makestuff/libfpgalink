#!/usr/bin/env python
#
# Copyright (C) 2014 Chris McClelland
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

# Use the low-level JTAG functions to query the JTAG chain.
#
import fpgalink
import binascii
import argparse

print("FPGALink Python JTAG Example Copyright (C) 2014 Chris McClelland\n")
parser = argparse.ArgumentParser(description='Load FX2LP firmware, load the FPGA, interact with the FPGA.')
parser.add_argument('-v', action="store", nargs=1, required=True, metavar="<VID:PID>", help="VID, PID and opt. dev ID (e.g 1D50:602B:0001)")
parser.add_argument('-q', action="store", nargs=1, required=True, metavar="<jtagPorts>", help="query the JTAG chain")
argList = parser.parse_args()
vp = argList.v[0]
jtagPorts = argList.q[0]
handle = fpgalink.FLHandle()
try:
    handle = fpgalink.flOpen(vp)
    fpgalink.progOpen(handle, jtagPorts)

    # Try special-purpose enum:
    fpgalink.jtagClockFSM(handle, 0x0000005F, 9)
    result = fpgalink.jtagShiftInOut(handle, 64, fpgalink.SHIFT_ZEROS)
    print('TDO data, given 64 zeros:\n  %s' % binascii.hexlify(result).decode("utf-8").upper())

    # Try bytes:
    fpgalink.jtagClockFSM(handle, 0x0000005F, 9)
    result = fpgalink.jtagShiftInOut(handle, 128, b"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xCA\xFE\xBA\xBE\xDE\xAD\xBE\xEF")
    print('TDO data, given 128 explicit bits:\n  %s' % binascii.hexlify(result).decode("utf-8").upper())

    fpgalink.progClose(handle)

except fpgalink.FLException as ex:
    print(ex)
finally:
    fpgalink.flClose(handle)

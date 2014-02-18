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
from fpgalink3 import *
import binascii

handle = flOpen("1d50:602b:0002")
progOpen(handle, "D0D2D3D4")

# Try special-purpose enum:
jtagClockFSM(handle, 0x0000005F, 9)
result = jtagShiftInOut(handle, 64, Shift.ZEROS)
print('TDO data, given 64 zeros:\n  %s' % binascii.hexlify(result).decode("utf-8").upper())

# Try bytes:
jtagClockFSM(handle, 0x0000005F, 9)
result = jtagShiftInOut(handle, 128, b"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xCA\xFE\xBA\xBE\xDE\xAD\xBE\xEF")
print('TDO data, given 128 explicit bits:\n  %s' % binascii.hexlify(result).decode("utf-8").upper())

progClose(handle)
flClose(handle)

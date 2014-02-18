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

# Read the JEDEC ID from the flash chip on the Lattice IceBlink40 board. This
# example demonstrates the usage of the bit-level I/O and SPI functions. The
# board has MISO=PB3, MOSI=PB2, SS=PB0, SCK=PB1, CRESET=PB6 & POWER=C2.
#
from fpgalink import *
import binascii

# Connect, reset the board and open an SPI interface:
handle = flOpen("1d50:602b:0001")
flMultiBitPortAccess(handle, "B6-,C2-")  # RESET low & cut the power
flSleep(10)
flSingleBitPortAccess(handle, 2, 2, PinConfig.HIGH)  # power on in RESET
progOpen(handle, "B3B2B0B1")  # open SPI
(port, bit) = progGetPort(handle, LogicalPort.SS)
flSingleBitPortAccess(handle, port, bit, PinConfig.HIGH)

# Send JEDEC device-id command, retrieve three bytes back
flSingleBitPortAccess(handle, port, bit, PinConfig.LOW)
spiSend(handle, 1, b"\x9F", BitOrder.MSBFIRST)
print("JEDEC ID: %s" % binascii.hexlify(spiRecv(handle, 3, BitOrder.MSBFIRST)))
flSingleBitPortAccess(handle, port, bit, PinConfig.HIGH)

# Close SPI interface, release reset and close connection
progClose(handle)
flMultiBitPortAccess(handle, "B6?")  # release RESET
flClose(handle)

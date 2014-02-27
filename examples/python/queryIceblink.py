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
import fpgalink
import binascii

# Connect, reset the board and open an SPI interface:
handle = fpgalink.flOpen("1d50:602b:0001")
fpgalink.flMultiBitPortAccess(handle, "B6-,C2-")  # RESET low & cut the power
fpgalink.flSleep(10)
fpgalink.flSingleBitPortAccess(handle, 2, 2, fpgalink.PIN_HIGH)  # power on in RESET
fpgalink.progOpen(handle, "B3B2B0B1")  # open SPI
(port, bit) = fpgalink.progGetPort(handle, fpgalink.LP_SS)
fpgalink.flSingleBitPortAccess(handle, port, bit, fpgalink.PIN_HIGH)

# Send JEDEC device-id command, retrieve three bytes back
fpgalink.flSingleBitPortAccess(handle, port, bit, fpgalink.PIN_LOW)
fpgalink.spiSend(handle, b"\x9F", fpgalink.SPI_MSBFIRST)
print("JEDEC ID: %s" % binascii.hexlify(fpgalink.spiRecv(handle, 3, fpgalink.SPI_MSBFIRST)))
fpgalink.flSingleBitPortAccess(handle, port, bit, fpgalink.PIN_HIGH)

# Close SPI interface, release reset and close connection
fpgalink.progClose(handle)
fpgalink.flMultiBitPortAccess(handle, "B6?")  # release RESET
fpgalink.flClose(handle)

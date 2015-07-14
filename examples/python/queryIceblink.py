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
import fl

VID_PID = "1D50:602B:0001"
PROG_CONFIG = "B3B2B0B1"
handle = fl.FLHandle()
try:
    fl.flInitialise(0)

    # Connect, reset the board, open SPI interface & get SS port:
    handle = fl.flOpen(VID_PID)
    fl.flMultiBitPortAccess(handle, "B6-,C2-")  # RESET low & cut the power
    fl.flSleep(10)
    fl.flSingleBitPortAccess(handle, 2, 2, fl.PIN_HIGH)  # power on in RESET
    fl.progOpen(handle, PROG_CONFIG)
    (port, bit) = fl.progGetPort(handle, fl.LP_SS)
    fl.flSingleBitPortAccess(handle, port, bit, fl.PIN_HIGH)
    
    # Send JEDEC device-id command, retrieve three bytes back
    fl.flSingleBitPortAccess(handle, port, bit, fl.PIN_LOW)
    fl.spiSend(handle, b"\x9F", fl.SPI_MSBFIRST)
    bs = fl.spiRecv(handle, 3, fl.SPI_MSBFIRST)
    print("JEDEC ID: {}".format(
        " ".join(["{:02X}".format(b) for b in bs])))
    fl.flSingleBitPortAccess(handle, port, bit, fl.PIN_HIGH)
    
    # Close SPI interface, release reset and close connection
    fl.progClose(handle)
    fl.flMultiBitPortAccess(handle, "B6?")  # release RESET

except fl.FLException as ex:
    print(ex)
finally:
    fl.flClose(handle)

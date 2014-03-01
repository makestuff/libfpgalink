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

import fl

VID_PID = "1d50:602b:0002"
PROG_CONFIG = "D0D2D3D4"
handle = fl.FLHandle()
try:
    fl.flInitialise(0)

    if ( not fl.flIsDeviceAvailable(VID_PID) ):
        print("Loading firmware...")
        fl.flLoadStandardFirmware("04b4:8613", VID_PID)

        print("Awaiting...")
        found = fl.flAwaitDevice(VID_PID, 600)
        print("Result: {}".format(found))

    handle = fl.flOpen(VID_PID)

    fl.flSingleBitPortAccess(handle, 3, 7, fl.PIN_LOW)
    print("fl.flMultiBitPortAccess() returned 0x{:010X}".format(
        fl.flMultiBitPortAccess(handle, "D7+")))
    fl.flSleep(100)

    fl.flSelectConduit(handle, 1)
    print("fl.flIsFPGARunning(): {}".format(fl.flIsFPGARunning(handle)))
    print("fl.flIsNeroCapable(): {}".format(fl.flIsNeroCapable(handle)))
    print("fl.flIsCommCapable(): {}".format(fl.flIsCommCapable(handle, 1)))
    print("fl.flGetFirmwareID(): {:04X}".format(fl.flGetFirmwareID(handle)))
    print("fl.flGetFirmwareVersion(): {:08X}".format(fl.flGetFirmwareVersion(handle)))

    print("fl.flProgram()...")
    fl.flProgram(handle, "J:{}:../../../../hdlmake/apps/makestuff/swled/cksum/vhdl/fpga.xsvf".format(PROG_CONFIG))
    print("...done.")

    fl.progOpen(handle, PROG_CONFIG)
    print("fl.progGetPort(): {")
    (port, bit) = fl.progGetPort(handle, fl.LP_MISO)
    print("  MISO: {:c}{}".format(ord('A')+port, bit))
    (port, bit) = fl.progGetPort(handle, fl.LP_MOSI)
    print("  MOSI: {:c}{}".format(ord('A')+port, bit))
    (port, bit) = fl.progGetPort(handle, fl.LP_SS)
    print("  SS:   {:c}{}".format(ord('A')+port, bit))
    (port, bit) = fl.progGetPort(handle, fl.LP_SCK)
    print("  SCK:  {:c}{}".format(ord('A')+port, bit))
    print("}")

    fl.jtagClockFSM(handle, 0x0000005F, 9)
    fl.jtagShiftInOnly(handle, 32, "\xCA\xFE\xBA\xBE")
    bs = fl.jtagShiftInOut(handle, 128, "\xCA\xFE\xBA\xBE\x15\xF0\x0D\x1E\xFF\xFF\xFF\xFF\x00\x00\x00\x00")
    print("fl.jtagShiftInOut() got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bs])))

    fl.jtagClockFSM(handle, 0x0000005F, 9)
    fl.spiSend(handle, "\xCA\xFE\xBA\xBE", fl.SPI_LSBFIRST)
    bs = fl.spiRecv(handle, 8, fl.SPI_LSBFIRST)
    print("fl.spiRecv() got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bs])))

    fl.progClose(handle)

    print("fl.jtagScanChain(): {")
    for idCode in fl.jtagScanChain(handle, PROG_CONFIG):
        print("  0x{:08X}".format(idCode))
    print("}")

    fl.flWriteChannel(handle, 0, b"\x01")

    bs = fl.flReadChannel(handle, 2, 16)
    print("Sync read got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bs])))
        
    print("Single byte read: {:02X}".format(fl.flReadChannel(handle, 2)))

    fl.flReadChannelAsyncSubmit(handle, 2, 16)
    fl.flReadChannelAsyncSubmit(handle, 2)
    bs = fl.flReadChannelAsyncAwait(handle)
    print("First async read got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bytearray(bs)])))

    bs = fl.flReadChannelAsyncAwait(handle)
    print("Second async read got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bytearray(bs)])))

except fl.FLException as ex:
    print(ex)
finally:
    fl.flClose(handle)

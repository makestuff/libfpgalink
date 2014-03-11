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
CONDUIT = 1
BYTE_ARRAY = b"\xCA\xFE\xBA\xBE"
conn = fl.FLHandle()
try:
    fl.flInitialise(0)

    if ( not fl.flIsDeviceAvailable(VID_PID) ):
        print("Loading firmware...")
        fl.flLoadStandardFirmware("04b4:8613", VID_PID)

        print("Awaiting...")
        fl.flAwaitDevice(VID_PID, 600)

    conn = fl.flOpen(VID_PID)

    fl.flSingleBitPortAccess(conn, 3, 7, fl.PIN_LOW)
    print("flMultiBitPortAccess() returned {:032b}".format(
        fl.flMultiBitPortAccess(conn, "D7+")))
    fl.flSleep(100)

    print("flGetFirmwareID(): {:04X}".format(fl.flGetFirmwareID(conn)))
    print("flGetFirmwareVersion(): {:08X}".format(fl.flGetFirmwareVersion(conn)))
    print("flIsNeroCapable(): {}".format(fl.flIsNeroCapable(conn)))
    print("flIsCommCapable(): {}".format(fl.flIsCommCapable(conn, CONDUIT)))

    fl.progOpen(conn, PROG_CONFIG)
    print("progGetPort(): {")
    (port, bit) = fl.progGetPort(conn, fl.LP_MISO)
    print("  MISO: {:c}{}".format(ord('A')+port, bit))
    (port, bit) = fl.progGetPort(conn, fl.LP_MOSI)
    print("  MOSI: {:c}{}".format(ord('A')+port, bit))
    (port, bit) = fl.progGetPort(conn, fl.LP_SS)
    print("  SS:   {:c}{}".format(ord('A')+port, bit))
    (port, bit) = fl.progGetPort(conn, fl.LP_SCK)
    print("  SCK:  {:c}{}".format(ord('A')+port, bit))
    print("}")

    fl.jtagClockFSM(conn, 0x0000005F, 9)
    fl.jtagShiftInOnly(conn, 32, BYTE_ARRAY)
    bs = fl.jtagShiftInOut(conn, 128, fl.SHIFT_ONES)
    print("jtagShiftInOut() got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bs])))

    fl.jtagClockFSM(conn, 0x0000005F, 9)
    fl.spiSend(conn, BYTE_ARRAY, fl.SPI_LSBFIRST)
    bs = fl.spiRecv(conn, 8, fl.SPI_LSBFIRST)
    print("spiRecv() got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bs])))
    fl.progClose(conn)

    print("jtagScanChain(): {")
    for idCode in fl.jtagScanChain(conn, PROG_CONFIG):
        print("  0x{:08X}".format(idCode))
    print("}")

    print("flProgram()...")
    fl.flProgram(conn, "J:{}:../../../../hdlmake/apps/makestuff/swled/cksum/vhdl/fpga.xsvf".format(PROG_CONFIG))
    print("...done.")

    fl.flSelectConduit(conn, CONDUIT)
    print("flIsFPGARunning(): {}".format(fl.flIsFPGARunning(conn)))

    fl.flWriteChannel(conn, 0, BYTE_ARRAY)

    bs = fl.flReadChannel(conn, 1, 16)
    print("flReadChannel(1, 16) got {} bytes: {{\n  {}\n}}".format(
        len(bs),
        " ".join(["{:02X}".format(b) for b in bs])))
    
    print("flReadChannel(2) got {:02X}".format(fl.flReadChannel(conn, 2)))

    fl.flReadChannelAsyncSubmit(conn, 0, 4)
    fl.flReadChannelAsyncSubmit(conn, 1, 8)
    fl.flReadChannelAsyncSubmit(conn, 2, 16)
    for i in range(3):
        bs = fl.flReadChannelAsyncAwait(conn)
        print("flReadChannelAsyncAwait() got {} bytes: {{\n  {}\n}}".format(
            len(bs),
            " ".join(["{:02X}".format(b) for b in bytearray(bs)])))

except fl.FLException as ex:
    print(ex)
finally:
    fl.flClose(conn)

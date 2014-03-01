#!/usr/bin/env python
#
# Copyright (C) 2009-2014 Chris McClelland
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
import fl;
import argparse

# File-reader which yields chunks of data
def readFile(fileName):
    with open(fileName, "rb") as f:
        while True:
            chunk = f.read(32768)
            if chunk:
                yield chunk
            else:
                break

print("FPGALink Python FLCLI Example Copyright (C) 2011-2014 Chris McClelland\n")
parser = argparse.ArgumentParser(description='Load FX2LP firmware, load the FPGA, interact with the FPGA.')
parser.add_argument('-i', action="store", nargs=1, metavar="<VID:PID>", help="vendor ID and product ID (e.g 04B4:8613)")
parser.add_argument('-v', action="store", nargs=1, required=True, metavar="<VID:PID>", help="VID, PID and opt. dev ID (e.g 1D50:602B:0001)")
parser.add_argument('-d', action="store", nargs=1, metavar="<port+>", help="read/write digital ports (e.g B13+,C1-,B2?)")
parser.add_argument('-q', action="store", nargs=1, metavar="<jtagPorts>", help="query the JTAG chain")
parser.add_argument('-p', action="store", nargs=1, metavar="<config>", help="program a device")
parser.add_argument('-c', action="store", nargs=1, metavar="<conduit>", help="which comm conduit to choose (default 0x01)")
parser.add_argument('-f', action="store", nargs=1, metavar="<dataFile>", help="binary data to write to channel 0")
argList = parser.parse_args()
handle = fl.FLHandle()
try:
    fl.flInitialise(0)

    vp = argList.v[0]
    print("Attempting to open connection to FPGALink device {}...".format(vp))
    try:
        handle = fl.flOpen(vp)
    except fl.FLException as ex:
        if ( argList.i ):
            ivp = argList.i[0]
            print("Loading firmware into {}...".format(ivp))
            fl.flLoadStandardFirmware(ivp, vp);

            print("Awaiting renumeration...")
            if ( not fl.flAwaitDevice(vp, 600) ):
                raise fl.FLException("FPGALink device did not renumerate properly as {}".format(vp))

            print("Attempting to open connection to FPGALink device {} again...".format(vp))
            handle = fl.flOpen(vp)
        else:
            raise fl.FLException("Could not open FPGALink device at {} and no initial VID:PID was supplied".format(vp))
    
    if ( argList.d ):
        print("Configuring ports...")
        rb = "{:0{}b}".format(fl.flMultiBitPortAccess(handle, argList.d[0]), 32)
        print("Readback:   28   24   20   16    12    8    4    0\n          {} {} {} {}  {} {} {} {}".format(
            rb[0:4], rb[4:8], rb[8:12], rb[12:16], rb[16:20], rb[20:24], rb[24:28], rb[28:32]))
        fl.flSleep(100)

    conduit = 1
    if ( argList.c ):
        conduit = int(argList.c[0])

    isNeroCapable = fl.flIsNeroCapable(handle)
    isCommCapable = fl.flIsCommCapable(handle, conduit)
    fl.flSelectConduit(handle, conduit)

    if ( argList.q ):
        if ( isNeroCapable ):
            chain = fl.jtagScanChain(handle, argList.q[0])
            if ( len(chain) > 0 ):
                print("The FPGALink device at {} scanned its JTAG chain, yielding:".format(vp))
                for idCode in chain:
                    print("  0x{:08X}".format(idCode))
            else:
                print("The FPGALink device at {} scanned its JTAG chain but did not find any attached devices".format(vp))
        else:
            raise fl.FLException("JTAG chain scan requested but FPGALink device at {} does not support NeroJTAG".format(vp))
    
    if ( argList.p ):
        progConfig = argList.p[0]
        print("Programming device with config {}...".format(progConfig))
        if ( isNeroCapable ):
            fl.flProgram(handle, progConfig)
        else:
            raise fl.FLException("Device program requested but device at {} does not support NeroProg".format(vp))
    
    if ( argList.f and not(isCommCapable) ):
        raise fl.FLException("Data file load requested but device at {} does not support CommFPGA".format(vp))

    if ( isCommCapable and fl.flIsFPGARunning(handle) ):
        print("Zeroing R1 & R2...")
        fl.flWriteChannel(handle, 0x01, 0x00)
        fl.flWriteChannel(handle, 0x02, 0x00)
        if ( argList.f ):
            dataFile = argList.f[0]
            print("Writing {} to FPGALink device {}...".format(dataFile, vp))
            for chunk in readFile(dataFile):
                fl.flWriteChannelAsync(handle, 0x00, chunk)
        
        print("Reading channel 0...")
        print("Got 0x{:02X}".format(fl.flReadChannel(handle, 0x00)))
        print("Reading channel 1...")
        print("Got 0x{:02X}".format(fl.flReadChannel(handle, 0x01)))
        print("Reading channel 2...")
        print("Got 0x{:02X}".format(fl.flReadChannel(handle, 0x02)))

except fl.FLException as ex:
    print(ex)
finally:
    fl.flClose(handle)

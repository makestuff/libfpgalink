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
import fpgalink;
import argparse

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
handle = fpgalink.FLHandle()
try:
    vp = argList.v[0]
    print("Attempting to open connection to FPGALink device %s..." % vp)
    try:
        handle = fpgalink.flOpen(vp)
    except fpgalink.FLException as ex:
        if ( argList.i ):
            ivp = argList.i[0]
            print("Loading firmware into %s..." % ivp)
            fpgalink.flLoadStandardFirmware(ivp, vp);

            print("Awaiting renumeration...")
            if ( not fpgalink.flAwaitDevice(vp, 600) ):
                raise fpgalink.FLException("FPGALink device did not renumerate properly as %s" % vp)

            print("Attempting to open connection to FPGALink device %s again..." % vp)
            handle = fpgalink.flOpen(vp)
        else:
            raise fpgalink.FLException("Could not open FPGALink device at %s and no initial VID:PID was supplied" % vp)
    
    if ( argList.d ):
        print("Configuring ports...")
        rb = "{:0{}b}".format(fpgalink.flMultiBitPortAccess(handle, argList.d[0]), 32)
        print("Readback:   28   24   20   16    12    8    4    0\n          %s %s %s %s  %s %s %s %s" % (rb[0:4], rb[4:8], rb[8:12], rb[12:16], rb[16:20], rb[20:24], rb[24:28], rb[28:32]))
        fpgalink.flSleep(100)

    conduit = 1
    if ( argList.c ):
        conduit = int(argList.c[0])

    isNeroCapable = fpgalink.flIsNeroCapable(handle)
    isCommCapable = fpgalink.flIsCommCapable(handle, conduit)
    fpgalink.flSelectConduit(handle, conduit)

    if ( argList.q ):
        if ( isNeroCapable ):
            chain = fpgalink.jtagScanChain(handle, argList.q[0])
            if ( len(chain) > 0 ):
                print("The FPGALink device at %s scanned its JTAG chain, yielding:" % vp)
                for i in chain:
                    print("  0x%08X" % i)
            else:
                print("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices" % vp)
        else:
            raise fpgalink.FLException("JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG" % vp)
    
    if ( argList.p ):
        progConfig = argList.p[0]
        print("Programming device with config %s..." % progConfig)
        if ( isNeroCapable ):
            fpgalink.flProgram(handle, progConfig)
        else:
            raise fpgalink.FLException("Device program requested but device at %s does not support NeroProg" % vp)
    
    if ( argList.f and not(isCommCapable) ):
        raise fpgalink.FLException("Data file load requested but device at %s does not support CommFPGA" % vp)

    if ( isCommCapable and fpgalink.flIsFPGARunning(handle) ):
        print("Zeroing R1 & R2...")
        fpgalink.flWriteChannel(handle, 0x01, 0x00)
        fpgalink.flWriteChannel(handle, 0x02, 0x00)
        if ( argList.f ):
            dataFile = argList.f[0]
            print("Writing %s to FPGALink device %s..." % (dataFile, vp))
            fpgalink.flWriteChannel(handle, 0x00, dataFile)
        
        print("Reading channel 0...")
        print("Got 0x%02X" % fpgalink.flReadChannel(handle, 0x00))
        print("Reading channel 1...")
        print("Got 0x%02X" % fpgalink.flReadChannel(handle, 0x01))
        print("Reading channel 2...")
        print("Got 0x%02X" % fpgalink.flReadChannel(handle, 0x02))

except fpgalink.FLException as ex:
    print(ex)
finally:
    fpgalink.flClose(handle)

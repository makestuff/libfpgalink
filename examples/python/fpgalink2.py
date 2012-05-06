#!/usr/bin/python
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
import array
import time
import sys
import argparse
from ctypes import *

# Define types
class FLContext(Structure):
    pass
class FLException(Exception):
    pass
FLHandle = POINTER(FLContext)
FLStatus = c_uint
FL_SUCCESS = 0
uint32 = c_uint
uint16 = c_ushort
uint8 = c_ubyte
ErrorString = c_char_p

# Get DLL
if ( sys.platform == "linux2" ):
    cdll.LoadLibrary("libfpgalink.so")
    fpgalink = CDLL("libfpgalink.so")
elif ( sys.platform == "darwin" ):
    cdll.LoadLibrary("libfpgalink.dylib")
    fpgalink = CDLL("libfpgalink.dylib")
elif ( sys.platform == "win32" ):
    windll.LoadLibrary("libfpgalink.dll")
    fpgalink = WinDLL("libfpgalink.dll")

# Miscellaneous Functions
fpgalink.flInitialise.argtypes = []
fpgalink.flInitialise.restype = None
fpgalink.flFreeError.argtypes = [c_char_p]
fpgalink.flFreeError.restype = None
fpgalink.flSleep.argtypes = [uint32]
fpgalink.flSleep.restype = None
fpgalink.flLoadFile.argtypes = [c_char_p, POINTER(uint32)]
fpgalink.flLoadFile.restype = POINTER(uint8)
fpgalink.flFreeFile.argtypes = [POINTER(uint8)]
fpgalink.flFreeFile.restype = None
fpgalink.flScanChain.argtypes = [FLHandle, POINTER(uint32), POINTER(uint32), uint32, POINTER(ErrorString)]
fpgalink.flScanChain.restype = FLStatus
fpgalink.flPortAccess.argtypes = [FLHandle, uint16, uint16, POINTER(uint16), POINTER(ErrorString)]
fpgalink.flPortAccess.restype = FLStatus

# Connection Lifecycle
fpgalink.flOpen.argtypes = [c_char_p, POINTER(FLHandle), POINTER(ErrorString)]
fpgalink.flOpen.restype = FLStatus
fpgalink.flClose.argtypes = [FLHandle]
fpgalink.flClose.restype = None

# Device Capabilities and Status
fpgalink.flIsDeviceAvailable.argtypes = [c_char_p, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flIsDeviceAvailable.restype = FLStatus
fpgalink.flIsNeroCapable.argtypes = [FLHandle]
fpgalink.flIsNeroCapable.restype = uint8
fpgalink.flIsCommCapable.argtypes = [FLHandle]
fpgalink.flIsCommCapable.restype = uint8

# CommFPGA Operations
fpgalink.flIsFPGARunning.argtypes = [FLHandle, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flIsFPGARunning.restype = FLStatus
fpgalink.flWriteChannel.argtypes = [FLHandle, uint32, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flWriteChannel.restype = FLStatus
fpgalink.flReadChannel.argtypes = [FLHandle, uint32, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flReadChannel.restype = FLStatus

# NeroJTAG Operations
fpgalink.flPlayXSVF.argtypes = [FLHandle, c_char_p, POINTER(ErrorString)]
fpgalink.flPlayXSVF.restype = FLStatus

# FX2 Firmware Operations
fpgalink.flLoadStandardFirmware.argtypes = [c_char_p, c_char_p, c_char_p, POINTER(ErrorString)]
fpgalink.flLoadStandardFirmware.restype = FLStatus
fpgalink.flFlashStandardFirmware.argtypes = [FLHandle, c_char_p, c_char_p, uint32, c_char_p, POINTER(ErrorString)]
fpgalink.flFlashStandardFirmware.restype = FLStatus
fpgalink.flSaveFirmware.argtypes = [FLHandle, uint32, c_char_p, POINTER(ErrorString)]
fpgalink.flSaveFirmware.restype = FLStatus
fpgalink.flFlashCustomFirmware.argtypes = [FLHandle, c_char_p, uint32, POINTER(ErrorString)]
fpgalink.flFlashCustomFirmware.restype = FLStatus
fpgalink.flCleanWriteBuffer.argtypes = [FLHandle]
fpgalink.flCleanWriteBuffer.restype = None
fpgalink.flAppendWriteChannelCommand.argtypes = [FLHandle, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flAppendWriteChannelCommand.restype = FLStatus

# Open a connection to the FPGALink device
def flOpen(vp):
    handle = FLHandle()
    error = ErrorString()
    status = fpgalink.flOpen(vp.encode('ascii'), byref(handle), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return handle

# Close the FPGALink connection
def flClose(handle):
    fpgalink.flClose(handle)

# Await renumeration - return true if found before timeout
def flAwaitDevice(vp, timeout):
    error = ErrorString()
    isAvailable = uint8()
    fpgalink.flSleep(1000);
    while ( True ):
        fpgalink.flSleep(100);
        status = fpgalink.flIsDeviceAvailable(vp.encode('ascii'), byref(isAvailable), byref(error))
        if ( status != FL_SUCCESS ):
            s = str(error.value)
            fpgalink.flFreeError(error)
            raise FLException(s)
        timeout = timeout - 1
        if ( isAvailable ):
            return True
        if ( timeout == 0 ):
            return False

# Query NeroJTAG capability
def flIsNeroCapable(handle):
    if ( fpgalink.flIsNeroCapable(handle) ):
        return True
    else:
        return False

# Query CommFPGA capability
def flIsCommCapable(handle):
    if ( fpgalink.flIsCommCapable(handle) ):
        return True
    else:
        return False

# Scan the JTAG chain
def flScanChain(handle):
    error = ErrorString()
    ChainType = (uint32 * 16)  # Guess there are fewer than 16 devices
    chain = ChainType()
    length = uint32(0)
    status = fpgalink.flScanChain(handle, byref(length), chain, 16, byref(error))
    if ( length > 16 ):
        # We know exactly how many devices there are, so try again
        ChainType = (uint32 * length.value)
        chain = ChainType()
        status = fpgalink.flScanChain(handle, None, chain, length, byref(error))
    return chain

# Access the I/O ports on the micro
def flPortAccess(handle, portWrite, ddr):
    error = ErrorString()
    portRead = uint16()
    status = fpgalink.flPortAccess(handle, portWrite, ddr, byref(portRead), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return portRead.value

# Return true if the FPGA is actually running
def flIsFPGARunning(handle):
    error = ErrorString()
    isRunning = uint8()
    status = fpgalink.flIsFPGARunning(handle, byref(isRunning), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    if ( isRunning ):
        return True
    else:
        return False

# Write one or more bytes to the specified channel
def flWriteChannel(handle, timeout, chan, values):
    error = ErrorString()
    if ( isinstance(values, bytearray) ):
        # Write the contents of the byte array:
        numValues = len(values)
        BufType = uint8*numValues
        buf = BufType.from_buffer(values)
        status = fpgalink.flWriteChannel(handle, timeout, chan, numValues, buf, byref(error))
    elif ( isinstance(values, int) ):
        # Write a single integer
        if ( values > 0xFF ):
            raise FLException("Supplied value won't fit in a byte!")
        status = fpgalink.flWriteChannel(handle, timeout, chan, 1, (uint8*1)(values), byref(error))
    else:
        # Write the contents of a file
        fileLen = uint32()
        fileData = fpgalink.flLoadFile(values.encode('ascii'), byref(fileLen))
        if ( fileData == None ):
            raise FLException("Cannot load file!")
        status = fpgalink.flWriteChannel(handle, timeout, chan, fileLen, fileData, byref(error))
        fpgalink.flFreeFile(fileData)
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Read one or more values from the specified channel
def flReadChannel(handle, timeout, chan, count = 1):
    error = ErrorString()
    if ( count == 1 ):
        # Read a single byte
        buf = uint8()
        status = fpgalink.flReadChannel(handle, timeout, chan, 1, byref(buf), byref(error))
        returnValue = buf.value
    else:
        # Read multiple bytes
        byteArray = bytearray(count)
        BufType = uint8*count
        buf = BufType.from_buffer(byteArray)
        status = fpgalink.flReadChannel(handle, timeout, chan, count, buf, byref(error))
        returnValue = byteArray
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return returnValue

# Play an XSVF file into the JTAG chain
def flPlayXSVF(handle, xsvfFile):
    error = ErrorString()
    status = fpgalink.flPlayXSVF(handle, xsvfFile.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Load standard firmware into the FX2 chip
def flLoadStandardFirmware(curVidPid, newVidPid, jtagPort):
    error = ErrorString()
    status = fpgalink.flLoadStandardFirmware(curVidPid.encode('ascii'), newVidPid.encode('ascii'), jtagPort.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Append channel write to init buffer
def flAppendWriteChannelCommand(handle, chan, values):
    error = ErrorString()
    if ( isinstance(values, (list, tuple, array.array)) ):
        numValues = len(values)
        status = fpgalink.flAppendWriteChannelCommand(handle, chan, numValues, (uint8*numValues)(*values), byref(error))
    elif ( isinstance(values, int) ):
        status = fpgalink.flAppendWriteChannelCommand(handle, chan, 1, (uint8*1)(values), byref(error))
    else:
        fileLen = uint32()
        fileData = fpgalink.flLoadFile(values, byref(fileLen))
        if ( fileData == None ):
            raise FLException("Cannot load file!")
        status = fpgalink.flAppendWriteChannelCommand(handle, chan, fileLen, fileData, byref(error))
        fpgalink.flFreeFile(fileData)
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Flash standard firmware into the FX2's EEPROM
def flFlashStandardFirmware(handle, newVidPid, jtagPort, eepromSize, xsvfFile = None):
    error = ErrorString()
    status = fpgalink.flFlashStandardFirmware(handle, newVidPid.encode('ascii'), jtagPort.encode('ascii'), eepromSize, xsvfFile.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Initialise the library
fpgalink.flInitialise()

# Main function if we're not loaded as a module
if __name__ == "__main__":
    print "FPGALink Python Example Copyright (C) 2011-2012 Chris McClelland\n"
    parser = argparse.ArgumentParser(description='Load FX2 firmware, load the FPGA, interact with the FPGA.')
    parser.add_argument('-p', action="store_true", default=False, help="FPGA is powered from USB (Nexys2 only!)")
    parser.add_argument('-s', action="store_true", default=False, help="scan the JTAG chain")
    parser.add_argument('-v', action="store", nargs=1, required=True, metavar="<VID:PID>", help="renumerated vendor and product ID of the FPGALink device")
    parser.add_argument('-i', action="store", nargs=1, metavar="<VID:PID>", help="initial vendor and product ID of the (FX2LP-based) FPGALink device")
    parser.add_argument('-j', action="store", nargs=1, metavar="<jtagPort>", help="JTAG port specification for the (FX2LP-based) FPGALink device")
    parser.add_argument('-x', action="store", nargs=1, metavar="<xsvfFile>", help="SVF, XSVF or CSVF file to play into the JTAG chain")
    parser.add_argument('-f', action="store", nargs=1, metavar="<dataFile>", help="binary data to write to channel 0")
    argList = parser.parse_args()
    handle = FLHandle()
    try:
        if ( argList.j and not argList.i ):
            raise FLException("You can't specify -j without -i")

        vp = argList.v[0]
        print "Attempting to open connection to FPGALink device %s..." % vp
        try:
            handle = flOpen(vp)
        except FLException, ex:
            if ( argList.i ):
                jtagPort = "D0234"
                if ( argList.j ):
                    jtagPort = argList.j[0]
                ivp = argList.i[0]
                print "Loading firmware into %s..." % ivp
                flLoadStandardFirmware(ivp, vp, jtagPort);

                print "Awaiting renumeration..."
                if ( not flAwaitDevice(vp, 600) ):
                    raise FLException("FPGALink device did not renumerate properly as %s" % vp)

                print "Attempting to open connection to FPGALink device %s again..." % vp
                handle = flOpen(vp)
            else:
                raise FLException("Could not open FPGALink device at %s and no initial VID:PID was supplied" % vp)
        
        if ( argList.p ):
            print "Connecting USB power to FPGA..."
            flPortAccess(handle, 0x0080, 0x0080);
            fpgalink.flSleep(100)

        isNeroCapable = flIsNeroCapable(handle)
        isCommCapable = flIsCommCapable(handle)
        if ( argList.s ):
            if ( isNeroCapable ):
                chain = flScanChain(handle)
                if ( len(chain) > 0 ):
                    print "The FPGALink device at %s scanned its JTAG chain, yielding:" % vp
                    for i in chain:
                        print "  0x%08X" % i
                else:
                    print "The FPGALink device at %s scanned its JTAG chain but did not find any attached devices" % vp
            else:
                raise FLException("JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG" % vp)
        
        if ( argList.x ):
            xsvfFile = argList.x[0]
            print "Playing \"%s\" into the JTAG chain on FPGALink device %s..." % (xsvfFile, vp)
            if ( isNeroCapable ):
                flPlayXSVF(handle, xsvfFile)
            else:
                raise FLException("XSVF play requested but device at %s does not support NeroJTAG" % vp)
        
        if ( argList.f and not(isCommCapable) ):
            raise FLException("Data file load requested but device at %s does not support CommFPGA" % vp)

        if ( isCommCapable ):
            print "Writing channel 1 to zero count..."
            flWriteChannel(handle, 1000, 0x01, 0x01)

            if ( argList.f ):
                dataFile = argList.f[0]
                print "Writing %s to FPGALink device %s..." % (dataFile, vp)
                flWriteChannel(handle, 32760, 0x00, dataFile)
            
            print "Reading channel 0..."
            print "Got 0x%02X" % flReadChannel(handle, 1000, 0x00)
            print "Reading channel 0..."
            print "Got 0x%02X" % flReadChannel(handle, 1000, 0x00)
            print "Reading channel 0..."
            print "Got 0x%02X" % flReadChannel(handle, 1000, 0x00)

    except FLException, ex:
        print ex
    finally:
        flClose(handle)

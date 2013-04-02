#!/usr/bin/env python
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
fpgalink.flInitialise.argtypes = [c_int, POINTER(ErrorString)]
fpgalink.flInitialise.restype = FLStatus
fpgalink.flFreeError.argtypes = [c_char_p]
fpgalink.flFreeError.restype = None
fpgalink.flSleep.argtypes = [uint32]
fpgalink.flSleep.restype = None
fpgalink.flLoadFile.argtypes = [c_char_p, POINTER(uint32)]
fpgalink.flLoadFile.restype = POINTER(uint8)
fpgalink.flFreeFile.argtypes = [POINTER(uint8)]
fpgalink.flFreeFile.restype = None
fpgalink.flPortAccess.argtypes = [FLHandle, uint8, uint8, uint8, uint8, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flPortAccess.restype = FLStatus
fpgalink.flPortConfig.argtypes = [FLHandle, c_char_p, POINTER(ErrorString)]
fpgalink.flPortConfig.restype = FLStatus

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
fpgalink.flFifoMode.argtypes = [FLHandle, uint8, POINTER(ErrorString)]
fpgalink.flFifoMode.restype = FLStatus
fpgalink.flIsFPGARunning.argtypes = [FLHandle, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flIsFPGARunning.restype = FLStatus
fpgalink.flWriteChannel.argtypes = [FLHandle, uint32, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flWriteChannel.restype = FLStatus
fpgalink.flReadChannel.argtypes = [FLHandle, uint32, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flReadChannel.restype = FLStatus
fpgalink.flCleanWriteBuffer.argtypes = [FLHandle]
fpgalink.flCleanWriteBuffer.restype = None
fpgalink.flAppendWriteChannelCommand.argtypes = [FLHandle, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flAppendWriteChannelCommand.restype = FLStatus
fpgalink.flPlayWriteBuffer.argtypes = [FLHandle, uint32, POINTER(ErrorString)]
fpgalink.flPlayWriteBuffer.restype = FLStatus

# NeroProg Operations
fpgalink.flProgram.argtypes = [FLHandle, c_char_p, c_char_p, POINTER(ErrorString)]
fpgalink.flProgram.restype = FLStatus
fpgalink.jtagScanChain.argtypes = [FLHandle, c_char_p, POINTER(uint32), POINTER(uint32), uint32, POINTER(ErrorString)]
fpgalink.jtagScanChain.restype = FLStatus
fpgalink.jtagOpen.argtypes = [FLHandle, c_char_p, uint32, POINTER(ErrorString)]
fpgalink.jtagOpen.restype = FLStatus
fpgalink.jtagClose.argtypes = [FLHandle, POINTER(ErrorString)]
fpgalink.jtagClose.restype = FLStatus
fpgalink.jtagClockFSM.argtypes = [FLHandle, uint32, uint8, POINTER(ErrorString)]
fpgalink.jtagClockFSM.restype = FLStatus
fpgalink.jtagClocks.argtypes = [FLHandle, uint32, POINTER(ErrorString)]
fpgalink.jtagClocks.restype = FLStatus

# FX2LP Firmware Operations
fpgalink.flLoadStandardFirmware.argtypes = [c_char_p, c_char_p, POINTER(ErrorString)]
fpgalink.flLoadStandardFirmware.restype = FLStatus
fpgalink.flFlashStandardFirmware.argtypes = [FLHandle, c_char_p, POINTER(ErrorString)]
fpgalink.flFlashStandardFirmware.restype = FLStatus
fpgalink.flSaveFirmware.argtypes = [FLHandle, uint32, c_char_p, POINTER(ErrorString)]
fpgalink.flSaveFirmware.restype = FLStatus
fpgalink.flLoadCustomFirmware.argtypes = [c_char_p, c_char_p, POINTER(ErrorString)]
fpgalink.flLoadCustomFirmware.restype = FLStatus
fpgalink.flFlashCustomFirmware.argtypes = [FLHandle, c_char_p, uint32, POINTER(ErrorString)]
fpgalink.flFlashCustomFirmware.restype = FLStatus

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

# Access the I/O ports on the micro
def flPortAccess(handle, portSelect, mask, ddrWrite, portWrite):
    error = ErrorString()
    portRead = uint8()
    status = fpgalink.flPortAccess(handle, portSelect, mask, ddrWrite, portWrite, byref(portRead), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return portRead.value

# Access the I/O ports on the micro
def flPortConfig(handle, portConfig):
    error = ErrorString()
    status = fpgalink.flPortConfig(handle, portConfig.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Set the FIFO mode
def flFifoMode(handle, fifoMode):
    error = ErrorString()
    status = fpgalink.flFifoMode(handle, fifoMode, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

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

# Load standard firmware into the FX2LP chip
def flLoadStandardFirmware(curVidPid, newVidPid):
    error = ErrorString()
    status = fpgalink.flLoadStandardFirmware(curVidPid.encode('ascii'), newVidPid.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Load standard firmware into the FX2LP chip
def flLoadCustomFirmware(curVidPid, fwFile):
    error = ErrorString()
    status = fpgalink.flLoadCustomFirmware(curVidPid.encode('ascii'), fwFile.encode('ascii'), byref(error))
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

# Flash standard firmware into the FX2LP's EEPROM
def flFlashStandardFirmware(handle, newVidPid):
    error = ErrorString()
    status = fpgalink.flFlashStandardFirmware(handle, newVidPid.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Initialise the library
def flInitialise(debugLevel):
    error = ErrorString()
    status = fpgalink.flInitialise(debugLevel, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Program a device using the given config string and file
def flProgram(handle, progConfig, progFile = None):
    error = ErrorString()
    if ( progFile != None ):
        progFile = progFile.encode('ascii')
    status = fpgalink.flProgram(handle, progConfig.encode('ascii'), progFile, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Scan the JTAG chain
def jtagScanChain(handle, portConfig):
    error = ErrorString()
    ChainType = (uint32 * 16)  # Guess there are fewer than 16 devices
    chain = ChainType()
    length = uint32(0)
    status = fpgalink.jtagScanChain(handle, portConfig, byref(length), chain, 16, byref(error))
    if ( length > 16 ):
        # We know exactly how many devices there are, so try again
        ChainType = (uint32 * length.value)
        chain = ChainType()
        status = fpgalink.jtagScanChain(handle, portConfig, None, chain, length, byref(error))
    return chain

# Open a JTAG port
#def jtagOpen(handle, portConfig, index, progFile):
#    error = ErrorString()
#    status = fpgalink.jtagOpen(handle, portConfig.encode('ascii'), index, byref(error))
#    if ( status != FL_SUCCESS ):
#        s = str(error.value)
#        fpgalink.flFreeError(error)
#        raise FLException(s)

flInitialise(0)

# Main function if we're not loaded as a module
if __name__ == "__main__":
    print "FPGALink Python Example Copyright (C) 2011-2012 Chris McClelland\n"
    parser = argparse.ArgumentParser(description='Load FX2LP firmware, load the FPGA, interact with the FPGA.')
    parser.add_argument('-i', action="store", nargs=1, metavar="<VID:PID>", help="vendor ID and product ID (e.g 04B4:8613)")
    parser.add_argument('-v', action="store", nargs=1, required=True, metavar="<VID:PID>", help="VID, PID and optional dev ID (e.g 1D50:602B:0001)")
    parser.add_argument('-w', action="store", nargs=1, metavar="<port[,port]*>", help="write/configure digital ports")
    parser.add_argument('-r', action="store_true", default=False, help="read digital ports")
    parser.add_argument('-q', action="store", nargs=1, metavar="<jtagPorts>", help="query the JTAG chain")
    parser.add_argument('-p', action="store", nargs=1, metavar="<config>", help="program a device")
    parser.add_argument('-f', action="store", nargs=1, metavar="<dataFile>", help="binary data to write to channel 0")
    argList = parser.parse_args()
    handle = FLHandle()
    try:
        vp = argList.v[0]
        print "Attempting to open connection to FPGALink device %s..." % vp
        try:
            handle = flOpen(vp)
        except FLException, ex:
            if ( argList.i ):
                ivp = argList.i[0]
                print "Loading firmware into %s..." % ivp
                flLoadStandardFirmware(ivp, vp);

                print "Awaiting renumeration..."
                if ( not flAwaitDevice(vp, 600) ):
                    raise FLException("FPGALink device did not renumerate properly as %s" % vp)

                print "Attempting to open connection to FPGALink device %s again..." % vp
                handle = flOpen(vp)
            else:
                raise FLException("Could not open FPGALink device at %s and no initial VID:PID was supplied" % vp)
        
        if ( argList.w ):
            print "Configuring ports..."
            flPortConfig(handle, argList.w[0]);
            fpgalink.flSleep(100)

        if ( argList.r ):
            print "State of port lines:"
            for i in range(5):
                print "  %c: %02X" % (chr(65+i), flPortAccess(handle, i, 0x00, 0x00, 0x00))

        isNeroCapable = flIsNeroCapable(handle)
        isCommCapable = flIsCommCapable(handle)
        if ( argList.q ):
            if ( isNeroCapable ):
                chain = flScanChain(handle, argList.q[0])
                if ( len(chain) > 0 ):
                    print "The FPGALink device at %s scanned its JTAG chain, yielding:" % vp
                    for i in chain:
                        print "  0x%08X" % i
                else:
                    print "The FPGALink device at %s scanned its JTAG chain but did not find any attached devices" % vp
            else:
                raise FLException("JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG" % vp)
        
        if ( argList.p ):
            progConfig = argList.p[0]
            print "Programming device with config %s..." % progConfig
            if ( isNeroCapable ):
                flProgram(handle, progConfig)
            else:
                raise FLException("Device program requested but device at %s does not support NeroProg" % vp)
        
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

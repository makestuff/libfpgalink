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

## @namespace fpgalink2
#
# The <b>FPGALink</b> library makes it easier to talk to an FPGA over USB (via a suitable micro).
#
# It performs four classes of function:
# - Load device firmware and EEPROM (specific to Cypress FX2LP).
# - Program an FPGA or CPLD using JTAG or one of the proprietary serial or parallel algorithms.
# - Read and write (over USB) up to 128 byte-wide data channels in the target FPGA.
# - Manipulate microcontroller digital I/O & SPI port(s).
#
import array
import time
import sys
import argparse
from ctypes import *

## @cond FALSE
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
size_t = c_size_t
ErrorString = c_char_p

def enum(**enums):
    return type('Enum', (), enums)

PinConfig = enum(
    HIGH  = uint8(0x01),
    LOW   = uint8(0x02),
    INPUT = uint8(0x03)
)
LogicalPort = enum(
    MISO = uint8(0x01),
    MOSI = uint8(0x02),
    SS   = uint8(0x03),
    SCK  = uint8(0x04)
)
class ReadReport(Structure):
    _fields_ = [("data", POINTER(uint8)),
                ("requestLength", uint32),
                ("actualLength", uint32)]

# Get DLL
if ( sys.platform.startswith("linux") ):
    cdll.LoadLibrary("libfpgalink.so")
    fpgalink = CDLL("libfpgalink.so")
elif ( sys.platform == "darwin" ):
    cdll.LoadLibrary("libfpgalink.dylib")
    fpgalink = CDLL("libfpgalink.dylib")
elif ( sys.platform == "win32" ):
    windll.LoadLibrary("libfpgalink.dll")
    fpgalink = WinDLL("libfpgalink.dll")
else:
    raise FLException("Unrecognised platform: " + sys.platform)

# Miscellaneous Functions
fpgalink.flInitialise.argtypes = [c_int, POINTER(ErrorString)]
fpgalink.flInitialise.restype = FLStatus
fpgalink.flFreeError.argtypes = [c_char_p]
fpgalink.flFreeError.restype = None

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
fpgalink.flIsCommCapable.restype = uint16
fpgalink.flGetFirmwareID.argtypes = [FLHandle]
fpgalink.flGetFirmwareID.restype = uint16
fpgalink.flGetFirmwareVersion.argtypes = [FLHandle]
fpgalink.flGetFirmwareVersion.restype = uint32

# CommFPGA Operations
fpgalink.flSelectConduit.argtypes = [FLHandle, uint8, POINTER(ErrorString)]
fpgalink.flSelectConduit.restype = FLStatus
fpgalink.flIsFPGARunning.argtypes = [FLHandle, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flIsFPGARunning.restype = FLStatus
fpgalink.flReadChannel.argtypes = [FLHandle, uint8, size_t, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flReadChannel.restype = FLStatus
fpgalink.flWriteChannel.argtypes = [FLHandle, uint8, size_t, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flWriteChannel.restype = FLStatus
fpgalink.flSetAsyncWriteChunkSize.argtypes = [FLHandle, uint16, POINTER(ErrorString)]
fpgalink.flSetAsyncWriteChunkSize.restype = FLStatus
fpgalink.flWriteChannelAsync.argtypes = [FLHandle, uint8, size_t, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flWriteChannelAsync.restype = FLStatus
fpgalink.flFlushAsyncWrites.argtypes = [FLHandle, POINTER(ErrorString)]
fpgalink.flFlushAsyncWrites.restype = FLStatus
fpgalink.flAwaitAsyncWrites.argtypes = [FLHandle, POINTER(ErrorString)]
fpgalink.flAwaitAsyncWrites.restype = FLStatus
fpgalink.flReadChannelAsyncSubmit.argtypes = [FLHandle, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flReadChannelAsyncSubmit.restype = FLStatus
fpgalink.flReadChannelAsyncAwait.argtypes = [FLHandle, POINTER(ReadReport), POINTER(ErrorString)]
fpgalink.flReadChannelAsyncAwait.restype = FLStatus

# NeroProg Operations
fpgalink.flProgram.argtypes = [FLHandle, c_char_p, c_char_p, POINTER(ErrorString)]
fpgalink.flProgram.restype = FLStatus
fpgalink.flProgramBlob.argtypes = [FLHandle, c_char_p, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flProgramBlob.restype = FLStatus
fpgalink.jtagScanChain.argtypes = [FLHandle, c_char_p, POINTER(uint32), POINTER(uint32), uint32, POINTER(ErrorString)]
fpgalink.jtagScanChain.restype = FLStatus
fpgalink.progOpen.argtypes = [FLHandle, c_char_p, POINTER(ErrorString)]
fpgalink.progOpen.restype = FLStatus
fpgalink.progClose.argtypes = [FLHandle, POINTER(ErrorString)]
fpgalink.progClose.restype = FLStatus
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

# Utility functions
fpgalink.flSleep.argtypes = [uint32]
fpgalink.flSleep.restype = None
fpgalink.flLoadFile.argtypes = [c_char_p, POINTER(size_t)]
fpgalink.flLoadFile.restype = POINTER(uint8)
fpgalink.flFreeFile.argtypes = [POINTER(uint8)]
fpgalink.flFreeFile.restype = None
fpgalink.flSingleBitPortAccess.argtypes = [FLHandle, uint8, uint8, uint8, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flSingleBitPortAccess.restype = FLStatus
fpgalink.flMultiBitPortAccess.argtypes = [FLHandle, c_char_p, POINTER(uint32), POINTER(ErrorString)]
fpgalink.flMultiBitPortAccess.restype = FLStatus
## @endcond

##
# @name Miscellaneous Functions
# @{

##
# @brief Initialise the library with the given log level.
#
# This may fail if LibUSB cannot talk to the USB host controllers through its kernel driver
# (e.g a Linux kernel with USB support disabled, or a machine lacking a USB host controller).
#
# @param debugLevel 0->none, 1, 2, 3->lots.
# @throw FLException if there were problems initialising LibUSB.
# 
def flInitialise(debugLevel):
    error = ErrorString()
    status = fpgalink.flInitialise(debugLevel, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
# @}

##
# @name Connection Lifecycle
# @{

##
# @brief Open a connection to the FPGALink device at the specified VID & PID.
#
# Connects to the device and verifies it's an FPGALink device, then queries its capabilities.
#
# @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
#            an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
#            selects the first device with matching VID:PID.
# @returns An opaque reference to an internal structure representing the connection. This must be
#            freed at some later time by a call to \c flClose(), or a resource-leak will ensue.
# @throw FLException
#     - If there was a memory allocation failure.
#     - If the VID:PID is invalid or the device cannot be found or opened.
#     - If the device is not an FPGALink device.
#
def flOpen(vp):
    handle = FLHandle()
    error = ErrorString()
    status = fpgalink.flOpen(vp.encode('ascii'), byref(handle), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return handle

##
# @brief Close the connection to the FPGALink device.
#
# @param handle The handle returned by \c flOpen().
#
def flClose(handle):
    fpgalink.flClose(handle)

# @}

##
# @name Device Capabilities and Status
# @{

##
# @brief Await renumeration - return true if found before timeout.
#
# This function will wait for the specified VID:PID to be added to the system (either due to a
# renumerating device, or due to a new physical connection). It will wait for a fixed period of
# 1s and then start polling the USB bus looking for the specified device. If no such device is
# detected within \c timeout deciseconds after the initial delay, it returns \c False, else it
# returns \c True.
#
# @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
#            an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
#            awaits the first device with matching VID:PID.
# @param timeout The number of tenths-of-a-second to wait, after the initial 1s delay.
# @throw FLException if the VID:PID is invalid or if no USB buses were found (did you remember to
#            call \c flInitialise()?).
#
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

##
# @brief Check to see if the device supports NeroProg.
#
# NeroProg is the collective name for all the various programming algorithms supported by
# FPGALink, including but not limited to JTAG. An affirmative response means you are free to
# call \c flProgram(), \c jtagScanChain(), \c progOpen(), \c progClose(), \c jtagShift(),
# \c jtagClockFSM() and \c jtagClocks().
#
# @param handle The handle returned by \c flOpen().
# @returns \c True if the device supports NeroProg, else \c False.
#
def flIsNeroCapable(handle):
    if ( fpgalink.flIsNeroCapable(handle) ):
        return True
    else:
        return False

##
# @brief Check to see if the device supports CommFPGA.
#
# CommFPGA is a set of channel read/write protocols. The micro may implement several
# different CommFPGA protocols, distinguished by the chosen conduit. A micro will typically
# implement its first CommFPGA protocol on conduit 1, and additional protocols on conduits
# 2, 3 etc. Conduit 0 is reserved for communication over JTAG using a virtual TAP
# state machine implemented in the FPGA, and is not implemented yet.
#
# This function returns \c True if the micro supports CommFPGA on the chosen conduit, else
# \c False.
#
# Note that this function can only know the capabilities of the micro itself; it cannot determine
# whether the FPGA contains suitable logic to implement the protocol, or even whether there is
# an FPGA physically wired to the micro in the first place.
#
# An affirmative response means you are free to call \c flReadChannel(),
# \c flReadChannelAsyncSubmit(), \c flReadChannelAsyncAwait(), \c flWriteChannel(),
# \c flWriteChannelAsync() and \c flIsFPGARunning().
#
# @param handle The handle returned by \c flOpen().
# @param conduit The conduit you're interested in (this will typically be 1).
# @returns \c True if the device supports CommFPGA, else \c False.
#
def flIsCommCapable(handle, conduit):
    if ( fpgalink.flIsCommCapable(handle, conduit) ):
        return True
    else:
        return False

##
# @brief Get the firmware ID.
#
# Each firmware (or fork of an existing firmware) has its own 16-bit ID, which this function
# retrieves.
#
# @param handle The handle returned by \c flOpen().
# @returns A 16-bit unsigned integer giving the firmware ID.
#
def flGetFirmwareID(handle):
    return fpgalink.flGetFirmwareID(handle)

##
# @brief Get the firmware version.
#
# Each firmware knows the GitHub tag from which is was built, or if it was built from a trunk,
# it knows the date on which it was built. This function returns a 32-bit integer giving that
# information. If printed as a hex number, it gives an eight-digit ISO date.
#
# @param handle The handle returned by \c flOpen().
# @returns A 32-bit unsigned integer giving the firmware version.
#
def flGetFirmwareVersion(handle):
    return fpgalink.flGetFirmwareVersion(handle)
# @}

##
# @name CommFPGA Operations
# @{

##
# @brief Select a different conduit.
#
# Select a different conduit for CommFPGA communication. Typically a micro will implement its
# first CommFPGA protocol on conduit 1. It may or may not also implement others on conduit 2, 3,
# 4 etc. It may also implement comms-over-JTAG using a virtual TAP FSM on the FPGA. You can use
# \c flIsCommCapable() to determine whether the micro supports CommFPGA on a given conduit.
#
# If mixing NeroProg operations with CommFPGA operations, it *may* be necessary to switch
# conduits. For example, if your PCB is wired to use some of the CommFPGA signals during
# programming, you will have to switch back and forth. But if the pins used for CommFPGA are
# independent of the pins used for NeroProg, you need only select the correct conduit on startup
# and then leave it alone.
#
# @param handle The handle returned by \c flOpen().
# @param conduit The conduit to select (current range 0-15).
# @throw FLException if the device doesn't respond, or the conduit is out of range.
#
def flSelectConduit(handle, conduit):
    error = ErrorString()
    status = fpgalink.flSelectConduit(handle, conduit, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Check to see if the FPGA is running.
#
# This may only be called if \c flIsCommCapable() returns \c True. It merely verifies that
# the FPGA is asserting that it's ready to read commands on the chosen conduit. Some conduits
# may not have the capability to determine this, and will therefore just optimistically report
# \c True. Before calling \c flIsFPGARunning(), you should verify that the FPGALink device
# actually supports CommFPGA using \c flIsCommCapable(), and select the conduit you wish to
# use with \c flSelectConduit().
#
# @param handle The handle returned by \c flOpen().
# @returns \c True if the FPGA is ready to accept commands, else \c False.
# @throw FLException if the device does not support CommFPGA.
#
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

##
# @brief Synchronously read one or more bytes from the specified channel.
#
# Read \c length bytes from the FPGA channel \c chan. Before calling \c flReadChannel(), you
# should verify that the FPGALink device actually supports CommFPGA using \c flIsCommCapable().
#
# Because this function is synchronous, it will block until the data has been returned.
#
# @param handle The handle returned by \c flOpen().
# @param chan The FPGA channel to read.
# @param length The number of bytes to read.
# @returns Either a single integer (0-255) or a \c bytearray.
# @throw FLException 
#     - If a USB read or write error occurred.
#     - If the device does not support CommFPGA.
#     - If there are async reads in progress.
#
def flReadChannel(handle, chan, length = 1):
    error = ErrorString()
    if ( length == 1 ):
        # Read a single byte
        buf = uint8()
        status = fpgalink.flReadChannel(handle, chan, 1, byref(buf), byref(error))
        returnValue = buf.value
    else:
        # Read multiple bytes
        byteArray = bytearray(length)
        BufType = uint8*length
        buf = BufType.from_buffer(byteArray)
        status = fpgalink.flReadChannel(handle, chan, length, buf, byref(error))
        returnValue = byteArray
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return returnValue

##
# @brief Synchronously write one or more bytes to the specified channel.
#
# Write one or more bytes to the specified channel, synchronously
def flWriteChannel(handle, chan, values):
    error = ErrorString()
    if ( isinstance(values, bytearray) ):
        # Write the contents of the byte array:
        numValues = len(values)
        BufType = uint8*numValues
        buf = BufType.from_buffer(values)
        status = fpgalink.flWriteChannel(handle, chan, numValues, buf, byref(error))
    elif ( isinstance(values, int) ):
        # Write a single integer
        if ( values > 0xFF ):
            raise FLException("Supplied value won't fit in a byte!")
        status = fpgalink.flWriteChannel(handle, chan, 1, (uint8*1)(values), byref(error))
    else:
        # Write the contents of a file
        fileLen = size_t()
        fileData = fpgalink.flLoadFile(values.encode('ascii'), byref(fileLen))
        if ( fileData == None ):
            raise FLException("Cannot load file!")
        status = fpgalink.flWriteChannel(handle, chan, fileLen, fileData, byref(error))
        fpgalink.flFreeFile(fileData)
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Set the chunk size to be used for future async writes.
#
def flSetAsyncWriteChunkSize(handle, chunkSize):
    error = ErrorString()
    status = fpgalink.flSetAsyncWriteChunkSize(handle, chunkSize, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Asynchronously write one or more bytes to the specified channel.
#
def flWriteChannelAsync(handle, chan, values):
    error = ErrorString()
    if ( isinstance(values, bytearray) ):
        # Write the contents of the byte array:
        numValues = len(values)
        BufType = uint8*numValues
        buf = BufType.from_buffer(values)
        status = fpgalink.flWriteChannelAsync(handle, chan, numValues, buf, byref(error))
    elif ( isinstance(values, int) ):
        # Write a single integer
        if ( values > 0xFF ):
            raise FLException("Supplied value won't fit in a byte!")
        status = fpgalink.flWriteChannelAsync(handle, chan, 1, (uint8*1)(values), byref(error))
    else:
        # Write the contents of a file
        fileLen = uint32()
        fileData = fpgalink.flLoadFile(values.encode('ascii'), byref(fileLen))
        if ( fileData == None ):
            raise FLException("Cannot load file!")
        status = fpgalink.flWriteChannelAsync(handle, chan, fileLen, fileData, byref(error))
        fpgalink.flFreeFile(fileData)
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Flush out any pending asynchronous writes.
#
def flFlushAsyncWrites(handle):
    error = ErrorString()
    status = fpgalink.flFlushAsyncWrites(handle, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Wait for confirmation that pending asynchronous writes were received by the micro.
#
def flAwaitAsyncWrites(handle):
    error = ErrorString()
    status = fpgalink.flAwaitAsyncWrites(handle, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Submit an asynchronous read of one or more bytes from the specified channel.
#
def flReadChannelAsyncSubmit(handle, chan, length = 1):
    error = ErrorString()
    status = fpgalink.flReadChannelAsyncSubmit(handle, chan, length, None, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Await the data from a previously-submitted asynchronous read.
#
# The ReadReport struct gives access to the raw memory owned by the FPGALink DLL. You can construct
# a string from it with something like ctypes.string_at(readReport.data, readReport.actualLength).
#
def flReadChannelAsyncAwait(handle):
    readReport = ReadReport(None, 0, 0)
    error = ErrorString()
    status = fpgalink.flReadChannelAsyncAwait(handle, byref(readReport), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return readReport
# @}

##
# @name NeroProg Operations
# @{

##
# @brief Program a device using the given config string and either a file-name or a bytearray.
#
def flProgram(handle, progConfig, progFile = None):
    error = ErrorString()
    if ( isinstance(progFile, bytearray) ):
        length = len(progFile)
        BufType = uint8*length
        buf = BufType.from_buffer(progFile)
        status = fpgalink.flProgramBlob(handle, progConfig.encode('ascii'), length, buf, byref(error))
    else:
        if ( progFile != None ):
            progFile = progFile.encode('ascii')
        status = fpgalink.flProgram(handle, progConfig.encode('ascii'), progFile, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Scan the JTAG chain and return an array of IDCODEs.
#
def jtagScanChain(handle, portConfig):
    error = ErrorString()
    ChainType = (uint32 * 16)  # Guess there are fewer than 16 devices
    chain = ChainType()
    length = uint32(0)
    status = fpgalink.jtagScanChain(handle, portConfig, byref(length), chain, 16, byref(error))
    if ( length.value > 16 ):
        # We know exactly how many devices there are, so try again
        ChainType = (uint32 * length.value)
        chain = ChainType()
        status = fpgalink.jtagScanChain(handle, portConfig, None, chain, length.value, byref(error))
    result = []
    for id in chain[:length.value]:
        result.append(id)
    return result

##
# @brief Open an SPI/JTAG connection.
#
def progOpen(handle, portConfig):
    error = ErrorString()
    status = fpgalink.progOpen(handle, portConfig.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Close an SPI/JTAG connection.
#
def progClose(handle):
    error = ErrorString()
    status = fpgalink.progClose(handle, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Clock \c transitionCount bits from \c bitPattern into TMS, starting with the LSB.
#
def jtagClockFSM(handle, bitPattern, transitionCount):
    error = ErrorString()
    status = fpgalink.jtagClockFSM(handle, bitPattern, transitionCount, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Toggle TCK \c numClocks times.
#
def jtagClocks(handle, numClocks):
    error = ErrorString()
    status = fpgalink.jtagClocks(handle, numClocks, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
# @}

##
# @name Firmware Operations
# @{

##
# @brief Load standard \b FPGALink firmware into the FX2's RAM.
#
# Load the FX2 chip at the "current" VID/PID with a precompiled firmware such that it will
# renumerate as the "new" VID/PID. The firmware is loaded into RAM, so the change is not
# permanent. Typically after calling \c flLoadStandardFirmware() applications should wait for
# the renumeration to complete by calling \c flIsDeviceAvailable() repeatedly until the "new"
# VID/PID becomes active.
#
# In addition to the "new" VID/PID, you can also customise the port pins used for JTAG
# operations. For this you must specify an FX2 port (C or D) and the bits within that port to
# be used for TDO, TDI, TMS and TCK respectively. For example, the port specification "D0234"
# means PD0=TDO, PD2=TDI, PD3=TMS and PD4=TCK, and is appropriate for Digilent boards (Nexys2,
# Nexys3, Atlys etc).
#
# @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
# @param newVidPid The Vendor/Product (i.e VVVV:PPPP) that you \b want the FX2 device to be.
# @throw FLException
#     - If one of the VID/PIDs was invalid or the current VID/PID was not found.
#     - If there was a problem talking to the FX2.
#     - If there was a memory allocation failure.
#
def flLoadStandardFirmware(curVidPid, newVidPid):
    error = ErrorString()
    status = fpgalink.flLoadStandardFirmware(curVidPid.encode('ascii'), newVidPid.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Flash standard FPGALink firmware into the FX2's EEPROM.
#
def flFlashStandardFirmware(handle, newVidPid):
    error = ErrorString()
    status = fpgalink.flFlashStandardFirmware(handle, newVidPid.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

##
# @brief Load custom firmware (<code>.hex</code>) into the FX2's RAM.
#
def flLoadCustomFirmware(curVidPid, fwFile):
    error = ErrorString()
    status = fpgalink.flLoadCustomFirmware(curVidPid.encode('ascii'), fwFile.encode('ascii'), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
# @}

##
# @name Utility Functions
# @{

##
# @brief Configure a single port bit on the microcontroller.
#
def flSingleBitPortAccess(handle, portNumber, bitNumber, pinConfig):
    error = ErrorString()
    bitRead = uint8()
    status = fpgalink.flSingleBitPortAccess(handle, portNumber, bitNumber, pinConfig, byref(bitRead), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return bitRead.value

##
# @brief Configure multiple port bits on the microcontroller.
#
def flMultiBitPortAccess(handle, portConfig):
    error = ErrorString()
    readState = uint32()
    status = fpgalink.flMultiBitPortAccess(handle, portConfig.encode('ascii'), byref(readState), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return readState.value
# @}

## @cond FALSE
flInitialise(0)

# Main function if we're not loaded as a module
if __name__ == "__main__":
    print "FPGALink Python Example Copyright (C) 2011-2014 Chris McClelland\n"
    parser = argparse.ArgumentParser(description='Load FX2LP firmware, load the FPGA, interact with the FPGA.')
    parser.add_argument('-i', action="store", nargs=1, metavar="<VID:PID>", help="vendor ID and product ID (e.g 04B4:8613)")
    parser.add_argument('-v', action="store", nargs=1, required=True, metavar="<VID:PID>", help="VID, PID and opt. dev ID (e.g 1D50:602B:0001)")
    parser.add_argument('-d', action="store", nargs=1, metavar="<port+>", help="read/write digital ports (e.g B13+,C1-,B2?)")
    parser.add_argument('-q', action="store", nargs=1, metavar="<jtagPorts>", help="query the JTAG chain")
    parser.add_argument('-p', action="store", nargs=1, metavar="<config>", help="program a device")
    parser.add_argument('-c', action="store", nargs=1, metavar="<conduit>", help="which comm conduit to choose (default 0x01)")
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
        
        if ( argList.d ):
            print "Configuring ports..."
            rb = "{:0{}b}".format(flMultiBitPortAccess(handle, argList.d[0]), 32)
            print "Readback:   28   24   20   16    12    8    4    0\n          %s %s %s %s  %s %s %s %s" % (rb[0:4], rb[4:8], rb[8:12], rb[12:16], rb[16:20], rb[20:24], rb[24:28], rb[28:32])
            fpgalink.flSleep(100)

        conduit = 1
        if ( argList.c ):
            conduit = int(argList.c[0])

        isNeroCapable = flIsNeroCapable(handle)
        isCommCapable = flIsCommCapable(handle, conduit)
        flSelectConduit(handle, conduit)

        if ( argList.q ):
            if ( isNeroCapable ):
                chain = jtagScanChain(handle, argList.q[0])
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
            print "Zeroing R1 & R2..."
            flWriteChannel(handle, 0x01, 0x00)
            flWriteChannel(handle, 0x02, 0x00)
            if ( argList.f ):
                dataFile = argList.f[0]
                print "Writing %s to FPGALink device %s..." % (dataFile, vp)
                flWriteChannel(handle, 0x00, dataFile)
            
            print "Reading channel 0..."
            print "Got 0x%02X" % flReadChannel(handle, 0x00)
            print "Reading channel 1..."
            print "Got 0x%02X" % flReadChannel(handle, 0x01)
            print "Reading channel 2..."
            print "Got 0x%02X" % flReadChannel(handle, 0x02)

    except FLException, ex:
        print ex
    finally:
        flClose(handle)
## @endcond

#!/usr/bin/python

import array
import time
import sys
from ctypes import *

# Defin types
class FLContext(Structure):
    pass
class FLException(Exception):
    pass
FLHandle = POINTER(FLContext)
FLStatus = c_ulong
FL_SUCCESS = 0
uint32 = c_ulong
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

# Connection Lifecycle
fpgalink.flOpen.argtypes = [uint16, uint16, POINTER(FLHandle), POINTER(ErrorString)]
fpgalink.flOpen.restype = FLStatus
fpgalink.flClose.argtypes = [FLHandle]
fpgalink.flClose.restype = None

# Device Capabilities and Status
fpgalink.flIsDeviceAvailable.argtypes = [uint16, uint16, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flIsDeviceAvailable.restype = FLStatus
fpgalink.flIsNeroCapable.argtypes = [FLHandle]
fpgalink.flIsNeroCapable.restype = uint8
fpgalink.flIsCommCapable.argtypes = [FLHandle]
fpgalink.flIsCommCapable.restype = uint8

# CommFPGA Operations
fpgalink.flIsFPGARunning.argtypes = [FLHandle, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flIsFPGARunning.restype = FLStatus
fpgalink.flWriteRegister.argtypes = [FLHandle, uint32, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flWriteRegister.restype = FLStatus
fpgalink.flReadRegister.argtypes = [FLHandle, uint32, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flReadRegister.restype = FLStatus

# NeroJTAG Operations
fpgalink.flPlayXSVF.argtypes = [FLHandle, c_char_p, POINTER(ErrorString)]
fpgalink.flPlayXSVF.restype = FLStatus

# FX2 Firmware Operations
fpgalink.flLoadStandardFirmware.argtypes = [uint16, uint16, uint16, uint16, POINTER(ErrorString)]
fpgalink.flLoadStandardFirmware.restype = FLStatus
fpgalink.flFlashStandardFirmware.argtypes = [FLHandle, uint16, uint16, uint32, c_char_p, POINTER(ErrorString)]
fpgalink.flFlashStandardFirmware.restype = FLStatus
fpgalink.flSaveFirmware.argtypes = [FLHandle, uint32, c_char_p, POINTER(ErrorString)]
fpgalink.flSaveFirmware.restype = FLStatus
fpgalink.flFlashCustomFirmware.argtypes = [FLHandle, c_char_p, uint32, POINTER(ErrorString)]
fpgalink.flFlashCustomFirmware.restype = FLStatus
fpgalink.flCleanWriteBuffer.argtypes = [FLHandle]
fpgalink.flCleanWriteBuffer.restype = None
fpgalink.flAppendWriteRegisterCommand.argtypes = [FLHandle, uint8, uint32, POINTER(uint8), POINTER(ErrorString)]
fpgalink.flAppendWriteRegisterCommand.restype = FLStatus

# Open a connection to the FPGALink device
def flOpen(vid, pid):
    handle = FLHandle()
    error = ErrorString()
    status = fpgalink.flOpen(vid, pid, byref(handle), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return handle

# Close the FPGALink connection
def flClose(handle):
    fpgalink.flClose(handle)

# Await renumeration - return true if found before timeout
def flAwaitDevice(vid, pid, timeout):
    error = ErrorString()
    isAvailable = uint8()
    while ( True ):
        fpgalink.flSleep(100);
        status = fpgalink.flIsDeviceAvailable(vid, pid, byref(isAvailable), byref(error))
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

# Write one or more bytes to the specified register
def flWriteRegister(handle, timeout, reg, values):
    error = ErrorString()
    if ( isinstance(values, (list, tuple, array.array)) ):
        numValues = len(values)
        status = fpgalink.flWriteRegister(handle, timeout, reg, numValues, (uint8*numValues)(*values), byref(error))
    elif ( isinstance(values, int) ):
        status = fpgalink.flWriteRegister(handle, timeout, reg, 1, (uint8*1)(values), byref(error))
    else:
        fileLen = uint32()
        fileData = fpgalink.flLoadFile(values, byref(fileLen))
        if ( fileData == None ):
            raise FLException("Cannot load file!")
        status = fpgalink.flWriteRegister(handle, timeout, reg, fileLen, fileData, byref(error))
        fpgalink.flFreeFile(fileData)
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Read one or more values from the specified register
def flReadRegister(handle, timeout, reg, count = 1):
    error = ErrorString()
    value = uint8()
    if ( count != 1 ):
        raise FLException('count > 1 not implemented in Python yet!')
    status = fpgalink.flReadRegister(handle, timeout, reg, 1, byref(value), byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)
    return value.value

# Play an XSVF file into the JTAG chain
def flPlayXSVF(handle, xsvfFile):
    error = ErrorString()
    status = fpgalink.flPlayXSVF(handle, xsvfFile, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Load standard firmware into the FX2 chip
def flLoadStandardFirmware(currentVid, currentPid, newVid, newPid):
    error = ErrorString()
    status = fpgalink.flLoadStandardFirmware(currentVid, currentPid, newVid, newPid, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Append register write to init buffer
def flAppendWriteRegisterCommand(handle, reg, values):
    error = ErrorString()
    if ( isinstance(values, (list, tuple, array.array)) ):
        numValues = len(values)
        status = fpgalink.flAppendWriteRegisterCommand(handle, reg, numValues, (uint8*numValues)(*values), byref(error))
    elif ( isinstance(values, int) ):
        status = fpgalink.flAppendWriteRegisterCommand(handle, reg, 1, (uint8*1)(values), byref(error))
    else:
        fileLen = uint32()
        fileData = fpgalink.flLoadFile(values, byref(fileLen))
        if ( fileData == None ):
            raise FLException("Cannot load file!")
        status = fpgalink.flAppendWriteRegisterCommand(handle, reg, fileLen, fileData, byref(error))
        fpgalink.flFreeFile(fileData)
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Flash standard firmware into the FX2's EEPROM
def flFlashStandardFirmware(handle, newVid, newPid, eepromSize, xsvfFile = None):
    error = ErrorString()
    status = fpgalink.flFlashStandardFirmware(handle, newVid, newPid, eepromSize, xsvfFile, byref(error))
    if ( status != FL_SUCCESS ):
        s = str(error.value)
        fpgalink.flFreeError(error)
        raise FLException(s)

# Initialise the library
fpgalink.flInitialise()

# Main function if we're not loaded as a module
if __name__ == "__main__":
    if ( len(sys.argv) == 5 or len(sys.argv) == 4 ):
        handle = FLHandle()
        try:
            xsvfFile = sys.argv[1]
            dataFile = sys.argv[2]
            VID = int(sys.argv[3][:4], 16)
            PID = int(sys.argv[3][5:], 16)
            if ( len(sys.argv) == 5 ):
                iVID = int(sys.argv[4][:4], 16)
                iPID = int(sys.argv[4][5:], 16)
            else:
                iVID = VID
                iPID = PID
            print "Attempting to open FPGALink connection on %04X:%04X" % (VID, PID)
            try:
                handle = flOpen(VID, PID)
            except FLException as ex:
                print "Loading firmware..."
                flLoadStandardFirmware(iVID, iPID, VID, PID);
                print "Awaiting renumeration..."
                if ( not flAwaitDevice(VID, PID, 600) ):
                    raise FLException("FPGALink device did not renumerate properly")
                print "Attempting to open FPGALink connection again..."
                handle = flOpen(VID, PID)
                
            isNeroCapable = flIsNeroCapable(handle)
            if ( isNeroCapable ):
                print "Device supports NeroJTAG"
            else:
                print "Device does not support NeroJTAG"
            
            isCommCapable = flIsCommCapable(handle)
            if ( isCommCapable ):
                print "Device supports CommFPGA"
                if ( isNeroCapable ):
                    if ( not flIsFPGARunning(handle) ):
                        print "FPGA is not running - programming it now..."
                        flPlayXSVF(handle, xsvfFile)
                    else:
                        print "FPGA is already running..."
                        
                print "Writing register..."
                flWriteRegister(handle, 1000, 0x01, 0x01)
                #flWriteRegister(handle, 1000, 0x00, (0x10, 0x20))
                flWriteRegister(handle, 32760, 0x00, dataFile)
                
                print "Reading register..."
                print "peek(0x00) == 0x%02X" % flReadRegister(handle, 1000, 0x00)
            else:
                print "Device does not support CommFPGA"
                if ( isNeroCapable ):
                    print "Programming FPGA..."
                    flPlayXSVF(handle, xsvfFile)
        except FLException as ex:
            print ex
        finally:
            flClose(handle)
    else:
            print "Synopsis: fpgalink.py <xsvfFile> <dataFile> <VID:PID> [<iVID:iPID>]"

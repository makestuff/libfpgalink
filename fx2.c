/*
 * Copyright (C) 2009-2012 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <string.h>
#include <makestuff.h>
#include <libfx2loader.h>
#include <liberror.h>
#include <libusbwrap.h>
#include "vendorCommands.h"
#include "libfpgalink.h"
#include "private.h"
#include "firmware.h"

// Load the standard FPGALink firmware into the FX2 at currentVid/currentPid.
DLLEXPORT(FLStatus) flLoadStandardFirmware(
	const char *curVidPid, const char *newVidPid, const char **error)
{
	FLStatus flStatus, retVal = FL_SUCCESS;
	struct Buffer ramBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	struct USBDevice *device = NULL;
	USBStatus uStatus;
	uint16 newVid, newPid, newDid;
	CHECK_STATUS(
		!usbValidateVidPid(newVidPid), FL_USB_ERR, cleanup,
		"flLoadStandardFirmware(): The supplied VID:PID:DID \"%s\" is invalid; it should look like 1D50:602B or 1D50:602B:0001",
		newVidPid);
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);
	newDid = (uint16)((strlen(newVidPid) == 14) ? strtoul(newVidPid+10, NULL, 16) : 0x0000);
	uStatus = usbOpenDevice(curVidPid, 1, 0, 0, &device, error);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flLoadStandardFirmware()");
	bStatus = bufInitialise(&ramBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadStandardFirmware()");
	flStatus = copyFirmwareAndRewriteIDs(
		&ramFirmware, newVid, newPid, newDid,
		&ramBuf, error);
	CHECK_STATUS(flStatus, flStatus, cleanup, "flLoadStandardFirmware()");
	fxStatus = fx2WriteRAM(device, ramBuf.data, (uint32)ramBuf.length, error);
	CHECK_STATUS(fxStatus, FL_FX2_ERR, cleanup, "flLoadStandardFirmware()");
cleanup:
	bufDestroy(&ramBuf);
	if ( device ) {
		usbCloseDevice(device, 0);
	}
	return retVal;
}

// Write the standard firmware into the FX2's external EEPROM
DLLEXPORT(FLStatus) flFlashStandardFirmware(
	struct FLContext *handle, const char *newVidPid, const char **error)
{
	FLStatus flStatus, retVal = FL_SUCCESS;
	struct Buffer i2cBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	uint16 newVid, newPid, newDid;
	CHECK_STATUS(
		!usbValidateVidPid(newVidPid), FL_USB_ERR, cleanup,
		"flFlashStandardFirmware(): The supplied new VID:PID \"%s\" is invalid; it should look like 1D50:602B or 1D50:602B:0001",
		newVidPid);
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);
	newDid = (uint16)((strlen(newVidPid) == 14) ? strtoul(newVidPid+10, NULL, 16) : 0x0000);
	bStatus = bufInitialise(&i2cBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flFlashStandardFirmware()");
	flStatus = copyFirmwareAndRewriteIDs(
		&eepromNoBootFirmware, newVid, newPid, newDid,
		&i2cBuf, error);
	CHECK_STATUS(flStatus, flStatus, cleanup, "flFlashStandardFirmware()");

	fxStatus = fx2WriteEEPROM(handle->device, i2cBuf.data, (uint32)i2cBuf.length, error);
	CHECK_STATUS(fxStatus, FL_FX2_ERR, cleanup, "flFlashStandardFirmware()");
cleanup:
	bufDestroy(&i2cBuf);
	return retVal;
}

// Load custom firmware (.hex) into the FX2's RAM
DLLEXPORT(FLStatus) flLoadCustomFirmware(
	const char *curVidPid, const char *fwFile, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	struct Buffer fwBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	struct USBDevice *device = NULL;
	USBStatus uStatus;
	const char *const ext = fwFile + strlen(fwFile) - 4;
	const bool isHex = (strcmp(".hex", ext) == 0) || (strcmp(".ihx", ext) == 0);
	CHECK_STATUS(
		!isHex, FL_FILE_ERR, cleanup,
		"flLoadCustomFirmware(): Filename should have .hex or .ihx extension");
	uStatus = usbOpenDevice(curVidPid, 1, 0, 0, &device, error);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flLoadCustomFirmware()");
	bStatus = bufInitialise(&fwBuf, 8192, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadCustomFirmware()");
	bStatus = bufReadFromIntelHexFile(&fwBuf, NULL, fwFile, error);
	CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flLoadCustomFirmware()");
	fxStatus = fx2WriteRAM(device, fwBuf.data, (uint32)fwBuf.length, error);
	CHECK_STATUS(fxStatus, FL_FX2_ERR, cleanup, "flLoadCustomFirmware()");
cleanup:
	bufDestroy(&fwBuf);
	if ( device ) {
		usbCloseDevice(device, 0);
	}
	return retVal;
}

// Flash custom firmware (.hex or .iic) into the FX2's EEPROM
DLLEXPORT(FLStatus) flFlashCustomFirmware(
	struct FLContext *handle, const char *fwFile, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	struct Buffer fwData = {0,};
	struct Buffer fwMask = {0,};
	struct Buffer iicBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	I2CStatus iStatus;
	const char *const ext = fwFile + strlen(fwFile) - 4;
	const bool isHex = (strcmp(".hex", ext) == 0) || (strcmp(".ihx", ext) == 0);
	const bool isI2C = (strcmp(".iic", ext) == 0);
	CHECK_STATUS(
		!isHex && !isI2C, FL_FX2_ERR, cleanup,
		"flFlashCustomFirmware(): Filename should have .hex, .ihx or .iic extension");
	bStatus = bufInitialise(&iicBuf, 8192, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flFlashCustomFirmware()");
	if ( isHex ) {
		// Load the .hex file, populate iicBuf:
		bStatus = bufInitialise(&fwData, 8192, 0x00, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flFlashCustomFirmware()");
		bStatus = bufInitialise(&fwMask, 8192, 0x00, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flFlashCustomFirmware()");
		bStatus = bufReadFromIntelHexFile(&fwData, &fwMask, fwFile, error);
		CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flFlashCustomFirmware()");
		i2cInitialise(&iicBuf, 0x0000, 0x0000, 0x0000, CONFIG_BYTE_400KHZ);
		iStatus = i2cWritePromRecords(&iicBuf, &fwData, &fwMask, error);
		CHECK_STATUS(iStatus, FL_FX2_ERR, cleanup, "flFlashCustomFirmware()");
		iStatus = i2cFinalise(&iicBuf, error);
		CHECK_STATUS(iStatus, FL_FX2_ERR, cleanup);
	} else if ( isI2C ) {
		// Load the .iic file into the iicBuf:
		bStatus = bufAppendFromBinaryFile(&iicBuf, fwFile, error);
		CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flFlashCustomFirmware()");
	}
	fxStatus = fx2WriteEEPROM(handle->device, iicBuf.data, (uint32)iicBuf.length, error);
	CHECK_STATUS(fxStatus, FL_FX2_ERR, cleanup, "flFlashCustomFirmware()");
cleanup:
	bufDestroy(&iicBuf);
	bufDestroy(&fwMask);
	bufDestroy(&fwData);
	return retVal;
}

// Save the EEPROM to an .iic file
DLLEXPORT(FLStatus) flSaveFirmware(
	struct FLContext *handle, uint32 eepromSize, const char *saveFile, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	struct Buffer i2cBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	const char *const ext = saveFile + strlen(saveFile) - 4;
	CHECK_STATUS(
		strcmp(".iic", ext), FL_FX2_ERR, cleanup,
		"flSaveFirmware(): Filename should have .iic extension");
	eepromSize <<= 7;  // convert from kbits to bytes
	bStatus = bufInitialise(&i2cBuf, eepromSize, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flSaveFirmware()");
	fxStatus = fx2ReadEEPROM(handle->device, eepromSize, &i2cBuf, error);
	CHECK_STATUS(fxStatus, FL_FX2_ERR, cleanup, "flSaveFirmware()");
	bStatus = bufWriteBinaryFile(&i2cBuf, saveFile, 0UL, i2cBuf.length, error);
	CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flSaveFirmware()");
cleanup:
	bufDestroy(&i2cBuf);
	return retVal;
}

FLStatus copyFirmwareAndRewriteIDs(
	const struct FirmwareInfo *fwInfo, uint16 vid, uint16 pid, uint16 did,
	struct Buffer *dest, const char **error)
{
	FLStatus retVal = FL_SUCCESS;  // Can return FL_ALLOC_ERR, FL_FX2_ERR and FL_INTERNAL_ERR
	BufferStatus bStatus;
	bStatus = bufAppendBlock(dest, fwInfo->data, fwInfo->length, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "copyFirmwareAndRewriteIDs()");
	dest->data[fwInfo->vp]     = (uint8)(vid & 0xFF);
	dest->data[fwInfo->vp + 1] = (uint8)(vid >> 8);
	dest->data[fwInfo->vp + 2] = (uint8)(pid & 0xFF);
	dest->data[fwInfo->vp + 3] = (uint8)(pid >> 8);
	dest->data[fwInfo->vp + 4] = (uint8)(did & 0xFF);
	dest->data[fwInfo->vp + 5] = (uint8)(did >> 8);
cleanup:
	return retVal;
}

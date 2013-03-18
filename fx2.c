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
	FLStatus flStatus, returnCode;
	struct Buffer ramBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	struct USBDevice *device = NULL;
	int uStatus;
	uint16 newVid, newPid, newDid;
	if ( !usbValidateVidPid(newVidPid) ) {
		errRender(error, "flLoadStandardFirmware(): The supplied VID:PID:DID \"%s\" is invalid; it should look like 1D50:602B or 1D50:602B:0001", newVidPid);
		FAIL(FL_USB_ERR);
	}
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);
	newDid = (strlen(newVidPid) == 14) ? (uint16)strtoul(newVidPid+10, NULL, 16) : 0x0000;
	uStatus = usbOpenDevice(curVidPid, 1, 0, 0, &device, error);
	CHECK_STATUS(uStatus, "flLoadStandardFirmware()", FL_USB_ERR);
	bStatus = bufInitialise(&ramBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, "flLoadStandardFirmware()", FL_ALLOC_ERR);
	flStatus = copyFirmwareAndRewriteIDs(
		&ramFirmware, newVid, newPid, newDid,
		&ramBuf, error);
	CHECK_STATUS(flStatus, "flLoadStandardFirmware()", flStatus);
	fxStatus = fx2WriteRAM(device, ramBuf.data, ramBuf.length, error);
	CHECK_STATUS(fxStatus, "flLoadStandardFirmware()", FL_FX2_ERR);
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&ramBuf);
	if ( device ) {
		usbCloseDevice(device, 0);
	}
	return returnCode;
}

DLLEXPORT(FLStatus) flFlashStandardFirmware(
	struct FLContext *handle, const char *newVidPid, const char **error)
{
	FLStatus flStatus, returnCode;
	struct Buffer i2cBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	uint16 newVid, newPid, newDid;
	if ( !usbValidateVidPid(newVidPid) ) {
		errRender(error, "flFlashStandardFirmware(): The supplied new VID:PID \"%s\" is invalid; it should look like 1D50:602B or 1D50:602B:0001", newVidPid);
		FAIL(FL_USB_ERR);
	}
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);
	newDid = (strlen(newVidPid) == 14) ? (uint16)strtoul(newVidPid+10, NULL, 16) : 0x0000;
	bStatus = bufInitialise(&i2cBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
	flStatus = copyFirmwareAndRewriteIDs(
		&eepromNoBootFirmware, newVid, newPid, newDid,
		&i2cBuf, error);
	CHECK_STATUS(flStatus, "flFlashStandardFirmware()", flStatus);

	fxStatus = fx2WriteEEPROM(handle->device, i2cBuf.data, i2cBuf.length, error);
	CHECK_STATUS(fxStatus, "flFlashStandardFirmware()", FL_FX2_ERR);

	returnCode = FL_SUCCESS;

cleanup:
	bufDestroy(&i2cBuf);
	return returnCode;
}

// Load custom firmware (.hex) into the FX2's RAM
DLLEXPORT(FLStatus) flLoadCustomFirmware(
	const char *curVidPid, const char *fwFile, const char **error)
{
	FLStatus returnCode;
	struct Buffer fwBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	struct USBDevice *device = NULL;
	int uStatus;
	const char *const ext = fwFile + strlen(fwFile) - 4;
	if ( strcmp(".hex", ext) ) {
		errRender(error, "flLoadCustomFirmware(): Filename should have .hex extension");
		FAIL(FL_FILE_ERR);
	}
	uStatus = usbOpenDevice(curVidPid, 1, 0, 0, &device, error);
	CHECK_STATUS(uStatus, "flLoadCustomFirmware()", FL_USB_ERR);
	bStatus = bufInitialise(&fwBuf, 8192, 0x00, error);
	CHECK_STATUS(bStatus, "flLoadCustomFirmware()", FL_ALLOC_ERR);
	bStatus = bufReadFromIntelHexFile(&fwBuf, NULL, fwFile, error);
	CHECK_STATUS(bStatus, "flLoadCustomFirmware()", FL_FILE_ERR);
	fxStatus = fx2WriteRAM(device, fwBuf.data, fwBuf.length, error);
	CHECK_STATUS(fxStatus, "flLoadCustomFirmware()", FL_FX2_ERR);
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&fwBuf);
	if ( device ) {
		usbCloseDevice(device, 0);
	}
	return returnCode;
}

// Flash custom firmware (.hex or .iic) into the FX2's EEPROM
DLLEXPORT(FLStatus) flFlashCustomFirmware(
	struct FLContext *handle, const char *fwFile, uint32 eepromSize, const char **error)
{
	FLStatus returnCode;
	struct Buffer fwData = {0,};
	struct Buffer fwMask = {0,};
	struct Buffer iicBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	I2CStatus iStatus;
	const char *const ext = fwFile + strlen(fwFile) - 4;
	if ( strcmp(".hex", ext) && strcmp(".iic", ext) ) {
		errRender(error, "flFlashCustomFirmware(): Filename should have .hex or .iic extension");
		FAIL(FL_FX2_ERR);
	}
	bStatus = bufInitialise(&iicBuf, 8192, 0x00, error);
	CHECK_STATUS(bStatus, "flFlashCustomFirmware()", FL_ALLOC_ERR);
	if ( !strcmp(".hex", ext) ) {
		// Load the .hex file, populate iicBuf:
		bStatus = bufInitialise(&fwData, 8192, 0x00, error);
		CHECK_STATUS(bStatus, "flFlashCustomFirmware()", FL_ALLOC_ERR);
		bStatus = bufInitialise(&fwMask, 8192, 0x00, error);
		CHECK_STATUS(bStatus, "flFlashCustomFirmware()", FL_ALLOC_ERR);
		bStatus = bufReadFromIntelHexFile(&fwData, &fwMask, fwFile, error);
		CHECK_STATUS(bStatus, "flFlashCustomFirmware()", FL_FILE_ERR);
		iStatus = i2cWritePromRecords(&iicBuf, &fwData, &fwMask, error);
		CHECK_STATUS(iStatus, "flFlashCustomFirmware()", FL_FX2_ERR);
	} else if ( !strcmp(".iic", ext) ) {
		// Load the .iic file into the iicBuf:
		bStatus = bufAppendFromBinaryFile(&iicBuf, fwFile, error);
		CHECK_STATUS(bStatus, "flFlashCustomFirmware()", FL_FILE_ERR);
	}
	if ( iicBuf.length > (eepromSize << 7) ) {
		errRender(
			error,
			"flFlashCustomFirmware(): Cannot load %lu bytes into an %lukbit EEPROM!",
			iicBuf.length, eepromSize);
		FAIL(FL_FX2_ERR);
	}
	fxStatus = fx2WriteEEPROM(handle->device, iicBuf.data, iicBuf.length, error);
	CHECK_STATUS(fxStatus, "flFlashCustomFirmware()", FL_FX2_ERR);
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&iicBuf);
	bufDestroy(&fwMask);
	bufDestroy(&fwData);
	return returnCode;
}

// Save the EEPROM to an .iic file
DLLEXPORT(FLStatus) flSaveFirmware(
	struct FLContext *handle, uint32 eepromSize, const char *saveFile, const char **error)
{
	FLStatus returnCode;
	struct Buffer i2cBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	const char *const ext = saveFile + strlen(saveFile) - 4;
	if ( strcmp(".iic", ext) ) {
		errRender(error, "flSaveFirmware(): Filename should have .iic extension");
		FAIL(FL_FX2_ERR);
	}
	eepromSize <<= 7;  // convert from kbits to bytes
	bStatus = bufInitialise(&i2cBuf, eepromSize, 0x00, error);
	CHECK_STATUS(bStatus, "flSaveFirmware()", FL_ALLOC_ERR);
	fxStatus = fx2ReadEEPROM(handle->device, eepromSize, &i2cBuf, error);
	CHECK_STATUS(fxStatus, "flSaveFirmware()", FL_FX2_ERR);
	bStatus = bufWriteBinaryFile(&i2cBuf, saveFile, 0UL, i2cBuf.length, error);
	CHECK_STATUS(bStatus, "flSaveFirmware()", FL_FILE_ERR);
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&i2cBuf);
	return returnCode;
}

FLStatus copyFirmwareAndRewriteIDs(
	const struct FirmwareInfo *fwInfo, uint16 vid, uint16 pid, uint16 did,
	struct Buffer *dest, const char **error)
{
	FLStatus returnCode;  // Can return FL_ALLOC_ERR, FL_FX2_ERR and FL_INTERNAL_ERR
	BufferStatus bStatus;
	bStatus = bufAppendBlock(dest, fwInfo->data, fwInfo->length, error);
	CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);

	dest->data[fwInfo->vp]     = (uint8)(vid & 0xFF);
	dest->data[fwInfo->vp + 1] = (uint8)(vid >> 8);
	dest->data[fwInfo->vp + 2] = (uint8)(pid & 0xFF);
	dest->data[fwInfo->vp + 3] = (uint8)(pid >> 8);
	dest->data[fwInfo->vp + 4] = (uint8)(did & 0xFF);
	dest->data[fwInfo->vp + 5] = (uint8)(did >> 8);

	returnCode = FL_SUCCESS;
cleanup:
	return returnCode;
}

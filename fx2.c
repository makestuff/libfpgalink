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
//#include <stdlib.h>
#include <string.h>
#include <makestuff.h>
#include <libfx2loader.h>
#include <liberror.h>
#include <libusbwrap.h>
#include <usb.h>
#include "firmware/defs.h"
#include "libfpgalink.h"
#include "private.h"

extern const uint8 eepromNoBootFirmwareData[];
extern const uint8 eepromWithBootFirmwareData[];
extern const uint8 ramFirmwareData[];
extern const uint32 eepromNoBootFirmwareSize;
extern const uint32 eepromWithBootFirmwareSize;
extern const uint32 ramFirmwareSize;

static FLStatus copyFirmwareAndRewriteIDs(
	const uint8 *src, uint32 length, uint16 vid, uint16 pid, struct Buffer *dest, const char **error
) WARN_UNUSED_RESULT;

static FLStatus appendCsvfFromXsvf(
	struct Buffer *dest, const char *xsvfFile, const char **error
) WARN_UNUSED_RESULT;

// Load the standard FPGALink firmware into the FX2 at currentVid/currentPid.
DLLEXPORT(FLStatus) flLoadStandardFirmware(
	const char *curVidPid, const char *newVidPid, const char **error)
{
	FLStatus flStatus, returnCode;
	struct Buffer ramBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	struct usb_dev_handle *device = NULL;
	int uStatus;
	uint16 currentVid, currentPid, newVid, newPid;
	if ( !usbValidateVidPid(curVidPid) ) {
		errRender(error, "The supplied current VID:PID \"%s\" is invalid; it should look like 04B4:8613", curVidPid);
		FAIL(FL_USB_ERR);
	}
	if ( !usbValidateVidPid(newVidPid) ) {
		errRender(error, "The supplied new VID:PID \"%s\" is invalid; it should look like 04B4:8613", newVidPid);
		FAIL(FL_USB_ERR);
	}
	currentVid = (uint16)strtoul(curVidPid, NULL, 16);
	currentPid = (uint16)strtoul(curVidPid+5, NULL, 16);
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);
	uStatus = usbOpenDevice(currentVid, currentPid, 1, 0, 0, &device, error);
	CHECK_STATUS(uStatus, "flLoadStandardFirmware()", FL_USB_ERR);
	bStatus = bufInitialise(&ramBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, "flLoadStandardFirmware()", FL_ALLOC_ERR);
	flStatus = copyFirmwareAndRewriteIDs(
		ramFirmwareData, ramFirmwareSize, newVid, newPid, &ramBuf, error);
	CHECK_STATUS(flStatus, "flLoadStandardFirmware()", flStatus);
	fxStatus = fx2WriteRAM(device, ramBuf.data, ramBuf.length, error);
	CHECK_STATUS(fxStatus, "flLoadStandardFirmware()", FL_FX2_ERR);
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&ramBuf);
	if ( device ) {
		usb_release_interface(device, 0);
		usb_close(device);
	}
	return returnCode;
}

DLLEXPORT(FLStatus) flFlashStandardFirmware(
	struct FLContext *handle, const char *newVidPid, uint32 eepromSize,
	const char *xsvfFile, const char **error)
{
	FLStatus flStatus, returnCode;
	struct Buffer i2cBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	uint32 fwSize, xsvfSize, initSize;
	uint16 newVid, newPid;
	if ( !usbValidateVidPid(newVidPid) ) {
		errRender(error, "The supplied new VID:PID \"%s\" is invalid; it should look like 04B4:8613", newVidPid);
		FAIL(FL_USB_ERR);
	}
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);	
	bStatus = bufInitialise(&i2cBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
	if ( xsvfFile ) {
		flStatus = copyFirmwareAndRewriteIDs(
			eepromWithBootFirmwareData, eepromWithBootFirmwareSize, newVid, newPid, &i2cBuf, error);
		CHECK_STATUS(flStatus, "flFlashStandardFirmware()", flStatus);
		fwSize = i2cBuf.length;
		flStatus = appendCsvfFromXsvf(&i2cBuf, xsvfFile, error);
		CHECK_STATUS(flStatus, "flFlashStandardFirmware()", flStatus);
		xsvfSize = i2cBuf.length - fwSize;
		if ( handle->writeBuffer.length ) {
			// Write a big-endian uint16 length for the init data, then the data itself
			if ( handle->writeBuffer.length > 0xFFFF ) {
				errRender(
					error,
					"flFlashStandardFirmware(): Cannot cope with %lu bytes of init data",
					handle->writeBuffer.length);
				FAIL(FL_FX2_ERR);
			}
			bStatus = bufAppendByte(&i2cBuf, (uint8)(handle->writeBuffer.length >> 8), error);
			CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
			bStatus = bufAppendByte(&i2cBuf, (uint8)(handle->writeBuffer.length & 0xFF), error);
			CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
			bStatus = bufAppendBlock(
				&i2cBuf, handle->writeBuffer.data, handle->writeBuffer.length, error);
			CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
			initSize = handle->writeBuffer.length + 2;
		} else {
			// Write a zero length so the firmware knows there's no init data to follow
			bStatus = bufAppendByte(&i2cBuf, 0x00, error);
			CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
			bStatus = bufAppendByte(&i2cBuf, 0x00, error);
			CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
			initSize = 2;
		}
	} else {
		flStatus = copyFirmwareAndRewriteIDs(
			eepromNoBootFirmwareData, eepromNoBootFirmwareSize, newVid, newPid, &i2cBuf, error);
		CHECK_STATUS(flStatus, "flFlashStandardFirmware()", flStatus);
		fwSize = i2cBuf.length;
		xsvfSize = 0;
		initSize = 0;
	}
	if ( i2cBuf.length > (eepromSize<<7) ) {
		errRender(
			error,
			"flFlashStandardFirmware(): Cannot load %lu bytes (%lu + %lu + %lu) into an %lukbit EEPROM!",
			i2cBuf.length, fwSize, xsvfSize, initSize, eepromSize);
		FAIL(FL_FX2_ERR);
	}

	//bStatus = bufWriteBinaryFile(&i2cBuf, "out.bin", 0UL, i2cBuf.length, error);
	//CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
	
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
	struct usb_dev_handle *device = NULL;
	int uStatus;
	const char *const ext = fwFile + strlen(fwFile) - 4;
	uint16 vid, pid;
	if ( !usbValidateVidPid(curVidPid) ) {
		errRender(error, "The supplied current VID:PID \"%s\" is invalid; it should look like 04B4:8613", curVidPid);
		FAIL(FL_USB_ERR);
	}
	vid = (uint16)strtoul(curVidPid, NULL, 16);
	pid = (uint16)strtoul(curVidPid+5, NULL, 16);	
	if ( strcmp(".hex", ext) ) {
		errRender(error, "flLoadCustomFirmware(): Filename should have .hex extension");
		FAIL(FL_FILE_ERR);
	}
	uStatus = usbOpenDevice(vid, pid, 1, 0, 0, &device, error);
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
		usb_release_interface(device, 0);
		usb_close(device);
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

static FLStatus copyFirmwareAndRewriteIDs(
	const uint8 *src, uint32 length, uint16 vid, uint16 pid, struct Buffer *dest,
	const char **error)
{
	FLStatus returnCode;  // Can return FL_ALLOC_ERR and FL_INTERNAL_ERR
	BufferStatus bStatus;
	const uint8 *const end = src + length;
	uint32 count = 0;
	while ( src < end ) {
		if ( src[0] == 0xB4 && src[1] == 0x04 && src[2] == 0x13 && src[3] == 0x86 ) {
			src += 4;
			bStatus = bufAppendByte(dest, (uint8)(vid & 0xFF), error);
			CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);
			bStatus = bufAppendByte(dest, (uint8)(vid >> 8), error);
			CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);
			bStatus = bufAppendByte(dest, (uint8)(pid & 0xFF), error);
			CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);
			bStatus = bufAppendByte(dest, (uint8)(pid >> 8), error);
			CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);
			count++;
		} else {
			bStatus = bufAppendByte(dest, *src++, error);
			CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);
		}
	}
	if ( count == 0 ) {
		// This should never happen
		errRender(
			error, "copyFirmwareAndRewriteIDs(): Unable to find VID/PID in firmware!");
		FAIL(FL_INTERNAL_ERR);
	} else if ( count > 1 ) {
		// This should never happen
		errRender(
			error,
			"copyFirmwareAndRewriteIDs(): Found more than one candidate VID/PID in firmware!");
		FAIL(FL_INTERNAL_ERR);
	}
	returnCode = FL_SUCCESS;
cleanup:
	return returnCode;
}

static FLStatus appendCsvfFromXsvf(struct Buffer *dest, const char *xsvfFile, const char **error) {
	FLStatus returnCode, fStatus;
	struct Buffer csvfBuf = {0,};
	BufferStatus bStatus;
	uint32 maxBufSize;
	const char *const ext = xsvfFile + strlen(xsvfFile) - 5;
	if ( strcmp(".xsvf", ext) == 0 ) {
		bStatus = bufInitialise(&csvfBuf, 0x20000, 0, error);
		CHECK_STATUS(bStatus, "appendCsvfFromXsvf()", FL_ALLOC_ERR);
		fStatus = flLoadXsvfAndConvertToCsvf(xsvfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, "appendCsvfFromXsvf()", fStatus);
		if ( maxBufSize > CSVF_BUF_SIZE ) {
			errRender(error, "appendCsvfFromXsvf(): This XSVF file requires CSVF_BUF_SIZE=%d", maxBufSize);
			FAIL(FL_JTAG_ERR);
		}
		fStatus = flCompressCsvf(&csvfBuf, error);
		CHECK_STATUS(fStatus, "appendCsvfFromXsvf()", fStatus);
		bStatus = bufAppendBlock(dest, csvfBuf.data, csvfBuf.length, error);
		CHECK_STATUS(bStatus, "appendCsvfFromXsvf()", FL_ALLOC_ERR);
	} else if ( strcmp(".csvf", ext) == 0 ) {
		bStatus = bufAppendFromBinaryFile(dest, xsvfFile, error);
		CHECK_STATUS(bStatus, "appendCsvfFromXsvf()", FL_FILE_ERR);
	} else {
		errRender(error, "appendCsvfFromXsvf(): Filename should have .xsvf or .csvf extension");
		FAIL(FL_FILE_ERR);
	}
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&csvfBuf);
	return returnCode;
}

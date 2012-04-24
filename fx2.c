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
#ifdef WIN32
	#include <lusb0_usb.h>
#else
	#include <usb.h>
#endif
#include "firmware/defs.h"
#include "libfpgalink.h"
#include "private.h"
#include "firmware.h"

static FLStatus convertJtagFileToCsvf(
	struct Buffer *dest, const char *xsvfFile, const char **error
) WARN_UNUSED_RESULT;

// Load the standard FPGALink firmware into the FX2 at currentVid/currentPid.
DLLEXPORT(FLStatus) flLoadStandardFirmware(
	const char *curVidPid, const char *newVidPid, const char *jtagPort, const char **error)
{
	FLStatus flStatus, returnCode;
	struct Buffer ramBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	struct usb_dev_handle *device = NULL;
	int uStatus;
	uint16 currentVid, currentPid, newVid, newPid;
	uint8 port, tdoBit, tdiBit, tmsBit, tckBit;
	if ( !usbValidateVidPid(curVidPid) ) {
		errRender(error, "flLoadStandardFirmware(): The supplied current VID:PID \"%s\" is invalid; it should look like 04B4:8613", curVidPid);
		FAIL(FL_USB_ERR);
	}
	if ( !usbValidateVidPid(newVidPid) ) {
		errRender(error, "flLoadStandardFirmware(): The supplied new VID:PID \"%s\" is invalid; it should look like 04B4:8613", newVidPid);
		FAIL(FL_USB_ERR);
	}
	currentVid = (uint16)strtoul(curVidPid, NULL, 16);
	currentPid = (uint16)strtoul(curVidPid+5, NULL, 16);
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);
	if ( strlen(jtagPort) != 5 ) {
		errRender(error, "flLoadStandardFirmware(): JTAG port specification must be <C|D><tdoBit><tdiBit><tmsBit><tckBit>");
		FAIL(FL_FX2_ERR);
	}
	if ( (jtagPort[0] & 0xDF) == 'C' ) {
		port = 0;
	} else if ( (jtagPort[0] & 0xDF) == 'D' ) {
		port = 1;
	} else {
		errRender(error, "flLoadStandardFirmware(): JTAG port specification must be <C|D><tdoBit><tdiBit><tmsBit><tckBit>");
		FAIL(FL_FX2_ERR);
	}
	if  (jtagPort[1] < '0' || jtagPort[1] > '7' || jtagPort[2] < '0' || jtagPort[2] > '7' || jtagPort[3] < '0' || jtagPort[3] > '7' || jtagPort[4] < '0' || jtagPort[4] > '7' ) {
		errRender(error, "flLoadStandardFirmware(): JTAG port specification must be <C|D><tdoBit><tdiBit><tmsBit><tckBit>");
		FAIL(FL_FX2_ERR);
	}
	tdoBit = jtagPort[1] - '0';
	tdiBit = jtagPort[2] - '0';
	tmsBit = jtagPort[3] - '0';
	tckBit = jtagPort[4] - '0';
	uStatus = usbOpenDevice(currentVid, currentPid, 1, 0, 0, &device, error);
	CHECK_STATUS(uStatus, "flLoadStandardFirmware()", FL_USB_ERR);
	bStatus = bufInitialise(&ramBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, "flLoadStandardFirmware()", FL_ALLOC_ERR);
	flStatus = copyFirmwareAndRewriteIDs(
		&ramFirmware, newVid, newPid,
		port, tdoBit, tdiBit, tmsBit, tckBit,
		&ramBuf, error);
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
	struct FLContext *handle, const char *newVidPid, const char *jtagPort,
	 uint32 eepromSize, const char *xsvfFile, const char **error)
{
	FLStatus flStatus, returnCode;
	struct Buffer i2cBuf = {0,};
	BufferStatus bStatus;
	FX2Status fxStatus;
	uint32 fwSize, xsvfSize, initSize;
	uint16 newVid, newPid;
	uint8 port, tdoBit, tdiBit, tmsBit, tckBit;
	if ( !usbValidateVidPid(newVidPid) ) {
		errRender(error, "flFlashStandardFirmware(): The supplied new VID:PID \"%s\" is invalid; it should look like 04B4:8613", newVidPid);
		FAIL(FL_USB_ERR);
	}
	newVid = (uint16)strtoul(newVidPid, NULL, 16);
	newPid = (uint16)strtoul(newVidPid+5, NULL, 16);	
	if ( strlen(jtagPort) != 5 ) {
		errRender(error, "flFlashStandardFirmware(): JTAG port specification must be <C|D><tdoBit><tdiBit><tmsBit><tckBit>");
		FAIL(FL_FX2_ERR);
	}
	if ( (jtagPort[0] & 0xDF) == 'C' ) {
		port = 0;
	} else if ( (jtagPort[0] & 0xDF) == 'D' ) {
		port = 1;
	} else {
		errRender(error, "flFlashStandardFirmware(): JTAG port specification must be <C|D><tdoBit><tdiBit><tmsBit><tckBit>");
		FAIL(FL_FX2_ERR);
	}
	if  (jtagPort[1] < '0' || jtagPort[1] > '7' || jtagPort[2] < '0' || jtagPort[2] > '7' || jtagPort[3] < '0' || jtagPort[3] > '7' || jtagPort[4] < '0' || jtagPort[4] > '7' ) {
		errRender(error, "flFlashStandardFirmware(): JTAG port specification must be <C|D><tdoBit><tdiBit><tmsBit><tckBit>");
		FAIL(FL_FX2_ERR);
	}
	tdoBit = jtagPort[1] - '0';
	tdiBit = jtagPort[2] - '0';
	tmsBit = jtagPort[3] - '0';
	tckBit = jtagPort[4] - '0';
	bStatus = bufInitialise(&i2cBuf, 0x4000, 0x00, error);
	CHECK_STATUS(bStatus, "flFlashStandardFirmware()", FL_ALLOC_ERR);
	if ( xsvfFile ) {
		flStatus = copyFirmwareAndRewriteIDs(
			&eepromWithBootFirmware, newVid, newPid,
			port, tdoBit, tdiBit, tmsBit, tckBit,
			&i2cBuf, error);
		CHECK_STATUS(flStatus, "flFlashStandardFirmware()", flStatus);
		fwSize = i2cBuf.length;
		flStatus = convertJtagFileToCsvf(&i2cBuf, xsvfFile, error);
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
			&eepromNoBootFirmware, newVid, newPid,
			port, tdoBit, tdiBit, tmsBit, tckBit,
			&i2cBuf, error);
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
		errRender(error, "flLoadCustomFirmware(): The supplied current VID:PID \"%s\" is invalid; it should look like 04B4:8613", curVidPid);
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

FLStatus copyFirmwareAndRewriteIDs(
	const struct FirmwareInfo *fwInfo, uint16 vid, uint16 pid,
	uint8 port, uint8 tdoBit, uint8 tdiBit, uint8 tmsBit, uint8 tckBit,
	struct Buffer *dest, const char **error)
{
	FLStatus returnCode;  // Can return FL_ALLOC_ERR, FL_FX2_ERR and FL_INTERNAL_ERR
	BufferStatus bStatus;
	uint8 reg;
	int i;
	bStatus = bufAppendBlock(dest, fwInfo->data, fwInfo->length, error);
	CHECK_STATUS(bStatus, "copyFirmwareAndRewriteIDs()", FL_ALLOC_ERR);

	dest->data[fwInfo->vp]     = (uint8)(vid & 0xFF);
	dest->data[fwInfo->vp + 1] = (uint8)(vid >> 8);
	dest->data[fwInfo->vp + 2] = (uint8)(pid & 0xFF);
	dest->data[fwInfo->vp + 3] = (uint8)(pid >> 8);

	if ( port == 0 ) {
		// Use port C for JTAG operations
		dest->data[fwInfo->d0A] = 0x08;
		dest->data[fwInfo->d0B] = 0x09;
		reg = 0xB4;
	} else if ( port == 1 ) {
		// Use port D for JTAG operations
		dest->data[fwInfo->d0A] = 0x0A;
		dest->data[fwInfo->d0B] = 0x0B;
		reg = 0xB5;
	} else {
		errRender(
			error,
			"copyFirmwareAndRewriteIDs(): Only port C or D may be used for JTAG operations!");
		FAIL(FL_FX2_ERR);
	}
	dest->data[fwInfo->outBits] = (1<<tdiBit) | (1<<tmsBit) | (1<<tckBit);
	dest->data[fwInfo->outBitsComp] = (uint8)~((1<<tdiBit) | (1<<tmsBit) | (1<<tckBit));
	i = NUM_OE_BITS;
	while ( i-- ) {
		dest->data[fwInfo->oeRegs[i]] = reg;
	}
	i = NUM_ALL_BITS;
	while ( i-- ) {
		dest->data[fwInfo->allBits[i]] = (1<<tdoBit) | (1<<tdiBit) | (1<<tmsBit) | (1<<tckBit);;
	}
	i = NUM_ALL_BITS;
	while ( i-- ) {
		dest->data[fwInfo->allBitsComp[i]] = (uint8)~((1<<tdoBit) | (1<<tdiBit) | (1<<tmsBit) | (1<<tckBit));
	}
	i = NUM_TDO_BIT;
	while ( i-- ) {
		dest->data[fwInfo->tdoBit[i]] = (0xA0 + (port<<4) + tdoBit);
	}
	i = NUM_TDI_BIT;
	while ( i-- ) {
		dest->data[fwInfo->tdiBit[i]] = (0xA0 + (port<<4) + tdiBit);
	}
	i = NUM_TMS_BIT;
	while ( i-- ) {
		dest->data[fwInfo->tmsBit[i]] = (0xA0 + (port<<4) + tmsBit);
	}
	i = NUM_TCK_BIT;
	while ( i-- ) {
		dest->data[fwInfo->tckBit[i]] = (0xA0 + (port<<4) + tckBit);
	}
	returnCode = FL_SUCCESS;
cleanup:
	return returnCode;
}

static FLStatus convertJtagFileToCsvf(struct Buffer *dest, const char *xsvfFile, const char **error) {
	FLStatus returnCode, fStatus;
	struct Buffer csvfBuf = {0,};
	BufferStatus bStatus;
	uint32 maxBufSize;
	const char *const ext = xsvfFile + strlen(xsvfFile) - 5;
	if ( strcmp(".svf", ext+1) == 0 ) {
		bStatus = bufInitialise(&csvfBuf, 0x20000, 0, error);
		CHECK_STATUS(bStatus, "convertJtagFileToCsvf()", FL_ALLOC_ERR);
		fStatus = flLoadSvfAndConvertToCsvf(xsvfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, "convertJtagFileToCsvf()", fStatus);
		if ( maxBufSize > CSVF_BUF_SIZE ) {
			errRender(error, "convertJtagFileToCsvf(): This SVF file requires CSVF_BUF_SIZE=%d", maxBufSize);
			FAIL(FL_JTAG_ERR);
		}
		fStatus = flCompressCsvf(&csvfBuf, error);
		CHECK_STATUS(fStatus, "convertJtagFileToCsvf()", fStatus);
		bStatus = bufAppendBlock(dest, csvfBuf.data, csvfBuf.length, error);
		CHECK_STATUS(bStatus, "convertJtagFileToCsvf()", FL_ALLOC_ERR);
	} else if ( strcmp(".xsvf", ext) == 0 ) {
		bStatus = bufInitialise(&csvfBuf, 0x20000, 0, error);
		CHECK_STATUS(bStatus, "convertJtagFileToCsvf()", FL_ALLOC_ERR);
		fStatus = flLoadXsvfAndConvertToCsvf(xsvfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, "convertJtagFileToCsvf()", fStatus);
		if ( maxBufSize > CSVF_BUF_SIZE ) {
			errRender(error, "convertJtagFileToCsvf(): This XSVF file requires CSVF_BUF_SIZE=%d", maxBufSize);
			FAIL(FL_JTAG_ERR);
		}
		fStatus = flCompressCsvf(&csvfBuf, error);
		CHECK_STATUS(fStatus, "convertJtagFileToCsvf()", fStatus);
		bStatus = bufAppendBlock(dest, csvfBuf.data, csvfBuf.length, error);
		CHECK_STATUS(bStatus, "convertJtagFileToCsvf()", FL_ALLOC_ERR);
	} else if ( strcmp(".csvf", ext) == 0 ) {
		bStatus = bufAppendFromBinaryFile(dest, xsvfFile, error);
		CHECK_STATUS(bStatus, "convertJtagFileToCsvf()", FL_FILE_ERR);
	} else {
		errRender(error, "convertJtagFileToCsvf(): Filename should have .svf, .xsvf or .csvf extension");
		FAIL(FL_FILE_ERR);
	}
	returnCode = FL_SUCCESS;
cleanup:
	bufDestroy(&csvfBuf);
	return returnCode;
}

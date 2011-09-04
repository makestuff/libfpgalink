/* 
 * Copyright (C) 2011 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>
#include <makestuff.h>
#include <libnero.h>
#include <liberror.h>
#include "libfpgalink.h"
#include "private.h"
#include "xsvf2csvf.h"
#include "csvfplay.h"

// Play an XSVF file into the JTAG chain.
//
DLLEXPORT(FLStatus) flPlayXSVF(struct FLContext *handle, const char *xsvfFile, const char **error) {
	FLStatus returnCode;
	struct NeroHandle nero = {0,};
	struct Buffer csvfBuf = {0,};
	BufferStatus bStatus;
	X2CStatus xStatus;
	NeroStatus nStatus;
	int cStatus;
	uint16 maxBufSize;
	const char *const ext = xsvfFile + strlen(xsvfFile) - 5;
	if ( strcmp(".xsvf", ext) ) {
		errRender(error, "flPlayXSVF(): Filename should have .xsvf extension");
		FAIL(FL_FILE_ERR);
	}
	if ( !handle->isNeroCapable ) {
		errRender(error, "flPlayXSVF(): This device does not support NeroJTAG");
		FAIL(FL_PROTOCOL_ERR);
	}
	bStatus = bufInitialise(&csvfBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, "flPlayXSVF()", FL_ALLOC_ERR);
	xStatus = loadXsvfAndConvertToCsvf(xsvfFile, &csvfBuf, &maxBufSize, error);
	CHECK_STATUS(xStatus, "flPlayXSVF()", FL_FILE_ERR);
	nStatus = neroInitialise(handle->device, &nero, error);
	CHECK_STATUS(nStatus, "flPlayXSVF()", FL_JTAG_ERR);
	cStatus = csvfPlay(csvfBuf.data, &nero, error);
	CHECK_STATUS(cStatus, "flPlayXSVF()", FL_JTAG_ERR);
	returnCode = FL_SUCCESS;
cleanup:
	nStatus = neroClose(&nero, NULL);
	bufDestroy(&csvfBuf);
	return returnCode;
}

// Reverse the array in-place by swapping the outer items and progressing inward until we meet in
// the middle
//
static void swap(uint32 *array, uint32 numWritten) {
	uint32 *hiPtr = array + numWritten - 1;  // last one
	uint32 *loPtr = array; // first one
	uint32 temp;
	while ( loPtr < hiPtr ) {
		temp = *loPtr;
		*loPtr++ = *hiPtr;
		*hiPtr-- = temp;
	}
}	

// Scan the JTAG chain and return an array of IDCODEs
//
FLStatus flScanChain(
	struct FLContext *handle, uint32 *numDevices, uint32 *deviceArray, uint32 arraySize,
	const char **error)
{
	FLStatus returnCode;
	struct NeroHandle nero = {0,};
	NeroStatus nStatus;
	uint32 i = 0;
	union {
		uint32 idCode;
		uint8 bytes[4];
	} u;
	nStatus = neroInitialise(handle->device, &nero, error);
	CHECK_STATUS(nStatus, "flScanChain()", FL_JTAG_ERR);
	nStatus = neroClockFSM(&nero, 0x0000005F, 9, error);  // Reset TAP, goto Shift-DR
	CHECK_STATUS(nStatus, "flScanChain()", FL_JTAG_ERR);
	for ( ; ; ) {
		nStatus = neroShift(&nero, 32, ZEROS, u.bytes, false, error);
		CHECK_STATUS(nStatus, "flScanChain()", FL_JTAG_ERR);
		if ( u.idCode == 0x00000000 || u.idCode == 0xFFFFFFFF ) {
			break;
		}
		if ( deviceArray && i < arraySize ) {
			deviceArray[i] = u.idCode;
		}
		i++;
	}
	if ( deviceArray && i ) {
		// The IDCODEs we have are in reverse order, so swap them to get the correct chain order.
		swap(deviceArray, (i > arraySize) ? arraySize : i);
	}
	if ( numDevices ) {
		*numDevices = i;
	}
	returnCode = FL_SUCCESS;
cleanup:
	nStatus = neroClose(&nero, NULL);
	return returnCode;
}



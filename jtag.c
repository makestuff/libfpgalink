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
#                             include <stdio.h>
#include <string.h>
#include <makestuff.h>
#include <libusbwrap.h>
#include <liberror.h>
#include "nero.h"
#include "libfpgalink.h"
#include "private.h"
#include "csvfplay.h"
#include "prog.h"

NeroStatus beginShift(
	struct FLContext *handle, uint32 numBits, ProgOp progOp, uint8 mode, const char **error);

NeroStatus doSend(
	struct FLContext *handle, const uint8 *sendPtr, uint16 chunkSize, const char **error);

#define GET_CHAR(func) \
	ch = *ptr; \
	if ( ch == '\0' ) { \
		errRender(error, func"(): Unexpected end of config string at char %d", ptr-portConfig); \
		FAIL(FL_CONF_FORMAT); \
	}

#define EXPECT_CHAR(ex, func) \
	GET_CHAR(func) \
	if ( ch != ex ) { \
		errRender(error, func"(): Expecting "#ex" but found '%c' at char %d", ch, ptr-portConfig); \
		FAIL(FL_CONF_FORMAT); \
	} \
	ptr++

#define GET_PORT(port, func) \
	GET_CHAR(func) \
	if ( ch < 'A' || ch > 'E' ) { \
		errRender(error, func"(): Port '%c' is not valid at char %d", ch, ptr-portConfig); \
		FAIL(FL_CONF_FORMAT); \
	} \
	port = ch - 'A'; \
	ptr++

#define GET_BIT(bit, func) \
	GET_CHAR(func) \
	if ( ch < '0' || ch > '7' ) { \
		errRender(error, func"(): Bit '%c' is not valid at char %d", ch, ptr-portConfig); \
		FAIL(FL_CONF_FORMAT); \
	} \
	bit = ch - '0'; \
	ptr++

#define GET_PAIR(port, bit, func) \
	GET_PORT(port, func); \
	GET_BIT(bit, func)

#define SET_BIT(port, bit, status, func) \
	if ( pinMap[port*8+bit] != UNUSED ) { \
		errRender(error, func"(): port '%c%d' is already used at char %d", port + 'A', bit, ptr-portConfig-2); \
		FAIL(FL_CONF_FORMAT); \
	} \
	pinMap[port*8+bit] = status

typedef enum {UNUSED, HIGH, LOW, INPUT} PinState;

static void makeMasks(const PinState *pinMap, uint8 *mask, uint8 *ddr, uint8 *port) {
	int i, j;
	for ( i = 0; i < 5; i++ ) {
		mask[i] = 0x00;
		ddr[i] = 0x00;
		port[i] = 0x00;
		for ( j = 0; j < 8; j++ ) {
			switch ( pinMap[i*8+j] ) {
			case HIGH:
				port[i] |= (1<<j); // note no break
			case LOW:
				ddr[i] |= (1<<j);  // note no break
			case INPUT:
				mask[i] |= (1<<j); // note no break
			default:
				break;
			}
		}
	}
}

static FLStatus populateMap(const char *portConfig, const char *ptr, const char **endPtr, PinState *pinMap, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	uint8 thisPort, thisBit;
	char ch;
	do {
		GET_PAIR(thisPort, thisBit, "populateMap");
		GET_CHAR("populateMap");
		if ( ch == '+' ) {
			SET_BIT(thisPort, thisBit, HIGH, "populateMap");
		} else if ( ch == '-' ) {
			SET_BIT(thisPort, thisBit, LOW, "populateMap");
		} else if ( ch == '/' ) {
			SET_BIT(thisPort, thisBit, INPUT, "populateMap");
		} else {
			errRender(error, "populateMap(): Expecting '+', '-' or '/' at char %d", ptr-portConfig);
			FAIL(FL_CONF_FORMAT);
		}
		ptr++;
		ch = *ptr++;
	} while ( ch == ',' );
	*endPtr = ptr - 1;
cleanup:
	return returnCode;
}

static FLStatus portMap(struct FLContext *handle, uint8 patchClass, uint8 port, uint8 bit, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	union {
		uint16 word;
		uint8 bytes[2];
	} index, value;
	int uStatus;
	index.bytes[0] = patchClass;
	index.bytes[1] = port;
	value.bytes[0] = bit;
	value.bytes[1] = 0x00;
	uStatus = usbControlWrite(
		handle->device,
		0x90,              // bRequest
		value.word,        // wValue
		index.word,        // wIndex
		NULL,              // no data
		0,                 // wLength
		1000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "portMap()", FL_PORTMAP);
cleanup:
	return returnCode;
}

static FLStatus dataWrite(struct FLContext *handle, ProgOp progOp, const uint8 *buf, uint32 len, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	NeroStatus nStatus = beginShift(handle, len, progOp, 0x00, error);
	uint16 chunkSize;
	CHECK_STATUS(nStatus, "dataWrite()", FL_JTAG_ERR);
	while ( len ) {
		chunkSize = (len >= 64) ? 64 : (uint16)len;
		nStatus = doSend(handle, buf, chunkSize, error);
		CHECK_STATUS(nStatus, "dataWrite()", FL_JTAG_ERR);
		buf += chunkSize;
		len -= chunkSize;
	}
cleanup:
	return returnCode;
}

static FLStatus fileWrite(struct FLContext *handle, ProgOp progOp, const char *fileName, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	FLStatus fStatus;
	uint8 *fileData = NULL;
	uint32 fileLen;
	fileData = flLoadFile(fileName, &fileLen);
	if ( !fileData ) {
		errRender(error, "fileWrite(): Unable to read from %s", fileName);
		FAIL(FL_JTAG_ERR);
	}
	printf("Loaded %d bytes\n", fileLen);
	fStatus = dataWrite(handle, progOp, fileData, fileLen, error);
	CHECK_STATUS(fStatus, "fileWrite()", fStatus);
cleanup:
	if ( fileData ) {
		flFreeFile(fileData);
	}
	return returnCode;
}

static FLStatus xpProgram(struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	FLStatus fStatus;
	uint8 progPort, progBit;
	uint8 initPort, initBit;
	uint8 donePort, doneBit;
	uint8 cclkPort, cclkBit;
	uint8 dataPort, dataBit[8];
	uint8 maskList[5], ddrList[5], portList[5];
	uint8 mask, ddr, port;
	uint8 progMask, initMask, doneMask, tempByte;
	const char *ptr = portConfig + 2;
	char ch;
	PinState pinMap[5*8] = {0,};
	int i;
	EXPECT_CHAR(':', "xpProgram");
	GET_PAIR(progPort, progBit, "xpProgram");
	progMask = 1 << progBit;
	SET_BIT(progPort, progBit, LOW, "xpProgram");
	GET_PAIR(initPort, initBit, "xpProgram");
	initMask = 1 << initBit;
	SET_BIT(initPort, initBit, INPUT, "xpProgram");
	GET_PAIR(donePort, doneBit, "xpProgram");
	doneMask = 1 << doneBit;
	SET_BIT(donePort, doneBit, INPUT, "xpProgram");
	GET_PAIR(cclkPort, cclkBit, "xpProgram");
	SET_BIT(cclkPort, cclkBit, LOW, "xpProgram");
	if ( progPort != initPort || progPort != donePort ) {
		errRender(error, "xpProgram(): For now, PROG, INIT & DONE must be on the same port");
		FAIL(FL_CONF_FORMAT);
	}
	GET_PORT(dataPort, "xpProgram");
	for ( i = 0; i < 8; i++ ) {
		GET_BIT(dataBit[i], "xpProgram");
		if ( dataBit[i] != i ) {
			errRender(error, "xpProgram(): For now, the SelectMAP data bus is hard-wired to '01234567'");
			FAIL(FL_CONF_FORMAT);
		}
		SET_BIT(dataPort, dataBit[i], LOW, "xpProgram");
	}
	EXPECT_CHAR(':', "xpProgram");
	GET_CHAR("xpProgram");
	if ( ch == '[' ) {
		ptr++;
		fStatus = populateMap(portConfig, ptr, &ptr, pinMap, error);
		CHECK_STATUS(fStatus, "xpProgram()", fStatus);
		EXPECT_CHAR(']', "xpProgram");
	}
	ch = *ptr;
	if ( ch == ':' ) {
		ptr++;
		if ( progFile ) {
			errRender(error, "xpProgram(): Config includes a filename, but a filename is already provided");
			FAIL(FL_CONF_FORMAT);
		}
		progFile = ptr;
	} else if ( ch != '\0' ) {
		errRender(error, "xpProgram(): Expecting ':' or end-of-string, not '%c' at char %d", ch, ptr-portConfig);
		FAIL(FL_CONF_FORMAT);
	} else if ( !progFile ) {
		errRender(error, "xpProgram(): No filename given");
		FAIL(FL_CONF_FORMAT);
	}

	//ptr = progFile + strlen(progFile) - 4;
	//if ( !strcmp(ptr, ".bin") ) {
	//	printf("BINFILE\n");
	//} else if ( !strcmp(ptr, ".bit") ) {
	//	printf("BITFILE\n");
	//} else {
	//	errRender(error, "xpProgram(): Unrecognised file type");
	//	FAIL(FL_CONF_FORMAT);
	//}

	makeMasks(pinMap, maskList, ddrList, portList);

	for ( i = 0; i < 5; i++ ) {
		if ( maskList[i] ) {
			printf("Port %c: {mask: %02X, ddr: %02X, port: %02X}\n", i + 'A', maskList[i], ddrList[i], portList[i]);
		}
	}

	// Map the CCLK bit & the SelectMAP data bus
	fStatus = portMap(handle, 3, cclkPort, cclkBit, error);
	CHECK_STATUS(fStatus, "xpProgram()", fStatus);
	fStatus = portMap(handle, 4, dataPort, 0x00, error);
	CHECK_STATUS(fStatus, "xpProgram()", fStatus);

	// Assert PROG & wait for INIT & DONE to go low
	do {
		fStatus = flPortAccess(
			handle, progPort,
			(progMask | initMask | doneMask),
			progMask,
			0x00,
			&tempByte,
			error
		);
		CHECK_STATUS(fStatus, "xpProgram()", fStatus);
	} while ( tempByte & (initMask | doneMask) );

	printf("Asserted PROG, saw INIT & DONE low: FPGA is in INIT mode\n");

	fStatus = flFifoMode(handle, false, error);
	CHECK_STATUS(fStatus, "xpProgram()", fStatus);

	printf("Disabled FIFO mode\n");

	for ( i = 0; i < 5; i++ ) {
		mask = maskList[i];
		ddr = ddrList[i];
		port = portList[i];
		if ( i == progPort ) {
			mask &= ~(progMask | initMask | doneMask);
			ddr &= ~(progMask | initMask | doneMask);
			port &= ~(progMask | initMask | doneMask);
		}
		if ( mask ) {
			fStatus = flPortAccess(
				handle, i,
				mask, ddr, port,
				NULL,
				error
			);
			CHECK_STATUS(fStatus, "xpProgram()", fStatus);
		}
	}

	printf("Configured ports\n");

	do {
		fStatus = flPortAccess(
			handle, progPort,  // deassert PROG
			progMask,               // mask: affect only PROG
			progMask,               // ddr: PROG is output
			progMask,               // port: drive PROG high (deasserted)
			&tempByte,
			error
		);
		CHECK_STATUS(fStatus, "xpProgram()", fStatus);
	} while ( !(tempByte & initMask) );

	printf("Deasserted PROG, saw INIT go high: FPGA is ready for data\n");

	fStatus = fileWrite(handle, PROG_PARALLEL, progFile, error);
	CHECK_STATUS(fStatus, "xpProgram()", fStatus);

	printf("Finished sending data\n");

	fStatus = flPortAccess(
		handle, progPort,  // check INIT & DONE
		0x00,                   // mask: affect nothing
		0x00,                   // ddr: ignored
		0x00,                   // port: ignored
		&tempByte,
		error
	);
	CHECK_STATUS(fStatus, "xpProgram()", fStatus);
	printf("init: %02X; done: %02X\n", tempByte&initMask, tempByte&doneMask);
	if ( !(tempByte & doneMask) && !(tempByte & initMask) ) {
		errRender(error, "xpProgram(): INIT unexpectedly low (CRC error during config)");
		FAIL(FL_JTAG_ERR);
	}

	printf("Saw DONE go high\n");

	for ( i = 0; i < 5; i++ ) {
		mask = maskList[i];
		if ( mask ) {
			fStatus = flPortAccess(
				handle, i,
				mask,
				(i == progPort) ? progMask : 0x00,
				0xFF,
				NULL,
				error
			);
			CHECK_STATUS(fStatus, "xpProgram()", fStatus);
		}
	}
	printf("De-configured ports\n");

	fStatus = flFifoMode(handle, true, error);
	CHECK_STATUS(fStatus, "xpProgram()", fStatus);

	printf("Enabled FIFO mode again\n");

cleanup:
	return returnCode;
}

static FLStatus xsProgram(struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	return FL_SUCCESS;
}

static FLStatus jProgram(struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	return FL_SUCCESS;
}

DLLEXPORT(FLStatus) flProgram(struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	const char algoVendor = portConfig[0];
	if ( algoVendor == 'X' ) {
		// This is a Xilinx algorithm
		const char algoType = portConfig[1];
		if ( algoType == 'P' ) {
			// This is Xilinx Slave Parallel ("SelectMAP")
			return xpProgram(handle, portConfig, progFile, error);
		} else if ( algoType == 'S' ) {
			// This is Xilinx Slave Serial
			return xsProgram(handle, portConfig, progFile, error);
		} else if ( algoType == '\0' ) {
			errRender(error, "flProgram(): Missing Xilinx algorithm code");
			FAIL(FL_CONF_FORMAT);
		} else {
			// This is not a valid Xilinx algorithm
			errRender(error, "flProgram(): '%c' is not a valid Xilinx algorithm code", algoType);
			FAIL(FL_CONF_FORMAT);
		}
	} else if ( algoVendor == 'J' ) {
		// This is a JTAG algorithm
		return jProgram(handle, portConfig + 2, progFile, error);
	} else if ( algoVendor == '\0' ) {
		errRender(error, "flProgram(): Missing algorithm vendor code");
		FAIL(FL_CONF_FORMAT);
	} else {
		errRender(error, "flProgram(): '%c' is not a valid algorithm vendor code", algoVendor);
		FAIL(FL_CONF_FORMAT);
	}
cleanup:
	return returnCode;
}

// Play an SVF, XSVF or CSVF file into the JTAG chain.
//
DLLEXPORT(FLStatus) flPlaySVF(struct FLContext *handle, const char *svfFile, const char *portConfig, const char **error) {
	FLStatus returnCode, fStatus;
	struct Buffer csvfBuf = {0,};
	BufferStatus bStatus;
	NeroStatus nStatus;
	int cStatus;
	uint32 maxBufSize;
	bool isCompressed;
		
	const char *const ext = svfFile + strlen(svfFile) - 5;
	if ( !handle->isNeroCapable ) {
		errRender(error, "flPlaySVF(): This device does not support NeroJTAG");
		FAIL(FL_PROTOCOL_ERR);
	}
	bStatus = bufInitialise(&csvfBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, "flPlaySVF()", FL_ALLOC_ERR);
	if ( strcmp(".svf", ext+1) == 0 ) {
		fStatus = flLoadSvfAndConvertToCsvf(svfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, "flPlaySVF()", fStatus);
		isCompressed = false;
	} else if ( strcmp(".xsvf", ext) == 0 ) {
		fStatus = flLoadXsvfAndConvertToCsvf(svfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, "flPlaySVF()", fStatus);
		isCompressed = false;
	} else if ( strcmp(".csvf", ext) == 0 ) {
		bStatus = bufAppendFromBinaryFile(&csvfBuf, svfFile, error);
		CHECK_STATUS(bStatus, "flPlaySVF()", FL_FILE_ERR);
		isCompressed = true;
	} else {
		errRender(error, "flPlaySVF(): Filename should have .svf, .xsvf or .csvf extension");
		FAIL(FL_FILE_ERR);
	}
	fStatus = flFifoMode(handle, false, error);
	CHECK_STATUS(fStatus, "flPlaySVF()", fStatus);
	nStatus = neroInitialise(handle, portConfig, error);
	CHECK_STATUS(nStatus, "flPlaySVF()", FL_JTAG_ERR);
	cStatus = csvfPlay(handle, csvfBuf.data, isCompressed, error);
	CHECK_STATUS(cStatus, "flPlaySVF()", FL_JTAG_ERR);
	fStatus = flFifoMode(handle, true, error);
	CHECK_STATUS(fStatus, "flPlaySVF()", fStatus);
	returnCode = FL_SUCCESS;
cleanup:
	nStatus = neroClose(handle, NULL);
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
DLLEXPORT(FLStatus) flScanChain(
	struct FLContext *handle, const char *portConfig,
	uint32 *numDevices, uint32 *deviceArray, uint32 arraySize,
	const char **error)
{
	FLStatus returnCode;
	NeroStatus nStatus;
	uint32 i = 0;
	union {
		uint32 idCode;
		uint8 bytes[4];
	} u;
	nStatus = neroInitialise(handle, portConfig, error);
	CHECK_STATUS(nStatus, "flScanChain()", FL_JTAG_ERR);
	nStatus = neroClockFSM(handle, 0x0000005F, 9, error);  // Reset TAP, goto Shift-DR
	CHECK_STATUS(nStatus, "flScanChain()", FL_JTAG_ERR);
	for ( ; ; ) {
		nStatus = neroShift(handle, 32, ZEROS, u.bytes, false, error);
		CHECK_STATUS(nStatus, "flScanChain()", FL_JTAG_ERR);
		if ( u.idCode == 0x00000000 || u.idCode == 0xFFFFFFFF ) {
			break;
		}
		if ( deviceArray && i < arraySize ) {
			deviceArray[i] = littleEndian32(u.idCode);
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
	nStatus = neroClose(handle, NULL);
	return returnCode;
}



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
#include <string.h>
#include <makestuff.h>
#include <libusbwrap.h>
#include <liberror.h>
#include "libfpgalink.h"
#include "private.h"
#include "csvfplay.h"
#include "vendorCommands.h"

// -------------------------------------------------------------------------------------------------
// Implementation of private functions
// -------------------------------------------------------------------------------------------------

// Kick off a shift operation on the micro. This will typically be followed by a bunch of sends and
// receives on EP1OUT & EP1IN. This operation specifies the operation (i.e one of four JTAG shift
// operations, parallel shift or serial shift. It also specifies a count, which is either a bit-
// count or a byte-count depending on the context.
//
// Called by:
//   jtagShift() -> beginShift()
//   flProgram() -> xProgram() -> fileWrite() -> dataWrite() -> beginShift()
//
static FLStatus beginShift(
	struct FLContext *handle, uint32 count, ProgOp progOp, uint8 mode, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	union {
		uint32 u32;
		uint8 bytes[4];
	} countUnion;
	countUnion.u32 = littleEndian32(count);
	uStatus = usbControlWrite(
		handle->device,
		CMD_JTAG_CLOCK_DATA,  // bRequest
		(uint8)mode,          // wValue
		(uint8)progOp,        // wIndex
	   countUnion.bytes,     // send count
		4,                    // wLength
		5000,                 // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_SHIFT, cleanup, "beginShift()");
cleanup:
	return retVal;
}

// Send a chunk of data to the micro on EP1OUT. The previous call to beginShift() specifies what the
// micro should actually do with the data.
//
// Called by:
//   jtagShift() -> doSend()
//   flProgram() -> xProgram() -> fileWrite() -> dataWrite() -> doSend()
//
static FLStatus doSend(
	struct FLContext *handle, const uint8 *sendPtr, uint16 chunkSize, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbBulkWrite(
		handle->device,
		handle->progOutEP,    // write to out endpoint
		sendPtr,              // write from send buffer
		chunkSize,            // write this many bytes
		5000,                 // timeout in milliseconds
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_SEND, cleanup, "doSend()");
cleanup:
	return retVal;
}

// Receive a chunk of data from the micro on EP1IN. The previous call to beginShift() specifies the
// source of the data.
//
// Called by:
//   jtagShift() -> doReceive()
//
static FLStatus doReceive(
	struct FLContext *handle, uint8 *receivePtr, uint16 chunkSize, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbBulkRead(
		handle->device,
		handle->progInEP,    // read from in endpoint
		receivePtr,          // read into the receive buffer
		chunkSize,           // read this many bytes
		5000,                // timeout in milliseconds
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_RECV, cleanup, "doReceive()");
cleanup:
	return retVal;
}

#define GET_CHAR(func) \
	ch = *ptr; \
	CHECK_STATUS( \
		ch == '\0', FL_CONF_FORMAT, cleanup, \
		func"(): Unexpected end of config string at char %d", ptr-portConfig)

#define EXPECT_CHAR(ex, func) \
	GET_CHAR(func) \
	CHECK_STATUS( \
		ch != ex, FL_CONF_FORMAT, cleanup, \
		func"(): Expecting "#ex" but found '%c' at char %d", ch, ptr-portConfig); \
	ptr++

#define GET_PORT(port, func) \
	GET_CHAR(func) \
	CHECK_STATUS( \
		ch < 'A' || ch > 'E', FL_CONF_FORMAT, cleanup, \
		func"(): Port '%c' is not valid at char %d", ch, ptr-portConfig); \
	port = (uint8)(ch - 'A'); \
	ptr++

#define GET_BIT(bit, func) \
	GET_CHAR(func) \
	CHECK_STATUS( \
		ch < '0' || ch > '7', FL_CONF_FORMAT, cleanup, \
		func"(): Bit '%c' is not valid at char %d", ch, ptr-portConfig); \
	bit = (uint8)(ch - '0'); \
	ptr++

#define GET_PAIR(port, bit, func) \
	GET_PORT(port, func); \
	GET_BIT(bit, func)

#define SET_BIT(port, bit, status, func) \
	CHECK_STATUS( \
		pinMap[port*8+bit] != UNUSED, FL_CONF_FORMAT, cleanup, \
		func"(): port '%c%d' is already used at char %d", port + 'A', bit, ptr-portConfig-2); \
	pinMap[port*8+bit] = status

typedef enum {UNUSED, HIGH, LOW, INPUT} PinState;

// The pinMap array describes the desired state (i.e whether high, low, input or unused) of each bit
// on all five ports. This function uses that information to deduce mask, ddr & port bytes for each
// of the five ports.
//
// Called by:
//   flProgram() -> xProgram() -> makeMasks()
//   jtagOpen() -> makeMasks()
//   flPortConfig() -> makeMasks()
//
static void makeMasks(const PinState *pinMap, uint8 *mask, uint8 *ddr, uint8 *port) {
	int i, j;
	for ( i = 0; i < 5; i++ ) {
		mask[i] = 0x00;
		ddr[i] = 0x00;
		port[i] = 0x00;
		for ( j = 0; j < 8; j++ ) {
			switch ( pinMap[i*8+j] ) {
			case HIGH:
				port[i] = (uint8)(port[i] | (1<<j)); // note no break
			case LOW:
				ddr[i] = (uint8)(ddr[i] | (1<<j)); // note no break
			case INPUT:
				mask[i] = (uint8)(mask[i] | (1<<j)); // note no break
			default:
				break;
			}
		}
	}
}

// This function parses a comma-separated list of ports with a suffix representing the desired state
// of the port, e.g "A0+,B5-,D7/" means "PA0 is an output driven high, PB5 is an output driven low,
// and PD7 is an input". The result is recorded in the pinMap array, which is assumed to be of
// length 5*8=40. The parse stops when it encounters something other than a comma separator, and the
// location of that character stored in *endPtr.
//
// Called by:
//   xProgram() -> populateMap()
//   flPortConfig() -> populateMap()
//
static FLStatus populateMap(const char *portConfig, const char *ptr, const char **endPtr, PinState *pinMap, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	uint8 thisPort, thisBit;
	char ch;
	do {
		GET_PAIR(thisPort, thisBit, "populateMap");
		GET_CHAR("populateMap");
		if ( ch == '+' ) {
			SET_BIT(thisPort, thisBit, HIGH, "populateMap");
		} else if ( ch == '-' ) {
			SET_BIT(thisPort, thisBit, LOW, "populateMap");
		} else if ( ch == '?' ) {
			SET_BIT(thisPort, thisBit, INPUT, "populateMap");
		} else {
			CHECK_STATUS(
				true, FL_CONF_FORMAT, cleanup,
				"populateMap(): Expecting '+', '-' or '?' at char %d", ptr-portConfig);
		}
		ptr++;
		ch = *ptr++;
	} while ( ch == ',' );
	if ( endPtr ) {
		*endPtr = ptr - 1;
	}
cleanup:
	return retVal;
}

// This function re-maps the port used by the micro for one of the five PatchOp operations, which
// are currently the four JTAG pins TDO, TDI, TMS & TCK plus the parallel programming port. For
// serial programming we re-use the TDI bit for DIN and the TCK bit for CCLK.
//
// Called by:
//   xProgram() -> portMap()
//   jtagOpen() -> portMap()
//
static FLStatus portMap(struct FLContext *handle, PatchOp patchOp, uint8 port, uint8 bit, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	union {
		uint16 word;
		uint8 bytes[2];
	} index, value;
	USBStatus uStatus;
	index.bytes[0] = (uint8)patchOp;
	index.bytes[1] = port;
	value.bytes[0] = bit;
	value.bytes[1] = 0x00;
	uStatus = usbControlWrite(
		handle->device,
		CMD_PORT_MAP,      // bRequest
		value.word,        // wValue
		index.word,        // wIndex
		NULL,              // no data
		0,                 // wLength
		1000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_PORTMAP, cleanup, "portMap()");
cleanup:
	return retVal;
}

// The bits in each byte of a programming file may need to be swapped before sending to the micro;
// this function makes a translation map for efficient bit-swapping. If bitOrder = {0,1,2,3,4,5,6,7}
// then the resulting translation map does nothing. If bitOrder = {7,6,5,4,3,2,1,0} then the
// resulting translation map mirrors the bits.
//
// Called by:
//   xProgram() -> makeLookup()
//
static void makeLookup(const uint8 bitOrder[8], uint8 lookupTable[256]) {
	uint8 thisByte;
	uint16 i;
	for ( i = 0; i < 256; i++ ) {
		thisByte = 0x00;
		if ( i & 0x80 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[7])); }
		if ( i & 0x40 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[6])); }
		if ( i & 0x20 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[5])); }
		if ( i & 0x10 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[4])); }
		if ( i & 0x08 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[3])); }
		if ( i & 0x04 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[2])); }
		if ( i & 0x02 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[1])); }
		if ( i & 0x01 ) { thisByte = (uint8)(thisByte | (1 << bitOrder[0])); }
		lookupTable[i] = thisByte;
	}
}	

// For serial & parallel programming, when the FPGA is ready to accept data, this function sends it,
// one 64-byte block at a time, with a bit-transformation applied to each block.
//
// Called by:
//   xProgram() -> fileWrite() -> dataWrite()
//   xProgram() -> dataWrite()
//
static FLStatus dataWrite(struct FLContext *handle, ProgOp progOp, const uint8 *buf, uint32 len, const uint8 lookupTable[256], const char **error) {
	FLStatus retVal = FL_SUCCESS;
	uint16 chunkSize;
	uint8 bitSwap[64];
	uint16 i;
	FLStatus fStatus = beginShift(handle, len, progOp, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "dataWrite()");
	while ( len ) {
		chunkSize = (uint16)((len >= 64) ? 64 : len);
		for ( i = 0; i < chunkSize; i++ ) {
			bitSwap[i] = lookupTable[buf[i]];
		}
		fStatus = doSend(handle, bitSwap, chunkSize, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "dataWrite()");
		buf += chunkSize;
		len -= chunkSize;
	}
cleanup:
	return retVal;
}

// This function just loads binary data from the specified file and sends it to the micro.
//
// Called by:
//   xProgram() -> fileWrite()
//
static FLStatus fileWrite(struct FLContext *handle, ProgOp progOp, const char *fileName, const uint8 lookupTable[256], const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint32 fileLen;
	uint8 *fileData = flLoadFile(fileName, &fileLen);
	CHECK_STATUS(
		!fileData, FL_JTAG_ERR, cleanup,
		"fileWrite(): Unable to read from %s", fileName);
	fStatus = dataWrite(handle, progOp, fileData, fileLen, lookupTable, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "fileWrite()");
cleanup:
	if ( fileData ) {
		flFreeFile(fileData);
	}
	return retVal;
}

// This function performs either a serial or a parallel programming operation on Xilinx FPGAs.
//
// Called by:
//   flProgram() -> xProgram()
//
static FLStatus xProgram(struct FLContext *handle, ProgOp progOp, const char *portConfig, const char *progFile, const char **error) {
	FLStatus retVal = FL_SUCCESS;
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
	PinState pinMap[5*8] = {0,};
	const uint8 zeroBlock[64] = {0,};
	uint8 lookupTable[256];
	int i;
	char ch;
	CHECK_STATUS(
		progOp != PROG_PARALLEL && progOp != PROG_SERIAL, FL_INTERNAL_ERR, cleanup,
		"xProgram(): unsupported ProgOp");
	EXPECT_CHAR(':', "xProgram");
	GET_PAIR(progPort, progBit, "xProgram");
	progMask = (uint8)(1 << progBit);
	SET_BIT(progPort, progBit, LOW, "xProgram");
	GET_PAIR(initPort, initBit, "xProgram");
	initMask = (uint8)(1 << initBit);
	SET_BIT(initPort, initBit, INPUT, "xProgram");
	GET_PAIR(donePort, doneBit, "xProgram");
	doneMask = (uint8)(1 << doneBit);
	SET_BIT(donePort, doneBit, INPUT, "xProgram");
	GET_PAIR(cclkPort, cclkBit, "xProgram");
	SET_BIT(cclkPort, cclkBit, LOW, "xProgram");
	CHECK_STATUS(
		progPort != initPort || progPort != donePort, FL_CONF_FORMAT, cleanup,
		"xProgram(): For now, PROG, INIT & DONE must be on the same port");
	GET_PORT(dataPort, "xProgram");

	if ( progOp == PROG_PARALLEL ) {
		for ( i = 0; i < 8; i++ ) {
			GET_BIT(dataBit[i], "xProgram");
			SET_BIT(dataPort, dataBit[i], LOW, "xProgram");
		}
		makeLookup(dataBit, lookupTable);
	} else if ( progOp == PROG_SERIAL ) {
		const uint8 bitOrder[8] = {7,6,5,4,3,2,1,0};
		makeLookup(bitOrder, lookupTable);
		GET_BIT(dataBit[0], "xProgram");
		SET_BIT(dataPort, dataBit[0], LOW, "xProgram");
	}

	EXPECT_CHAR(':', "xProgram");
	GET_CHAR("xProgram");
	if ( ch == '[' ) {
		ptr++;
		fStatus = populateMap(portConfig, ptr, &ptr, pinMap, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		EXPECT_CHAR(']', "xProgram");
	}
	ch = *ptr;
	if ( ch == ':' ) {
		ptr++;
		CHECK_STATUS(
			progFile, FL_CONF_FORMAT, cleanup,
			"xProgram(): Config includes a filename, but a filename is already provided");
		progFile = ptr;
	} else if ( ch != '\0' ) {
		CHECK_STATUS(
			true, FL_CONF_FORMAT, cleanup,
			"xProgram(): Expecting ':' or end-of-string, not '%c' at char %d", ch, ptr-portConfig);
	} else if ( !progFile ) {
		CHECK_STATUS(
			true, FL_CONF_FORMAT, cleanup,
			"xProgram(): No filename given");
	}

	makeMasks(pinMap, maskList, ddrList, portList);

	// Map the CCLK bit & the SelectMAP data bus
	fStatus = portMap(handle, PATCH_TCK, cclkPort, cclkBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	if ( progOp == PROG_PARALLEL ) {
		fStatus = portMap(handle, PATCH_D8, dataPort, 0x00, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	} else if ( progOp == PROG_SERIAL ) {
		fStatus = portMap(handle, PATCH_TDI, dataPort, dataBit[0], error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	}

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
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	} while ( tempByte & (initMask | doneMask) );

	fStatus = flFifoMode(handle, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");

	for ( i = 0; i < 5; i++ ) {
		mask = maskList[i];
		ddr = ddrList[i];
		port = portList[i];
		if ( i == progPort ) {
			mask = (uint8)(mask & ~(progMask | initMask | doneMask));
			ddr = (uint8)(ddr & ~(progMask | initMask | doneMask));
			port = (uint8)(port & ~(progMask | initMask | doneMask));
		}
		if ( mask ) {
			fStatus = flPortAccess(
				handle, (uint8)i,
				mask, ddr, port,
				NULL,
				error
			);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		}
	}

	do {
		fStatus = flPortAccess(
			handle, progPort,  // deassert PROG
			progMask,               // mask: affect only PROG
			progMask,               // ddr: PROG is output
			progMask,               // port: drive PROG high (deasserted)
			&tempByte,
			error
		);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	} while ( !(tempByte & initMask) );

	fStatus = fileWrite(handle, progOp, progFile, lookupTable, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");

	i = 0;
	for ( ; ; ) {
		fStatus = flPortAccess(
			handle, progPort,  // check INIT & DONE
			0x00,              // mask: affect nothing
			0x00,              // ddr: ignored
			0x00,              // port: ignored
			&tempByte,
			error
		);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		if ( tempByte & doneMask ) {
			// If DONE goes high, we've finished.
			break;
		} else if ( tempByte & initMask ) {
			// If DONE remains low and INIT remains high, we probably just need more clocks
			i++;
			CHECK_STATUS(i == 10, FL_JTAG_ERR, cleanup, "xProgram(): DONE did not assert");
			fStatus = dataWrite(handle, progOp, zeroBlock, 64, lookupTable, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		} else {
			// If DONE remains low and INIT goes low, an error occurred
			CHECK_STATUS(
				true, FL_JTAG_ERR, cleanup,
				"xProgram(): INIT unexpectedly low (CRC error during config)");
		}
	}

	for ( i = 0; i < 5; i++ ) {
		mask = maskList[i];
		if ( mask ) {
			fStatus = flPortAccess(
				handle, (uint8)i,
				mask,
				(uint8)((i == progPort) ? progMask : 0x00),
				0xFF,
				NULL,
				error
			);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		}
	}

cleanup:
	return retVal;
}

// This function plays the specified SVF, XSVF or CSVF file into the attached FPGA.
//
// Called by:
//   flProgram() -> jProgram() -> playSVF()
//
static FLStatus playSVF(struct FLContext *handle, const char *svfFile, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	struct Buffer csvfBuf = {0,};
	BufferStatus bStatus;
	uint32 maxBufSize;
	const char *const ext = svfFile + strlen(svfFile) - 5;
	CHECK_STATUS(
		!handle->isNeroCapable, FL_PROTOCOL_ERR, cleanup,
		"playSVF(): This device does not support NeroJTAG");
	bStatus = bufInitialise(&csvfBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "playSVF()");
	if ( strcmp(".svf", ext+1) == 0 ) {
		fStatus = flLoadSvfAndConvertToCsvf(svfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "playSVF()");
	} else if ( strcmp(".xsvf", ext) == 0 ) {
		fStatus = flLoadXsvfAndConvertToCsvf(svfFile, &csvfBuf, &maxBufSize, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "playSVF()");
	} else if ( strcmp(".csvf", ext) == 0 ) {
		bStatus = bufAppendFromBinaryFile(&csvfBuf, svfFile, error);
		CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "playSVF()");
	} else {
		CHECK_STATUS(
			true, FL_FILE_ERR, cleanup,
			"playSVF(): Filename should have .svf, .xsvf or .csvf extension");
	}
	fStatus = csvfPlay(handle, csvfBuf.data, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "playSVF()");
cleanup:
	bufDestroy(&csvfBuf);
	return retVal;
}

// Program a device over JTAG.
//
// Called by:
//   flProgram() -> jProgram()
//
static FLStatus jProgram(struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	const char *ptr = portConfig + 1;
	char ch;
	EXPECT_CHAR(':', "jProgram");
	fStatus = jtagOpen(handle, portConfig, (uint32)(ptr - portConfig), error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jProgram()");

	ptr += 8;
	ch = *ptr;
	if ( ch == ':' ) {
		ptr++;
		CHECK_STATUS(
			progFile, FL_CONF_FORMAT, cleanup,
			"jProgram(): Config includes a filename, but a filename is already provided");
		progFile = ptr;
	} else if ( ch != '\0' ) {
		CHECK_STATUS(
			true, FL_CONF_FORMAT, cleanup,
			"jProgram(): Expecting ':' or end-of-string, not '%c' at char %d", ch, ptr-portConfig);
	} else if ( !progFile ) {
		CHECK_STATUS(
			true, FL_CONF_FORMAT, cleanup,
			"jProgram(): No filename given");
	}

	fStatus = playSVF(handle, progFile, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jProgram()");

	fStatus = jtagClose(handle, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jProgram()");

cleanup:
	return retVal;
}

// Reverse the array in-place by swapping the outer items and progressing inward until we meet in
// the middle.
//
// Called by:
//   jtagScanChain()
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

// -------------------------------------------------------------------------------------------------
// Implementation of public functions
// -------------------------------------------------------------------------------------------------

DLLEXPORT(FLStatus) jtagOpen(struct FLContext *handle, const char *portConfig, uint32 index, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint8 thisPort, thisBit, mask;
	PinState pinMap[5*8] = {0,};
	const char *ptr = portConfig + index;
	char ch;
	int i;

	GET_PAIR(thisPort, thisBit, "jtagOpen");        // TDO
	SET_BIT(thisPort, thisBit, INPUT, "jtagOpen");
	fStatus = portMap(handle, PATCH_TDO, thisPort, thisBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagOpen()");

	GET_PAIR(thisPort, thisBit, "jtagOpen");        // TDI
	SET_BIT(thisPort, thisBit, LOW, "jtagOpen");
	fStatus = portMap(handle, PATCH_TDI, thisPort, thisBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagOpen()");

	GET_PAIR(thisPort, thisBit, "jtagOpen");        // TMS
	SET_BIT(thisPort, thisBit, LOW, "jtagOpen");
	fStatus = portMap(handle, PATCH_TMS, thisPort, thisBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagOpen()");

	GET_PAIR(thisPort, thisBit, "jtagOpen");        // TCK
	SET_BIT(thisPort, thisBit, LOW, "jtagOpen");
	fStatus = portMap(handle, PATCH_TCK, thisPort, thisBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagOpen()");

	makeMasks(pinMap, handle->maskList, handle->ddrList, handle->portList);

	for ( i = 0; i < 5; i++ ) {
		mask = handle->maskList[i];
		if ( mask ) {
			fStatus = flPortAccess(
				handle, (uint8)i,
				mask, handle->ddrList[i], handle->portList[i],
				NULL,
				error
			);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagOpen()");
		}
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) jtagClose(struct FLContext *handle, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint8 mask;
	int i;
	for ( i = 0; i < 5; i++ ) {
		mask = handle->maskList[i];
		if ( mask ) {
			fStatus = flPortAccess(
				handle, (uint8)i,
				mask,
				0x00,
				0x00,
				NULL,
				error
			);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagClose()");
		}
	}
cleanup:
	return retVal;
}

// Shift data into and out of JTAG chain.
//   In pointer may be ZEROS (shift in zeros) or ONES (shift in ones).
//   Out pointer may be NULL (not interested in data shifted out of the chain).
//
DLLEXPORT(FLStatus) jtagShift(
	struct FLContext *handle, uint32 numBits, const uint8 *inData, uint8 *outData, bool isLast,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	uint32 numBytes = bitsToBytes(numBits);
	uint16 chunkSize;
	uint8 mode = 0x00;
	bool isSending = false;

	if ( inData == ONES ) {
		mode |= bmSENDONES;
	} else if ( inData != ZEROS ) {
		isSending = true;
	}
	if ( isLast ) {
		mode |= bmISLAST;
	}
	if ( isSending ) {
		if ( outData ) {
			fStatus = beginShift(handle, numBits, PROG_JTAG_ISSENDING_ISRECEIVING, mode, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
			while ( numBytes ) {
				chunkSize = (uint16)((numBytes >= 64) ? 64 : numBytes);
				fStatus = doSend(handle, inData, chunkSize, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
				inData += chunkSize;
				fStatus = doReceive(handle, outData, chunkSize, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
				outData += chunkSize;
				numBytes -= chunkSize;
			}
		} else {
			fStatus = beginShift(handle, numBits, PROG_JTAG_ISSENDING_NOTRECEIVING, mode, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
			while ( numBytes ) {
				chunkSize = (uint16)((numBytes >= 64) ? 64 : numBytes);
				fStatus = doSend(handle, inData, chunkSize, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
				inData += chunkSize;
				numBytes -= chunkSize;
			}
		}
	} else {
		if ( outData ) {
			fStatus = beginShift(handle, numBits, PROG_JTAG_NOTSENDING_ISRECEIVING, mode, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
			while ( numBytes ) {
				chunkSize = (uint16)((numBytes >= 64) ? 64 : numBytes);
				fStatus = doReceive(handle, outData, chunkSize, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
				outData += chunkSize;
				numBytes -= chunkSize;
			}
		} else {
			fStatus = beginShift(handle, numBits, PROG_JTAG_NOTSENDING_NOTRECEIVING, mode, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShift()");
		}
	}
cleanup:
	return retVal;
}

// Apply the supplied bit pattern to TMS, to move the TAP to a specific state.
//
DLLEXPORT(FLStatus) jtagClockFSM(
	struct FLContext *handle, uint32 bitPattern, uint8 transitionCount, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	union {
		uint32 u32;
		uint8 bytes[4];
	} lePattern;
	lePattern.u32 = littleEndian32(bitPattern);
	uStatus = usbControlWrite(
		handle->device,
		CMD_JTAG_CLOCK_FSM,       // bRequest
		(uint16)transitionCount,  // wValue
		0x0000,                   // wIndex
		lePattern.bytes,          // bit pattern
		4,                        // wLength
		5000,                     // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_JTAG_FSM, cleanup, "jtagClockFSM()");
cleanup:
	return retVal;
}

// Cycle the TCK line for the given number of times.
//
DLLEXPORT(FLStatus) jtagClocks(struct FLContext *handle, uint32 numClocks, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbControlWrite(
		handle->device,
		CMD_JTAG_CLOCK,                // bRequest
		(uint16)(numClocks & 0xFFFF),  // wValue
		(uint16)(numClocks >> 16),     // wIndex
		NULL,                          // no data
		0,                             // wLength
		5000,                          // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_CLOCKS, cleanup, "jtagClocks()");
cleanup:
	return retVal;
}

// Scan the JTAG chain and return an array of IDCODEs
//
DLLEXPORT(FLStatus) jtagScanChain(
	struct FLContext *handle, const char *portConfig,
	uint32 *numDevices, uint32 *deviceArray, uint32 arraySize,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint32 i = 0;
	union {
		uint32 idCode;
		uint8 bytes[4];
	} u;
	fStatus = jtagOpen(handle, portConfig, 0, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");

	i = 0;
	fStatus = jtagClockFSM(handle, 0x0000005F, 9, error);  // Reset TAP, goto Shift-DR
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");
	for ( ; ; ) {
		fStatus = jtagShift(handle, 32, ZEROS, u.bytes, false, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");
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

	fStatus = jtagClose(handle, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");

cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flProgram(struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	const char algoVendor = portConfig[0];
	if ( algoVendor == 'X' ) {
		// This is a Xilinx algorithm
		const char algoType = portConfig[1];
		if ( algoType == 'P' ) {
			// This is Xilinx Slave Parallel ("SelectMAP")
			return xProgram(handle, PROG_PARALLEL, portConfig, progFile, error);
		} else if ( algoType == 'S' ) {
			// This is Xilinx Slave Serial
			return xProgram(handle, PROG_SERIAL, portConfig, progFile, error);
		} else if ( algoType == '\0' ) {
			CHECK_STATUS(true, FL_CONF_FORMAT, cleanup, "flProgram(): Missing Xilinx algorithm code");
		} else {
			// This is not a valid Xilinx algorithm
			CHECK_STATUS(
				true, FL_CONF_FORMAT, cleanup,
				"flProgram(): '%c' is not a valid Xilinx algorithm code", algoType);
		}
	} else if ( algoVendor == 'J' ) {
		// This is a JTAG algorithm
		return jProgram(handle, portConfig, progFile, error);
	} else if ( algoVendor == '\0' ) {
		CHECK_STATUS(true, FL_CONF_FORMAT, cleanup, "flProgram(): Missing algorithm vendor code");
	} else {
		CHECK_STATUS(
			true, FL_CONF_FORMAT, cleanup,
			"flProgram(): '%c' is not a valid algorithm vendor code", algoVendor);
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flPortConfig(struct FLContext *handle, const char *portConfig, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	PinState pinMap[5*8] = {0,};
	uint8 maskList[5], ddrList[5], portList[5], mask;
	int i;
	fStatus = populateMap(portConfig, portConfig, NULL, pinMap, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flPortConfig()");
	makeMasks(pinMap, maskList, ddrList, portList);
	for ( i = 0; i < 5; i++ ) {
		mask = maskList[i];
		if ( mask ) {
			fStatus = flPortAccess(
				handle, (uint8)i,
				mask, ddrList[i], portList[i],
				NULL,
				error
			);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flPortConfig()");
		}
	}
cleanup:
	return retVal;
}

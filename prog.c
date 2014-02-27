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
//   jtagShiftInOut() -> beginShift()
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
		CMD_PROG_CLOCK_DATA,  // bRequest
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
//   jtagShiftInOut() -> doSend()
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
//   jtagShiftInOut() -> doReceive()
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

static const char *spaces(ptrdiff_t n) {
	const char *const s =
		"                                                                "
		"                                                                "
		"                                                                "
		"                                                                ";
	return s + 256-n;
}

#define GET_CHAR(func) \
	ch = *ptr; \
	CHECK_STATUS( \
		ch == '\0', FL_CONF_FORMAT, cleanup, \
		func"(): Unexpected end of config string:\n  %s\n  %s^", portConfig, spaces(ptr-portConfig))

#define EXPECT_CHAR(ex, func) \
	GET_CHAR(func) \
	CHECK_STATUS( \
		ch != ex, FL_CONF_FORMAT, cleanup, \
		func"(): Expecting "#ex":\n  %s\n  %s^", portConfig, spaces(ptr-portConfig)); \
	ptr++

#define GET_PORT(port, func) \
	GET_CHAR(func) \
	CHECK_STATUS( \
		ch < 'A' || ch > 'Z', FL_CONF_FORMAT, cleanup, \
		func"(): Port is not valid (try 'A', 'B', 'C', etc):\n  %s\n  %s^", portConfig, spaces(ptr-portConfig)); \
	port = (uint8)(ch - 'A'); \
	ptr++

#define GET_BIT(bit, func) \
	GET_CHAR(func) \
	CHECK_STATUS( \
		ch < '0' || ch > '9', FL_CONF_FORMAT, cleanup, \
		func"(): Bit number is not valid:\n  %s\n  %s^", portConfig, spaces(ptr-portConfig)); \
	bit = (uint8)(strtoul(ptr, (char**)&ptr, 10));

#define GET_DIGIT(bit, func) \
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
		pinMap[port][bit] != PIN_UNUSED, FL_CONF_FORMAT, cleanup,					\
		func"(): port '%c%d' is already used:\n  %s\n  %s^", port+'A', bit, portConfig, spaces(ptr-portConfig-1)); \
	pinMap[port][bit] = status

// This function parses a comma-separated list of ports with a suffix representing the desired state
// of the port, e.g "A0+,B5-,D7/" means "PA0 is an output driven high, PB5 is an output driven low,
// and PD7 is an input". The result is recorded in the pinMap array, which is assumed to be of
// length 5*8=40. The parse stops when it encounters something other than a comma separator, and the
// location of that character stored in *endPtr.
//
// Called by:
//   xProgram() -> populateMap()
//
static FLStatus populateMap(
	const char *portConfig, const char *ptr, const char **endPtr,
	PinConfig pinMap[26][32], const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	uint8 thisPort, thisBit;
	char ch;
	do {
		GET_PAIR(thisPort, thisBit, "populateMap");
		GET_CHAR("populateMap");
		if ( ch == '+' ) {
			SET_BIT(thisPort, thisBit, PIN_HIGH, "populateMap");
		} else if ( ch == '-' ) {
			SET_BIT(thisPort, thisBit, PIN_LOW, "populateMap");
		} else if ( ch == '?' ) {
			SET_BIT(thisPort, thisBit, PIN_INPUT, "populateMap");
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

// This function re-maps the physical port used by the micro for its logical programming ports.
//
// Called by:
//   xProgram() -> portMap()
//   progOpen() -> portMap()
//
static FLStatus portMap(
	struct FLContext *handle, LogicalPort patchOp, uint8 port, uint8 bit,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	const uint16 index = (uint16)((port << 8) | patchOp);
	const uint16 value = (uint16)bit;
	uStatus = usbControlWrite(
		handle->device,
		CMD_PORT_MAP,  // bRequest
		value,         // wValue
		index,         // wIndex
		NULL,          // no data
		0,             // wLength
		1000,          // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_PORT_MAP, cleanup, "portMap()");
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
static FLStatus dataWrite(struct FLContext *handle, ProgOp progOp, const uint8 *buf, uint32 len, const uint8 *lookupTable, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	uint16 chunkSize;
	FLStatus fStatus = beginShift(handle, len, progOp, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "dataWrite()");
	if ( lookupTable ) {
		uint8 bitSwap[64];
		uint16 i;
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
	} else {
		while ( len ) {
			chunkSize = (uint16)((len >= 64) ? 64 : len);
			fStatus = doSend(handle, buf, chunkSize, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "dataWrite()");
			buf += chunkSize;
			len -= chunkSize;
		}
	}
cleanup:
	return retVal;
}

// This function performs either a serial or a parallel programming operation on Xilinx FPGAs.
//
// Called by:
//   flProgram() -> xProgram()
//
static FLStatus xProgram(struct FLContext *handle, ProgOp progOp, const char *portConfig, const uint8 *data, uint32 len, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint8 progPort, progBit;
	uint8 initPort, initBit;
	uint8 donePort, doneBit;
	uint8 cclkPort, cclkBit;
	uint8 dataPort, dataBit[8];
	uint8 port, bit;
	uint8 initStatus, doneStatus;
	const char *ptr = portConfig + 2;
	PinConfig pinMap[26][32] = {{0,},};
	PinConfig thisPin;
	const uint8 zeroBlock[64] = {0,};
	uint8 lookupTable[256];
	int i;
	char ch;
	CHECK_STATUS(
		progOp != PROG_PARALLEL && progOp != PROG_SPI_SEND, FL_CONF_FORMAT, cleanup,
		"xProgram(): unsupported ProgOp");
	EXPECT_CHAR(':', "xProgram");

	GET_PAIR(progPort, progBit, "xProgram");
	SET_BIT(progPort, progBit, PIN_LOW, "xProgram");

	GET_PAIR(initPort, initBit, "xProgram");
	SET_BIT(initPort, initBit, PIN_INPUT, "xProgram");

	GET_PAIR(donePort, doneBit, "xProgram");
	SET_BIT(donePort, doneBit, PIN_INPUT, "xProgram");

	GET_PAIR(cclkPort, cclkBit, "xProgram");
	SET_BIT(cclkPort, cclkBit, PIN_LOW, "xProgram");

	GET_PORT(dataPort, "xProgram");
	if ( progOp == PROG_PARALLEL ) {
		for ( i = 0; i < 8; i++ ) {
			GET_DIGIT(dataBit[i], "xProgram");
			SET_BIT(dataPort, dataBit[i], PIN_LOW, "xProgram");
		}
		makeLookup(dataBit, lookupTable);
	} else if ( progOp == PROG_SPI_SEND ) {
		const uint8 bitOrder[8] = {7,6,5,4,3,2,1,0};
		makeLookup(bitOrder, lookupTable);
		GET_BIT(dataBit[0], "xProgram");
		SET_BIT(dataPort, dataBit[0], PIN_LOW, "xProgram");
	}

	GET_CHAR("xProgram");
	if ( ch == '[' ) {
		ptr++;
		fStatus = populateMap(portConfig, ptr, &ptr, pinMap, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		EXPECT_CHAR(']', "xProgram");
	}
	ch = *ptr;
	CHECK_STATUS(
		ch != '\0' && ch != ':', FL_CONF_FORMAT, cleanup,
		"xProgram(): Expecting ':' or end-of-string:\n  %s\n  %s^", portConfig, spaces(ptr-portConfig));

	// Map the CCLK bit & the SelectMAP data bus
	fStatus = portMap(handle, LP_SCK, cclkPort, cclkBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	if ( progOp == PROG_PARALLEL ) {
		fStatus = portMap(handle, LP_D8, dataPort, 0x00, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	} else if ( progOp == PROG_SPI_SEND ) {
		fStatus = portMap(handle, LP_MOSI, dataPort, dataBit[0], error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	}
	fStatus = portMap(handle, LP_CHOOSE, 0x00, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");

	// Assert PROG & wait for INIT & DONE to go low
	fStatus = flSingleBitPortAccess(handle, initPort, initBit, PIN_INPUT, NULL, error); // INIT is input
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	fStatus = flSingleBitPortAccess(handle, donePort, doneBit, PIN_INPUT, NULL, error); // DONE is input
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	fStatus = flSingleBitPortAccess(handle, progPort, progBit, PIN_LOW, NULL, error); // PROG is low
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	do {
		fStatus = flSingleBitPortAccess(handle, initPort, initBit, PIN_INPUT, &initStatus, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		fStatus = flSingleBitPortAccess(handle, donePort, doneBit, PIN_INPUT, &doneStatus, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	} while ( initStatus || doneStatus );

	// Now it's safe to switch to conduit mode zero (=JTAG, etc)
	fStatus = flSelectConduit(handle, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");

	// Apply requested configuration to each specified pin
	pinMap[progPort][progBit] = PIN_UNUSED;
	pinMap[initPort][initBit] = PIN_UNUSED;
	pinMap[donePort][doneBit] = PIN_UNUSED;
	for ( port = 0; port < 26; port++ ) {
		for ( bit = 0; bit < 32; bit++ ) {
			thisPin = pinMap[port][bit];
			if ( thisPin != PIN_UNUSED ) {
				fStatus = flSingleBitPortAccess(handle, port, bit, thisPin, NULL, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
			}
		}
	}

	// Deassert PROG and wait for INIT to go high
	fStatus = flSingleBitPortAccess(handle, progPort, progBit, PIN_HIGH, NULL, error); // PROG is high
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	do {
		fStatus = flSingleBitPortAccess(handle, initPort, initBit, PIN_INPUT, &initStatus, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
	} while ( !initStatus );

	// Write the programming file into the FPGA
	fStatus = dataWrite(handle, progOp, data, len, lookupTable, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");

	i = 0;
	for ( ; ; ) {
		fStatus = flSingleBitPortAccess(handle, initPort, initBit, PIN_INPUT, &initStatus, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		fStatus = flSingleBitPortAccess(handle, donePort, doneBit, PIN_INPUT, &doneStatus, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		if ( doneStatus ) {
			// If DONE goes high, we've finished.
			break;
		} else if ( initStatus ) {
			// If DONE remains low and INIT remains high, we probably just need more clocks
			i++;
			CHECK_STATUS(i == 10, FL_PROG_ERR, cleanup, "xProgram(): DONE did not assert");
			fStatus = dataWrite(handle, progOp, zeroBlock, 64, lookupTable, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
		} else {
			// If DONE remains low and INIT goes low, an error occurred
			CHECK_STATUS(
				true, FL_PROG_ERR, cleanup,
				"xProgram(): INIT unexpectedly low (CRC error during config)");
		}
	}

	// Make all specified pins inputs; leave INIT & DONE as inputs and leave PROG driven high
	for ( port = 0; port < 26; port++ ) {
		for ( bit = 0; bit < 32; bit++ ) {
			thisPin = pinMap[port][bit];
			if ( thisPin != PIN_UNUSED ) {
				fStatus = flSingleBitPortAccess(handle, port, bit, PIN_INPUT, NULL, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "xProgram()");
			}
		}
	}
cleanup:
	return retVal;
}

// This function performs a "passive" serial programming operation on Altera FPGAs.
//
// Called by:
//   flProgramBlob() -> xProgram()
//
static FLStatus aProgram(struct FLContext *handle, const char *portConfig, const uint8 *data, uint32 len, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint8 ncfgPort, ncfgBit;
	uint8 donePort, doneBit;
	uint8 dclkPort, dclkBit;
	uint8 dataPort, dataBit;
	uint8 port, bit;
	uint8 doneStatus;
	const char *ptr = portConfig + 2;
	PinConfig pinMap[26][32] = {{0,},};
	PinConfig thisPin;
	uint8 lookupTable[256];
	const uint8 bitOrder[8] = {0,1,2,3,4,5,6,7};
	char ch;
	EXPECT_CHAR(':', "aProgram");

	GET_PAIR(ncfgPort, ncfgBit, "aProgram");
	SET_BIT(ncfgPort, ncfgBit, PIN_LOW, "aProgram");

	fStatus = flSingleBitPortAccess(handle, ncfgPort, ncfgBit, PIN_LOW, NULL, error); // nCONFIG is low
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");

	GET_PAIR(donePort, doneBit, "aProgram");
	SET_BIT(donePort, doneBit, PIN_INPUT, "aProgram");

	GET_PAIR(dclkPort, dclkBit, "aProgram");
	SET_BIT(dclkPort, dclkBit, PIN_LOW, "aProgram");

	GET_PAIR(dataPort, dataBit, "aProgram");
	SET_BIT(dataPort, dataBit, PIN_LOW, "aProgram");

	GET_CHAR("aProgram");
	if ( ch == '[' ) {
		ptr++;
		fStatus = populateMap(portConfig, ptr, &ptr, pinMap, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");
		EXPECT_CHAR(']', "aProgram");
	}
	ch = *ptr;
	CHECK_STATUS(
		ch != '\0' && ch != ':', FL_CONF_FORMAT, cleanup,
		"aProgram(): Expecting ':' or end-of-string:\n  %s\n  %s^", portConfig, spaces(ptr-portConfig));

	// Make a lookup table to swap the bits
	makeLookup(bitOrder, lookupTable);

	// Map DCLK & DATA0
	fStatus = portMap(handle, LP_SCK, dclkPort, dclkBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");
	fStatus = portMap(handle, LP_MOSI, dataPort, dataBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");
	fStatus = portMap(handle, LP_CHOOSE, 0x00, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");

	// Switch to conduit mode zero (=JTAG, etc)
	fStatus = flSelectConduit(handle, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");

	// Apply requested configuration to each specified pin
	pinMap[ncfgPort][ncfgBit] = PIN_UNUSED;
	for ( port = 0; port < 26; port++ ) {
		for ( bit = 0; bit < 32; bit++ ) {
			thisPin = pinMap[port][bit];
			if ( thisPin != PIN_UNUSED ) {
				fStatus = flSingleBitPortAccess(handle, port, bit, thisPin, NULL, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");
			}
		}
	}

	// Deassert nCONFIG
	fStatus = flSingleBitPortAccess(handle, ncfgPort, ncfgBit, PIN_INPUT, NULL, error); // nCONFIG pulled up
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");

	// Write the programming file into the FPGA
	fStatus = dataWrite(handle, PROG_SPI_SEND, data, len, lookupTable, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");

	// Verify that CONF_DONE went high
	fStatus = flSingleBitPortAccess(handle, donePort, doneBit, PIN_INPUT, &doneStatus, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");
	CHECK_STATUS(
		!doneStatus, FL_PROG_ERR, cleanup,
		"aProgram(): CONF_DONE remained low (CRC error during config)");

	// Make all specified pins inputs; leave CONF_DONE as input and leave nCONFIG driven high
	for ( port = 0; port < 26; port++ ) {
		for ( bit = 0; bit < 32; bit++ ) {
			thisPin = pinMap[port][bit];
			if ( thisPin != PIN_UNUSED ) {
				fStatus = flSingleBitPortAccess(handle, port, bit, PIN_INPUT, NULL, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "aProgram()");
			}
		}
	}
cleanup:
	return retVal;
}

static FLStatus progOpenInternal(struct FLContext *handle, const char *portConfig, const char *ptr, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint8 misoPort, misoBit;
	uint8 mosiPort, mosiBit;
	uint8 ssPort, ssBit;
	uint8 sckPort, sckBit;
	PinConfig pinMap[26][32] = {{0,},};
	char ch;

	// Get all four JTAG bits and tell the micro which ones to use
	GET_PAIR(misoPort, misoBit, "progOpen");        // MISO/TDO
	SET_BIT(misoPort, misoBit, PIN_INPUT, "progOpen");
	fStatus = portMap(handle, LP_MISO, misoPort, misoBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");

	GET_PAIR(mosiPort, mosiBit, "progOpen");        // MOSI/TDI
	SET_BIT(mosiPort, mosiBit, PIN_LOW, "progOpen");
	fStatus = portMap(handle, LP_MOSI, mosiPort, mosiBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");

	GET_PAIR(ssPort, ssBit, "progOpen");        // SS/TMS
	SET_BIT(ssPort, ssBit, PIN_LOW, "progOpen");
	fStatus = portMap(handle, LP_SS, ssPort, ssBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");

	GET_PAIR(sckPort, sckBit, "progOpen");        // SCK/TCK
	SET_BIT(sckPort, sckBit, PIN_LOW, "progOpen");
	fStatus = portMap(handle, LP_SCK, sckPort, sckBit, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");

	fStatus = portMap(handle, LP_CHOOSE, 0x00, 0x00, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");

	// Set MISO/TDO as an input and the other three as outputs
	fStatus = flSingleBitPortAccess(handle, misoPort, misoBit, PIN_INPUT, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");
	fStatus = flSingleBitPortAccess(handle, mosiPort, mosiBit, PIN_LOW, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");
	fStatus = flSingleBitPortAccess(handle, ssPort, ssBit, PIN_LOW, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");
	fStatus = flSingleBitPortAccess(handle, sckPort, sckBit, PIN_LOW, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progOpen()");

	// Remember the ports and bits for the benefit of progClose()
	handle->misoPort = misoPort;
	handle->misoBit = misoBit;
	handle->mosiPort = mosiPort;
	handle->mosiBit = mosiBit;
	handle->ssPort = ssPort;
	handle->ssBit = ssBit;
	handle->sckPort = sckPort;
	handle->sckBit = sckBit;
cleanup:
	return retVal;
}

// Program a device over JTAG.
//
// Called by:
//   flProgram() -> jProgram()
//
static FLStatus jProgram(struct FLContext *handle, const char *portConfig, const uint8 *csvfData, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	const char *ptr = portConfig + 1;
	char ch;
	EXPECT_CHAR(':', "jProgram");
	fStatus = progOpenInternal(handle, portConfig, ptr, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jProgram()");
	fStatus = csvfPlay(handle, csvfData, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jProgram()");
	fStatus = progClose(handle, error);
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

DLLEXPORT(FLStatus) progOpen(struct FLContext *handle, const char *portConfig, const char **error) {
	return progOpenInternal(handle, portConfig, portConfig, error);
}

DLLEXPORT(FLStatus) progClose(struct FLContext *handle, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;

	// Set MISO/TDO, MOSI/TDI, SS/TMS & SCK/TCK as inputs
	fStatus = flSingleBitPortAccess(handle, handle->misoPort, handle->misoBit, PIN_INPUT, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progClose()");
	fStatus = flSingleBitPortAccess(handle, handle->mosiPort, handle->mosiBit, PIN_INPUT, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progClose()");
	fStatus = flSingleBitPortAccess(handle, handle->ssPort, handle->ssBit, PIN_INPUT, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progClose()");
	fStatus = flSingleBitPortAccess(handle, handle->sckPort, handle->sckBit, PIN_INPUT, NULL, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "progClose()");
cleanup:
	return retVal;
}

// Shift data into and out of JTAG chain.
//   In pointer may be SHIFT_ZEROS (shift in zeros) or SHIFT_ONES (shift in ones).
//   Out pointer may be NULL (not interested in data shifted out of the chain).
//
DLLEXPORT(FLStatus) jtagShiftInOut(
	struct FLContext *handle, uint32 numBits, const uint8 *inData, uint8 *outData, uint8 isLast,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	uint32 numBytes = bitsToBytes(numBits);
	uint16 chunkSize;
	uint8 mode = 0x00;
	bool isSending = false;

	if ( inData == SHIFT_ONES ) {
		mode |= bmSENDONES;
	} else if ( inData != SHIFT_ZEROS ) {
		isSending = true;
	}
	if ( isLast ) {
		mode |= bmISLAST;
	}
	if ( isSending ) {
		fStatus = beginShift(handle, numBits, PROG_JTAG_ISSENDING_ISRECEIVING, mode, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
		while ( numBytes ) {
			chunkSize = (uint16)((numBytes >= 64) ? 64 : numBytes);
			fStatus = doSend(handle, inData, chunkSize, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
			inData += chunkSize;
			fStatus = doReceive(handle, outData, chunkSize, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
			outData += chunkSize;
			numBytes -= chunkSize;
		}
	} else {
		fStatus = beginShift(handle, numBits, PROG_JTAG_NOTSENDING_ISRECEIVING, mode, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
		while ( numBytes ) {
			chunkSize = (uint16)((numBytes >= 64) ? 64 : numBytes);
			fStatus = doReceive(handle, outData, chunkSize, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
			outData += chunkSize;
			numBytes -= chunkSize;
		}
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) jtagShiftInOnly(
	struct FLContext *handle, uint32 numBits, const uint8 *inData, uint8 isLast,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	uint32 numBytes = bitsToBytes(numBits);
	uint16 chunkSize;
	uint8 mode = 0x00;
	bool isSending = false;
	if ( inData == SHIFT_ONES ) {
		mode |= bmSENDONES;
	} else if ( inData != SHIFT_ZEROS ) {
		isSending = true;
	}
	if ( isLast ) {
		mode |= bmISLAST;
	}
	if ( isSending ) {
		fStatus = beginShift(handle, numBits, PROG_JTAG_ISSENDING_NOTRECEIVING, mode, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
		while ( numBytes ) {
			chunkSize = (uint16)((numBytes >= 64) ? 64 : numBytes);
			fStatus = doSend(handle, inData, chunkSize, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
			inData += chunkSize;
			numBytes -= chunkSize;
		}
	} else {
		fStatus = beginShift(handle, numBits, PROG_JTAG_NOTSENDING_NOTRECEIVING, mode, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "jtagShiftInOut()");
	}
cleanup:
	return retVal;
}

// Apply the supplied bit pattern to SS/TMS, to move the TAP to a specific state.
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

// Cycle the SCK/TCK line for the given number of times.
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
		60000,                         // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROG_JTAG_CLOCKS, cleanup, "jtagClocks()");
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
	fStatus = progOpenInternal(handle, portConfig, portConfig, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");

	i = 0;
	fStatus = jtagClockFSM(handle, 0x0000005F, 9, error);  // Reset TAP, goto Shift-DR
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");
	for ( ; ; ) {
		fStatus = jtagShiftInOut(handle, 32, SHIFT_ZEROS, u.bytes, false, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");
		if ( u.idCode == 0x00000000 || u.idCode == U32MAX ) {
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

	fStatus = progClose(handle, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "jtagScanChain()");

cleanup:
	return retVal;
}

// Programs a device using in-memory configuration information
//
DLLEXPORT(FLStatus) flProgramBlob(
	struct FLContext *handle, const char *portConfig, uint32 blobLength, const uint8 *blobData,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	const char algoVendor = portConfig[0];
	if ( algoVendor == 'X' ) {
		// This is a Xilinx algorithm
		const char algoType = portConfig[1];
		if ( algoType == 'P' ) {
			// This is Xilinx Slave Parallel ("SelectMAP")
			return xProgram(handle, PROG_PARALLEL, portConfig, blobData, blobLength, error);
		} else if ( algoType == 'S' ) {
			// This is Xilinx Slave Serial
			return xProgram(handle, PROG_SPI_SEND, portConfig, blobData, blobLength, error);
		} else if ( algoType == '\0' ) {
			CHECK_STATUS(true, FL_CONF_FORMAT, cleanup, "flProgram(): Missing Xilinx algorithm code");
		} else {
			CHECK_STATUS(
				true, FL_CONF_FORMAT, cleanup,
				"flProgram(): '%c' is not a valid Xilinx algorithm code", algoType);
		}
	} else if ( algoVendor == 'A' ) {
		// This is an Altera algorithm
		const char algoType = portConfig[1];
		if ( algoType == 'S' ) {
			// This is Altera Passive Serial
			return aProgram(handle, portConfig, blobData, blobLength, error);
		} else if ( algoType == '\0' ) {
			CHECK_STATUS(true, FL_CONF_FORMAT, cleanup, "flProgram(): Missing Altera algorithm code");
		} else {
			CHECK_STATUS(
				true, FL_CONF_FORMAT, cleanup,
				"flProgram(): '%c' is not a valid Altera algorithm code", algoType);
		}
	} else if ( algoVendor == 'J' ) {
		// This is a JTAG algorithm
		return jProgram(handle, portConfig, blobData, error);
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
	
// Programs a device using configuration information loaded from a file. If progFile is NULL,
// it expects to find a filename at the end of portConfig.
//
DLLEXPORT(FLStatus) flProgram(
	struct FLContext *handle, const char *portConfig, const char *progFile, const char **error) {
	FLStatus retVal = FL_SUCCESS, fStatus;
	const char algoVendor = portConfig[0];
	struct Buffer fileBuf = {0,};
	BufferStatus bStatus = bufInitialise(&fileBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "playSVF()");
	if ( progFile == NULL ) {
		// Expect to find prog file at the end of portConfig
		progFile = portConfig;
		while ( *progFile && *progFile != ':' ) {
			progFile++;
		}
		CHECK_STATUS(
			*progFile == '\0', FL_CONF_FORMAT, cleanup,
			"flProgram(): portConfig terminated before first ':'");
		progFile++;
		while ( *progFile && *progFile != ':' ) {
			progFile++;
		}
		CHECK_STATUS(
			*progFile == '\0', FL_CONF_FORMAT, cleanup,
			"flProgram(): progFile was NULL, and portConfig didn't specify a file");
		progFile++;
	}
	if ( algoVendor == 'J' ) {
		// JTAG file
		const char *const ext = progFile + strlen(progFile) - 5;
		if ( strcmp(".svf", ext+1) == 0 ) {
			fStatus = flLoadSvfAndConvertToCsvf(progFile, &fileBuf, NULL, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flProgram()");
		} else if ( strcmp(".xsvf", ext) == 0 ) {
			fStatus = flLoadXsvfAndConvertToCsvf(progFile, &fileBuf, NULL, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flProgram()");
		} else if ( strcmp(".csvf", ext) == 0 ) {
			bStatus = bufAppendFromBinaryFile(&fileBuf, progFile, error);
			CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flProgram()");
		} else {
			CHECK_STATUS(
				true, FL_FILE_ERR, cleanup,
				"flProgram(): JTAG files should have .svf, .xsvf or .csvf extension");
		}
	} else {
		// Just load it
		bStatus = bufAppendFromBinaryFile(&fileBuf, progFile, error);
		CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flProgram()");
	}
	fStatus = flProgramBlob(handle, portConfig, (uint32)fileBuf.length, fileBuf.data, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flProgram()");
cleanup:
	bufDestroy(&fileBuf);
	return retVal;
}

// Actual values to send to microcontroller for PIN_UNUSED, PIN_HIGH, PIN_LOW and PIN_INPUT:
static const uint16 indexValues[] = {0xFFFF, 0x0101, 0x0001, 0x0000};

DLLEXPORT(FLStatus) flSingleBitPortAccess(
	struct FLContext *handle, uint8 portNumber, uint8 bitNumber,
   uint8 pinConfig, uint8 *pinRead, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	uint8 byte;
	const uint16 value = (uint16)((bitNumber << 8) | portNumber);
	const uint16 index = indexValues[pinConfig];
	uStatus = usbControlRead(
		handle->device,
		CMD_PORT_BIT_IO, // bRequest
		value,           // wValue
		index,           // wIndex
		&byte,           // buffer to receive current state of ports
		1,               // wLength
		1000,            // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PORT_IO, cleanup, "flSingleBitPortAccess()");
	if ( pinRead ) {
		*pinRead = byte;
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flMultiBitPortAccess(
	struct FLContext *handle, const char *portConfig, uint32 *readState, const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	const char *ptr = portConfig;
	uint32 result = 0;
	uint8 thisPort, thisBit;
	char ch;
	PinConfig pinConfig;
	uint8 bitState;
	do {
		GET_PAIR(thisPort, thisBit, "flMultiBitPortAccess");
		GET_CHAR("flMultiBitPortAccess");
		if ( ch == '+' ) {
			pinConfig = PIN_HIGH;
		} else if ( ch == '-' ) {
			pinConfig = PIN_LOW;
		} else if ( ch == '?' ) {
			pinConfig = PIN_INPUT;
		} else {
			CHECK_STATUS(
				true, FL_CONF_FORMAT, cleanup,
				"flMultiBitPortAccess(): Expecting '+', '-' or '?':\n  %s\n  %s^", portConfig, spaces(ptr-portConfig));
		}
		fStatus = flSingleBitPortAccess(handle, thisPort, thisBit, pinConfig, &bitState, error);
		CHECK_STATUS(fStatus, fStatus, cleanup);
		result <<= 1;
		if ( bitState ) {
			result |= 1;
		}
		ptr++;
		ch = *ptr++;
	} while ( ch == ',' );
	CHECK_STATUS(
		ch != '\0', FL_CONF_FORMAT, cleanup,
		"flMultiBitPortAccess(): Expecting ',' or '\\0' here:\n  %s\n  %s^", portConfig, spaces(ptr-portConfig-1));
	if ( readState ) {
		*readState = result;
	}
cleanup:
	return retVal;
}

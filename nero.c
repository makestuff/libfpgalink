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
#include <makestuff.h>
#include <libusbwrap.h>
#include <liberror.h>
#include <string.h>
#include "vendorCommands.h"
#include "private.h"
#include "nero.h"
#include "prog.h"

// -------------------------------------------------------------------------------------------------
// Declaration of private types & functions
// -------------------------------------------------------------------------------------------------

NeroStatus beginShift(
	struct FLContext *handle, uint32 numBits, ProgOp progOp, uint8 mode, const char **error
) WARN_UNUSED_RESULT;

NeroStatus doSend(
	struct FLContext *handle, const uint8 *sendPtr, uint16 chunkSize, const char **error
) WARN_UNUSED_RESULT;

static NeroStatus doReceive(
	struct FLContext *handle, uint8 *receivePtr, uint16 chunkSize, const char **error
) WARN_UNUSED_RESULT;

//static NeroStatus setEndpointSize(
//	struct NeroContext *handle, const char **error
//) WARN_UNUSED_RESULT;

static NeroStatus setJtagMode(
	struct FLContext *handle, bool enable, const char **error
) WARN_UNUSED_RESULT;

//#define ORIG_PORTMAP

#ifdef ORIG_PORTMAP
static NeroStatus portMap(
	struct FLContext *handle, const uint8 *binConfig, const char **error
) WARN_UNUSED_RESULT;
#else
static NeroStatus portMap(
	struct FLContext *handle, uint8 patchClass, uint8 port, uint8 bit, const char **error
) WARN_UNUSED_RESULT;
#endif

// -------------------------------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------------------------------

// Find the NeroJTAG device, open it.
//
NeroStatus neroInitialise(
	struct FLContext *handle, const char *portConfig, const char **error)
{
	NeroStatus returnCode, nStatus;
	uint8 i;
	handle->endpointSize = 64;
	if ( portConfig != NULL ) {
		FLStatus fStatus;
		uint8 tdoPort;
		uint8 tdoBit;
		uint8 tdiPort;
		uint8 tdiBit;
		uint8 tmsPort;
		uint8 tmsBit;
		uint8 tckPort;
		uint8 tckBit;
		uint8 thisBit;
		uint8 binConfig[8];
		handle->usesCustomPorts = true;
		if (
			strlen(portConfig) != 8 ||
			portConfig[0] < 'A' || portConfig[0] > 'D' ||  // TDO port
			portConfig[1] < '0' || portConfig[1] > '7' ||  // TDO bit
			portConfig[2] < 'A' || portConfig[2] > 'D' ||  // TDI port
			portConfig[3] < '0' || portConfig[3] > '7' ||  // TDI bit
			portConfig[4] < 'A' || portConfig[4] > 'D' ||  // TMS port
			portConfig[5] < '0' || portConfig[5] > '7' ||  // TMS bit
			portConfig[6] < 'A' || portConfig[6] > 'D' ||  // TCK port
			portConfig[7] < '0' || portConfig[7] > '7'     // TCK bit
		) {
			errRender(error, "neroInitialise(): JTAG config should be of the form <tdoPort><tdoBit><tdiPort><tdiBit><tmsPort><tmsBit><tckPort><tckBit>, e.g A7A0A3A1");
			FAIL(NERO_PORTMAP);
		}
		binConfig[0] = tdoPort = portConfig[0] - 'A';
		binConfig[1] = tdoBit  = portConfig[1] - '0';
		binConfig[2] = tdiPort = portConfig[2] - 'A';
		binConfig[3] = tdiBit  = portConfig[3] - '0';
		binConfig[4] = tmsPort = portConfig[4] - 'A';
		binConfig[5] = tmsBit  = portConfig[5] - '0';
		binConfig[6] = tckPort = portConfig[6] - 'A';
		binConfig[7] = tckBit  = portConfig[7] - '0';
#ifdef ORIG_PORTMAP
		nStatus = portMap(handle, binConfig, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
#else
		nStatus = portMap(handle, 0, tdoPort, tdoBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
		nStatus = portMap(handle, 1, tdiPort, tdiBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
		nStatus = portMap(handle, 2, tmsPort, tmsBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
		nStatus = portMap(handle, 3, tckPort, tckBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
#endif
		for ( i = 0; i < 4; i++ ) {
			handle->masks[i] = 0x00;
			handle->ports[i] = 0x00;
		}
		
		// Do TDO - only set bit in mask because TDO is an input
		thisBit = (1 << tdoBit);
		handle->masks[tdoPort] |= thisBit;

		// Do TDI - first output
		thisBit = (1 << tdiBit);
		if ( handle->masks[tdiPort] & thisBit ) {
			errRender(error, "neroInitialise(): TDI bit already used");
			FAIL(NERO_PORTMAP);
		}
		handle->masks[tdiPort] |= thisBit;
		handle->ports[tdiPort] |= thisBit;
		
		// Do TMS - second output
		thisBit = (1 << tmsBit);
		if ( handle->masks[tmsPort] & thisBit ) {
			errRender(error, "neroInitialise(): TMS bit already used");
			FAIL(NERO_PORTMAP);
		}
		handle->masks[tmsPort] |= thisBit;
		handle->ports[tmsPort] |= thisBit;
		
		// Do TCK - third and final output
		thisBit = (1 << tckBit);
		if ( handle->masks[tckPort] & thisBit ) {
			errRender(error, "neroInitialise(): TCK bit already used");
			FAIL(NERO_PORTMAP);
		}
		handle->masks[tckPort] |= thisBit;
		handle->ports[tckPort] |= thisBit;
		
		// Now actually do the port config
		for ( i = 0; i < 4; i++ ) {
			if ( handle->masks[i] ) {
				fStatus = flPortAccess(
					handle, i, handle->masks[i], handle->ports[i], 0xFF, NULL, error);
				CHECK_STATUS(fStatus, "neroInitialise()", NERO_ENABLE);
			}
		}
	} else {
		handle->usesCustomPorts = false;
		nStatus = setJtagMode(handle, true, error);
		CHECK_STATUS(nStatus, "neroInitialise()", NERO_ENABLE);
	}
	return NERO_SUCCESS;
cleanup:
	for ( i = 0; i < 4; i++ ) {
		handle->masks[i] = 0x00;
		handle->ports[i] = 0x00;
	}
	handle->usesCustomPorts = false;
	return returnCode;
}

// Close the cable...drop the USB connection.
//
NeroStatus neroClose(struct FLContext *handle, const char **error) {
	NeroStatus returnCode, nStatus;
	FLStatus fStatus;
	uint8 i;
	if ( handle->usesCustomPorts ) {
		for ( i = 0; i < 4; i++ ) {
			if ( handle->masks[i] ) {
				fStatus = flPortAccess(
					handle, i, handle->masks[i], 0x00, 0xFF, NULL, error);
				CHECK_STATUS(fStatus, "neroClose()", NERO_ENABLE);
			}
		}
	} else {
		nStatus = setJtagMode(handle, false, error);
		CHECK_STATUS(nStatus, "neroClose()", NERO_ENABLE);
	}
	returnCode = NERO_SUCCESS;
cleanup:
	for ( i = 0; i < 4; i++ ) {
		handle->masks[i] = 0x00;
		handle->ports[i] = 0x00;
	}
	handle->usesCustomPorts = false;
	return returnCode;
}

// Shift data into and out of JTAG chain.
//   In pointer may be ZEROS (shift in zeros) or ONES (shift in ones).
//   Out pointer may be NULL (not interested in data shifted out of the chain).
//
NeroStatus neroShift(
	struct FLContext *handle, uint32 numBits, const uint8 *inData, uint8 *outData, bool isLast,
	const char **error)
{
	NeroStatus returnCode, nStatus;
	uint32 numBytes;
	uint16 chunkSize;
	ProgOp progOp;
	uint8 mode = 0x00;
	bool isSending = false;
	bool isReceiving = false;

	if ( inData == ONES ) {
		mode |= bmSENDONES;
	} else if ( inData != ZEROS ) {
		isSending = true;
	}
	if ( outData ) {
		isReceiving = true;
	}
	if ( isLast ) {
		mode |= bmISLAST;
	}
	if ( isSending ) {
		if ( isReceiving ) {
			progOp = PROG_JTAG_ISSENDING_ISRECEIVING;
		} else {
			progOp = PROG_JTAG_ISSENDING_NOTRECEIVING;
		}
	} else {
		if ( isReceiving ) {
			progOp = PROG_JTAG_NOTSENDING_ISRECEIVING;
		} else {
			progOp = PROG_JTAG_NOTSENDING_NOTRECEIVING;
		}
	}
	nStatus = beginShift(handle, numBits, progOp, mode, error);
	CHECK_STATUS(nStatus, "neroShift()", NERO_BEGIN_SHIFT);
	numBytes = bitsToBytes(numBits);
	while ( numBytes ) {
		chunkSize = (numBytes>=handle->endpointSize) ? handle->endpointSize : (uint16)numBytes;
		if ( isSending ) {
			nStatus = doSend(handle, inData, chunkSize, error);
			CHECK_STATUS(nStatus, "neroShift()", NERO_SEND);
			inData += chunkSize;
		}
		if ( isReceiving ) {
			nStatus = doReceive(handle, outData, chunkSize, error);
			CHECK_STATUS(nStatus, "neroShift()", NERO_RECEIVE);
			outData += chunkSize;
		}
		numBytes -= chunkSize;
	}
	return NERO_SUCCESS;
cleanup:
	return returnCode;
}

// Apply the supplied bit pattern to TMS, to move the TAP to a specific state.
//
NeroStatus neroClockFSM(
	struct FLContext *handle, uint32 bitPattern, uint8 transitionCount, const char **error)
{
	NeroStatus returnCode = NERO_SUCCESS;
	int uStatus;
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
	CHECK_STATUS(uStatus, "neroClockFSM()", NERO_CLOCKFSM);
cleanup:
	return returnCode;
}

// Cycle the TCK line for the given number of times.
//
NeroStatus neroClocks(struct FLContext *handle, uint32 numClocks, const char **error) {
	NeroStatus returnCode = NERO_SUCCESS;
	int uStatus = usbControlWrite(
		handle->device,
		CMD_JTAG_CLOCK,    // bRequest
		numClocks&0xFFFF,  // wValue
		numClocks>>16,     // wIndex
		NULL,              // no data
		0,                 // wLength
		5000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "neroClocks()", NERO_CLOCKS);
cleanup:
	return returnCode;
}

// -------------------------------------------------------------------------------------------------
// Implementation of private functions
// -------------------------------------------------------------------------------------------------
#ifdef ORIG_PORTMAP
static NeroStatus portMap(
	struct FLContext *handle, const uint8 *binConfig, const char **error)
{
	NeroStatus returnCode = NERO_SUCCESS;
	int uStatus = usbControlWrite(
		handle->device,
		CMD_PORT_MAP,      // bRequest
		0x0000,            // wValue
		0x0000,            // wIndex
		binConfig,         // no data
		8,                 // wLength
		1000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "portMap()", NERO_PORTMAP);
cleanup:
	return returnCode;
}
#else
static NeroStatus portMap(
	struct FLContext *handle, uint8 patchClass, uint8 port, uint8 bit, const char **error)
{
	NeroStatus returnCode = NERO_SUCCESS;
	union {
		uint16 word;
		uint8 bytes[2];
	} index, value;
	index.bytes[0] = patchClass;
	index.bytes[1] = port;
	value.bytes[0] = bit;
	value.bytes[1] = 0x00;
	int uStatus = usbControlWrite(
		handle->device,
		0x90,              // bRequest
		value.word,        // wValue
		index.word,        // wIndex
		NULL,              // no data
		0,                 // wLength
		1000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "portMap()", NERO_PORTMAP);
cleanup:
	return returnCode;
}
#endif

// Kick off a shift operation on the micro. This will be followed by a bunch of sends and receives.
//
NeroStatus beginShift(
	struct FLContext *handle, uint32 numBits, ProgOp progOp, uint8 mode, const char **error)
{
	NeroStatus returnCode = NERO_SUCCESS;
	int uStatus;
	union {
		uint32 u32;
		uint8 bytes[4];
	} leNumBits;
	leNumBits.u32 = littleEndian32(numBits);
	uStatus = usbControlWrite(
		handle->device,
		CMD_JTAG_CLOCK_DATA,  // bRequest
		(uint8)mode,          // wValue
		(uint8)progOp,        // wIndex
	   leNumBits.bytes,      // send bit count
		4,                    // wLength
		5000,                 // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "beginShift()", NERO_BEGIN_SHIFT);
cleanup:
	return returnCode;
}

// Send a chunk of data to the micro.
//
NeroStatus doSend(
	struct FLContext *handle, const uint8 *sendPtr, uint16 chunkSize, const char **error)
{
	NeroStatus returnCode = NERO_SUCCESS;
	int uStatus = usbBulkWrite(
		handle->device,
		handle->jtagOutEP,    // write to out endpoint
		sendPtr,              // write from send buffer
		chunkSize,            // write this many bytes
		5000,                 // timeout in milliseconds
		error
	);
	CHECK_STATUS(uStatus, "doSend()", NERO_SEND);
cleanup:
	return returnCode;
}

// Receive a chunk of data from the micro.
//
static NeroStatus doReceive(
	struct FLContext *handle, uint8 *receivePtr, uint16 chunkSize, const char **error)
{
	NeroStatus returnCode = NERO_SUCCESS;
	int uStatus = usbBulkRead(
		handle->device,
		handle->jtagInEP,    // read from in endpoint
		receivePtr,          // read into the receive buffer
		chunkSize,           // read this many bytes
		5000,                // timeout in milliseconds
		error
	);
	CHECK_STATUS(uStatus, "doReceive()", NERO_RECEIVE);
cleanup:
	return returnCode;
}

/*
// Find the size of the out & in bulk endpoints (they must be the same)
//
static NeroStatus setEndpointSize(struct NeroContext *handle, const char **error) {
	NeroStatus returnCode;
	int uStatus;
	char descriptorBuffer[1024];  // TODO: Fix by doing two queries
	char *ptr = descriptorBuffer;
	uint8 endpointNum;
	uint16 outEndpointSize = 0;
	uint16 inEndpointSize = 0;
	struct usb_config_descriptor *configDesc;
	struct usb_interface_descriptor *interfaceDesc;
	struct usb_endpoint_descriptor *endpointDesc;
	handle->endpointSize = 0;
	uStatus = usb_control_msg(
		handle->device,
		USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
		USB_REQ_GET_DESCRIPTOR,    // bRequest
		0x0200,                    // wValue
		0x0000,     // wIndex
		descriptorBuffer,
		1024,                 // wLength
		5000               // timeout (ms)
	);
	if ( uStatus < 0 ) {
		errRender(
			error, "setEndpointSize(): Failed to get config descriptor: %s (%d)", usb_strerror(), uStatus);
		FAIL(NERO_ENDPOINTS);
	}
	if ( uStatus > 0 ) {
		configDesc = (struct usb_config_descriptor *)ptr;
		ptr += configDesc->bLength;
		interfaceDesc = (struct usb_interface_descriptor *)ptr;
		ptr += interfaceDesc->bLength;			
		endpointNum = interfaceDesc->bNumEndpoints;
		while ( endpointNum-- ) {
			endpointDesc = (struct usb_endpoint_descriptor *)ptr;
			if ( endpointDesc-> bmAttributes == 0x02 ) {
				if ( endpointDesc->bEndpointAddress == handle->outEndpoint ) {
					outEndpointSize = littleEndian16(endpointDesc->wMaxPacketSize);
				} else if ( endpointDesc->bEndpointAddress == (handle->inEndpoint | 0x80) ) {
					inEndpointSize = littleEndian16(endpointDesc->wMaxPacketSize);
				}
			}
			ptr += endpointDesc->bLength;
		}
	}
	if ( !outEndpointSize ) {
		errRender(
			error, "setEndpointSize(): EP%dOUT not found or not configured as a bulk endpoint!",
			handle->outEndpoint
		);
		FAIL(NERO_ENDPOINTS);
	}
	if ( !inEndpointSize ) {
		errRender(
			error, "setEndpointSize(): EP%dIN not found or not configured as a bulk endpoint!",
			handle->inEndpoint
		);
		FAIL(NERO_ENDPOINTS);
	}
	if ( outEndpointSize != inEndpointSize ) {
		errRender(
			error, "setEndpointSize(): EP%dOUT's wMaxPacketSize differs from that of EP%dIN",
			handle->outEndpoint, handle->inEndpoint
		);
		FAIL(NERO_ENDPOINTS);
	}
	handle->endpointSize = outEndpointSize;
	return NERO_SUCCESS;
cleanup:
	return returnCode;
}
*/

// Put the device in jtag mode (i.e drive or tristate the JTAG lines)
//
static NeroStatus setJtagMode(struct FLContext *handle, bool enable, const char **error) {
	NeroStatus returnCode;
	int uStatus = usbControlWrite(
		handle->device,
		CMD_MODE_STATUS,         // bRequest
		enable ? MODE_JTAG : 0,  // wValue
		MODE_JTAG,               // wMask
		NULL,                    // no data
		0,                       // wLength
		5000,                    // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "setJtagMode()", NERO_ENABLE);
	return NERO_SUCCESS;
cleanup:
	return returnCode;
}

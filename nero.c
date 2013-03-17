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

static NeroStatus setJtagMode(
	struct FLContext *handle, bool enable, const char **error
) WARN_UNUSED_RESULT;

static NeroStatus portMap(
	struct FLContext *handle, uint8 patchClass, uint8 port, uint8 bit, const char **error
) WARN_UNUSED_RESULT;

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
		tdoPort = portConfig[0] - 'A';
		tdoBit  = portConfig[1] - '0';
		tdiPort = portConfig[2] - 'A';
		tdiBit  = portConfig[3] - '0';
		tmsPort = portConfig[4] - 'A';
		tmsBit  = portConfig[5] - '0';
		tckPort = portConfig[6] - 'A';
		tckBit  = portConfig[7] - '0';
		nStatus = portMap(handle, 0, tdoPort, tdoBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
		nStatus = portMap(handle, 1, tdiPort, tdiBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
		nStatus = portMap(handle, 2, tmsPort, tmsBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
		nStatus = portMap(handle, 3, tckPort, tckBit, error);
		CHECK_STATUS(nStatus, "neroInitialise()", nStatus);
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

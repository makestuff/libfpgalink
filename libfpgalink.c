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
#include <makestuff.h>
#include <libusbwrap.h>
#include <liberror.h>
#include <libbuffer.h>
#include "vendorCommands.h"
#include "libfpgalink.h"
#include "private.h"

static FLStatus getStatus(struct FLContext *handle, uint8 *statusBuffer, const char **error);

DLLEXPORT(void) flInitialise(void) {
	usbInitialise();
}

DLLEXPORT(void) flFreeError(const char *err) {
	errFree(err);
}

DLLEXPORT(FLStatus) flIsDeviceAvailable(
	const char *vp, bool *isAvailable, const char **error)
{
	FLStatus returnCode = FL_SUCCESS;
	int uStatus = usbIsDeviceAvailable(vp, isAvailable, error);
	CHECK_STATUS(uStatus, "flIsDeviceAvailable()", FL_USB_ERR);
cleanup:
	return returnCode;
}

DLLEXPORT(FLStatus) flOpen(const char *vp, struct FLContext **handle, const char **error) {
	FLStatus returnCode, fStatus;
	int uStatus;
	uint8 statusBuffer[16];
	struct FLContext *newCxt = (struct FLContext *)calloc(sizeof(struct FLContext), 1);
	uint8 jtagEndpoints, commEndpoints;
	CHECK_STATUS(!newCxt, "flOpen()", FL_ALLOC_ERR);
	uStatus = usbOpenDevice(vp, 1, 0, 0, &newCxt->device, error);
	CHECK_STATUS(uStatus, "flOpen()", FL_USB_ERR);
	fStatus = getStatus(newCxt, statusBuffer, error);
	CHECK_STATUS(fStatus, "flOpen()", fStatus);
	if ( statusBuffer[0] != 'N' || statusBuffer[1] != 'E' ||
	     statusBuffer[2] != 'M' || statusBuffer[3] != 'I' )
	{
		errRender(error, "flOpen(): Device at %s not recognised", vp);
		FAIL(FL_PROTOCOL_ERR);
	}
	if ( !statusBuffer[6] && !statusBuffer[7] ) {
		errRender(error, "flOpen(): Device at %s supports neither NeroJTAG nor CommFPGA", vp);
		FAIL(FL_PROTOCOL_ERR);
	}
	jtagEndpoints = statusBuffer[6];
	commEndpoints = statusBuffer[7];
	newCxt->jtagOutEP = 0;
	newCxt->jtagInEP = 0;
	newCxt->commOutEP = 0;
	newCxt->commInEP = 0;
	if ( jtagEndpoints ) {
		newCxt->isNeroCapable = true;
		newCxt->jtagOutEP = (jtagEndpoints >> 4);
		newCxt->jtagInEP = (jtagEndpoints & 0x0F);
	}
	if ( commEndpoints ) {
		newCxt->isCommCapable = true;
		newCxt->commOutEP = (commEndpoints >> 4);
		newCxt->commInEP = (commEndpoints & 0x0F);
	}
	*handle = newCxt;
	return FL_SUCCESS;
cleanup:
	if ( newCxt ) {
		if ( newCxt->device ) {
			usbCloseDevice(newCxt->device, 0);
		}
		free((void*)newCxt);
	}
	*handle = NULL;
	return returnCode;
}


DLLEXPORT(void) flClose(struct FLContext *handle) {
	if ( handle ) {
		usbCloseDevice(handle->device, 0);
		if ( handle->writeBuffer.data ) {
			bufDestroy(&handle->writeBuffer);
		}
		free((void*)handle);
	}
}

// Check to see if the device supports NeroJTAG
//
DLLEXPORT(bool) flIsNeroCapable(struct FLContext *handle) {
	return handle->isNeroCapable;
}

// Check to see if the device supports CommFPGA
//
DLLEXPORT(bool) flIsCommCapable(struct FLContext *handle) {
	return handle->isCommCapable;
}

DLLEXPORT(FLStatus) flIsFPGARunning(
	struct FLContext *handle, bool *isRunning, const char **error)
{
	FLStatus returnCode = FL_SUCCESS, fStatus;
	uint8 statusBuffer[16];
	if ( !handle->isCommCapable ) {
		errRender(error, "flIsFPGARunning(): This device does not support CommFPGA");
		FAIL(FL_PROTOCOL_ERR);
	}
	fStatus = getStatus(handle, statusBuffer, error);
	CHECK_STATUS(fStatus, "flIsFPGARunning()", fStatus);
	*isRunning = (statusBuffer[5] & 0x01) ? true : false;
cleanup:
	return returnCode;
}

FLStatus flWrite(
	struct FLContext *handle, const uint8 *bytes, uint32 count, uint32 timeout, const char **error)
{
	int returnCode = FL_SUCCESS;
	int uStatus = usbBulkWrite(
		handle->device,
		handle->commOutEP,  // endpoint to write
		bytes,              // data to send
		count,              // number of bytes
		timeout,
		error
	);
	CHECK_STATUS(uStatus, "flWrite()", FL_USB_ERR);
cleanup:
	return returnCode;
}

FLStatus flRead(
	struct FLContext *handle, uint8 *buffer, uint32 count, uint32 timeout, const char **error)
{
	int returnCode = FL_SUCCESS;
	int uStatus = usbBulkRead(
		handle->device,
		handle->commInEP,  // endpoint to read
		buffer,            // space for data
		count,             // number of bytes
		timeout,
		error
	);
	CHECK_STATUS(uStatus, "flRead()", FL_USB_ERR);
cleanup:
	return returnCode;
}

DLLEXPORT(FLStatus) flWriteChannel(
	struct FLContext *handle, uint32 timeout, uint8 chan, uint32 count, const uint8 *data,
	const char **error)
{
	FLStatus returnCode = FL_SUCCESS, fStatus;
	uint8 command[] = {chan & 0x7F, 0x00, 0x00, 0x00, 0x00};
	if ( !handle->isCommCapable ) {
		errRender(error, "flWriteChannel(): This device does not support CommFPGA");
		FAIL(FL_PROTOCOL_ERR);
	}
	flWriteLong(count, command+1);
	fStatus = flWrite(handle, command, 5, 1000, error);
	CHECK_STATUS(fStatus, "flWriteChannel()", fStatus);
	fStatus = flWrite(handle, data, count, timeout, error);
	CHECK_STATUS(fStatus, "flWriteChannel()", fStatus);
cleanup:
	return returnCode;
}

// Append a write command to the end of the write buffer
DLLEXPORT(FLStatus) flAppendWriteChannelCommand(
	struct FLContext *handle, uint8 chan, uint32 count, const uint8 *data, const char **error)
{
	FLStatus returnCode = FL_SUCCESS;
	BufferStatus bStatus;
	uint8 command[] = {chan & 0x7F, 0x00, 0x00, 0x00, 0x00};
	if ( !handle->writeBuffer.data ) {
		bStatus = bufInitialise(&handle->writeBuffer, 1024, 0x00, error);
		CHECK_STATUS(bStatus, "flAppendWriteChannelCommand()", FL_ALLOC_ERR);
	}
	flWriteLong(count, command+1);
	bStatus = bufAppendBlock(&handle->writeBuffer, command, 5, error);
	CHECK_STATUS(bStatus, "flAppendWriteChannelCommand()", FL_ALLOC_ERR);
	bStatus = bufAppendBlock(&handle->writeBuffer, data, count, error);
	CHECK_STATUS(bStatus, "flAppendWriteChannelCommand()", FL_ALLOC_ERR);
cleanup:
	return returnCode;
}

// Play the write buffer into the USB cable immediately
//
DLLEXPORT(FLStatus) flPlayWriteBuffer(struct FLContext *handle, uint32 timeout, const char **error)
{
	FLStatus returnCode = FL_SUCCESS, fStatus;
	if ( handle->writeBuffer.length == 0 ) {
		errRender(error, "flPlayWriteBuffer(): The write buffer is empty");
		FAIL(FL_WBUF_ERR);
	}
	fStatus = flWrite(handle, handle->writeBuffer.data, handle->writeBuffer.length, timeout, error);
	CHECK_STATUS(fStatus, "flPlayWriteBuffer()", fStatus);
cleanup:
	return returnCode;
}

// Clear the write buffer (if any)
//
DLLEXPORT(void) flCleanWriteBuffer(struct FLContext *handle) {
	if ( handle->writeBuffer.data ) {
		bufZeroLength(&handle->writeBuffer);
	}
}

DLLEXPORT(FLStatus) flReadChannel(
	struct FLContext *handle, uint32 timeout, uint8 chan, uint32 count, uint8 *buf,
	const char **error)
{
	FLStatus returnCode = FL_SUCCESS, fStatus;
	uint8 command[] = {chan | 0x80, 0x00, 0x00, 0x00, 0x00};
	if ( !handle->isCommCapable ) {
		errRender(error, "flReadChannel(): This device does not support CommFPGA");
		FAIL(FL_PROTOCOL_ERR);
	}
	flWriteLong(count, command+1);
	fStatus = flWrite(handle, command, 5, 1000, error);
	CHECK_STATUS(fStatus, "flReadChannel()", fStatus);
	fStatus = flRead(handle, buf, count, timeout, error);
	CHECK_STATUS(fStatus, "flReadChannel()", fStatus);
cleanup:
	return returnCode;
}

DLLEXPORT(FLStatus) flPortAccess(
	struct FLContext *handle, uint16 portWrite, uint16 ddr, uint16 *portRead, const char **error)
{
	FLStatus returnCode = FL_SUCCESS;
	int uStatus;
	union {
		uint16 word;
		uint8 bytes[2];
	} u;
	uStatus = usbControlRead(
		handle->device,
		CMD_PORT_IO,     // bRequest
		portWrite,       // wValue
		ddr,             // wIndex
		u.bytes,         // buffer to receive current state of ports
		2,               // wLength
		1000,            // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "flPortAccess()", FL_USB_ERR);
	if ( portRead ) {
		*portRead = u.word;
	}
cleanup:
	return returnCode;
}

uint16 flReadWord(const uint8 *p) {
	uint16 value = *p++;
	value <<= 8;
	value |= *p;
	return value;
}

uint32 flReadLong(const uint8 *p) {
	uint32 value = *p++;
	value <<= 8;
    value |= *p++;
    value <<= 8;
    value |= *p++;
    value <<= 8;
    value |= *p;
	return value;
}

void flWriteWord(uint16 value, uint8 *p) {
	p[1] = value & 0x00FF;
	value >>= 8;
	p[0] = value & 0x00FF;
}

void flWriteLong(uint32 value, uint8 *p) {
	p[3] = value & 0x000000FF;
	value >>= 8;
	p[2] = value & 0x000000FF;
	value >>= 8;
	p[1] = value & 0x000000FF;
	value >>= 8;
	p[0] = value & 0x000000FF;
}

static FLStatus getStatus(struct FLContext *handle, uint8 *statusBuffer, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	int uStatus;
	uStatus = usbControlRead(
		handle->device,
		CMD_MODE_STATUS,          // bRequest
		0x0000,                   // wValue : off
		0x0000,                   // wMask
		statusBuffer,
		16,                       // wLength
		1000,                     // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, "getStatus()", FL_PROTOCOL_ERR);
cleanup:
	return returnCode;
}

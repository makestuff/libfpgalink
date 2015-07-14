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
#include <libbuffer.h>
#include "vendorCommands.h"
#include "libfpgalink.h"
#include "private.h"

static FLStatus getStatus(struct FLContext *handle, uint8 *statusBuffer, const char **error);

// Initialise library for use.
//
DLLEXPORT(FLStatus) flInitialise(int logLevel, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbInitialise(logLevel, error);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flInitialise()");
cleanup:
	return retVal;
}

// Convenience function to avoid having to include liberror.h.
//
DLLEXPORT(void) flFreeError(const char *err) {
	errFree(err);
}

// Return with true in isAvailable if the given VID:PID[:DID] is available.
//
DLLEXPORT(FLStatus) flIsDeviceAvailable(
	const char *vp, uint8 *isAvailable, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	bool flag;
	USBStatus uStatus = usbIsDeviceAvailable(vp, &flag, error);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flIsDeviceAvailable()");
	*isAvailable = flag ? 0x01 : 0x00;
cleanup:
	return retVal;
}

// Open a connection, get device status & sanity-check it.
//
DLLEXPORT(FLStatus) flOpen(const char *vp, struct FLContext **handle, const char **error) {
	FLStatus retVal = FL_SUCCESS, fStatus;
	USBStatus uStatus;
	uint8 statusBuffer[16];
	struct FLContext *newCxt = (struct FLContext *)calloc(sizeof(struct FLContext), 1);
	uint8 progEndpoints, commEndpoints;
	CHECK_STATUS(!newCxt, FL_ALLOC_ERR, cleanup, "flOpen()");
	uStatus = usbOpenDevice(vp, 1, 0, 0, &newCxt->device, error);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flOpen()");
	fStatus = getStatus(newCxt, statusBuffer, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flOpen()");
	CHECK_STATUS(
		statusBuffer[0] != 'N' || statusBuffer[1] != 'E' ||
		statusBuffer[2] != 'M' || statusBuffer[3] != 'I',
		FL_PROTOCOL_ERR, cleanup,
		"flOpen(): Device at %s not recognised", vp);
	CHECK_STATUS(
		!statusBuffer[6] && !statusBuffer[7], FL_PROTOCOL_ERR, cleanup,
		"flOpen(): Device at %s supports neither NeroProg nor CommFPGA", vp);
	progEndpoints = statusBuffer[6];
	commEndpoints = statusBuffer[7];
	if ( progEndpoints ) {
		newCxt->isNeroCapable = true;
		newCxt->progOutEP = (progEndpoints >> 4);
		newCxt->progInEP = (progEndpoints & 0x0F);
	}
	if ( commEndpoints ) {
		newCxt->isCommCapable = true;
		newCxt->commOutEP = (commEndpoints >> 4);
		newCxt->commInEP = (commEndpoints & 0x0F);
	}
	newCxt->firmwareID = (uint16)(
		(statusBuffer[8] << 8) |
		statusBuffer[9]
	);
	newCxt->firmwareVersion = (uint32)(
		(statusBuffer[10] << 24) |
		(statusBuffer[11] << 16) |
		(statusBuffer[12] << 8)  |
		statusBuffer[13]
	);
	newCxt->chunkSize = 0x10000;  // default maximum libusbwrap chunk size
	*handle = newCxt;
	return retVal;
cleanup:
	if ( newCxt ) {
		if ( newCxt->device ) {
			usbCloseDevice(newCxt->device, 0);
		}
		free((void*)newCxt);
	}
	*handle = NULL;
	return retVal;
}

// Disconnect and cleanup, if necessary.
//
DLLEXPORT(void) flClose(struct FLContext *handle) {
	if ( handle ) {
		USBStatus uStatus;
		struct CompletionReport completionReport;
		FLStatus fStatus = flFlushAsyncWrites(handle, NULL);
		size_t queueDepth = usbNumOutstandingRequests(handle->device);
		while ( queueDepth-- ) {
			uStatus = usbBulkAwaitCompletion(handle->device, &completionReport, NULL);
		}
		usbCloseDevice(handle->device, 0);
		free((void*)handle);
		(void)fStatus;
		(void)uStatus;
	}
}

// Check to see if the device supports NeroProg.
//
DLLEXPORT(uint8) flIsNeroCapable(struct FLContext *handle) {
	return handle->isNeroCapable ? 0x01 : 0x00;
}

// Check to see if the device supports CommFPGA.
//
DLLEXPORT(uint8) flIsCommCapable(struct FLContext *handle, uint8 conduit) {
	// TODO: actually consider conduit
	(void)conduit;
	return handle->isCommCapable ? 0x01 : 0x00;
}

// Retrieve the firmware ID (e.g FX2 trunk = 0xFFFF, AVR trunk = 0xAAAA).
//
DLLEXPORT(uint16) flGetFirmwareID(struct FLContext *handle) {
	return handle->firmwareID;
}

// Retrieve the firmware version (e.g 0x20131217). This is an 8-digit ISO date when printed in hex.
//
DLLEXPORT(uint32) flGetFirmwareVersion(struct FLContext *handle) {
	return handle->firmwareVersion;
}

// Select the conduit that should be used to communicate with the FPGA. Each device may support one
// or more different conduits to the same FPGA, or different FPGAs.
//
DLLEXPORT(FLStatus) flSelectConduit(
	struct FLContext *handle, uint8 conduit, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbControlWrite(
		handle->device,
		CMD_MODE_STATUS,   // bRequest
		0x0000,            // wValue
		(uint16)conduit,   // wIndex
		NULL,              // buffer to receive current state of ports
		0,                 // wLength
		1000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flSelectConduit()");
cleanup:
	return retVal;
}

// Return with 0x01 in isRunning if the firmware thinks the FPGA is running.
//
DLLEXPORT(FLStatus) flIsFPGARunning(
	struct FLContext *handle, uint8 *isRunning, const char **error)
{
	FLStatus retVal;
	uint8 statusBuffer[16];
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flIsFPGARunning(): This device does not support CommFPGA");
	retVal = getStatus(handle, statusBuffer, error);
	CHECK_STATUS(retVal, retVal, cleanup, "flIsFPGARunning()");
	*isRunning = (statusBuffer[5] & 0x01) ? 0x01 : 0x00;
cleanup:
	return retVal;
}

static FLStatus bufferAppend(
	struct FLContext *handle, const uint8 *data, size_t count, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	size_t spaceAvailable;
	size_t queueDepth = usbNumOutstandingRequests(handle->device);
	USBStatus uStatus;
	if ( !handle->writePtr ) {
		// There is not an active write buffer
		uStatus = usbBulkWriteAsyncPrepare(handle->device, &handle->writePtr, error);
		CHECK_STATUS(uStatus, FL_ALLOC_ERR, cleanup, "bufferAppend()");
		handle->writeBuf = handle->writePtr;
	}
	spaceAvailable = handle->chunkSize - (size_t)(handle->writePtr - handle->writeBuf);
	while ( count > spaceAvailable ) {
		// Reduce the depth of the work queue a little
		while ( queueDepth > 2 && !handle->completionReport.flags.isRead ) {
			uStatus = usbBulkAwaitCompletion(
				handle->device, &handle->completionReport, error);
			CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "bufferAppend()");
			queueDepth--;
		}
		
		// Fill up this buffer
		memcpy(handle->writePtr, data, spaceAvailable);
		data += spaceAvailable;
		count -= spaceAvailable;
		
		// Submit it
		uStatus = usbBulkWriteAsyncSubmit(
			handle->device, handle->commOutEP, handle->chunkSize, U32MAX, error);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "bufferAppend()");
		queueDepth++;
		
		// Get a new buffer
		uStatus = usbBulkWriteAsyncPrepare(handle->device, &handle->writePtr, error);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "bufferAppend()");
		handle->writeBuf = handle->writePtr;
		spaceAvailable = handle->chunkSize;
	}
	if ( count == spaceAvailable ) {
		// Reduce the depth of the work queue a little
		while ( queueDepth > 2 && !handle->completionReport.flags.isRead ) {
			uStatus = usbBulkAwaitCompletion(
				handle->device, &handle->completionReport, error);
			CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "bufferAppend()");
			queueDepth--;
		}
		
		// Fill up this buffer
		memcpy(handle->writePtr, data, spaceAvailable);
		data += spaceAvailable;
		count -= spaceAvailable;
		
		// Submit it
		uStatus = usbBulkWriteAsyncSubmit(
			handle->device, handle->commOutEP, handle->chunkSize, U32MAX, error);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "bufferAppend()");
		queueDepth++;

		// Zero the pointers
		handle->writeBuf = handle->writePtr = NULL;
	} else {
		// Count is less than spaceAvailable
		memcpy(handle->writePtr, data, count);
		handle->writePtr += count;
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flSetAsyncWriteChunkSize(
	struct FLContext *handle, uint16 chunkSize, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	CHECK_STATUS(
		handle->writePtr, FL_BAD_STATE, cleanup,
		"flSetAsyncWriteChunkSize(): cannot change chunk size when there's some data pending");
	if ( chunkSize ) {
		handle->chunkSize = chunkSize;
	} else {
		handle->chunkSize = 0x10000;
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flFlushAsyncWrites(struct FLContext *handle, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	if ( handle->writePtr && handle->writeBuf && handle->writePtr > handle->writeBuf ) {
		CHECK_STATUS(
			!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
			"flFlushAsyncWrites(): This device does not support CommFPGA");
		uStatus = usbBulkWriteAsyncSubmit(
			handle->device, handle->commOutEP,
			(uint32)(handle->writePtr - handle->writeBuf),
			U32MAX, NULL);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flFlushAsyncWrites()");
		handle->writePtr = handle->writeBuf = NULL;
	}
cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flAwaitAsyncWrites(struct FLContext *handle, const char **error) {
	FLStatus retVal = FL_SUCCESS, fStatus;
	USBStatus uStatus;
	size_t queueDepth;
	fStatus = flFlushAsyncWrites(handle, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flAwaitAsyncWrites()");
	queueDepth = usbNumOutstandingRequests(handle->device);
	while ( queueDepth && !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flAwaitAsyncWrites()");
		queueDepth--;
	}
	CHECK_STATUS(
		queueDepth, FL_BAD_STATE, cleanup,
		"flAwaitAsyncWrites(): An asynchronous read is in flight");
cleanup:
	return retVal;
}

// Write some bytes to the specified channel, synchronously.
//
DLLEXPORT(FLStatus) flWriteChannel(
	struct FLContext *handle, uint8 chan, size_t count, const uint8 *data, const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	fStatus = flWriteChannelAsync(handle, chan, count, data, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flWriteChannel()");
	fStatus = flAwaitAsyncWrites(handle, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flWriteChannel()");
cleanup:
	return retVal;
}

// Write some bytes to the specified channel, asynchronously.
//
DLLEXPORT(FLStatus) flWriteChannelAsync(
	struct FLContext *handle, uint8 chan, size_t count, const uint8 *data,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	uint8 command[3];
	USBStatus uStatus;
	CHECK_STATUS(
		count == 0, FL_PROTOCOL_ERR, cleanup,
		"flWriteChannelAsync(): Zero-length writes are illegal!");
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flWriteChannelAsync(): This device does not support CommFPGA");
	command[0] = chan & 0x7F;
	command[1] = 0x00;
	command[2] = 0x00;
	while ( count >= 0x10000 ) {
		fStatus = bufferAppend(handle, command, 3, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "flWriteChannelAsync()");
		fStatus = bufferAppend(handle, data, 0x10000, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "flWriteChannelAsync()");
		count -= 0x10000;
		data += 0x10000;
	}
	if ( count ) {
		flWriteWord((uint16)count, command+1);
		fStatus = bufferAppend(handle, command, 3, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "flWriteChannelAsync()");
		fStatus = bufferAppend(handle, data, count, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "flWriteChannelAsync()");
	}
cleanup:
	return retVal;
}

// Read some bytes from the specified channel, synchronously.
// TODO: Deal with early-termination properly - it should not be treated like an error.
//       This will require changes in usbBulkRead(). Async API is already correct.
//
DLLEXPORT(FLStatus) flReadChannel(
	struct FLContext *handle, uint8 chan, size_t count, uint8 *buffer,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	const uint8 *data;
	uint32 requestLength, actualLength;
	CHECK_STATUS(
		count == 0, FL_PROTOCOL_ERR, cleanup,
		"flReadChannel(): Zero-length reads are illegal!");
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flReadChannel(): This device does not support CommFPGA");
	if ( count >= 0x10000 ) {
		fStatus = flReadChannelAsyncSubmit(handle, chan, 0x10000, buffer, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
		count -= 0x10000;
		buffer += 0x10000;
		while ( count >= 0x10000 ) {
			fStatus = flReadChannelAsyncSubmit(handle, chan, 0x10000, buffer, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
			count -= 0x10000;
			buffer += 0x10000;
			fStatus = flReadChannelAsyncAwait(handle, &data, &requestLength, &actualLength, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
			CHECK_STATUS(
				actualLength != requestLength,
				FL_EARLY_TERM, cleanup, "flReadChannel()");
		}
		if ( count ) {
			fStatus = flReadChannelAsyncSubmit(handle, chan, (uint32)count, buffer, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
			fStatus = flReadChannelAsyncAwait(handle, &data, &requestLength, &actualLength, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
			CHECK_STATUS(
				actualLength != requestLength,
				FL_EARLY_TERM, cleanup, "flReadChannel()");
		}
	} else {
		fStatus = flReadChannelAsyncSubmit(handle, chan, (uint32)count, buffer, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
	}
	fStatus = flReadChannelAsyncAwait(handle, &data, &requestLength, &actualLength, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
	CHECK_STATUS(
		actualLength != requestLength,
		FL_EARLY_TERM, cleanup, "flReadChannel()");
cleanup:
	return retVal;
}

// Read bytes asynchronously from the specified channel (1 <= count <= 0x10000).
//
DLLEXPORT(FLStatus) flReadChannelAsyncSubmit(
	struct FLContext *handle, uint8 chan, uint32 count, uint8 *buffer, const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	uint8 command[3];
	USBStatus uStatus;
	size_t queueDepth;
	const uint16 count16 = (count == 0x10000) ? 0x0000 : (uint16)count;
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flReadChannelAsyncSubmit(): This device does not support CommFPGA");
	CHECK_STATUS(
		count == 0, FL_PROTOCOL_ERR, cleanup,
		"flReadChannelAsyncSubmit(): Zero-length reads are illegal!");
	CHECK_STATUS(
		count > 0x10000, FL_PROTOCOL_ERR, cleanup,
		"flReadChannelAsyncSubmit(): Transfer length exceeds 0x10000");

	// Write command
	command[0] = chan | 0x80;
	flWriteWord(count16, command+1);
	fStatus = bufferAppend(handle, command, 3, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannelAsyncSubmit()");

	// Flush outstanding async writes
	fStatus = flFlushAsyncWrites(handle, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannelAsyncSubmit()");

	// Maybe do a few awaits, to keep things balanced
	queueDepth = usbNumOutstandingRequests(handle->device);
	while ( queueDepth > 2 && !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncSubmit()");
		queueDepth--;
	}

	// Then request the data
	uStatus = usbBulkReadAsync(
		handle->device,
		handle->commInEP,   // endpoint to read
		buffer,             // pointer to buffer, or null
		count,              // number of data bytes
		U32MAX,             // max timeout: 49 days
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncSubmit()");
cleanup:
	return retVal;
}

// Await a previously-submitted async read.
//
DLLEXPORT(FLStatus) flReadChannelAsyncAwait(
	struct FLContext *handle, const uint8 **data, uint32 *requestLength, uint32 *actualLength,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	while ( !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncAwait()");
	}
	*data = handle->completionReport.buffer;
	*requestLength = handle->completionReport.requestLength;
	*actualLength = handle->completionReport.actualLength;
	memset(&handle->completionReport, 0, sizeof(struct CompletionReport));
cleanup:
	return retVal;
}

// Reset the USB toggle on the device by explicitly calling SET_INTERFACE. This is a dirty hack
// that appears to be necessary when running FPGALink on a Linux VM running on a Windows host.
//
DLLEXPORT(FLStatus) flResetToggle(
	struct FLContext *handle, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbControlWrite(
		handle->device,
		0x0B,            // bRequest
		0x0000,          // wValue
		0x0000,          // wIndex
		NULL,            // buffer to write
		0,               // wLength
		1000,            // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flResetToggle()");
cleanup:
	return retVal;
}

uint16 flReadWord(const uint8 *p) {
	uint16 value = *p++;
	return (uint16)((value << 8) | *p);
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
	p[1] = (uint8)(value & 0xFF);
	value >>= 8;
	p[0] = (uint8)(value & 0xFF);
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
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbControlRead(
		handle->device,
		CMD_MODE_STATUS,          // bRequest
		0x0000,                   // wValue : off
		0x0000,                   // wMask
		statusBuffer,
		16,                       // wLength
		1000,                     // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_PROTOCOL_ERR, cleanup, "getStatus()");
cleanup:
	return retVal;
}

DLLEXPORT(uint8) progGetPort(struct FLContext *handle, uint8 logicalPort) {
	switch ( logicalPort ) {
	case LP_MISO:
		return handle->misoPort;
	case LP_MOSI:
		return handle->mosiPort;
	case LP_SS:
		return handle->ssPort;
	case LP_SCK:
		return handle->sckPort;
	case LP_CHOOSE:
	case LP_D8:
	default:
		return 0xFF;
	}
}

DLLEXPORT(uint8) progGetBit(struct FLContext *handle, uint8 logicalPort) {
	switch ( logicalPort ) {
	case LP_MISO:
		return handle->misoBit;
	case LP_MOSI:
		return handle->mosiBit;
	case LP_SS:
		return handle->ssBit;
	case LP_SCK:
		return handle->sckBit;
	case LP_CHOOSE:
	case LP_D8:
	default:
		return 0xFF;
	}
}

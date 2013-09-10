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
	const char *vp, bool *isAvailable, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbIsDeviceAvailable(vp, isAvailable, error);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flIsDeviceAvailable()");
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
	newCxt->progOutEP = 0;
	newCxt->progInEP = 0;
	newCxt->commOutEP = 0;
	newCxt->commInEP = 0;
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
		size_t queueDepth = usbNumOutstandingRequests(handle->device);
		struct CompletionReport completionReport;
		while ( queueDepth-- ) {
			uStatus = usbBulkAwaitCompletion(handle->device, &completionReport, NULL);
		}
		(void)uStatus;
		usbCloseDevice(handle->device, 0);
		if ( handle->writeBuffer.data ) {
			bufDestroy(&handle->writeBuffer);
		}
		free((void*)handle);
	}
}

// Check to see if the device supports NeroProg.
//
DLLEXPORT(bool) flIsNeroCapable(struct FLContext *handle) {
	return handle->isNeroCapable;
}

// Check to see if the device supports CommFPGA.
//
DLLEXPORT(bool) flIsCommCapable(struct FLContext *handle) {
	return handle->isCommCapable;
}

// Return with true in isRunning if the firmware thinks the FPGA is running.
//
DLLEXPORT(FLStatus) flIsFPGARunning(
	struct FLContext *handle, bool *isRunning, const char **error)
{
	FLStatus retVal;
	uint8 statusBuffer[16];
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flIsFPGARunning(): This device does not support CommFPGA");
	retVal = getStatus(handle, statusBuffer, error);
	CHECK_STATUS(retVal, retVal, cleanup, "flIsFPGARunning()");
	*isRunning = (statusBuffer[5] & 0x01) ? true : false;
cleanup:
	return retVal;
}

// Write some raw bytes to the device.
//
FLStatus flWrite(
	struct FLContext *handle, const uint8 *bytes, uint32 count, uint32 timeout, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbBulkWrite(
		handle->device,
		handle->commOutEP,  // endpoint to write
		bytes,              // data to send
		count,              // number of bytes
		timeout,
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flWrite()");
cleanup:
	return retVal;
}

// Read some raw bytes from the device.
//
FLStatus flRead(
	struct FLContext *handle, uint8 *buffer, uint32 count, uint32 timeout, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbBulkRead(
		handle->device,
		handle->commInEP,  // endpoint to read
		buffer,            // space for data
		count,             // number of bytes
		timeout,
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flRead()");
cleanup:
	return retVal;
}

// Write some bytes to the specified channel, synchronously.
//
DLLEXPORT(FLStatus) flWriteChannel(
	struct FLContext *handle, uint32 timeout, uint8 chan, uint32 count, const uint8 *data,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	size_t queueDepth = usbNumOutstandingRequests(handle->device);
	uint8 command[5];
	USBStatus uStatus;
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flWriteChannel(): This device does not support CommFPGA");

	// Clear out any outstanding write awaits
	while ( queueDepth && !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannel()");
		queueDepth--;
	}
	CHECK_STATUS(
		queueDepth, FL_PROTOCOL_ERR, cleanup,
		"flReadChannel(): Cannot do synchronous writes when an asynchronous read is in flight");

	// Proceed with the write
	command[0] = chan & 0x7F;
	flWriteLong(count, command+1);
	retVal = flWrite(handle, command, 5, 1000, error);
	CHECK_STATUS(retVal, retVal, cleanup, "flWriteChannel()");
	retVal = flWrite(handle, data, count, timeout, error);
	CHECK_STATUS(retVal, retVal, cleanup, "flWriteChannel()");
cleanup:
	return retVal;
}

// Write some bytes to the specified channel, asynchronously.
//
DLLEXPORT(FLStatus) flWriteChannelAsync(
	struct FLContext *handle, uint8 chan, uint32 count, const uint8 *data,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	uint8 command[5];
	USBStatus uStatus;
	size_t queueDepth = usbNumOutstandingRequests(handle->device);
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flWriteChannelAsync(): This device does not support CommFPGA");

	// Maybe do a few awaits, to keep things balanced
	while ( queueDepth > 2 && !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flWriteChannelAsync()");
		queueDepth--;
	}

	// Send write command
	command[0] = chan & 0x7F;
	flWriteLong(count, command+1);
	uStatus = usbBulkWriteAsync(
		handle->device,
		handle->commOutEP,  // endpoint to write
		command,            // command to send
		5,                  // channel and four length bytes
		0xFFFFFFFFU,        // max timeout: 49 days
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flWriteChannelAsync()");

	// Then send the data
	uStatus = usbBulkWriteAsync(
		handle->device,
		handle->commOutEP,  // endpoint to write
		data,               // data to send
		count,              // number of data bytes
		0xFFFFFFFFU,        // max timeout: 49 days
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flWriteChannelAsync()");
cleanup:
	return retVal;
}

// Append a write command to the end of the write buffer.
//
DLLEXPORT(FLStatus) flAppendWriteChannelCommand(
	struct FLContext *handle, uint8 chan, uint32 count, const uint8 *data, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	BufferStatus bStatus;
	uint8 command[5];
	if ( !handle->writeBuffer.data ) {
		// write buffer is lazily initialised
		bStatus = bufInitialise(&handle->writeBuffer, 1024, 0x00, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flAppendWriteChannelCommand()");
	}
	command[0] = chan & 0x7F;
	flWriteLong(count, command+1);
	bStatus = bufAppendBlock(&handle->writeBuffer, command, 5, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flAppendWriteChannelCommand()");
	bStatus = bufAppendBlock(&handle->writeBuffer, data, count, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flAppendWriteChannelCommand()");
cleanup:
	return retVal;
}

// Play the write buffer into the USB cable immediately
//
DLLEXPORT(FLStatus) flPlayWriteBuffer(struct FLContext *handle, uint32 timeout, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	CHECK_STATUS(
		handle->writeBuffer.length == 0, FL_WBUF_ERR, cleanup,
		"flPlayWriteBuffer(): The write buffer is empty");
	retVal = flWrite(handle, handle->writeBuffer.data, handle->writeBuffer.length, timeout, error);
	CHECK_STATUS(retVal, retVal, cleanup, "flPlayWriteBuffer()");
cleanup:
	return retVal;
}

// Clear the write buffer (if any)
//
DLLEXPORT(void) flCleanWriteBuffer(struct FLContext *handle) {
	if ( handle->writeBuffer.data ) {
		bufZeroLength(&handle->writeBuffer);
	}
}

// Read some bytes from the specified channel, synchronously.
//
DLLEXPORT(FLStatus) flReadChannel(
	struct FLContext *handle, uint32 timeout, uint8 chan, uint32 count, uint8 *buf,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	size_t queueDepth = usbNumOutstandingRequests(handle->device);
	USBStatus uStatus;
	uint8 command[5];
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flReadChannel(): This device does not support CommFPGA");

	// Clear out any outstanding write awaits
	while ( queueDepth && !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannel()");
		queueDepth--;
	}
	CHECK_STATUS(
		queueDepth, FL_PROTOCOL_ERR, cleanup,
		"flReadChannel(): Cannot do synchronous reads when an asynchronous read is in flight");

	// Proceed with the read
	command[0] = chan | 0x80;
	flWriteLong(count, command+1);
	fStatus = flWrite(handle, command, 5, 1000, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
	fStatus = flRead(handle, buf, count, timeout, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flReadChannel()");
cleanup:
	return retVal;
}

// Read some bytes asynchronously from the specified channel.
//
DLLEXPORT(FLStatus) flReadChannelAsyncSubmit(
	struct FLContext *handle, uint8 chan, uint32 count, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	uint8 command[5];
	USBStatus uStatus;
	size_t queueDepth = usbNumOutstandingRequests(handle->device);
	CHECK_STATUS(
		!handle->isCommCapable, FL_PROTOCOL_ERR, cleanup,
		"flReadChannelAsyncSubmit(): This device does not support CommFPGA");
	CHECK_STATUS(
		count > 0x10000, FL_USB_ERR, cleanup,
		"flReadChannelAsyncSubmit(): Transfer length exceeds 0x10000");

	// Maybe do a few awaits, to keep things balanced
	while ( queueDepth > 2 && !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncSubmit()");
		queueDepth--;
	}

	// Send read command
	command[0] = chan | 0x80;
	flWriteLong(count, command+1);
	uStatus = usbBulkWriteAsync(
		handle->device,
		handle->commOutEP,  // endpoint to write
		command,            // command to send
		5,                  // channel and four length bytes
		0xFFFFFFFFU,        // max timeout: 49 days
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncSubmit()");

	// Then request the data
	uStatus = usbBulkReadAsync(
		handle->device,
		handle->commInEP,   // endpoint to read
		count,              // number of data bytes
		0xFFFFFFFFU,        // max timeout: 49 days
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncSubmit()");
cleanup:
	return retVal;
}

// Await a previously-submitted async read.
//
DLLEXPORT(FLStatus) flReadChannelAsyncAwait(
	struct FLContext *handle, struct ReadReport *readReport, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus;
	while ( !handle->completionReport.flags.isRead ) {
		uStatus = usbBulkAwaitCompletion(
			handle->device, &handle->completionReport, error
		);
		CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flReadChannelAsyncAwait()");
	}
	readReport->data = handle->completionReport.buffer;
	readReport->actualLength = handle->completionReport.actualLength;
	readReport->requestLength = handle->completionReport.requestLength;
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

// Select the conduit that should be used to communicate with the FPGA. Each device may support one
// or more different conduits to the same FPGA, or different FPGAs.
//
DLLEXPORT(FLStatus) flFifoMode(
	struct FLContext *handle, uint8 fifoMode, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	USBStatus uStatus = usbControlWrite(
		handle->device,
		0x80,              // bRequest
		0x0000,            // wValue
		(uint16)fifoMode,  // wIndex
		NULL,              // buffer to receive current state of ports
		0,                 // wLength
		1000,              // timeout (ms)
		error
	);
	CHECK_STATUS(uStatus, FL_USB_ERR, cleanup, "flFifoMode()");
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

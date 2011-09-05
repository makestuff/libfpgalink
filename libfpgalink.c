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
#include <stdio.h>
#include <stdlib.h>
#include <makestuff.h>
#include <libusbwrap.h>
#include <usb.h>
#include <libsync.h>
#include <liberror.h>
#include <libbuffer.h>
#include <vendorCommands.h>
#include "libfpgalink.h"
#include "private.h"

static FLStatus getStatus(struct FLContext *handle, uint8 *statusBuffer, const char **error);
static FLStatus trySync(struct FLContext *newCxt, const uint8 *statusBuffer);

DLLEXPORT(void) flInitialise(void) {
	usbInitialise();
}

DLLEXPORT(void) flFreeError(const char *err) {
	errFree(err);
}

DLLEXPORT(FLStatus) flIsDeviceAvailable(
	const char *vp, bool *isAvailable, const char **error)
{
	FLStatus returnCode;
	USBStatus uStatus;
	uint16 vid, pid;
	if ( !usbValidateVidPid(vp) ) {
		errRender(error, "The supplied VID:PID \"%s\" is invalid; it should look like 04B4:8613", vp);
		FAIL(FL_USB_ERR);
	}
	vid = (uint16)strtoul(vp, NULL, 16);
	pid = (uint16)strtoul(vp+5, NULL, 16);
	uStatus = usbIsDeviceAvailable(vid, pid, isAvailable, error);
	CHECK_STATUS(uStatus, "flIsDeviceAvailable()", FL_USB_ERR);
	return FL_SUCCESS;
cleanup:
	return returnCode;
}

DLLEXPORT(FLStatus) flOpen(const char *vp, struct FLContext **handle, const char **error) {
	FLStatus returnCode, fStatus;
	int uStatus;
	uint8 statusBuffer[16];
	struct FLContext *newCxt = (struct FLContext *)calloc(sizeof(struct FLContext), 1);
	uint16 vid, pid;
	if ( !usbValidateVidPid(vp) ) {
		errRender(error, "The supplied VID:PID \"%s\" is invalid; it should look like 04B4:8613", vp);
		FAIL(FL_USB_ERR);
	}
	vid = (uint16)strtoul(vp, NULL, 16);
	pid = (uint16)strtoul(vp+5, NULL, 16);
	CHECK_STATUS(!newCxt, "flOpen()", FL_ALLOC_ERR);
	uStatus = usbOpenDevice(vid, pid, 1, 0, 0, &newCxt->device, error);
	CHECK_STATUS(uStatus, "flOpen()", FL_USB_ERR);
	fStatus = getStatus(newCxt, statusBuffer, error);
	CHECK_STATUS(fStatus, "flOpen()", fStatus);
	if ( statusBuffer[0] != 'N' || statusBuffer[1] != 'E' ||
	     statusBuffer[2] != 'M' || statusBuffer[3] != 'I' )
	{
		errRender(
			error,
			"flOpen(): Device at %04X:%04X not recognised",
			vid, pid);
		FAIL(FL_PROTOCOL_ERR);
	}
	if ( !statusBuffer[6] && !statusBuffer[7] ) {
		errRender(
			error,
			"flOpen(): Device at %04X:%04X supports neither NeroJTAG nor CommFPGA",
			vid, pid);
		FAIL(FL_PROTOCOL_ERR);
	}
	fStatus = trySync(newCxt, statusBuffer);
	if ( fStatus != FL_SUCCESS ) {
		errRender(error, "flOpen(): Unable to sync device at %04X:%04X", vid, pid);
		FAIL(FL_PROTOCOL_ERR);
	}
	*handle = newCxt;
	return FL_SUCCESS;
cleanup:
	if ( newCxt ) {
		if ( newCxt->device ) {
			usb_release_interface(newCxt->device, 0);
			usb_close(newCxt->device);
		}
		free((void*)newCxt);
	}
	*handle = NULL;
	return returnCode;
}


DLLEXPORT(void) flClose(struct FLContext *handle) {
	if ( handle ) {
		usb_release_interface(handle->device, 0);
		usb_close(handle->device);
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
	FLStatus returnCode, fStatus;
	uint8 statusBuffer[16];
	if ( !handle->isCommCapable ) {
		errRender(error, "flIsFPGARunning(): This device does not support CommFPGA");
		FAIL(FL_PROTOCOL_ERR);
	}
	fStatus = getStatus(handle, statusBuffer, error);
	CHECK_STATUS(fStatus, "flIsFPGARunning()", fStatus);
	*isRunning = (statusBuffer[5] & 0x01) ? true : false;
	return FL_SUCCESS;
cleanup:
	return returnCode;
}

FLStatus flWrite(
	struct FLContext *handle, const uint8 *bytes, uint32 count, uint32 timeout, const char **error)
{
	int returnCode = usb_bulk_write(
		handle->device, USB_ENDPOINT_OUT | 6, (char*)bytes, count, timeout);
	if ( returnCode < 0 ) {
		errRender(
			error,
			"flWrite(): usb_bulk_write() failed returnCode %d: %s",
			returnCode, usb_strerror());
		return FL_USB_ERR;
	} else if ( (uint32)returnCode != count ) {
		errRender(
			error,
			"flWrite(): usb_bulk_write() wrote %d bytes instead of the expected %lu",
			returnCode, count);
		return FL_USB_ERR;
	}
	return FL_SUCCESS;
}

FLStatus flRead(
	struct FLContext *handle, uint8 *buffer, uint32 count, uint32 timeout, const char **error)
{
	int returnCode = usb_bulk_read(
		handle->device, USB_ENDPOINT_IN | 8, (char*)buffer, count, timeout);
	if ( returnCode < 0 ) {
		errRender(
			error,
			"flRead(): usb_bulk_read() failed returnCode %d: %s",
			returnCode, usb_strerror());
		return FL_USB_ERR;
	} else if ( (uint32)returnCode != count ) {
		errRender(
			error,
			"flRead(): usb_bulk_read() read %d bytes instead of the expected %lu",
			returnCode, count);
		return FL_USB_ERR;
	}
	return FL_SUCCESS;
}

DLLEXPORT(FLStatus) flWriteRegister(
	struct FLContext *handle, uint32 timeout, uint8 reg, uint32 count, const uint8 *data,
	const char **error)
{
	FLStatus returnCode, fStatus;
	uint8 command[] = {reg & 0x7F, 0x00, 0x00, 0x00, 0x00};
	if ( !handle->isCommCapable ) {
		errRender(error, "flWriteRegister(): This device does not support CommFPGA");
		FAIL(FL_PROTOCOL_ERR);
	}
	flWriteLong(count, command+1);
	fStatus = flWrite(handle, command, 5, 100, error);
	CHECK_STATUS(fStatus, "flWriteRegister()", fStatus);
	fStatus = flWrite(handle, data, count, timeout, error);
	CHECK_STATUS(fStatus, "flWriteRegister()", fStatus);
	return FL_SUCCESS;
cleanup:
	return returnCode;
}

// Append a write command to the end of the write buffer
DLLEXPORT(FLStatus) flAppendWriteRegisterCommand(
	struct FLContext *handle, uint8 reg, uint32 count, const uint8 *data, const char **error)
{
	FLStatus returnCode;
	BufferStatus bStatus;
	uint8 command[] = {reg & 0x7F, 0x00, 0x00, 0x00, 0x00};
	if ( !handle->writeBuffer.data ) {
		bStatus = bufInitialise(&handle->writeBuffer, 1024, 0x00, error);
		CHECK_STATUS(bStatus, "flAppendWriteRegisterCommand()", FL_ALLOC_ERR);
	}
	flWriteLong(count, command+1);
	bStatus = bufAppendBlock(&handle->writeBuffer, command, 5, error);
	CHECK_STATUS(bStatus, "flAppendWriteRegisterCommand()", FL_ALLOC_ERR);
	bStatus = bufAppendBlock(&handle->writeBuffer, data, count, error);
	CHECK_STATUS(bStatus, "flAppendWriteRegisterCommand()", FL_ALLOC_ERR);
	return FL_SUCCESS;
cleanup:
	return returnCode;
}

// Play the write buffer into the USB cable immediately
//
DLLEXPORT(FLStatus) flPlayWriteBuffer(struct FLContext *handle, uint32 timeout, const char **error)
{
	FLStatus returnCode, fStatus;
	if ( handle->writeBuffer.length == 0 ) {
		errRender(error, "flPlayWriteBuffer(): The write buffer is empty");
		FAIL(FL_WBUF_ERR);
	}
	fStatus = flWrite(handle, handle->writeBuffer.data, handle->writeBuffer.length, timeout, error);
	CHECK_STATUS(fStatus, "flPlayWriteBuffer()", fStatus);
	return FL_SUCCESS;
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

DLLEXPORT(FLStatus) flReadRegister(
	struct FLContext *handle, uint32 timeout, uint8 reg, uint32 count, uint8 *buf,
	const char **error)
{
	FLStatus returnCode, fStatus;
	uint8 command[] = {reg | 0x80, 0x00, 0x00, 0x00, 0x00};
	if ( !handle->isCommCapable ) {
		errRender(error, "flReadRegister(): This device does not support CommFPGA");
		FAIL(FL_PROTOCOL_ERR);
	}
	flWriteLong(count, command+1);
	fStatus = flWrite(handle, command, 5, 100, error);
	CHECK_STATUS(fStatus, "flReadRegister()", fStatus);
	fStatus = flRead(handle, buf, count, timeout, error);
	CHECK_STATUS(fStatus, "flReadRegister()", fStatus);
	return FL_SUCCESS;
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
	FLStatus returnCode;
	int uStatus;
	uStatus = usb_control_msg(
		handle->device,
		USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		CMD_MODE_STATUS,          // bRequest
		0x0000,                   // wValue : off
		0x0000,                   // wMask
		(char*)statusBuffer,
		16,                       // wLength
		100                       // timeout (ms)
	);
	if ( uStatus < 0 ) {
		errRender(
			error, "getStatus(): Unable to get status: %s", usb_strerror());
		FAIL(FL_PROTOCOL_ERR);
	}
	return FL_SUCCESS;
cleanup:
	return returnCode;
}

static FLStatus trySync(struct FLContext *newCxt, const uint8 *statusBuffer) {
	FLStatus returnCode;
	if ( statusBuffer[6] ) {
		// TODO: actually honour the endpoints specified by the device. For now assume 2&4.
		newCxt->isNeroCapable = true;
		if ( syncBulkEndpoints(newCxt->device, SYNC_24, NULL) ) {
			FAIL(FL_SYNC_ERR);
		}
	}
	if ( statusBuffer[7] ) {
		// TODO: actually honour the endpoints specified by the device. For now assume 6&8.
		newCxt->isCommCapable = true;
		if ( syncBulkEndpoints(newCxt->device, SYNC_68, NULL) ) {
			FAIL(FL_SYNC_ERR);
		}
	}
	return FL_SUCCESS;
cleanup:
	return returnCode;
}

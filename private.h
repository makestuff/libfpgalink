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
#ifndef PRIVATE_H
#define PRIVATE_H

#include <makestuff.h>
#include <libbuffer.h>
#include "libfpgalink.h"

#ifdef __cplusplus
extern "C" {
#endif

	// Struct used to maintain context for most of the FPGALink operations
	struct usb_dev_handle;
	struct FLContext {
		struct usb_dev_handle *device;
		struct Buffer initBuffer;
		FLBool isNeroCapable;
		FLBool isCommCapable;
	};

	// Write some raw bytes to the FL. Sync problems (requiring power-cycle to clear) will
	// arise if these bytes are not valid FPGALink READ or WRITE commands:
	//   WRITE (six or more bytes):  [Reg,      N, Data0, Data1, ... DataN]
	//   READ (exactly five bytes):  [Reg|0x80, N]
	//     Reg is the FPGA register number (0-127)
	//     N is a big-endian uint32 representing the number of data bytes to read or write
	//
	// Immediately after sending a read command you MUST call flRead() with count=N.
	//
	// The timeout should be sufficiently large to actually transfer the number of bytes requested,
	// or sync problems (requiring power-cycle to clear) will arise. A good rule of thumb is
	// 100 + K/10 where K is the number of kilobytes to transfer.
	FLStatus flWrite(
		struct FLContext *handle, const uint8 *bytes, uint32 count, uint32 timeout,
		const char **error
	) WARN_UNUSED_RESULT;

	// Read some raw bytes from the FL. Bytes will only be available if they have been
	// previously requested with a FPGALink READ command sent with flWrite(). The count value
	// should be the same as the actual number of bytes requested by the flWrite() READ command.
	//
	// The timeout should be sufficiently large to actually transfer the number of bytes requested,
	// or sync problems (requiring power-cycle to clear) will arise. A good rule of thumb is
	// 100 + K/10 where K is the number of kilobytes to transfer.
	FLStatus flRead(
		struct FLContext *handle, uint8 *buffer, uint32 count, uint32 timeout, const char **error
	) WARN_UNUSED_RESULT;

	// Utility functions for manipulating big-endian words
	uint16 flReadWord(const uint8 *p);
	uint32 flReadLong(const uint8 *p);
	void flWriteWord(uint16 value, uint8 *p);
	void flWriteLong(uint32 value, uint8 *p);

#ifdef __cplusplus
}
#endif

#endif

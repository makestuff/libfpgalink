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
#ifndef PRIVATE_H
#define PRIVATE_H

#include <makestuff.h>
#include <libbuffer.h>
#include "libfpgalink.h"
#include "firmware.h"

#ifdef __cplusplus
extern "C" {
#endif

	// Struct used to maintain context for most of the FPGALink operations
	struct usb_dev_handle;
	struct FLContext {
		struct usb_dev_handle *device;
		struct Buffer writeBuffer;
		bool isNeroCapable;
		bool isCommCapable;
		uint8 jtagOutEP;
		uint8 jtagInEP;
		uint8 commOutEP;
		uint8 commInEP;
	};

	// Write some raw bytes to the FL. Sync problems (requiring power-cycle to clear) will
	// arise if these bytes are not valid FPGALink READ or WRITE commands:
	//   WRITE (six or more bytes):  [Chan,      N, Data0, Data1, ... DataN]
	//   READ (exactly five bytes):  [Chan|0x80, N]
	//     Chan is the FPGA channel (0-127)
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

	/**
	 * @brief Load an XSVF file and convert it to CSVF
	 *
	 * XSVF is a weird format. It represents everything (even long bit-sequences) in big-endian,
	 * which is daft because to play a bit-sequence into the JTAG chain, you have to seek forward
	 * to the end of the bit-sequence, then read backwards until you get to the beginning, playing
	 * bytes into the JTAG chain as you go. This is fine on a powerful desktop computer with lots of
	 * memory, but a resource-constrained microcontroller would have trouble.
	 *
	 * The CSVF format ("Compressed Serial Vector Format") swaps the byte order so bit sequences
	 * can be played by reading forwards, and it replaces each XSDRB, XSDRC*, XSDRE command sequence
	 * with one big XSDR command. CSVF buffers should be compressed with /c flCompressCsvf() before
	 * being written to persistent storage.
	 *
	 * @param xsvfFile The XSVF filename.
	 * @param csvfBuf A pointer to a \c Buffer to be populated with the CSVF data.
	 * @param maxBufSize A pointer to a \c uint32 which will be set on exit to the number of bytes
	 *            necessary for buffering in the playback logic. If this is greater than the
	 *            \c CSVF_BUF_SIZE defined for the firmware, bad things will happen.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the command completed successfully.
	 *     - \c FL_BUF_INIT_ERR If the CSVF buffer could not be allocated.
	 *     - \c FL_BUF_APPEND_ERR If the CSVF buffer could not be grown.
	 *     - \c FL_BUF_LOAD_ERR If the XSVF file could not be loaded.
	 *     - \c FL_UNSUPPORTED_CMD_ERR If the XSVF file contains an unsupported command.
	 *     - \c FL_UNSUPPORTED_DATA_ERR if the XSVF file contains an unsupported XENDIR or XENDDR.
	 *     - \c FL_UNSUPPORTED_SIZE_ERR if the XSVF file requires more buffer space than is available.
	 */
	DLLEXPORT(FLStatus) flLoadXsvfAndConvertToCsvf(
		const char *xsvfFile, struct Buffer *csvfBuf, uint32 *maxBufSize, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Perform run-length encoding on a CSVF buffer.
	 *
	 * @param csvfBuf A pointer to a \c Buffer containing CSVF data to be RLE-compressed.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the command completed successfully.
	 *     - \c FL_BUF_INIT_ERR If the compress buffer could not be allocated.
	 *     - \c FL_BUF_APPEND_ERR If the compress buffer could not be grown.
	 */
	DLLEXPORT(FLStatus) flCompressCsvf(
		struct Buffer *csvfBuf, const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) flLoadSvfAndConvertToCsvf(
		const char *svfFile, struct Buffer *csvfBuf, uint32 *maxBufSize, const char **error
	) WARN_UNUSED_RESULT;

	FLStatus copyFirmwareAndRewriteIDs(
		const struct FirmwareInfo *fwInfo, uint16 vid, uint16 pid,
		uint8 port, uint8 tdoBit, uint8 tdiBit, uint8 tmsBit, uint8 tckBit,
		struct Buffer *dest, const char **error
	) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif

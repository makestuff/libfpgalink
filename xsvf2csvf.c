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
#include <libfpgalink.h>
#include <libbuffer.h>
#include <liberror.h>
#include "xsvf.h"

#define bitsToBytes(x) ((x>>3) + (x&7 ? 1 : 0))

#define ENABLE_SWAP
#define BUF_SIZE 128

// Global buffer and offset used to implement the iterator
//
typedef struct {
	struct Buffer xsvfBuf;
	uint32 offset;
} XC;

// The buffer iterator. TODO: refactor to return error code on end of buffer.
//
static uint8 getNextByte(XC *xc) {
	return xc->xsvfBuf.data[xc->offset++];
}

// Read "numBytes" bytes from the stream and write them out in reverse order to the supplied buffer
// "outBuf". If ENABLE_SWAP is undefined, no swapping is done.
//
static FLStatus swapBytes(XC *xc, uint32 numBytes, struct Buffer *outBuf, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	uint8 *ptr;
	BufferStatus bStatus;
	#ifdef ENABLE_SWAP
		bStatus = bufAppendConst(outBuf, 0x00, numBytes, error);
		CHECK_STATUS(bStatus, "swapBytes()", FL_BUF_APPEND_ERR);
		ptr = outBuf->data + outBuf->length - 1;
		while ( numBytes-- ) {
			*ptr-- = getNextByte(xc);
		}
	#else
		const uint32 initLength = outBuf->length;
		bStatus = bufAppendConst(outBuf, 0x00, numBytes, error);
		CHECK_STATUS(bStatus, "swapBytes()", FL_BUF_APPEND_ERR);
		ptr = outBuf->data + initLength - 1;
		while ( numBytes-- ) {
			*ptr++ = getNextByte(xc);
		}
	#endif
cleanup:
	return returnCode;
}

// Reverse and interleave the incoming tdi, tdoExpected arrays:
// 0123456789ABCDEFGHIJ -> 9J8I7H6G5F4E3D2C1B0A
//
static FLStatus swapAndInterleaveBytes(XC *xc, uint32 numBytes, struct Buffer *outBuf, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	uint8 *ptr;
	BufferStatus bStatus;
	uint32 i = numBytes;
	bStatus = bufAppendConst(outBuf, 0x00, numBytes*2, error);
	CHECK_STATUS(bStatus, "swapAndInterleaveBytes()", FL_BUF_APPEND_ERR);
	ptr = outBuf->data + outBuf->length - 2;
	while ( i-- ) {
		*ptr = getNextByte(xc);
		ptr -= 2;
	}
	i = numBytes;
	ptr = outBuf->data + outBuf->length - 1;
	while ( i-- ) {
		*ptr = getNextByte(xc);
		ptr -= 2;
	}
cleanup:
	return returnCode;
}

static FLStatus sendXSize(struct Buffer *outBuf, uint32 xSize, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	BufferStatus bStatus;
	bStatus = bufAppendByte(outBuf, XSDRSIZE, error);
	CHECK_STATUS(bStatus, "sendXSize()", FL_BUF_APPEND_ERR);
	bStatus = bufAppendLongBE(outBuf, xSize, error);
	CHECK_STATUS(bStatus, "sendXSize()", FL_BUF_APPEND_ERR);
cleanup:
	return returnCode;
}

// Parse the XSVF, reversing the byte-ordering of all the bytestreams.
//
static FLStatus xsvfSwapBytes(XC *xc, struct Buffer *outBuf, uint32 *maxBufSize, const char **error) {
	FLStatus fStatus, returnCode = FL_SUCCESS;
	uint32 newXSize = 0, curXSize = 0, totOffset = 0;
	uint32 numBytes;
	BufferStatus bStatus;
	uint8 thisByte;
	uint32 dummy;
	bool zeroMask = false;

	if ( !maxBufSize ) {
		maxBufSize = &dummy;
	}
	*maxBufSize = 0;
	thisByte = getNextByte(xc);
	while ( thisByte != XCOMPLETE ) {
		switch ( thisByte ) {
		case XTDOMASK:{
			// Swap the XTDOMASK bytes.
			uint32 initLength;
			const uint8 *p;
			const uint8 *end;
			if ( newXSize != curXSize ) {
				curXSize = newXSize;
				sendXSize(outBuf, curXSize, error);
			}
			initLength = outBuf->length;
			numBytes = bitsToBytes(curXSize);
			bStatus = bufAppendByte(outBuf, XTDOMASK, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			fStatus = swapBytes(xc, numBytes, outBuf, error);
			CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			p = outBuf->data + initLength + 1;
			end = outBuf->data + outBuf->length;
			while ( *p == 0 && p < end ) p++;
			if ( p == end ) {
				// All zeros so delete the command
				outBuf->length = initLength;
				zeroMask = true;
			} else {
				// Keep the command
				if ( numBytes > *maxBufSize ) {
					*maxBufSize = numBytes;
				}
				zeroMask = false;
			}
			break;
		}

		case XSDRTDO:
			// Swap the tdiValue and tdoExpected bytes.
			if ( newXSize != curXSize ) {
				curXSize = newXSize;
				sendXSize(outBuf, curXSize, error);
			}
			numBytes = bitsToBytes(curXSize);
			if ( zeroMask ) {
				// The last mask was all zeros, so replace this XSDRTDO with an XSDR and throw away
				// the tdoExpected bytes.
				bStatus = bufAppendByte(outBuf, XSDR, error);
				CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
				fStatus = swapBytes(xc, numBytes, outBuf, error);
				CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
				while ( numBytes-- ) {
					getNextByte(xc);
				}
			} else {
				// The last mask was not all zeros, so we must honour the XSDRTDO's tdoExpected bytes.
				if ( numBytes > BUF_SIZE ) {
					FAIL(FL_UNSUPPORTED_SIZE_ERR);
				}
				if ( numBytes > *maxBufSize ) {
					*maxBufSize = numBytes;
				}
				bStatus = bufAppendByte(outBuf, XSDRTDO, error);
				CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
				fStatus = swapAndInterleaveBytes(xc, numBytes, outBuf, error);
				CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			}
			break;

		case XREPEAT:
			// Drop XREPEAT for now. Will probably be needed for CPLDs.
			getNextByte(xc);
			break;
			
		case XRUNTEST:
			// Copy the XRUNTEST bytes as-is.
			bStatus = bufAppendByte(outBuf, XRUNTEST, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			break;

		case XSIR:
			// Swap the XSIR bytes.
			bStatus = bufAppendByte(outBuf, XSIR, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			thisByte = getNextByte(xc);
			bStatus = bufAppendByte(outBuf, thisByte, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			fStatus = swapBytes(xc, bitsToBytes(thisByte), outBuf, error);
			CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			break;

		case XSDRSIZE:
			// Just store it; if it differs from the old one it will be sent when required
			newXSize = getNextByte(xc);  // Get MSB
			newXSize <<= 8;
			newXSize |= getNextByte(xc);
			newXSize <<= 8;
			newXSize |= getNextByte(xc);
			newXSize <<= 8;
			newXSize |= getNextByte(xc); // Get LSB
			break;

		case XSDR:
			// Copy over
			if ( newXSize != curXSize ) {
				curXSize = newXSize;
				sendXSize(outBuf, curXSize, error);
			}
			bStatus = bufAppendByte(outBuf, XSDR, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			fStatus = swapBytes(xc, bitsToBytes(curXSize), outBuf, error);
			CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			break;

		case XSDRB:
			// Roll XSDRB, XSDRC*, XSDRE into one XSDR
			curXSize = newXSize;
			sendXSize(outBuf, curXSize, error);
			totOffset = outBuf->length - 4; // each subsequent XSDRC & XSDRE updates this XSDRSIZE
			bStatus = bufAppendByte(outBuf, XSDR, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			fStatus = swapBytes(xc, bitsToBytes(newXSize), outBuf, error);
			CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			break;

		case XSDRC:
			// Just add the XSDRC data to the end of the previous XSDR
			curXSize += newXSize;
			bStatus = bufWriteLongBE(outBuf, totOffset, curXSize, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			fStatus = swapBytes(xc, bitsToBytes(newXSize), outBuf, error);
			CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			break;

		case XSDRE:
			// Just add the XSDRE data to the end of the previous XSDR
			curXSize += newXSize;
			bStatus = bufWriteLongBE(outBuf, totOffset, curXSize, error);
			CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);
			fStatus = swapBytes(xc, bitsToBytes(newXSize), outBuf, error);
			CHECK_STATUS(fStatus, "xsvfSwapBytes()", fStatus);
			break;

		case XSTATE:
			// There doesn't seem to be much point in these commands, since the other commands have
			// implied state transitions anyway. Just make sure the TAP is initialised to be at
			// Run-Test/Idle before playing the CSVF stream.
			getNextByte(xc);
			break;

		case XENDIR:
			// Only the default XENDIR state (TAPSTATE_RUN_TEST_IDLE) is supported. Fail fast if
			// there's an attempt to switch the XENDIR state to PAUSE_IR.
			thisByte = getNextByte(xc);
			if ( thisByte ) {
				FAIL(FL_UNSUPPORTED_DATA_ERR);
			}
			break;

		case XENDDR:
			// Only the default XENDDR state (TAPSTATE_RUN_TEST_IDLE) is supported. Fail fast if
			// there's an attempt to switch the XENDDR state to PAUSE_DR.
			thisByte = getNextByte(xc);
			if ( thisByte ) {
				FAIL(FL_UNSUPPORTED_DATA_ERR);
			}
			break;

		default:
			// All other commands are unsupported, so fail if they're encountered.
			FAIL(FL_UNSUPPORTED_CMD_ERR);
		}
		thisByte = getNextByte(xc);
	}

	// Add the XCOMPLETE command
	bStatus = bufAppendByte(outBuf, XCOMPLETE, error);
	CHECK_STATUS(bStatus, "xsvfSwapBytes()", FL_BUF_APPEND_ERR);

cleanup:
	return returnCode;
}

static FLStatus compress(const struct Buffer *inBuf, struct Buffer *outBuf, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	const uint8 *runStart, *runEnd, *bufEnd, *chunkStart, *chunkEnd;
	uint32 runLen, chunkLen;
	BufferStatus bStatus;
	bufEnd = inBuf->data + inBuf->length;
	runStart = chunkStart = inBuf->data;
	bStatus = bufAppendByte(outBuf, 0x00, error);  // Hdr byte: defaults
	CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
	while ( runStart < bufEnd ) {
		// Find next zero
		while ( runStart < bufEnd && *runStart ) {
			runStart++;
		}
		
		// Remember the position of the zero
		runEnd = runStart;

		// Find the end of this run of zeros
		while ( runEnd < bufEnd && !*runEnd ) {
			runEnd++;
		}
		
		// Get the length of this run
		runLen = runEnd - runStart;
		
		// If this run is more than four zeros, break the chunk
		if ( runLen > 8 || runEnd == bufEnd ) {
			chunkEnd = runStart;
			chunkLen = chunkEnd - chunkStart;

			// There is now a chunk starting at chunkStart and ending at chunkEnd (length chunkLen),
			// Followed by a run of zeros starting at runStart and ending at runEnd (length runLen).
			//printf("Chunk: %d bytes followed by %d zeros\n", chunkLen, runLen);
			if ( chunkLen < 256 ) {
				// Short chunk: uint8
				bStatus = bufAppendByte(outBuf, (uint8)chunkLen, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
			} else if ( chunkLen < 65536 ) {
				// Medium chunk: uint16 (big-endian)
				bStatus = bufAppendByte(outBuf, 0x00, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
				bStatus = bufAppendWordBE(outBuf, (uint16)chunkLen, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
			} else {
				// Long chunk: uint32 (big-endian)
				bStatus = bufAppendConst(outBuf, 0x00, 3, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
				bStatus = bufAppendLongBE(outBuf, chunkLen, error);
			}
			while ( chunkStart < chunkEnd ) {
				bStatus = bufAppendByte(outBuf, *chunkStart++, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
			}
			if ( runLen < 256 ) {
				// Short run: uint8
				bStatus = bufAppendByte(outBuf, (uint8)runLen, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
			} else if ( runLen < 65536 ) {
				// Medium run: uint16 (big-endian)
				bStatus = bufAppendByte(outBuf, 0x00, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
				bStatus = bufAppendWordBE(outBuf, (uint16)runLen, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
			} else {
				// Long run: uint32 (big-endian)
				bStatus = bufAppendConst(outBuf, 0x00, 3, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
				bStatus = bufAppendLongBE(outBuf, runLen, error);
				CHECK_STATUS(bStatus, "compress()", FL_BUF_APPEND_ERR);
			}

			chunkStart = runEnd;
		}
		
		// Start the next round from the end of this run
		runStart = runEnd;
	}

cleanup:
	return returnCode;
}

DLLEXPORT(FLStatus) flLoadXsvfAndConvertToCsvf(
	const char *xsvfFile, struct Buffer *csvfBuf, uint32 *maxBufSize, const char **error)
{
	FLStatus fStatus, returnCode = FL_SUCCESS;
	BufferStatus bStatus;
	XC xc;
	xc.offset = 0;
	bStatus = bufInitialise(&xc.xsvfBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, "flLoadXsvfAndConvertToCsvf()", FL_BUF_INIT_ERR);
	bStatus = bufAppendFromBinaryFile(&xc.xsvfBuf, xsvfFile, error);
	CHECK_STATUS(bStatus, "flLoadXsvfAndConvertToCsvf()", FL_BUF_LOAD_ERR);
	fStatus = xsvfSwapBytes(&xc, csvfBuf, maxBufSize, error);
	CHECK_STATUS(fStatus, "flLoadXsvfAndConvertToCsvf()", fStatus);
cleanup:
	bufDestroy(&xc.xsvfBuf);
	return returnCode;
}

DLLEXPORT(FLStatus) flCompressCsvf(
	struct Buffer *csvfBuf, const char **error)
{
	FLStatus fStatus, returnCode = FL_SUCCESS;
	struct Buffer swapBuf = {0,};
	BufferStatus bStatus;
	bStatus = bufInitialise(&swapBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, "flCompressCsvf()", FL_BUF_INIT_ERR);
	fStatus = compress(csvfBuf, &swapBuf, error);
	CHECK_STATUS(fStatus, "flCompressCsvf()", fStatus);
	bufSwap(csvfBuf, &swapBuf);
cleanup:
	bufDestroy(&swapBuf);
	return returnCode;
}

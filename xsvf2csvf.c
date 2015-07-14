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
#include "private.h"

#define ENABLE_SWAP

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
	FLStatus retVal = FL_SUCCESS;
	uint8 *ptr;
	BufferStatus bStatus;
	#ifdef ENABLE_SWAP
		bStatus = bufAppendConst(outBuf, 0x00, numBytes, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "swapBytes()");
		ptr = outBuf->data + outBuf->length - 1;
		while ( numBytes-- ) {
			*ptr-- = getNextByte(xc);
		}
	#else
		const uint32 initLength = outBuf->length;
		bStatus = bufAppendConst(outBuf, 0x00, numBytes, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "swapBytes()");
		ptr = outBuf->data + initLength - 1;
		while ( numBytes-- ) {
			*ptr++ = getNextByte(xc);
		}
	#endif
cleanup:
	return retVal;
}

// Reverse and interleave the incoming tdi, tdoExpected arrays:
// 0123456789ABCDEFGHIJ -> 9J8I7H6G5F4E3D2C1B0A
//
static FLStatus swapAndInterleaveBytes(XC *xc, uint32 numBytes, struct Buffer *outBuf, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	uint8 *ptr;
	BufferStatus bStatus;
	uint32 i = numBytes;
	bStatus = bufAppendConst(outBuf, 0x00, numBytes*2, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "swapAndInterleaveBytes()");
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
	return retVal;
}

static FLStatus sendXSize(struct Buffer *outBuf, uint32 xSize, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	BufferStatus bStatus;
	bStatus = bufAppendByte(outBuf, XSDRSIZE, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "sendXSize()");
	bStatus = bufAppendLongBE(outBuf, xSize, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "sendXSize()");
cleanup:
	return retVal;
}

// Parse the XSVF, reversing the byte-ordering of all the bytestreams.
//
static FLStatus xsvfSwapBytes(XC *xc, struct Buffer *outBuf, uint32 *maxBufSize, const char **error) {
	FLStatus fStatus, retVal = FL_SUCCESS;
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
			initLength = (uint32)outBuf->length;
			numBytes = bitsToBytes(curXSize);
			bStatus = bufAppendByte(outBuf, XTDOMASK, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			fStatus = swapBytes(xc, numBytes, outBuf, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
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
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
				fStatus = swapBytes(xc, numBytes, outBuf, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
				while ( numBytes-- ) {
					getNextByte(xc);
				}
			} else {
				// The last mask was not all zeros, so we must honour the XSDRTDO's tdoExpected bytes.
				CHECK_STATUS(
					numBytes > BUF_SIZE, FL_UNSUPPORTED_SIZE_ERR, cleanup,
					"xsvfSwapBytes(): Previous mask was nonzero, but no room to compare %d bytes", numBytes);
				if ( numBytes > *maxBufSize ) {
					*maxBufSize = numBytes;
				}
				bStatus = bufAppendByte(outBuf, XSDRTDO, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
				fStatus = swapAndInterleaveBytes(xc, numBytes, outBuf, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
			}
			break;

		case XREPEAT:
			// Drop XREPEAT for now. Will probably be needed for CPLDs.
			getNextByte(xc);
			break;
			
		case XRUNTEST:
			// Copy the XRUNTEST bytes as-is.
			bStatus = bufAppendByte(outBuf, XRUNTEST, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			bStatus = bufAppendByte(outBuf, getNextByte(xc), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			break;

		case XSIR:
			// Swap the XSIR bytes.
			bStatus = bufAppendByte(outBuf, XSIR, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			thisByte = getNextByte(xc);
			bStatus = bufAppendByte(outBuf, thisByte, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			fStatus = swapBytes(xc, (uint32)bitsToBytes(thisByte), outBuf, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
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
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			fStatus = swapBytes(xc, bitsToBytes(curXSize), outBuf, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
			break;

		case XSDRB:
			// Roll XSDRB, XSDRC*, XSDRE into one XSDR
			curXSize = newXSize;
			sendXSize(outBuf, curXSize, error);
			totOffset = (uint32)outBuf->length - 4; // each subsequent XSDRC & XSDRE updates this XSDRSIZE
			bStatus = bufAppendByte(outBuf, XSDR, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			fStatus = swapBytes(xc, bitsToBytes(newXSize), outBuf, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
			break;

		case XSDRC:
			// Just add the XSDRC data to the end of the previous XSDR
			curXSize += newXSize;
			bStatus = bufWriteLongBE(outBuf, totOffset, curXSize, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			fStatus = swapBytes(xc, bitsToBytes(newXSize), outBuf, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
			break;

		case XSDRE:
			// Just add the XSDRE data to the end of the previous XSDR
			curXSize += newXSize;
			bStatus = bufWriteLongBE(outBuf, totOffset, curXSize, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");
			fStatus = swapBytes(xc, bitsToBytes(newXSize), outBuf, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "xsvfSwapBytes()");
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
			CHECK_STATUS(
				thisByte, FL_UNSUPPORTED_DATA_ERR, cleanup,
				"xsvfSwapBytes(): Only XENDIR(TAPSTATE_RUN_TEST_IDLE) is supported!");
			break;

		case XENDDR:
			// Only the default XENDDR state (TAPSTATE_RUN_TEST_IDLE) is supported. Fail fast if
			// there's an attempt to switch the XENDDR state to PAUSE_DR.
			thisByte = getNextByte(xc);
			CHECK_STATUS(
				thisByte, FL_UNSUPPORTED_DATA_ERR, cleanup,
				"xsvfSwapBytes(): Only XENDDR(TAPSTATE_RUN_TEST_IDLE) is supported!");
			break;

		default:
			// All other commands are unsupported, so fail if they're encountered.
			CHECK_STATUS(
				true, FL_UNSUPPORTED_CMD_ERR, cleanup,
				"xsvfSwapBytes(): Unsupported command 0x%02X!", thisByte);
		}
		thisByte = getNextByte(xc);
	}

	// Add the XCOMPLETE command
	bStatus = bufAppendByte(outBuf, XCOMPLETE, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "xsvfSwapBytes()");

cleanup:
	return retVal;
}

DLLEXPORT(FLStatus) flLoadXsvfAndConvertToCsvf(
	const char *xsvfFile, struct Buffer *csvfBuf, uint32 *maxBufSize, const char **error)
{
	FLStatus fStatus, retVal = FL_SUCCESS;
	BufferStatus bStatus;
	XC xc;
	xc.offset = 0;
	bStatus = bufInitialise(&xc.xsvfBuf, 0x20000, 0, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadXsvfAndConvertToCsvf()");
	bStatus = bufAppendFromBinaryFile(&xc.xsvfBuf, xsvfFile, error);
	CHECK_STATUS(bStatus, FL_FILE_ERR, cleanup, "flLoadXsvfAndConvertToCsvf()");
	fStatus = xsvfSwapBytes(&xc, csvfBuf, maxBufSize, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flLoadXsvfAndConvertToCsvf()");
cleanup:
	bufDestroy(&xc.xsvfBuf);
	return retVal;
}

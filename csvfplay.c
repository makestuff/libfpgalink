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
// Uncomment for help debugging JTAG issues
//#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <makestuff.h>
#include <liberror.h>
#include "private.h"
#include "vendorCommands.h"
#include "xsvf.h"
#include "csvfplay.h"

// -------------------------------------------------------------------------------------------------
// Declaration of private types & functions
// -------------------------------------------------------------------------------------------------

static void dumpSimple(const unsigned char *input, unsigned int length, char *p);
static bool tdoMatchFailed(
	const uint8 *tdoData, const uint8 *tdoMask, const uint8 *tdoExpected, uint32 numBytes);

// -------------------------------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------------------------------

// Play the CSVF stream into the JTAG port.
//
FLStatus csvfPlay(struct FLContext *handle, const uint8 *csvfData, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	FLStatus fStatus;
	uint8 thisByte, numBits;
	uint32 numBytes;
	uint8 *tdoPtr, *tdiPtr;
	uint8 i;
	uint32 xsdrSize = 0;
	uint32 xruntest = 0;
	uint8 tdoMask[BUF_SIZE];
	uint8 tdiData[BUF_SIZE];
	uint8 tdoData[BUF_SIZE];
	uint8 tdoExpected[BUF_SIZE];
	
	char data[BUF_SIZE*2+1];
	char mask[BUF_SIZE*2+1];
	char expected[BUF_SIZE*2+1];
	
	uint8 *tdiAll;
	const uint8 *ptr = csvfData;

	fStatus = jtagClockFSM(handle, 0x0000001F, 6, error);  // Reset TAP, goto Run-Test/Idle
	CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");

	thisByte = *ptr++;
	while ( thisByte != XCOMPLETE ) {
		switch ( thisByte ) {
		case XTDOMASK:
			#ifdef DEBUG
				printf("XTDOMASK(");
			#endif
			numBytes = bitsToBytes(xsdrSize);
			tdoPtr = tdoMask;
			while ( numBytes-- ) {
				thisByte = *ptr++;
				#ifdef DEBUG
					printf("%02X", thisByte);
				#endif
				*tdoPtr++ = thisByte;
			}
			#ifdef DEBUG
				printf(")\n");
			#endif
			break;

		case XRUNTEST:
			xruntest = *ptr++;
			xruntest <<= 8;
			xruntest |= *ptr++;
			xruntest <<= 8;
			xruntest |= *ptr++;
			xruntest <<= 8;
			xruntest |= *ptr++;
			#ifdef DEBUG
				printf("XRUNTEST(%08X)\n", xruntest);
			#endif
			break;

		case XSIR:
			fStatus = jtagClockFSM(handle, 0x00000003, 4, error);  // -> Shift-IR
			CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			numBits = *ptr++;
			#ifdef DEBUG
				printf("XSIR(%02X, ", numBits);
			#endif
				numBytes = bitsToBytes((uint32)numBits);
			tdiPtr = tdiData;
			while ( numBytes-- ) {
				thisByte = *ptr++;
				#ifdef DEBUG
					printf("%02X", thisByte);
				#endif
				*tdiPtr++ = thisByte;
			}
			#ifdef DEBUG
				printf(")\n");
			#endif
			fStatus = jtagShiftInOnly(handle, numBits, tdiData, true, error);  // -> Exit1-DR
			CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			fStatus = jtagClockFSM(handle, 0x00000001, 2, error);  // -> Run-Test/Idle
			CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			if ( xruntest ) {
				fStatus = jtagClocks(handle, xruntest, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			}
			break;

		case XSDRSIZE:
			xsdrSize = *ptr++;
			xsdrSize <<= 8;
			xsdrSize |= *ptr++;
			xsdrSize <<= 8;
			xsdrSize |= *ptr++;
			xsdrSize <<= 8;
			xsdrSize |= *ptr++;
			#ifdef DEBUG
				printf("XSDRSIZE(%08X)\n", xsdrSize);
			#endif
			break;

		case XSDRTDO:
			numBytes = bitsToBytes(xsdrSize);
			tdiPtr = tdiData;
			tdoPtr = tdoExpected;
			while ( numBytes-- ) {
				*tdiPtr++ = *ptr++;
				*tdoPtr++ = *ptr++;
			}
			numBytes = bitsToBytes(xsdrSize);
			i = 0;
			do {
				fStatus = jtagClockFSM(handle, 0x00000001, 3, error);  // -> Shift-DR
				CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
				fStatus = jtagShiftInOut(handle, xsdrSize, tdiData, tdoData, true, error);  // -> Exit1-DR
				CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
				fStatus = jtagClockFSM(handle, 0x0000001A, 6, error);  // -> Run-Test/Idle
				CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
				if ( xruntest ) {
					fStatus = jtagClocks(handle, xruntest, error);
					CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
				}
				i++;
				#ifdef DEBUG
					dumpSimple(tdoData, numBytes, data);
					dumpSimple(tdoMask, numBytes, mask);
					dumpSimple(tdoExpected, numBytes, expected);
					printf("XSDRTDO(attempt: %d; mask: %s; expecting: %s; got: %s)\n", i, mask, expected, data);
				#endif
			} while ( tdoMatchFailed(tdoData, tdoMask, tdoExpected, numBytes) && i < 32 );

			if ( i == 32 ) {
				dumpSimple(tdoData, numBytes, data);
				dumpSimple(tdoMask, numBytes, mask);
				dumpSimple(tdoExpected, numBytes, expected);
				CHECK_STATUS(
					true, FL_PROG_SVF_COMPARE, cleanup,
					"csvfPlay(): XSDRTDO failed:\n  Got: %s\n  Mask: %s\n  Expecting: %s",
					data, mask, expected);
			}
			break;

		case XSDR:
			#ifdef DEBUG
				// TODO: Need to print actual TDO data too
				printf("XSDR(%08X)\n", xsdrSize);
			#endif
			fStatus = jtagClockFSM(handle, 0x00000001, 3, error);  // -> Shift-DR
			CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			numBytes = bitsToBytes(xsdrSize);
			tdiAll = malloc(numBytes);
			tdiPtr = tdiAll;
			while ( numBytes-- ) {
				*tdiPtr++ = *ptr++;
			}
			fStatus = jtagShiftInOnly(handle, xsdrSize, tdiAll, true, error);  // -> Exit1-DR
			free(tdiAll);
			CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			fStatus = jtagClockFSM(handle, 0x00000001, 2, error);  // -> Run-Test/Idle
			CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			if ( xruntest ) {
				fStatus = jtagClocks(handle, xruntest, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "csvfPlay()");
			}
			break;

		default:
			CHECK_STATUS(
				true, FL_PROG_SVF_UNKNOWN_CMD, cleanup,
				"csvfPlay(): Unsupported command 0x%02X", thisByte);
		}
		thisByte = *ptr++;
	}
cleanup:
	return retVal;
}

// -------------------------------------------------------------------------------------------------
// Implementation of private functions
// -------------------------------------------------------------------------------------------------

static const char *const nibbles = "0123456789ABCDEF";

// Dump some hex bytes to a buffer.
//
static void dumpSimple(const unsigned char *input, unsigned int length, char *p) {
	uint8 upperNibble, lowerNibble;
	while ( length ) {
		upperNibble = lowerNibble = *input++;
		upperNibble >>= 4;
		lowerNibble &= 15;
		*p++ = nibbles[upperNibble];
		*p++ = nibbles[lowerNibble];
		--length;
	}
	*p = '\0';
}

static bool tdoMatchFailed(
	const uint8 *tdoData, const uint8 *tdoMask, const uint8 *tdoExpected, uint32 numBytes)
{
	while ( numBytes-- ) {
		if ( (*tdoData & *tdoMask) != (*tdoExpected & *tdoMask) ) {
			return true;
		}
		tdoData++;
		tdoExpected++;
		tdoMask++;
	}
	return false;
}


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
#include <liberror.h>
#include <libnero.h>
#include "firmware/defs.h"
#include "xsvf.h"
#include "csvfplay.h"
#include "csvfreader.h"

// -------------------------------------------------------------------------------------------------
// Declaration of private types & functions
// -------------------------------------------------------------------------------------------------

static void dumpSimple(const unsigned char *input, unsigned int length, char *p);
static bool tdoMatchFailed(
	const uint8 *tdoData, const uint8 *tdoMask, const uint8 *tdoExpected, uint32 numBytes);

// -------------------------------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------------------------------

// Play the uncompressed CSVF stream into the JTAG port.
//
int csvfPlay(const uint8 *csvfData, bool isCompressed, struct NeroHandle *nero, const char **error) {
	int returnCode;
	NeroStatus nStatus;
	uint8 thisByte;
	uint32 numBytes;
	uint8 *tdoPtr, *tdiPtr;
	uint8 i;
	uint32 xsdrSize = 0;
	uint32 xruntest = 0;
	uint8 tdoMask[128];
	uint8 tdiData[128];
	uint8 tdoData[128];
	uint8 tdoExpected[128];
	uint8 *tdiAll;
	struct Context cp;

	nStatus = neroClockFSM(nero, 0x0000001F, 6, error);  // Reset TAP, goto Run-Test/Idle
	CHECK_STATUS(nStatus, "csvfPlay()", nStatus);

	if ( csvfInitReader(&cp, csvfData, isCompressed) ) {
		// Header byte may be used for something later. For now, ensure it's zero.
		errRender(error, "csvfPlay(): Bad CSVF header!");
		FAIL(CPLAY_HEADER_ERR);
	}

	thisByte = csvfGetByte(&cp);
	while ( thisByte != XCOMPLETE ) {
		switch ( thisByte ) {
		case XTDOMASK:
			numBytes = bitsToBytes(xsdrSize);
			tdoPtr = tdoMask;
			while ( numBytes-- ) {
				*tdoPtr++ = csvfGetByte(&cp);
			}
			break;

		case XRUNTEST:
			xruntest = csvfGetLong(&cp);
			break;

		case XSIR:
			nStatus = neroClockFSM(nero, 0x00000003, 4, error);  // -> Shift-IR
			CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			thisByte = csvfGetByte(&cp);
			numBytes = bitsToBytes(thisByte);
			tdiPtr = tdiData;
			while ( numBytes-- ) {
				*tdiPtr++ = csvfGetByte(&cp);
			}
			nStatus = neroShift(nero, thisByte, tdiData, NULL, true, error);  // -> Exit1-DR
			CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			nStatus = neroClockFSM(nero, 0x00000001, 2, error);  // -> Run-Test/Idle
			CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			if ( xruntest ) {
				nStatus = neroClocks(nero, xruntest, error);
				CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			}
			break;

		case XSDRSIZE:
			xsdrSize = csvfGetLong(&cp);
			break;

		case XSDRTDO:
			numBytes = bitsToBytes(xsdrSize);
			tdiPtr = tdiData;
			tdoPtr = tdoExpected;
			while ( numBytes-- ) {
				*tdiPtr++ = csvfGetByte(&cp);
				*tdoPtr++ = csvfGetByte(&cp);
			}
			numBytes = bitsToBytes(xsdrSize);
			i = 0;
			do {
				nStatus = neroClockFSM(nero, 0x00000001, 3, error);  // -> Shift-DR
				CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
				nStatus = neroShift(nero, xsdrSize, tdiData, tdoData, true, error);  // -> Exit1-DR
				CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
				nStatus = neroClockFSM(nero, 0x0000001A, 6, error);  // -> Run-Test/Idle
				CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
				if ( xruntest ) {
					nStatus = neroClocks(nero, xruntest, error);
					CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
				}
				i++;
			} while ( tdoMatchFailed(tdoData, tdoMask, tdoExpected, numBytes) && i < 32 );

			if ( i == 32 ) {
				char data[CSVF_BUF_SIZE*2+1];
				char mask[CSVF_BUF_SIZE*2+1];
				char expected[CSVF_BUF_SIZE*2+1];
				dumpSimple(tdoData, numBytes, data);
				dumpSimple(tdoMask, numBytes, mask);
				dumpSimple(tdoExpected, numBytes, expected);
				errRender(
					error,
					"csvfPlay(): XSDRTDO failed:\n  Got: %s\n  Mask: %s\n  Expecting: %s",
					data, mask, expected);
				FAIL(CPLAY_COMPARE_ERR);
			}
			break;

		case XSDR:
			nStatus = neroClockFSM(nero, 0x00000001, 3, error);  // -> Shift-DR
			CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			numBytes = bitsToBytes(xsdrSize);
			tdiAll = malloc(numBytes);
			tdiPtr = tdiAll;
			while ( numBytes-- ) {
				*tdiPtr++ = csvfGetByte(&cp);
			}
			nStatus = neroShift(nero, xsdrSize, tdiAll, NULL, true, error);  // -> Exit1-DR
			free(tdiAll);
			CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			nStatus = neroClockFSM(nero, 0x00000001, 2, error);  // -> Run-Test/Idle
			CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			if ( xruntest ) {
				nStatus = neroClocks(nero, xruntest, error);
				CHECK_STATUS(nStatus, "csvfPlay()", nStatus);
			}
			break;

		default:
			errRender(error, "csvfPlay(): Unsupported command 0x%02X", thisByte);
			FAIL(CPLAY_UNKNOWN_CMD_ERR);
		}
		thisByte = csvfGetByte(&cp);
	}
	returnCode = NERO_SUCCESS;
cleanup:
	return returnCode;
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


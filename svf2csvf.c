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
#include <libbuffer.h>
#include <liberror.h>
#include "svf2csvf.h"
#include "xsvf.h"
#include "private.h"

static FLStatus shiftLeft(
	struct Buffer *buffer, uint32 numBits, uint32 shiftCount, const char **error
) WARN_UNUSED_RESULT;

static FLStatus initBitStore(
	struct BitStore *store, const char **error
) WARN_UNUSED_RESULT;

static FLStatus processLine(
	struct BitStore *store,
	uint32 newLength, const char *tdi, const char *tdo, const char *mask,
	const char **error
) WARN_UNUSED_RESULT;

static FLStatus appendSwapped(
	struct Buffer *buf, const uint8 *src, uint32 count, const char **error
) WARN_UNUSED_RESULT;

static bool getHexNibble(char hexDigit, uint8 *nibble) {
	if ( hexDigit >= '0' && hexDigit <= '9' ) {
		*nibble = (uint8)(hexDigit - '0');
		return false;
	} else if ( hexDigit >= 'a' && hexDigit <= 'f' ) {
		*nibble = (uint8)(hexDigit - 'a' + 10);
		return false;
	} else if ( hexDigit >= 'A' && hexDigit <= 'F' ) {
		*nibble = (uint8)(hexDigit - 'A' + 10);
		return false;
	} else {
		return true;
	}
}

static int getHexByte(const char *p, uint8 *byte) {
	uint8 upperNibble;
	uint8 lowerNibble;
	if ( !getHexNibble(p[0], &upperNibble) && !getHexNibble(p[1], &lowerNibble) ) {
		*byte = (uint8)((upperNibble << 4) | lowerNibble);
		byte += 2;
		return 0;
	} else {
		return 1;
	}
}

uint32 readLongBE(const uint8 *p) {
	uint32 result;
	result = p[0];
	result <<= 8;
	result |= p[1];
	result <<= 8;
	result |= p[2];
	result <<= 8;
	result |= p[3];
	return result;
}

FLStatus readBytes(
	struct Buffer *buffer, const char *hexDigits, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	uint32 length = (uint32)strlen(hexDigits);
	uint8 *p = buffer->data;
	BufferStatus bStatus;
	CHECK_STATUS(
		length & 1, FL_SVF_PARSE_ERR, cleanup,
		"readBytes(): I need an even number of hex digits");
	bufZeroLength(buffer);
	length >>= 1;  // Number of bytes
	bStatus = bufAppendConst(buffer, 0x00, length, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "readBytes()");
	while ( length-- ) {
		CHECK_STATUS(getHexByte(hexDigits, p++), FL_SVF_PARSE_ERR, cleanup, "readBytes()");
		hexDigits += 2;
	}
cleanup:
	return retVal;
}

static FLStatus shiftLeft(
	struct Buffer *buffer, uint32 numBits, uint32 shiftCount, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	uint32 shiftBytes = shiftCount>>3;
	uint32 shiftBits = shiftCount&7;
	uint16 accum;
	const uint8 *p = buffer->data;
	const uint8 *const end = buffer->data + buffer->length;
	struct Buffer newBuffer = {0,};
	BufferStatus bStatus;
	if ( shiftBits ) {
		bStatus = bufInitialise(&newBuffer, 1024, 0x00, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "shiftLeft()");
		numBits &= 7;  // Now the number of significant bits in first byte.
		if ( numBits ) {
			numBits = 8 - numBits; // Now the number of insignificant bits in first byte.
		}
		accum = p[0];
		if ( p < end ) {
			accum = (uint16)(accum >> (8-shiftBits));
			if ( shiftBits > numBits ) {
				// We're shifting by more than the number of insignificant bits
				bStatus = bufAppendByte(&newBuffer, (uint8)(accum&0xFF), error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "shiftLeft()");
			}
			accum = (uint16)((p[0]<<8) + p[1]);
			p++;
			while ( p < end ) {
				accum = (uint16)(accum >> (8-shiftBits));
				bStatus = bufAppendByte(&newBuffer, (uint8)(accum&0xFF), error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "shiftLeft()");
				accum = (uint16)((p[0]<<8) + p[1]);
				p++;
			}
		}
		accum &= 0xFF00;
		accum = (uint16)(accum >> (8-shiftBits));
		bStatus = bufAppendByte(&newBuffer, (uint8)(accum&0xFF), error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "shiftLeft()");
		bufSwap(&newBuffer, buffer);
	}
	if ( shiftBytes ) {
		bStatus = bufAppendConst(buffer, 0x00, shiftBytes, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "shiftLeft()");
	}
cleanup:
	bufDestroy(&newBuffer);
	return retVal;
}

FLStatus headTail(
	struct Buffer *dataBuf, struct Buffer *headBuf, struct Buffer *tailBuf,
	uint32 dataBits, uint32 headBits, uint32 tailBits, const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	const uint8 *src = headBuf->data;
	const uint8 *dstEnd;
	uint8 *dst;
	struct Buffer newBuffer = {0,};
	BufferStatus bStatus;

	// Shift data left by headBits
	fStatus = shiftLeft(dataBuf, dataBits, headBits, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "headTail()");

	//dumpSimple(dataBuf->data, dataBuf->length);

	// Add headBits to dataBits to get the new total
	dataBits += headBits;

	// Add header
	dstEnd = dst = dataBuf->data + dataBuf->length;
	dst -= headBuf->length;
	while ( dst < dstEnd ) {
		*dst++ |= *src++;
	}

	if ( tailBits ) {
		uint32 tailShift = dataBits & 7;
		uint32 bytesFromTail;
		// Shift the tail to align with the MSB of the data
		if ( tailShift ) {
			fStatus = shiftLeft(tailBuf, tailBits, tailShift, error);
			CHECK_STATUS(fStatus, fStatus, cleanup, "headTail()");
		}

		// How much data do we need to copy from the tail?
		bytesFromTail = (uint32)tailBuf->length;
		src = tailBuf->data;

		// Init new buffer
		bStatus = bufInitialise(&newBuffer, 1024, 0x00, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "headTail()");

		dataBits += tailBits;
		if ( bitsToBytes(dataBits) == dataBuf->length + bytesFromTail ) {
			// There's no overlap, so copy back-to-back
			bStatus = bufAppendBlock(&newBuffer, src, bytesFromTail, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "headTail()");
			bStatus = bufAppendByte(&newBuffer, *dataBuf->data, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "headTail()");
		} else if ( bitsToBytes(dataBits) == dataBuf->length + bytesFromTail - 1 ) {
			// There's a single byte overlap, so OR the overlap byte
			bytesFromTail--;
			bStatus = bufAppendBlock(&newBuffer, src, bytesFromTail, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "headTail()");
			src += bytesFromTail;
			bStatus = bufAppendByte(&newBuffer, *src | *dataBuf->data, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "headTail()");
		} else {
			// Ooops, this should never happen!
			CHECK_STATUS(true, FL_INTERNAL_ERR, cleanup, "headTail(): Internal error");
		}
		bStatus = bufAppendBlock(&newBuffer, dataBuf->data+1, dataBuf->length-1, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "headTail()");
		bufSwap(&newBuffer, dataBuf);
	}
	
cleanup:
	bufDestroy(&newBuffer);
	return retVal;
}

static FLStatus initBitStore(struct BitStore *store, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	BufferStatus bStatus;
	store->numBits = 0;
	bStatus = bufInitialise(&store->tdi, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "initBitStore()");
	bStatus = bufInitialise(&store->tdo, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "initBitStore()");
	bStatus = bufInitialise(&store->mask, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "initBitStore()");
cleanup:
	return retVal;
}

static void destroyBitStore(struct BitStore *store) {
	store->numBits = 0;
	bufDestroy(&store->tdi);
	bufDestroy(&store->tdo);
	bufDestroy(&store->mask);
}

FLStatus cxtInitialise(struct ParseContext *cxt, const char **error) {
	FLStatus retVal = FL_SUCCESS, fStatus;
	BufferStatus bStatus;
	fStatus = initBitStore(&cxt->dataHead, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "cxtInitialise()");
	fStatus = initBitStore(&cxt->dataBody, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "cxtInitialise()");
	fStatus = initBitStore(&cxt->dataTail, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "cxtInitialise()");
	fStatus = initBitStore(&cxt->insnHead, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "cxtInitialise()");
	fStatus = initBitStore(&cxt->insnBody, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "cxtInitialise()");
	fStatus = initBitStore(&cxt->insnTail, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "cxtInitialise()");
	cxt->curLength = 0;
	bStatus = bufInitialise(&cxt->curMaskBuf, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "cxtInitialise()");
	cxt->curMaskBits = 0;
	cxt->numCommands = 0;
	cxt->newMaskWritten = false;
cleanup:
	return retVal;
}

void cxtDestroy(struct ParseContext *cxt) {
	destroyBitStore(&cxt->dataHead);
	destroyBitStore(&cxt->dataBody);
	destroyBitStore(&cxt->dataTail);
	destroyBitStore(&cxt->insnHead);
	destroyBitStore(&cxt->insnBody);
	destroyBitStore(&cxt->insnTail);
	bufDestroy(&cxt->curMaskBuf);
}

typedef enum {
	HEAD = 'H',
	TAIL = 'T',
	BODY = 'S'
} ShiftOperation;

/**
 * Read the data from the tdi, tdo and mask ASCII hex byte sequences into the supplied BitStore as
 * binary data.
 */
static FLStatus processLine(
	struct BitStore *store,
	uint32 newLength, const char *tdi, const char *tdo, const char *mask,
	const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	BufferStatus bStatus;

	// The TDI & MASK values are remembered; the TDO value defaults to all zeros. If the length
	// changes, all remembered values are forgotten.
	bufZeroLength(&store->tdo);
	if ( newLength ) {
		bStatus = bufAppendConst(&store->tdo, 0x00, bitsToBytes(newLength), error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "processLine()");
	}
	if ( store->numBits != newLength ) {
		// The length changed, so we can't reuse previous values, and new TDI data is required:
		// "The TDI parameter must be explicitly specified for the first command or when the
		// length changes."
		bufZeroLength(&store->tdi);
		bufZeroLength(&store->mask);
		if ( newLength ) {
			bStatus = bufAppendConst(&store->tdi, 0x00, bitsToBytes(newLength), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "processLine()");
			bStatus = bufAppendConst(&store->mask, 0xFF, bitsToBytes(newLength), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "processLine()");
		}
	}
	store->numBits = newLength;
	if ( tdi ) {
		fStatus = readBytes(&store->tdi, tdi, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "processLine()");
	}
	if ( tdo ) {
		fStatus = readBytes(&store->tdo, tdo, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "processLine()");
	}
	if ( mask ) {
		fStatus = readBytes(&store->mask, mask, error);
		CHECK_STATUS(fStatus, fStatus, cleanup, "processLine()");
	}

cleanup:
	return retVal;
}

/**
 * Return true if the buffer is all zeros
 */
static bool isAllZero(struct Buffer *buf) {
	const uint8 *p = buf->data;
	uint32 length = (uint32)buf->length;
	while ( length-- ) {
		if ( *p++ ) {
			return false;
		}
	}
	return true;
}

// Reverse and interleave the incoming tdi, tdoExpected arrays:
// 0123456789ABCDEFGHIJ -> 9J8I7H6G5F4E3D2C1B0A
//
static FLStatus appendSwappedAndInterleaved(
	struct Buffer *buf, const uint8 *tdi, const uint8 *exp, uint32 count, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	BufferStatus bStatus;
	const uint8 *tdiPtr = tdi + count - 1;
	const uint8 *expPtr = exp + count - 1;
	while ( count-- ) {
		bStatus = bufAppendByte(buf, *tdiPtr--, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "appendSwapped()");
		bStatus = bufAppendByte(buf, *expPtr--, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "appendSwapped()");
	}
cleanup:
	return retVal;
}

static FLStatus appendSwapped(
	struct Buffer *buf, const uint8 *src, uint32 count, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	BufferStatus bStatus;
	src += count - 1;
	while ( count-- ) {
		bStatus = bufAppendByte(buf, *src--, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "appendSwapped()");
	}
cleanup:
	return retVal;
}

/**
 * Parse the supplied SVF line, calling processLine() for shift operations as necessary.
 */
FLStatus parseLine(
	struct ParseContext *cxt, const struct Buffer *lineBuf, struct Buffer *csvfBuf,
	uint32 *maxBufSize, const char **error)
{
	FLStatus retVal = FL_SUCCESS, fStatus;
	BufferStatus bStatus;
	char *const line = (char *)lineBuf->data;
	char *const lineEnd = (char *)lineBuf->data + lineBuf->length - 1;  // Space for NUL-terminator
	struct Buffer tmpHead = {0,};
	struct Buffer tmpBody1 = {0,};
	struct Buffer tmpBody2 = {0,};
	struct Buffer tmpTail = {0,};
	if ( !strncmp(line, "RUNTEST", 7) ) {
		// RUNTEST line is of the form "RUNTEST [IDLE] <count> TCK [ENDSTATE IDLE]"
		const char *p = line + 7;
		char *end;
		double count1, count2;
		CHOMP();
		if ( !strncmp(p, "IDLE", 4) ) {
			p += 4;
			CHOMP();
		}
		count1 = strtod(p, &end);
		p = end;
		CHOMP();
		if ( !strncmp(p, "TCK", 3) ) {
			p += 3;
			CHOMP();
		} else if ( !strncmp(p, "SEC", 3) ) {
			count1 *= 1000000.0;
			p += 3;
			CHOMP();
		} else {
			CHECK_STATUS(
				true, FL_SVF_PARSE_ERR, cleanup,
				"parseLine(): RUNTEST must be of the form \"RUNTEST [IDLE] <number> TCK|SEC [<number> TCK|SEC] [ENDSTATE IDLE]\"");
		}
		count2 = strtod(p, &end);
		if ( end != p ) {
			p = end;
			CHOMP();
			if ( !strncmp(p, "TCK", 3) ) {
				p += 3;
				CHOMP();
			} else if ( !strncmp(p, "SEC", 3) ) {
				count2 *= 1000000.0;
				p += 3;
				CHOMP();
			}
		}
		if ( !strncmp(p, "ENDSTATE IDLE", 13) ) {
			p += 13;
		}
		CHOMP();
		if ( count2 > count1 ) {
			count1 = count2;
		}
		CHECK_STATUS(
			p != lineEnd, FL_SVF_PARSE_ERR, cleanup,
			"parseLine(): RUNTEST must be of the form \"RUNTEST [IDLE] <number> TCK|SEC [<number> TCK|SEC] [ENDSTATE IDLE]\"");
		cxt->numCommands++;
		bStatus = bufAppendByte(csvfBuf, XRUNTEST, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
		bStatus = bufAppendLongBE(csvfBuf, (uint32)count1, error);
		CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
	} else if (
		(line[0] == 'H' || line[0] == 'S' || line[0] == 'T') &&
		(line[1] == 'I' || line[1] == 'D') &&
		line[2] == 'R' && (line[3] == ' ' || line[3] == '\t')
	) {
		// HIR/HDR, TIR/TDR, SIR/SDR are of the form "**R <length> [TDI (<tdi>)] [TDO (<tdo>)] [MASK (<mask>)] [SMASK (<smask>)]"
		char *p = line + 3;
		char *tmp;
		const char *tdi = NULL, *tdo =  NULL, *mask = NULL, *smask = NULL;
		uint32 length;
		const bool isDataReg = (line[1] == 'D');
		ShiftOperation op;
		line[2] = '\0';
		switch ( line[0] ) {
		case 'H':
			op = HEAD;
			break;
		case 'T':
			op = TAIL;
			break;
		default:
			op = BODY;
			break;
		}
		CHOMP();
		length = (uint32)strtoul(p, &tmp, 10);
		p = tmp;
		CHOMP();
		while ( *p ) {
			if ( !strncmp(p, "TDI", 3) ) {
				p += 3;
				CHOMP();
				#define EXPECT_CHAR(x, y) \
					CHECK_STATUS( \
						*p != x, FL_SVF_PARSE_ERR, cleanup, \
						"parseLine(): %sR must be of the form \"%sR ... " y "\"", line, line);
				#define FIX_ODD(x) \
					if ( strlen(x) & 1 ) { \
						x--; \
					}
				EXPECT_CHAR('(', "TDI (<tdi>)");
				*p++ = '0';
				tdi = p++;
				while ( p < lineEnd && *p != ')' ) {
					p++;
				}
				EXPECT_CHAR(')', "TDI (<tdi>)");
				*p++ = '\0';
				FIX_ODD(tdi);
			} else if ( !strncmp(p, "SMASK", 5) ) {
				p += 5;
				CHOMP();
				EXPECT_CHAR('(', "SMASK (<smask>)");
				*p++ = '0';
				smask = p++;
				while ( p < lineEnd && *p != ')' ) {
					p++;
				}
				EXPECT_CHAR(')', "SMASK (<smask>)");
				*p++ = '\0';
				FIX_ODD(smask);
			} else if ( !strncmp(p, "TDO", 3) ) {
				p += 3;
				CHOMP();
				EXPECT_CHAR('(', "TDO (<tdo>)");
				*p++ = '0';
				tdo = p++;
				while ( p < lineEnd && *p != ')' ) {
					p++;
				}
				EXPECT_CHAR(')', "TDO (<tdo>)");
				*p++ = '\0';
				FIX_ODD(tdo);
			} else if ( !strncmp(p, "MASK", 4) ) {
				p += 4;
				CHOMP();
				EXPECT_CHAR('(', "MASK (<mask>)");
				*p++ = '0';
				mask = p++;
				while ( p < lineEnd && *p != ')' ) {
					p++;
				}
				EXPECT_CHAR(')', "MASK (<mask>)");
				*p++ = '\0';
				FIX_ODD(mask);
			} else {
				CHECK_STATUS(
					true, FL_SVF_PARSE_ERR, cleanup,
					"parseLine(): Junk in [HTS][IR]R line at column %d", p-line);
			}
			CHOMP();
		}
		if ( isDataReg ) {
			bool zeroMask;
			switch ( op ) {
			case HEAD:
				fStatus = processLine(&cxt->dataHead, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				break;
			case TAIL:
				fStatus = processLine(&cxt->dataTail, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				break;
			case BODY:
				fStatus = processLine(&cxt->dataBody, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				if (
					cxt->dataHead.numBits + cxt->dataBody.numBits + cxt->dataTail.numBits
					!= cxt->curLength
				) {
					cxt->curLength = cxt->dataHead.numBits + cxt->dataBody.numBits + cxt->dataTail.numBits;
					cxt->numCommands++;
					bStatus = bufAppendByte(csvfBuf, XSDRSIZE, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					bStatus = bufAppendLongBE(csvfBuf, cxt->curLength, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				}
				bStatus = bufDeepCopy(&tmpHead, &cxt->dataHead.mask, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpBody1, &cxt->dataBody.mask, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpTail, &cxt->dataTail.mask, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				fStatus = headTail(
					&tmpBody1, &tmpHead, &tmpTail,
					cxt->dataBody.numBits, cxt->dataHead.numBits, cxt->dataTail.numBits,
					error);
				zeroMask = isAllZero(&tmpBody1);
				if (
					tmpBody1.length != cxt->curMaskBuf.length ||
					memcmp(tmpBody1.data, cxt->curMaskBuf.data, tmpBody1.length)
				) {
					bufSwap(&cxt->curMaskBuf, &tmpBody1);
					cxt->newMaskWritten = false;
				}
				if ( !zeroMask && tdo && !cxt->newMaskWritten ) {
					// New mask is nonzero and different from the last one sent
					cxt->numCommands++;
					bStatus = bufAppendByte(csvfBuf, XTDOMASK, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					fStatus = appendSwapped(csvfBuf, cxt->curMaskBuf.data, (uint32)cxt->curMaskBuf.length, error);
					CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
					cxt->newMaskWritten = true;
				}

				bStatus = bufDeepCopy(&tmpHead, &cxt->dataHead.tdi, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpBody1, &cxt->dataBody.tdi, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpTail, &cxt->dataTail.tdi, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				fStatus = headTail(
					&tmpBody1, &tmpHead, &tmpTail,
					cxt->dataBody.numBits, cxt->dataHead.numBits, cxt->dataTail.numBits,
					error);
				if ( zeroMask || !tdo ) {
					cxt->numCommands++;
					bStatus = bufAppendByte(csvfBuf, XSDR, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					fStatus = appendSwapped(csvfBuf, tmpBody1.data, (uint32)tmpBody1.length, error);
					CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				} else {
					bStatus = bufDeepCopy(&tmpHead, &cxt->dataHead.tdo, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					bStatus = bufDeepCopy(&tmpBody2, &cxt->dataBody.tdo, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					bStatus = bufDeepCopy(&tmpTail, &cxt->dataTail.tdo, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					fStatus = headTail(
						&tmpBody2, &tmpHead, &tmpTail,
						cxt->dataBody.numBits, cxt->dataHead.numBits, cxt->dataTail.numBits,
						error);
					if ( maxBufSize && tmpBody2.length > *maxBufSize ) {
						*maxBufSize = (uint32)tmpBody2.length;
					}
					cxt->numCommands++;
					bStatus = bufAppendByte(csvfBuf, XSDRTDO, error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
					fStatus = appendSwappedAndInterleaved(
						csvfBuf, tmpBody1.data, tmpBody2.data, (uint32)tmpBody2.length, error);
					CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				}
				break;
			}
		} else {
			switch ( op ) {
			case HEAD:
				fStatus = processLine(&cxt->insnHead, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				break;
			case TAIL:
				fStatus = processLine(&cxt->insnTail, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				break;
			case BODY:
				fStatus = processLine(&cxt->insnBody, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpHead, &cxt->insnHead.tdi, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpBody1, &cxt->insnBody.tdi, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufDeepCopy(&tmpTail, &cxt->insnTail.tdi, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				fStatus = headTail(
					&tmpBody1, &tmpHead, &tmpTail,
					cxt->insnBody.numBits, cxt->insnHead.numBits, cxt->insnTail.numBits,
					error);
				cxt->numCommands++;
				bStatus = bufAppendByte(csvfBuf, XSIR, error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				bStatus = bufAppendByte(csvfBuf, (uint8)(cxt->insnBody.numBits + cxt->insnHead.numBits + cxt->insnTail.numBits), error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "parseLine()");
				fStatus = appendSwapped(csvfBuf, tmpBody1.data, (uint32)tmpBody1.length, error);
				CHECK_STATUS(fStatus, fStatus, cleanup, "parseLine()");
				break;
			}
		}
	} else {
		CHECK_STATUS(
			true, FL_SVF_PARSE_ERR, cleanup,
			"parseLine(): Unrecognised command \"%s\"\n", line);
	}
cleanup:
	bufDestroy(&tmpHead);
	bufDestroy(&tmpBody1);
	bufDestroy(&tmpBody2);
	bufDestroy(&tmpTail);
	return retVal;
}

static const char *const cmdNames[] = {
	"XCOMPLETE",    // 0
	"XTDOMASK",     // 1
	"XSIR",         // 2
	"XSDR",         // 3
	"XRUNTEST",     // 4
	"ILLEGAL",      // 5
	"ILLEGAL",      // 6
	"XREPEAT",      // 7
	"XSDRSIZE",     // 8
	"XSDRTDO",      // 9
	"XSETSDRMASKS", // A
	"XSDRINC",      // B
	"XSDRB",        // C
	"XSDRC",        // D
	"XSDRE",        // E
	"XSDRTDOB",     // F
	"XSDRTDOC",     // 10
	"XSDRTDOE",     // 11
	"XSTATE",       // 12
	"XENDIR",       // 13
	"XENDDR",       // 14
	"XSIR2",        // 15
	"XCOMMENT",     // 16
	"XWAIT"         // 17
};

const char *getCmdName(CmdPtr cmd) {
	const uint8 op = *cmd;
	return cmdNames[op];
}

#define SET_BYTES(rt) rt.b[0] = (*ptr)[1]; rt.b[1] = (*ptr)[2]; rt.b[2] = (*ptr)[3]; rt.b[3] = (*ptr)[4]
static const uint8 xrtZero[] = {XRUNTEST, 0, 0, 0, 0};
static const uint32 illegal32 = U32MAX;

void processIndex(const CmdPtr *srcIndex, CmdPtr *dstIndex) {
	union {
		uint32 i;
		uint8 b[4];
	} oldrt, newrt;
	const CmdPtr *ptr = srcIndex;
	uint8 thisCmd = **ptr;
	oldrt.i = illegal32;
	newrt.i = 0;
	while ( thisCmd != XCOMPLETE ) {
		while ( thisCmd != XCOMPLETE && thisCmd != XSDR && thisCmd != XSDRTDO && thisCmd != XSIR ) {
			thisCmd = **++ptr;
		}
		if ( thisCmd != XCOMPLETE ) {
			thisCmd = **++ptr;  // now points at command AFTER shift command
		}
		if ( thisCmd == XRUNTEST ) {
			// There is an explicit XRUNTEST, so hoist it to the top, maybe...
			SET_BYTES(newrt);
			if ( newrt.i != oldrt.i ) {
				*dstIndex++ = *ptr;
				oldrt = newrt;
			}

			// ...then copy the commands...
			while ( srcIndex < ptr ) {
				*dstIndex++ = *srcIndex++;
			}

			// ...and finally get the next command
			ptr++;  // now points at command after XRUNTEST, ready for next loop
			srcIndex = ptr;
			thisCmd = **ptr;
		} else {
			// There is not an explicit XRUNTEST, meaning it's implicitly zero:
			newrt.i = 0;
			if ( newrt.i != oldrt.i ) {
				*dstIndex++ = xrtZero;
				oldrt = newrt;
			}

			// Copy the backlog
			while ( srcIndex < ptr ) {
				*dstIndex++ = *srcIndex++;
			}
			srcIndex = ptr;
		}
	}
	*dstIndex = *ptr;
}

FLStatus buildIndex(struct ParseContext *cxt, struct Buffer *csvfBuf, const char **error) {
	FLStatus retVal = FL_SUCCESS;
	const uint8 *const start = csvfBuf->data;
	const uint8 *ptr = start;
	struct Buffer newBuf = {0,};
	uint32 numBytes;
	uint8 thisByte = *ptr;
	int i = 0;
	int offset;
	const CmdPtr *cmdPtr;
	BufferStatus bStatus;
	const uint8 **const srcIndex = malloc(sizeof(const uint8*) * cxt->numCommands);
	const uint8 **const dstIndex = malloc(sizeof(const uint8*) * cxt->numCommands * 3 / 2); // abs worst case {XSIR, XCOMPLETE} -> {XRUNTEST, XSIR, XCOMPLETE}
	CHECK_STATUS(srcIndex == NULL, FL_ALLOC_ERR, cleanup, "buildIndex()");
	CHECK_STATUS(dstIndex == NULL, FL_ALLOC_ERR, cleanup, "buildIndex()");
	bStatus = bufInitialise(&newBuf, csvfBuf->length * 4 / 3, 0x00, error);  // common worst case
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "buildIndex()");
	numBytes = illegal32;
	while ( thisByte != XCOMPLETE ) {
		srcIndex[i++] = ptr++;
		switch ( thisByte ) {
		case XSDRSIZE:
			numBytes = bitsToBytes(readLongBE(ptr));
		case XRUNTEST:
			ptr += 4;
			break;
		case XTDOMASK:
		case XSDR:
			CHECK_STATUS(numBytes == illegal32, FL_INTERNAL_ERR, cleanup, "buildIndex(): No XSDRSIZE before shift operation!");
			ptr += numBytes;
			break;
		case XSDRTDO:
			CHECK_STATUS(numBytes == illegal32, FL_INTERNAL_ERR, cleanup, "buildIndex(): No XSDRSIZE before shift operation!");
			ptr += 2*numBytes;
			break;
		case XSIR:
			offset = *ptr++;
			ptr += bitsToBytes(offset);
			break;
		default:
			CHECK_STATUS(
				true, FL_INTERNAL_ERR, cleanup,
				"buildIndex(): Unrecognised CSVF command (cmd=0x%02X, srcOffset=%d)!", thisByte, ptr - start);
		}
		thisByte = *ptr;
	}
	srcIndex[i++] = ptr++;
	processIndex(srcIndex, dstIndex);
	cmdPtr = dstIndex;
	ptr = *cmdPtr;
	thisByte = *ptr;
	numBytes = illegal32;
	while ( thisByte != XCOMPLETE ) {
		switch ( thisByte ) {
		case XSDRSIZE:
			numBytes = bitsToBytes(readLongBE(ptr + 1));
		case XRUNTEST:
			bStatus = bufAppendBlock(&newBuf, ptr, 5, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "buildIndex()");
			break;
		case XTDOMASK:
		case XSDR:
			CHECK_STATUS(numBytes == illegal32, FL_INTERNAL_ERR, cleanup, "buildIndex(): No XSDRSIZE before shift operation!");
			bStatus = bufAppendBlock(&newBuf, ptr, numBytes + 1, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "buildIndex()");
			break;
		case XSDRTDO:
			CHECK_STATUS(numBytes == illegal32, FL_INTERNAL_ERR, cleanup, "buildIndex(): No XSDRSIZE before shift operation!");
			bStatus = bufAppendBlock(&newBuf, ptr, 2*numBytes + 1, error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "buildIndex()");
			break;
		case XSIR:
			offset = ptr[1];
			bStatus = bufAppendBlock(&newBuf, ptr, (uint32)(bitsToBytes(offset) + 2), error);
			CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "buildIndex()");
			break;
		default:
			CHECK_STATUS(
				true, FL_INTERNAL_ERR, cleanup,
				"buildIndex(): Unrecognised CSVF command (cmd=0x%02X)!", thisByte);
		}
		cmdPtr++;
		ptr = *cmdPtr;
		thisByte = *ptr;
	}
	bStatus = bufAppendByte(&newBuf, XCOMPLETE, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "buildIndex()");
	bufSwap(&newBuf, csvfBuf);
cleanup:
	if ( dstIndex ) {
		free((void*)dstIndex);
	}
	if ( srcIndex ) {
		free((void*)srcIndex);
	}
	bufDestroy(&newBuf);
	return retVal;
}

DLLEXPORT(FLStatus) flLoadSvfAndConvertToCsvf(
	const char *svfFile, struct Buffer *csvfBuf, uint32 *maxBufSize, const char **error)
{
	FLStatus retVal = FL_SUCCESS;
	struct Buffer lineBuf = {0,};
	BufferStatus bStatus;
	FLStatus fStatus;
	const uint8 *buffer = NULL, *p, *end, *line;
	size_t fileLength;
	bool gotSemicolon;
	struct ParseContext cxt = {{0,},};

	// Initialise context and line buffer
	fStatus = cxtInitialise(&cxt, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flLoadSvfAndConvertToCsvf()");
	bStatus = bufInitialise(&lineBuf, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadSvfAndConvertToCsvf()");

	// Load SVF file
	buffer = flLoadFile(svfFile, &fileLength);
	if ( !buffer ) {
		//errRender(error, "flLoadSvfAndConvertToCsvf(): Unable to load SVF file %s", svfFile);
		errRenderStd(error);
		errPrefix(error, "flLoadSvfAndConvertToCsvf()");
		FAIL(FL_FILE_ERR, cleanup);
	}
	end = buffer + fileLength;
	p = buffer;
	while ( p < end ) {
		if ( p[0] == '\n' || p[0] == '\r' ) {
			p++;
		} else if (
			p[0] == '!' ||
			(p[0] == '/' && p[1] == '/') ||
			!memcmp(p, "TRST", 4) ||
			!memcmp(p, "END", 3) ||
			!memcmp(p, "STATE", 5) ||
			!memcmp(p, "FREQ", 4)
		) {
			while ( p < end && *p != '\n' && *p != '\r' ) {
				p++;
			}
			p++;
		} else {
			CHOMP();
			line = p;
			while ( p < end && *p != '\n' && *p != '\r' && *p != ';' ) {
				p++;
			}
			gotSemicolon = (*p == ';');
			if ( *p == '\n' || *p == '\r' || *p == ';' ) {
				do {
					p--;
				} while ( *p == ' ' || *p == '\t' );
				p++; // go back to first space char
				bStatus = bufAppendBlock(&lineBuf, line, (uint32)(p - line), error);
				CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadSvfAndConvertToCsvf()");
				while ( p < end && *p != '\n' && *p != '\r' ) {
					p++;
				}
				p++; // Skip over CR
				if ( gotSemicolon ) {
					bStatus = bufAppendByte(&lineBuf, '\0', error);
					CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadSvfAndConvertToCsvf()");
					fStatus = parseLine(&cxt, &lineBuf, csvfBuf, maxBufSize, error);
					CHECK_STATUS(fStatus, fStatus, cleanup, "flLoadSvfAndConvertToCsvf()");
					bufZeroLength(&lineBuf);
				}
			}
		}
	}
	bStatus = bufAppendByte(csvfBuf, XCOMPLETE, error);
	CHECK_STATUS(bStatus, FL_ALLOC_ERR, cleanup, "flLoadSvfAndConvertToCsvf()");
	cxt.numCommands++;

	fStatus = buildIndex(&cxt, csvfBuf, error);
	CHECK_STATUS(fStatus, fStatus, cleanup, "flLoadSvfAndConvertToCsvf()");
cleanup:
	cxtDestroy(&cxt);
	bufDestroy(&lineBuf);
	flFreeFile((void*)buffer);
	return retVal;
}	

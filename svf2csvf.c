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

static FLStatus postProcess(
	struct Buffer *buf, uint32 *maxBufSize, const char **error
) WARN_UNUSED_RESULT;

static bool getHexNibble(char hexDigit, uint8 *nibble) {
	if ( hexDigit >= '0' && hexDigit <= '9' ) {
		*nibble = hexDigit - '0';
		return false;
	} else if ( hexDigit >= 'a' && hexDigit <= 'f' ) {
		*nibble = hexDigit - 'a' + 10;
		return false;
	} else if ( hexDigit >= 'A' && hexDigit <= 'F' ) {
		*nibble = hexDigit - 'A' + 10;
		return false;
	} else {
		return true;
	}
}

static int getHexByte(const char *p, uint8 *byte) {
	uint8 upperNibble;
	uint8 lowerNibble;
	if ( !getHexNibble(p[0], &upperNibble) && !getHexNibble(p[1], &lowerNibble) ) {
		*byte = (upperNibble << 4) | lowerNibble;
		byte += 2;
		return 0;
	} else {
		return 1;
	}
}

static uint32 readLongBE(const uint8 *p) {
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
	FLStatus returnCode = FL_SUCCESS;
	uint32 length = strlen(hexDigits);
	uint8 *p = buffer->data;
	BufferStatus bStatus;
	if ( length & 1 ) {
		errRender(error, "readBytes(): I need an even number of hex digits");
		FAIL(FL_SVF_PARSE_ERR);
	}
	bufZeroLength(buffer);
	length >>= 1;  // Number of bytes
	bStatus = bufAppendConst(buffer, 0x00, length, error);
	CHECK_STATUS(bStatus, "readBytes()", FL_BUF_APPEND_ERR);
	while ( length-- ) {
		CHECK_STATUS(getHexByte(hexDigits, p++), "readBytes()", FL_SVF_PARSE_ERR);
		hexDigits += 2;
	}
cleanup:
	return returnCode;
}

static FLStatus shiftLeft(
	struct Buffer *buffer, uint32 numBits, uint32 shiftCount, const char **error)
{
	FLStatus returnCode = FL_SUCCESS;
	uint32 shiftBytes = shiftCount>>3;
	uint32 shiftBits = shiftCount&7;
	uint16 accum;
	const uint8 *p = buffer->data;
	const uint8 *const end = buffer->data + buffer->length;
	struct Buffer newBuffer = {0,};
	BufferStatus status;
	if ( shiftBits ) {
		status = bufInitialise(&newBuffer, 1024, 0x00, error);
		CHECK_STATUS(status, "shiftLeft()", FL_BUF_INIT_ERR);
		numBits &= 7;  // Now the number of significant bits in first byte.
		if ( numBits ) {
			numBits = 8 - numBits; // Now the number of insignificant bits in first byte.
		}
		accum = p[0];
		if ( p < end ) {
			accum >>= 8-shiftBits;
			if ( shiftBits > numBits ) {
				// We're shifting by more than the number of insignificant bits
				status = bufAppendByte(&newBuffer, (uint8)(accum&0xFF), error);
				CHECK_STATUS(status, "shiftLeft()", FL_BUF_APPEND_ERR);
			}
			accum = (p[0]<<8) + p[1];
			p++;
			while ( p < end ) {
				accum >>= 8-shiftBits;
				status = bufAppendByte(&newBuffer, (uint8)(accum&0xFF), error);
				CHECK_STATUS(status, "shiftLeft()", FL_BUF_APPEND_ERR);
				accum = (p[0]<<8) + p[1];
				p++;
			}
		}
		accum &= 0xFF00;
		accum >>= 8-shiftBits;
		status = bufAppendByte(&newBuffer, (uint8)(accum&0xFF), error);
		CHECK_STATUS(status, "shiftLeft()", FL_BUF_APPEND_ERR);
		bufSwap(&newBuffer, buffer);
	}
	if ( shiftBytes ) {
		status = bufAppendConst(buffer, 0x00, shiftBytes, error);
		CHECK_STATUS(status, "shiftLeft()", FL_BUF_APPEND_ERR);
	}
cleanup:
	bufDestroy(&newBuffer);
	return returnCode;
}

FLStatus headTail(
	struct Buffer *dataBuf, struct Buffer *headBuf, struct Buffer *tailBuf,
	uint32 dataBits, uint32 headBits, uint32 tailBits, const char **error)
{
	FLStatus returnCode = FL_SUCCESS, fStatus;
	const uint8 *src = headBuf->data;
	const uint8 *dstEnd;
	uint8 *dst;
	struct Buffer newBuffer = {0,};
	BufferStatus bStatus;

	// Shift data left by headBits
	fStatus = shiftLeft(dataBuf, dataBits, headBits, error);
	CHECK_STATUS(fStatus, "headTail()", fStatus);

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
			CHECK_STATUS(fStatus, "headTail()", fStatus);
		}

		// How much data do we need to copy from the tail?
		bytesFromTail = tailBuf->length;
		src = tailBuf->data;

		// Init new buffer
		bStatus = bufInitialise(&newBuffer, 1024, 0x00, error);
		CHECK_STATUS(bStatus, "headTail()", FL_BUF_INIT_ERR);

		dataBits += tailBits;
		if ( bitsToBytes(dataBits) == dataBuf->length + bytesFromTail ) {
			// There's no overlap, so copy back-to-back
			bStatus = bufAppendBlock(&newBuffer, src, bytesFromTail, error);
			CHECK_STATUS(bStatus, "headTail()", FL_BUF_APPEND_ERR);
			bStatus = bufAppendByte(&newBuffer, *dataBuf->data, error);
			CHECK_STATUS(bStatus, "headTail()", FL_BUF_APPEND_ERR);
		} else if ( bitsToBytes(dataBits) == dataBuf->length + bytesFromTail - 1 ) {
			// There's a single byte overlap, so OR the overlap byte
			bytesFromTail--;
			bStatus = bufAppendBlock(&newBuffer, src, bytesFromTail, error);
			CHECK_STATUS(bStatus, "headTail()", FL_BUF_APPEND_ERR);
			src += bytesFromTail;
			bStatus = bufAppendByte(&newBuffer, *src | *dataBuf->data, error);
			CHECK_STATUS(bStatus, "headTail()", FL_BUF_APPEND_ERR);
		} else {
			// Ooops, this should never happen!
			errRender(error, "headTail(): Internal error");
			FAIL(FL_INTERNAL_ERR);
		}
		bStatus = bufAppendBlock(&newBuffer, dataBuf->data+1, dataBuf->length-1, error);
		CHECK_STATUS(bStatus, "headTail()", FL_BUF_APPEND_ERR);
		bufSwap(&newBuffer, dataBuf);
	}
	
cleanup:
	bufDestroy(&newBuffer);
	return returnCode;
}

typedef enum {
	XCOMPLETE    = 0x00,
	XTDOMASK     = 0x01,
	XSIR         = 0x02,
	XSDR         = 0x03,
	XRUNTEST     = 0x04,
	XREPEAT      = 0x07,
	XSDRSIZE     = 0x08,
	XSDRTDO      = 0x09,
	XSETSDRMASKS = 0x0A,
	XSDRINC      = 0x0B,
	XSDRB        = 0x0C,
	XSDRC        = 0x0D,
	XSDRE        = 0x0E,
	XSDRTDOB     = 0x0F,
	XSDRTDOC     = 0x10,
	XSDRTDOE     = 0x11,
	XSTATE       = 0x12,
	XENDIR       = 0x13,
	XENDDR       = 0x14,
	XSIR2        = 0x15,
	XCOMMENT     = 0x16,
	XWAIT        = 0x17
} Command;

static FLStatus initBitStore(struct BitStore *store, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	BufferStatus bStatus;
	store->numBits = 0;
	bStatus = bufInitialise(&store->tdi, 1024, 0x00, error);
	CHECK_STATUS(bStatus, "initBitStore()", FL_BUF_INIT_ERR);
	bStatus = bufInitialise(&store->tdo, 1024, 0x00, error);
	CHECK_STATUS(bStatus, "initBitStore()", FL_BUF_INIT_ERR);
	bStatus = bufInitialise(&store->mask, 1024, 0x00, error);
	CHECK_STATUS(bStatus, "initBitStore()", FL_BUF_INIT_ERR);
cleanup:
	return returnCode;
}

static void destroyBitStore(struct BitStore *store) {
	store->numBits = 0;
	bufDestroy(&store->tdi);
	bufDestroy(&store->tdo);
	bufDestroy(&store->mask);
}

FLStatus cxtInitialise(struct ParseContext *cxt, const char **error) {
	FLStatus returnCode = FL_SUCCESS, fStatus;
	BufferStatus bStatus;
	fStatus = initBitStore(&cxt->dataHead, error);
	CHECK_STATUS(fStatus, "cxtInitialise()", fStatus);
	fStatus = initBitStore(&cxt->dataBody, error);
	CHECK_STATUS(fStatus, "cxtInitialise()", fStatus);
	fStatus = initBitStore(&cxt->dataTail, error);
	CHECK_STATUS(fStatus, "cxtInitialise()", fStatus);
	fStatus = initBitStore(&cxt->insnHead, error);
	CHECK_STATUS(fStatus, "cxtInitialise()", fStatus);
	fStatus = initBitStore(&cxt->insnBody, error);
	CHECK_STATUS(fStatus, "cxtInitialise()", fStatus);
	fStatus = initBitStore(&cxt->insnTail, error);
	CHECK_STATUS(fStatus, "cxtInitialise()", fStatus);
	cxt->curLength = 0;
	bStatus = bufInitialise(&cxt->curMaskBuf, 1024, 0x00, error);
	CHECK_STATUS(bStatus, "cxtInitialise()", FL_BUF_INIT_ERR);
	cxt->curMaskBits = 0;
	cxt->runTestOffset = 0;
cleanup:
	return returnCode;
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
	FLStatus returnCode = FL_SUCCESS, fStatus;
	BufferStatus bStatus;

	// The TDI & MASK values are remembered; the TDO value defaults to all zeros. If the length
	// changes, all remembered values are forgotten.
	bufZeroLength(&store->tdo);
	if ( newLength ) {
		bStatus = bufAppendConst(&store->tdo, 0x00, bitsToBytes(newLength), error);
		CHECK_STATUS(bStatus, "processLine()", FL_BUF_APPEND_ERR);
	}
	if ( store->numBits != newLength ) {
		// The length changed, so we can't reuse previous values, and new TDI data is required:
		// "The TDI parameter must be explicitly specified for the first command or when the
		// length changes."
		bufZeroLength(&store->tdi);
		bufZeroLength(&store->mask);
		if ( newLength ) {
			bStatus = bufAppendConst(&store->tdi, 0x00, bitsToBytes(newLength), error);
			CHECK_STATUS(bStatus, "processLine()", FL_BUF_APPEND_ERR);
			bStatus = bufAppendConst(&store->mask, 0x00, bitsToBytes(newLength), error);
			CHECK_STATUS(bStatus, "processLine()", FL_BUF_APPEND_ERR);
		}
	}
	store->numBits = newLength;
	if ( tdi ) {
		fStatus = readBytes(&store->tdi, tdi, error);
		CHECK_STATUS(fStatus, "processLine()", fStatus);
	}
	if ( tdo ) {
		fStatus = readBytes(&store->tdo, tdo, error);
		CHECK_STATUS(fStatus, "processLine()", fStatus);
	}
	if ( mask ) {
		fStatus = readBytes(&store->mask, mask, error);
		CHECK_STATUS(fStatus, "processLine()", fStatus);
	}

cleanup:
	return returnCode;
}

/**
 * Return true if the buffer is all zeros
 */
static bool isAllZero(struct Buffer *buf) {
	const uint8 *p = buf->data;
	uint32 length = buf->length;
	while ( length-- ) {
		if ( *p++ ) {
			return false;
		}
	}
	return true;
}

static FLStatus appendSwapped(
	struct Buffer *buf, const uint8 *src, uint32 count, const char **error)
{
	FLStatus returnCode = FL_SUCCESS;
	BufferStatus bStatus;
	src += count - 1;
	while ( count-- ) {
		bStatus = bufAppendByte(buf, *src--, error);
		CHECK_STATUS(bStatus, "appendSwapped()", FL_BUF_APPEND_ERR);
	}
cleanup:
	return returnCode;
}

/**
 * Parse the supplied SVF line, calling processLine() for shift operations as necessary.
 */
FLStatus parseLine(
	struct ParseContext *cxt, const struct Buffer *lineBuf, struct Buffer *csvfBuf,
	const char **error)
{
	FLStatus returnCode = FL_SUCCESS, fStatus;
	BufferStatus bStatus;
	char *const line = (char *)lineBuf->data;
	char *const lineEnd = (char *)lineBuf->data + lineBuf->length - 1;  // Space for NUL-terminator
	struct Buffer tmpHead = {0,};
	struct Buffer tmpBody1 = {0,};
	struct Buffer tmpBody2 = {0,};
	struct Buffer tmpTail = {0,};
	if ( !strncmp(line, "RUNTEST ", 8) ) {
		// RUNTEST line is of the form "RUNTEST [IDLE] <count> TCK [ENDSTATE IDLE]"
		const char *p = line + 7;
		char *end;
		uint32 count;
		uint32 oldCount;
		CHOMP();
		if ( !strncmp(p, "IDLE ", 5) ) {
			p += 4;
		}
		CHOMP();
		count = strtoul(p, &end, 10);
		p = end;
		CHOMP();
		if ( strncmp(p, "TCK", 3) ) {
			errRender(error, "parseLine(): RUNTEST must be of the form \"RUNTEST [IDLE] <number> TCK [ENDSTATE IDLE]\"");
			FAIL(FL_SVF_PARSE_ERR);
		}
		p += 3;
		CHOMP();
		if ( !strncmp(p, "ENDSTATE IDLE", 13) ) {
			p += 13;
		}
		CHOMP();
		if ( p != lineEnd ) {
			errRender(error, "parseLine(): RUNTEST must be of the form \"RUNTEST [IDLE] <number> TCK [ENDSTATE IDLE]\"");
			FAIL(FL_SVF_PARSE_ERR);
		}
		// Write count value into csvfBuf at the last XRUNTEST
		if ( cxt->runTestOffset == 0 ) {
			errRender(error, "parseLine(): RUNTEST not expected here");
			FAIL(FL_SVF_PARSE_ERR);
		}
		oldCount = readLongBE(csvfBuf->data + cxt->runTestOffset);
		bStatus = bufWriteLongBE(csvfBuf, cxt->runTestOffset, oldCount + count, error);
		CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
	} else if (
		(line[0] == 'H' || line[0] == 'S' || line[0] == 'T') &&
		(line[1] == 'I' || line[1] == 'D') &&
		line[2] == 'R' && line[3] == ' '
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
		length = strtoul(p, &tmp, 10);
		p = tmp;
		CHOMP();
		while ( *p ) {
			if ( !strncmp(p, "TDI ", 4) ) {
				p += 3;
				CHOMP();
				#define EXPECT_CHAR(x, y) \
					if ( *p != x ) { \
						errRender(error, "parseLine(): %sR must be of the form \"%sR ... " y "\"", line, line); \
						FAIL(FL_SVF_PARSE_ERR);						\
					}
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
			} else if ( !strncmp(p, "SMASK ", 6) ) {
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
			} else if ( !strncmp(p, "TDO ", 4) ) {
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
			} else if ( !strncmp(p, "MASK ", 5) ) {
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
				errRender(error, "parseLine(): Junk in [HTS][IR]R line at column %d", p-line);
				FAIL(FL_SVF_PARSE_ERR);
			}
			CHOMP();
		}
		if ( isDataReg ) {
			bool zeroMask;
			switch ( op ) {
			case HEAD:
				fStatus = processLine(&cxt->dataHead, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				break;
			case TAIL:
				fStatus = processLine(&cxt->dataTail, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				break;
			case BODY:
				fStatus = processLine(&cxt->dataBody, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				if (
					cxt->dataHead.numBits + cxt->dataBody.numBits + cxt->dataTail.numBits
					!= cxt->curLength
				) {
					cxt->curLength = cxt->dataHead.numBits + cxt->dataBody.numBits + cxt->dataTail.numBits;
					bStatus = bufAppendByte(csvfBuf, XSDRSIZE, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					bStatus = bufAppendLongBE(csvfBuf, cxt->curLength, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				}
				bStatus = bufDeepCopy(&tmpHead, &cxt->dataHead.mask, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufDeepCopy(&tmpBody1, &cxt->dataBody.mask, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufDeepCopy(&tmpTail, &cxt->dataTail.mask, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				fStatus = headTail(
					&tmpBody1, &tmpHead, &tmpTail,
					cxt->dataBody.numBits, cxt->dataHead.numBits, cxt->dataTail.numBits,
					error);
				zeroMask = isAllZero(&tmpBody1);
				if (
					!zeroMask &&
					(tmpBody1.length != cxt->curMaskBuf.length ||
					 memcmp(tmpBody1.data, cxt->curMaskBuf.data, tmpBody1.length))
				) {
					// New mask is nonzero and different from the last one sent
					bStatus = bufAppendByte(csvfBuf, XTDOMASK, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					fStatus = appendSwapped(csvfBuf, tmpBody1.data, tmpBody1.length, error);
					CHECK_STATUS(fStatus, "parseLine()", fStatus);
				}
				bufSwap(&cxt->curMaskBuf, &tmpBody1);

				bStatus = bufDeepCopy(&tmpHead, &cxt->dataHead.tdi, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufDeepCopy(&tmpBody1, &cxt->dataBody.tdi, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufDeepCopy(&tmpTail, &cxt->dataTail.tdi, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				fStatus = headTail(
					&tmpBody1, &tmpHead, &tmpTail,
					cxt->dataBody.numBits, cxt->dataHead.numBits, cxt->dataTail.numBits,
					error);
				if ( zeroMask ) {
					bStatus = bufAppendByte(csvfBuf, XSDR, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					fStatus = appendSwapped(csvfBuf, tmpBody1.data, tmpBody1.length, error);
					CHECK_STATUS(fStatus, "parseLine()", fStatus);
				} else {
					bStatus = bufDeepCopy(&tmpHead, &cxt->dataHead.tdo, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					bStatus = bufDeepCopy(&tmpBody2, &cxt->dataBody.tdo, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					bStatus = bufDeepCopy(&tmpTail, &cxt->dataTail.tdo, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					fStatus = headTail(
						&tmpBody2, &tmpHead, &tmpTail,
						cxt->dataBody.numBits, cxt->dataHead.numBits, cxt->dataTail.numBits,
						error);
					bStatus = bufAppendByte(csvfBuf, XSDRTDO, error);
					CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
					fStatus = appendSwapped(csvfBuf, tmpBody2.data, tmpBody2.length, error);
					CHECK_STATUS(fStatus, "parseLine()", fStatus);
					fStatus = appendSwapped(csvfBuf, tmpBody1.data, tmpBody1.length, error);
					CHECK_STATUS(fStatus, "parseLine()", fStatus);
				}
				break;
			}
		} else {
			switch ( op ) {
			case HEAD:
				fStatus = processLine(&cxt->insnHead, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				break;
			case TAIL:
				fStatus = processLine(&cxt->insnTail, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				break;
			case BODY:
				fStatus = processLine(&cxt->insnBody, length, tdi, tdo, mask, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				bStatus = bufDeepCopy(&tmpHead, &cxt->insnHead.tdi, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufDeepCopy(&tmpBody1, &cxt->insnBody.tdi, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufDeepCopy(&tmpTail, &cxt->insnTail.tdi, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				fStatus = headTail(
					&tmpBody1, &tmpHead, &tmpTail,
					cxt->insnBody.numBits, cxt->insnHead.numBits, cxt->insnTail.numBits,
					error);
				//if ( readLongBE(csvfBuf->data + cxt->runTestOffset) != 
				bStatus = bufAppendByte(csvfBuf, XRUNTEST, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				cxt->runTestOffset = csvfBuf->length;
				bStatus = bufAppendConst(csvfBuf, 0x00, 4, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufAppendByte(csvfBuf, XSIR, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				bStatus = bufAppendByte(csvfBuf, cxt->insnBody.numBits + cxt->insnHead.numBits + cxt->insnTail.numBits, error);
				CHECK_STATUS(bStatus, "parseLine()", FL_BUF_APPEND_ERR);
				fStatus = appendSwapped(csvfBuf, tmpBody1.data, tmpBody1.length, error);
				CHECK_STATUS(fStatus, "parseLine()", fStatus);
				break;
			}
		}
	} else {
		errRender(error, "parseLine(): Unrecognised command \"%s\"\n", line);
		FAIL(FL_SVF_PARSE_ERR);
	}
cleanup:
	bufDestroy(&tmpHead);
	bufDestroy(&tmpBody1);
	bufDestroy(&tmpBody2);
	bufDestroy(&tmpTail);
	return returnCode;
}

#define COPY_BLOCK(x) i = x; while ( i-- ) *dst++ = *src++

// Remove duplicate XRUNTEST commands.
//
static FLStatus postProcess(struct Buffer *buf, uint32 *maxBufSize, const char **error) {
	FLStatus returnCode = FL_SUCCESS;
	uint8 *dst = buf->data;
	const uint8 *src = dst;
	uint32 tmp, i, numBytes = 0;
	uint32 oldRunTest = 0xFFFFFFFF, newRunTest;
	uint8 thisByte = *src++;
	while ( thisByte != XCOMPLETE ) {
		switch ( thisByte ) {
		case XSDRSIZE:
			numBytes = bitsToBytes(readLongBE(src));
			*dst++ = XSDRSIZE;
			COPY_BLOCK(4);
			break;
		case XTDOMASK:
			*dst++ = XTDOMASK;
			COPY_BLOCK(numBytes);
			if ( maxBufSize && numBytes > *maxBufSize ) {
				*maxBufSize = numBytes;
			}
			break;
		case XRUNTEST:
			newRunTest = readLongBE(src);
			if ( newRunTest != oldRunTest ) {
				*dst++ = XRUNTEST;
				COPY_BLOCK(4);
				oldRunTest = newRunTest;
			} else {
				src += 4;
			}
			break;
		case XSIR:
			*dst++ = XSIR;
			i = *src++;
			tmp = bitsToBytes(i);
			*dst++ = i;
			COPY_BLOCK(tmp);
			break;
		case XSDRTDO:
			*dst++ = XSDRTDO;
			COPY_BLOCK(numBytes);
			COPY_BLOCK(numBytes);
			if ( maxBufSize && numBytes > *maxBufSize ) {
				*maxBufSize = numBytes;
			}
			break;
		case XSDR:
			*dst++ = XSDR;
			COPY_BLOCK(numBytes);
			break;
		default:
			errRender(error, "deduplicate(): Unrecognised CSVF command (cmd=0x%02X, srcOffset=%d)!", thisByte, src - buf->data);
			FAIL(FL_INTERNAL_ERR);
		}
		thisByte = *src++;
	}
	*dst++ = XCOMPLETE;
	buf->length = dst - buf->data;
cleanup:
	return returnCode;
}

DLLEXPORT(FLStatus) flLoadSvfAndConvertToCsvf(
	const char *svfFile, struct Buffer *csvfBuf, uint32 *maxBufSize, const char **error)
{
	FLStatus returnCode = FL_SUCCESS;
	struct Buffer lineBuf = {0,};
	BufferStatus bStatus;
	FLStatus fStatus;
	const uint8 *buffer = NULL, *p, *end, *line;
	uint32 fileLength;
	bool gotSemicolon;
	struct ParseContext cxt = {{0,},};

	fStatus = cxtInitialise(&cxt, error);
	CHECK_STATUS(fStatus, "flLoadSvfAndConvertToCsvf()", fStatus);
	bStatus = bufInitialise(&lineBuf, 1024, 0x00, error);
	CHECK_STATUS(bStatus, "flLoadSvfAndConvertToCsvf()", FL_BUF_INIT_ERR);
	buffer = flLoadFile(svfFile, &fileLength);
	CHECK_STATUS(!buffer, "flLoadSvfAndConvertToCsvf()", FL_BUF_LOAD_ERR);
	end = buffer + fileLength;
	p = buffer;
	while ( p < end ) {
		if ( p[0] == '\n' ) {
			p++;
		} else if (
			p[0] == '!' ||
			(p[0] == '/' && p[1] == '/') ||
			!memcmp(p, "TRST", 4) ||
			!memcmp(p, "END", 3) ||
			!memcmp(p, "STATE", 5) ||
			!memcmp(p, "FREQ", 4)
		) {
			while ( p < end && *p != '\n' ) {
				p++;
			}
			p++;
		} else {
			CHOMP();
			line = p;
			while ( p < end && *p != '\n' && *p != ';' ) {
				p++;
			}
			gotSemicolon = (*p == ';');
			if ( *p == '\n' || *p == ';' ) {
				do {
					p--;
				} while ( *p == ' ' || *p == '\t' || *p == '\r' );
				p++; // go back to first space char
				bStatus = bufAppendBlock(&lineBuf, line, p-line, error);
				CHECK_STATUS(bStatus, "flLoadSvfAndConvertToCsvf()", FL_BUF_APPEND_ERR);
				while ( p < end && *p != '\n' ) {
					p++;
				}
				p++; // Skip over CR
				if ( gotSemicolon ) {
					bStatus = bufAppendByte(&lineBuf, '\0', error);
					CHECK_STATUS(bStatus, "flLoadSvfAndConvertToCsvf()", FL_BUF_APPEND_ERR);
					fStatus = parseLine(&cxt, &lineBuf, csvfBuf, error);
					CHECK_STATUS(fStatus, "flLoadSvfAndConvertToCsvf()", fStatus);
					bufZeroLength(&lineBuf);
				}
			}
		}
	}
	bStatus = bufAppendByte(csvfBuf, 0x00, error);
	CHECK_STATUS(bStatus, "flLoadSvfAndConvertToCsvf()", FL_BUF_APPEND_ERR);
	if ( maxBufSize ) {
		*maxBufSize = 0;
	}
	fStatus = postProcess(csvfBuf, maxBufSize, error);
	CHECK_STATUS(fStatus, "flLoadSvfAndConvertToCsvf()", fStatus);

cleanup:
	cxtDestroy(&cxt);
	bufDestroy(&lineBuf);
	flFreeFile((void*)buffer);
	return returnCode;
}	

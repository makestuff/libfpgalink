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
#include <cstdio>
#include <cstring>
#include <UnitTest++.h>
#include <makestuff.h>
#include <libbuffer.h>
#include "../svf2csvf.h"
#include "../xsvf.h"

using namespace std;

void testReadBytes(struct Buffer *buf, const char *expected) {
	FLStatus fStatus;
	char result[1024];
	const uint32 expectedLength = strlen(expected)/2;
	bufZeroLength(buf);
	fStatus = readBytes(buf, expected, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK_EQUAL(expectedLength, buf->length);
	for ( uint32 i = 0; i < 2*buf->length; i++ ) {
		sprintf(result + 2*i, "%02X", buf->data[i]);
	}
	result[2*buf->length] = '\0';
	CHECK_EQUAL(expected, result);
}

TEST(FPGALink_testReadBytes) {
	struct Buffer lineBuf;
	BufferStatus bStatus;
	bStatus = bufInitialise(&lineBuf, 1024, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);

	testReadBytes(&lineBuf, "FE");
	testReadBytes(&lineBuf, "CAFE");
	testReadBytes(&lineBuf, "F00D1E");
	testReadBytes(&lineBuf, "DEADF00D");
	testReadBytes(&lineBuf, "CAFEF00D1E");
	testReadBytes(&lineBuf, "DEADCAFEBABE");

	bufDestroy(&lineBuf);
}

void testShift(const char *line, uint32 lineBits, const char *head, uint32 headBits, const char *tail, uint32 tailBits, const char *expectedHex) {
	struct Buffer lineBuf;
	struct Buffer headBuf;
	struct Buffer tailBuf;
	BufferStatus bStatus;
	FLStatus fStatus;
	char resultHex[1024];
	bStatus = bufInitialise(&lineBuf, 1024, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	bStatus = bufInitialise(&headBuf, 1024, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	bStatus = bufInitialise(&tailBuf, 1024, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	fStatus = readBytes(&lineBuf, line, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	fStatus = readBytes(&headBuf, head, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	fStatus = readBytes(&tailBuf, tail, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	fStatus = headTail(&lineBuf, &headBuf, &tailBuf, lineBits, headBits, tailBits, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK_EQUAL(strlen(expectedHex)/2, lineBuf.length);
	for ( uint32 i = 0; i < 2*lineBuf.length; i++ ) {
        sprintf(resultHex + 2*i, "%02X", lineBuf.data[i]);
    }
    resultHex[2*lineBuf.length] = '\0';
    CHECK_EQUAL(expectedHex, resultHex);
	bufDestroy(&tailBuf);
	bufDestroy(&headBuf);
	bufDestroy(&lineBuf);
}	

TEST(FPGALink_testShift) {
	testShift(
		"F1C2E093", 32,
		"00", 0,
		"06", 3,
		"06F1C2E093"
	);
	testShift(
		"F1C2E093", 32,
		"01", 1,
		"06", 3,
		"0DE385C127"
	);
	testShift(
		"F1C2E093", 32,
		"02", 2,
		"06", 3,
		"1BC70B824E"
	);
	testShift(
		"F1C2E093", 32,
		"06", 3,
		"06", 3,
		"378E17049E"
	);
	testShift(
		"F1C2E093", 32,
		"0A", 4,
		"06", 3,
		"6F1C2E093A"
	);
	testShift(
		"F1C2E093", 32,
		"15", 5,
		"06", 3,
		"DE385C1275"
	);
	testShift(
		"F1C2E093", 32,
		"25", 6,
		"06", 3,
		"01BC70B824E5"
	);
	testShift(
		"F1C2E093", 32,
		"75", 7,
		"06", 3,
		"0378E17049F5"
	);
	testShift(
		"F1C2E093", 32,
		"e5", 8,
		"06", 3,
		"06F1C2E093E5"
	);
	testShift(
		"F1C2E093", 32,
		"0115", 9,
		"06", 3,
		"0DE385C12715"
	);
	testShift(
		"F1C2E093", 32,
		"0315", 10,
		"06", 3,
		"1BC70B824F15"
	);
	testShift(
		"F1C2E093", 32,
		"0715", 11,
		"06", 3,
		"378E17049F15"
	);
	testShift(
		"F1C2E093", 32,
		"0c15", 12,
		"06", 3,
		"6F1C2E093C15"
	);
	testShift(
		"F1C2E093", 32,
		"1c15", 13,
		"06", 3,
		"DE385C127C15"
	);
	testShift(
		"F1C2E093", 32,
		"2c15", 14,
		"06", 3,
		"01BC70B824EC15"
	);
	testShift(
		"F1C2E093", 32,
		"4015", 15,
		"06", 3,
		"0378E17049C015"
	);
	testShift(
		"F1C2E093", 32,
		"8015", 16,
		"06", 3,
		"06F1C2E0938015"
	);
	testShift(
		"F1C2E093", 32,
		"018015", 17,
		"06", 3,
		"0DE385C1278015"
	);
	testShift(
		"F1C2E093", 32,
		"00", 0,
		"0A", 4,
		"0AF1C2E093"
	);
	testShift(
		"F1C2E093", 32,
		"01", 1,
		"0A", 4,
		"15E385C127"
	);
	testShift(
		"F1C2E093", 32,
		"02", 2,
		"0A", 4,
		"2BC70B824E"
	);
	testShift(
		"F1C2E093", 32,
		"06", 3,
		"0A", 4,
		"578E17049E"
	);
	testShift(
		"F1C2E093", 32,
		"0A", 4,
		"0A", 4,
		"AF1C2E093A"
	);
	testShift(
		"F1C2E093", 32,
		"15", 5,
		"0A", 4,
		"015E385C1275"
	);
	testShift(
		"F1C2E093", 32,
		"25", 6,
		"0A", 4,
		"02BC70B824E5"
	);
	testShift(
		"F1C2E093", 32,
		"75", 7,
		"0A", 4,
		"0578E17049F5"
	);
	testShift(
		"F1C2E093", 32,
		"e5", 8,
		"0A", 4,
		"0AF1C2E093E5"
	);
	testShift(
		"F1C2E093", 32,
		"0115", 9,
		"0A", 4,
		"15E385C12715"
	);
	testShift(
		"F1C2E093", 32,
		"0315", 10,
		"0A", 4,
		"2BC70B824F15"
	);
	testShift(
		"F1C2E093", 32,
		"0715", 11,
		"0A", 4,
		"578E17049F15"
	);
	testShift(
		"F1C2E093", 32,
		"0c15", 12,
		"0A", 4,
		"AF1C2E093C15"
	);
	testShift(
		"F1C2E093", 32,
		"1c15", 13,
		"0A", 4,
		"015E385C127C15"
	);
	testShift(
		"F1C2E093", 32,
		"2c15", 14,
		"0A", 4,
		"02BC70B824EC15"
	);
	testShift(
		"F1C2E093", 32,
		"4015", 15,
		"0A", 4,
		"0578E17049C015"
	);
	testShift(
		"F1C2E093", 32,
		"8015", 16,
		"0A", 4,
		"0AF1C2E0938015"
	);
	testShift(
		"F1C2E093", 32,
		"018015", 17,
		"0A", 4,
		"15E385C1278015"
	);
	testShift(
		"F1C2E093", 32,
		"00", 0,
		"0135", 9,
		"0135F1C2E093"
	);
	testShift(
		"F1C2E093", 32,
		"01", 1,
		"0135", 9,
		"026BE385C127"
	);
	testShift(
		"F1C2E093", 32,
		"02", 2,
		"0135", 9,
		"04D7C70B824E"
	);
	testShift(
		"F1C2E093", 32,
		"06", 3,
		"0135", 9,
		"09AF8E17049E"
	);
	testShift(
		"F1C2E093", 32,
		"0A", 4,
		"0135", 9,
		"135F1C2E093A"
	);
	testShift(
		"F1C2E093", 32,
		"15", 5,
		"0135", 9,
		"26BE385C1275"
	);
	testShift(
		"F1C2E093", 32,
		"25", 6,
		"0135", 9,
		"4D7C70B824E5"
	);
	testShift(
		"F1C2E093", 32,
		"75", 7,
		"0135", 9,
		"9AF8E17049F5"
	);
	testShift(
		"F1C2E093", 32,
		"e5", 8,
		"0135", 9,
		"0135F1C2E093E5"
	);
	testShift(
		"F1C2E093", 32,
		"0115", 9,
		"0135", 9,
		"026BE385C12715"
	);
	testShift(
		"F1C2E093", 32,
		"0315", 10,
		"0135", 9,
		"04D7C70B824F15"
	);
	testShift(
		"F1C2E093", 32,
		"0715", 11,
		"0135", 9,
		"09AF8E17049F15"
	);
	testShift(
		"F1C2E093", 32,
		"0c15", 12,
		"0135", 9,
		"135F1C2E093C15"
	);
	testShift(
		"F1C2E093", 32,
		"1c15", 13,
		"0135", 9,
		"26BE385C127C15"
	);
	testShift(
		"F1C2E093", 32,
		"2c15", 14,
		"0135", 9,
		"4D7C70B824EC15"
	);
	testShift(
		"F1C2E093", 32,
		"4015", 15,
		"0135", 9,
		"9AF8E17049C015"
	);
	testShift(
		"F1C2E093", 32,
		"8015", 16,
		"0135", 9,
		"0135F1C2E0938015"
	);
	testShift(
		"F1C2E093", 32,
		"018015", 17,
		"0135", 9,
		"026BE385C1278015"
	);
	testShift(
		"71C2E093", 31,
		"00", 0,
		"0135", 9,
		"9AF1C2E093"
	);
	testShift(
		"71C2E093", 31,
		"01", 1,
		"0135", 9,
		"0135E385C127"
	);
	testShift(
		"71C2E093", 31,
		"02", 2,
		"0135", 9,
		"026BC70B824E"
	);
	testShift(
		"71C2E093", 31,
		"06", 3,
		"0135", 9,
		"04D78E17049E"
	);
	testShift(
		"71C2E093", 31,
		"0A", 4,
		"0135", 9,
		"09AF1C2E093A"
	);
	testShift(
		"71C2E093", 31,
		"15", 5,
		"0135", 9,
		"135E385C1275"
	);
	testShift(
		"71C2E093", 31,
		"25", 6,
		"0135", 9,
		"26BC70B824E5"
	);
	testShift(
		"71C2E093", 31,
		"75", 7,
		"0135", 9,
		"4D78E17049F5"
	);
	testShift(
		"71C2E093", 31,
		"e5", 8,
		"0135", 9,
		"9AF1C2E093E5"
	);
	testShift(
		"71C2E093", 31,
		"0115", 9,
		"0135", 9,
		"0135E385C12715"
	);
	testShift(
		"71C2E093", 31,
		"0315", 10,
		"0135", 9,
		"026BC70B824F15"
	);
	testShift(
		"71C2E093", 31,
		"0715", 11,
		"0135", 9,
		"04D78E17049F15"
	);
	testShift(
		"71C2E093", 31,
		"0c15", 12,
		"0135", 9,
		"09AF1C2E093C15"
	);
	testShift(
		"71C2E093", 31,
		"1c15", 13,
		"0135", 9,
		"135E385C127C15"
	);
	testShift(
		"71C2E093", 31,
		"2c15", 14,
		"0135", 9,
		"26BC70B824EC15"
	);
	testShift(
		"71C2E093", 31,
		"4015", 15,
		"0135", 9,
		"4D78E17049C015"
	);
	testShift(
		"71C2E093", 31,
		"8015", 16,
		"0135", 9,
		"9AF1C2E0938015"
	);
	testShift(
		"71C2E093", 31,
		"018015", 17,
		"0135", 9,
		"0135E385C1278015"
	);
	testShift(
		"5423DF3A8F129B2C91B9425D", 95,
		"", 0,
		"01", 1,
		"D423DF3A8F129B2C91B9425D"
	);
	testShift(
		"5423DF3A8F129B2C91B9425D", 95,
		"01", 1,
		"", 0,
		"A847BE751E253659237284BB"
	);
}

void parseString(ParseContext *cxt, const char *str, struct Buffer *csvfBuf) {
	Buffer line;
	BufferStatus bStatus;
	FLStatus fStatus;
	const uint32 lineLen = strlen(str) + 1;
	bStatus = bufInitialise(&line, lineLen, 0, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	bStatus = bufAppendBlock(&line, (const uint8 *)str, lineLen, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	fStatus = parseLine(cxt, &line, csvfBuf, NULL, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	bufDestroy(&line);
}

void renderBuffer(char *&p, const Buffer *buf) {
	const char *src = (const char *)buf->data;
	uint32 length = buf->length;
	*p++ = '{';
	while ( length-- ) {
		sprintf(p, "%02X", *src & 0xFF);
		src++;
		p += 2;
	}
	*p++ = '}';
}

void renderBitStore(char *&p, const BitStore *store) {
	sprintf(p, "%d, ", store->numBits);
	p += strlen(p);
	renderBuffer(p, &store->tdi);
	*p++ = ','; *p++ = ' ';
	renderBuffer(p, &store->tdo);
	*p++ = ','; *p++ = ' ';
	renderBuffer(p, &store->mask);
}

const char *renderCxt(const ParseContext *cxt) {
	static char result[1024];
	char *p = result;

	renderBitStore(p, &cxt->dataHead);
	*p++ = ','; *p++ = ' ';
	renderBitStore(p, &cxt->insnHead);
	*p++ = ','; *p++ = ' ';

	renderBitStore(p, &cxt->dataBody);
	*p++ = ','; *p++ = ' ';
	renderBitStore(p, &cxt->insnBody);
	*p++ = ','; *p++ = ' ';

	renderBitStore(p, &cxt->dataTail);
	*p++ = ','; *p++ = ' ';
	renderBitStore(p, &cxt->insnTail);
	
	*p = '\0';

	return result;
}

TEST(FPGALink_testParse) {
	FLStatus fStatus;
	struct ParseContext cxt;
	struct Buffer csvfBuf;
	BufferStatus bStatus;
	bStatus = bufInitialise(&csvfBuf, 1024, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	fStatus = cxtInitialise(&cxt, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);

	// HDR, HIR, SDR, SIR, TDR, TIR
	parseString(&cxt, "HDR 8 TDI (aa)", &csvfBuf);
	CHECK_EQUAL(
		"8, {AA}, {00}, {00}, "    // HDR
		"0, {}, {}, {}, "          // HIR
		"0, {}, {}, {}, "          // SDR
		"0, {}, {}, {}, "          // SIR
		"0, {}, {}, {}, "          // TDR
		"0, {}, {}, {}",           // TIR
		renderCxt(&cxt));

	// HDR, HIR, SDR, SIR, TDR, TIR
	parseString(&cxt, "HDR 8 MASK (55)", &csvfBuf);
	CHECK_EQUAL(
		"8, {AA}, {00}, {55}, "    // HDR
		"0, {}, {}, {}, "          // HIR
		"0, {}, {}, {}, "          // SDR
		"0, {}, {}, {}, "          // SIR
		"0, {}, {}, {}, "          // TDR
		"0, {}, {}, {}",           // TIR
		renderCxt(&cxt));

	// HDR, HIR, SDR, SIR, TDR, TIR
	parseString(&cxt, "HDR 6 TDI (3A)", &csvfBuf);
	CHECK_EQUAL(
		"6, {3A}, {00}, {00}, "    // HDR
		"0, {}, {}, {}, "          // HIR
		"0, {}, {}, {}, "          // SDR
		"0, {}, {}, {}, "          // SIR
		"0, {}, {}, {}, "          // TDR
		"0, {}, {}, {}",           // TIR
		renderCxt(&cxt));

	cxtDestroy(&cxt);
	bufDestroy(&csvfBuf);
}

TEST(FPGALink_testInsertRunTest) {
	FLStatus fStatus;
	struct Buffer buf;
	BufferStatus bStatus;
	const uint8 initArray[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB,                                   0xFA, 0xF9, 0xF8                                  };
	const uint8 expected[]  = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, XRUNTEST, 0xCA, 0xFE, 0xBA, 0xBE, 0xFA, 0xF9, 0xF8, XRUNTEST, 0x00, 0x00, 0x00, 0x00};
	uint32 lastRunTestOffset = 0xFFFFFFFF, lastRunTestValue = 0;
	bStatus = bufInitialise(&buf, 1024, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	bStatus = bufAppendBlock(&buf, initArray, 8, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	fStatus = insertRunTestBefore(&buf, 5UL, 0xCAFEBABEUL, &lastRunTestOffset, &lastRunTestValue, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK_EQUAL(8UL+5UL+5UL, buf.length);
	CHECK_ARRAY_EQUAL(expected, buf.data, buf.length);
}

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
	const uint32 expectedLength = (uint32)strlen(expected)/2;
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
	const uint32 lineLen = (uint32)strlen(str) + 1;
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
	uint32 length = (uint32)buf->length;
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
		"8, {AA}, {00}, {FF}, "    // HDR
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
		"6, {3A}, {00}, {FF}, "    // HDR
		"0, {}, {}, {}, "          // HIR
		"0, {}, {}, {}, "          // SDR
		"0, {}, {}, {}, "          // SIR
		"0, {}, {}, {}, "          // TDR
		"0, {}, {}, {}",           // TIR
		renderCxt(&cxt));

	cxtDestroy(&cxt);
	bufDestroy(&csvfBuf);
}

static void compare(int j, CmdPtr pExpected, CmdPtr pActual) {
	const char *const sExpected = getCmdName(pExpected);
	const char *const sActual = getCmdName(pActual);
	uint32 expected = pExpected[0];
	uint32 actual = pActual[0];
	printf("  [%d] %s:%s\n", j, sExpected, sActual);
	CHECK_EQUAL(sExpected, sActual);
	if ( expected == XRUNTEST && actual == XRUNTEST ) {
		expected = readLongBE(pExpected + 1);
		actual = readLongBE(pActual + 1);
		CHECK_EQUAL(expected, actual);
	}
}

TEST(FPGALink_testProcessIndex) {
	int i, j, num;
	const CmdPtr *pExpected;
	const CmdPtr *pActual;
	CmdArray xruntest0  = {XRUNTEST, 0x00, 0x00, 0x00, 0x00};
	CmdArray xruntest1  = {XRUNTEST, 0x00, 0x00, 0x00, 0x0A};
	CmdArray xruntest2  = {XRUNTEST, 0x00, 0x00, 0x27, 0x10};
	CmdArray xcomplete = {XCOMPLETE};
	CmdArray xsdr      = {XSDR};
	CmdArray xsdrsize  = {XSDRSIZE};
	CmdArray xsdrtdo   = {XSDRTDO};
	CmdArray xsir      = {XSIR};
	CmdArray xtdomask  = {XTDOMASK};
	const CmdPtr src0[] = {
		xsir,
		xsdrsize,
		xtdomask,
		xsdrtdo,
		xsir,
		xsdrtdo,
		xsir,
		xsdrtdo,
		xsir,
		xsir,
		xsdrtdo,
		xsir,
		xruntest1,
		xsir,
		xsir,
		xruntest2,
		xsir,
		xsdrsize,
		xsdr,
		xruntest1,
		xsir,
		xsdrsize,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xcomplete
	};
	const CmdPtr exp0[] = {
		xruntest0,
		xsir,
		xsdrsize,
		xtdomask,
		xsdrtdo,
		xsir,
		xsdrtdo,
		xsir,
		xsdrtdo,
		xsir,
		xsir,
		xsdrtdo,
		xruntest1,
		xsir,
		xruntest0,
		xsir,
		xruntest2,
		xsir,
		xruntest0,
		xsir,
		xruntest1,
		xsdrsize,
		xsdr,
		xruntest0,
		xsir,
		xsdrsize,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xsdr,
		xcomplete
	};
	const CmdPtr src1[] = {
		xsir,
		xsdrsize,
		xsdr,
		xsdr,
		xcomplete
	};
	const CmdPtr exp1[] = {
		xruntest0,
		xsir,
		xsdrsize,
		xsdr,
		xsdr,
		xcomplete
	};
	const CmdPtr src2[] = {
		xsir,
		xsdrsize,
		xsdr,
		xsdr,
		xruntest1,
		xcomplete
	};
	const CmdPtr exp2[] = {
		xruntest0,
		xsir,
		xsdrsize,
		xsdr,
		xruntest1,
		xsdr,
		xcomplete
	};
	const CmdPtr src3[] = {
		xsir,
		xsdrsize,
		xsdr,
		xruntest1,
		xsdr,
		xcomplete
	};
	const CmdPtr exp3[] = {
		xruntest0,
		xsir,
		xruntest1,
		xsdrsize,
		xsdr,
		xruntest0,
		xsdr,
		xcomplete
	};
	const CmdPtr src4[] = {
		xsir,
		xsdrsize,
		xtdomask,
		xsdrtdo,
		xsdrsize,
		xsdr,
		xruntest1,
		xcomplete
	};
	const CmdPtr exp4[] = {
		xruntest0,
		xsir,
		xsdrsize,
		xtdomask,
		xsdrtdo,
		xruntest1,
		xsdrsize,
		xsdr,
		xcomplete
	};
	const CmdPtr *const src[] = {src0, src1, src2, src3, src4};
	const CmdPtr *const exp[] = {exp0, exp1, exp2, exp3, exp4};
	const uint8 *dst[1024];  // worst case 2x
	num = sizeof(src) / sizeof(src[0]);
	for ( i = 0; i < num; i++ ) {
		printf("Dataset %d\n", i);
		processIndex(src[i], dst);
		pExpected = exp[i];
		pActual = dst;
		j = 0;
		while ( **pExpected != XCOMPLETE && **pActual != XCOMPLETE ) {
			compare(j++, *pExpected++, *pActual++);
		}
		printf("  [%d] %s:%s\n", j, getCmdName(*pExpected), getCmdName(*pActual));
		CHECK(**pExpected == XCOMPLETE);
		CHECK(**pActual == XCOMPLETE);
	}
}

TEST(FPGALink_testBuildIndex) {
	const uint8 src[] = {
		0x02, 0x06, 0x09, 0x08,  0x00, 0x00, 0x00, 0x20,
		0x01, 0xFF, 0xFF, 0xFF,  0x0F, 0x09, 0x00, 0x93,
		0x00, 0x80, 0x00, 0x61,  0x00, 0xF2, 0x08, 0x00,
		0x00, 0x00, 0x05, 0x03,  0x00, 0x04, 0x00, 0x00,
		0x00, 0x10, 0x00
	};
	const size_t len = sizeof(src);
	struct Buffer csvfBuf;
	FLStatus fStatus;
	struct ParseContext cxt;
	BufferStatus bStatus = bufInitialise(&csvfBuf, len, 0x00, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	memset(&cxt, 0, sizeof(cxt));
	cxt.numCommands = 8;
	bStatus = bufAppendBlock(&csvfBuf, src, len, NULL);
	CHECK_EQUAL(BUF_SUCCESS, bStatus);
	fStatus = buildIndex(&cxt, &csvfBuf, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	bufDestroy(&csvfBuf);
}

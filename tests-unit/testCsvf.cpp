/* 
 * Copyright (C) 2010 Chris McClelland
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
#include <UnitTest++.h>
#include <makestuff.h>
#include <libbuffer.h>
#include "../xsvf2csvf.h"
#include "../csvf2xsvf.h"
#include "../xsvf.h"

static void testRoundTrip(const char *xsvfFile, uint32 expectedMaxBufSize) {
	Buffer csvfBuf, uncompressedBuf;
	BufferStatus bStatus;
	uint32 maxBufSize;
	X2CStatus xStatus;
	bStatus = bufInitialise(&csvfBuf, 1024, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	bStatus = bufInitialise(&uncompressedBuf, 1024, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	xStatus = loadXsvfAndConvertToCsvf(xsvfFile, &csvfBuf, &maxBufSize, &uncompressedBuf, NULL);
	CHECK(xStatus == X2C_SUCCESS);
	CHECK_EQUAL(expectedMaxBufSize, maxBufSize);

	Context cp;
	uint8 thisByte;
	thisByte = csvfInitReader(&cp, csvfBuf.data);
	CHECK(thisByte == 0x00);

	Buffer reconstituteBuf;
	bStatus = bufInitialise(&reconstituteBuf, uncompressedBuf.length, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	for ( uint32 i = 0; i < uncompressedBuf.length; i++ ) {
		bStatus = bufAppendByte(&reconstituteBuf, csvfGetByte(&cp), NULL);
		CHECK(bStatus == BUF_SUCCESS);
	}
	CHECK_EQUAL(uncompressedBuf.length, reconstituteBuf.length);
	CHECK_ARRAY_EQUAL(uncompressedBuf.data, reconstituteBuf.data, uncompressedBuf.length);

	//bStatus = bufWriteBinaryFile(&uncompressedBuf, "uncompressed.dat", 0x00000000, uncompressedBuf.length, NULL);
	//CHECK(bStatus == BUF_SUCCESS);
	//bStatus = bufWriteBinaryFile(&reconstituteBuf, "reconstitute.dat", 0x00000000, reconstituteBuf.length, NULL);
	//CHECK(bStatus == BUF_SUCCESS);

	bufDestroy(&reconstituteBuf);
	bufDestroy(&uncompressedBuf);
	bufDestroy(&csvfBuf);
}

TEST(FPGALink_testRoundTrip) {
	testRoundTrip("../gen_xsvf/s3board.xsvf", 12U);
	testRoundTrip("../gen_xsvf/nexys2-500.xsvf", 12U);
	testRoundTrip("../gen_xsvf/nexys2-1200.xsvf", 12U);
	testRoundTrip("../gen_xsvf/atlys.xsvf", 4U);
	testRoundTrip("../gen_xsvf/nexys3.xsvf", 4U);
}

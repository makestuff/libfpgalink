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
#include <UnitTest++.h>
#include <makestuff.h>
#include <libbuffer.h>
#include "../private.h"
#include "../xsvf.h"
#include "../csvfreader.h"

static void testRoundTrip(const char *xsvfFile, uint32 expectedMaxBufSize) {
	Buffer csvfBuf, uncompressedBuf;
	BufferStatus bStatus;
	uint32 maxBufSize;
	FLStatus fStatus;
	bStatus = bufInitialise(&csvfBuf, 1024, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	bStatus = bufInitialise(&uncompressedBuf, 1024, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);

	// Load XSVF, convert to uncompressed CSVF
	fStatus = flLoadXsvfAndConvertToCsvf(xsvfFile, &csvfBuf, &maxBufSize, NULL);
	CHECK(fStatus == FL_SUCCESS);
	CHECK_EQUAL(expectedMaxBufSize, maxBufSize);

	// Make a copy of the uncompressed buffer
	bStatus = bufDeepCopy(&uncompressedBuf, &csvfBuf, NULL);
	CHECK(bStatus == BUF_SUCCESS);

	// Compress the CSVF buffer
	fStatus = flCompressCsvf(&csvfBuf, NULL);
	CHECK(fStatus == FL_SUCCESS);

	// Make a reader to iterate over the compressed data
	Context cp;
	uint8 thisByte;
	thisByte = csvfInitReader(&cp, csvfBuf.data, true);
	CHECK(thisByte == 0x00);

	// Uncompress the compressed data into the reconstituteBuf
	Buffer reconstituteBuf;
	bStatus = bufInitialise(&reconstituteBuf, uncompressedBuf.length, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	for ( uint32 i = 0; i < uncompressedBuf.length; i++ ) {
		bStatus = bufAppendByte(&reconstituteBuf, csvfGetByte(&cp), NULL);
		CHECK(bStatus == BUF_SUCCESS);
	}

	// Make sure the result of the compress-uncompress operation is the same as the original data
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
	testRoundTrip("../gen_xsvf/ex_cksum_atlys_fx2_verilog.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_cksum_atlys_fx2_vhdl.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_cksum_lx9_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_lx9_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_nexys2-1200_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_nexys2-1200_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_nexys2-500_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_nexys2-500_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_nexys3_fx2_verilog.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_cksum_nexys3_fx2_vhdl.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_cksum_s3board_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_s3board_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_cksum_xylo-l_fx2_verilog.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_cksum_xylo-l_fx2_vhdl.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_fifo_atlys_fx2_verilog.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_fifo_atlys_fx2_vhdl.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_fifo_lx9_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_lx9_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_nexys2-1200_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_nexys2-1200_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_nexys2-500_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_nexys2-500_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_nexys3_fx2_verilog.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_fifo_nexys3_fx2_vhdl.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_fifo_s3board_fx2_verilog.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_s3board_fx2_vhdl.xsvf", 5U);
	testRoundTrip("../gen_xsvf/ex_fifo_xylo-l_fx2_verilog.xsvf", 4U);
	testRoundTrip("../gen_xsvf/ex_fifo_xylo-l_fx2_vhdl.xsvf", 4U);
}

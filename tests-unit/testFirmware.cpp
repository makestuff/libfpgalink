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
#include <UnitTest++.h>
#include <makestuff.h>
#include <libbuffer.h>
#include <libfx2loader.h>
#include "../private.h"
#include "../firmware.h"

void testPatchRamFirmware(const char *expFile, uint8 port, uint8 tdo, uint8 tdi, uint8 tms, uint8 tck) {
	Buffer actual, expected;
	FLStatus fStatus;
	BufferStatus bStatus;
	bStatus = bufInitialise(&actual, 1024, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	fStatus = copyFirmwareAndRewriteIDs(
		&ramFirmware, 0x04B4, 0x8613,
		port, tdo, tdi, tms, tck,
		&actual, NULL);

	bStatus = bufInitialise(&expected, 0x4000, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	bStatus = bufReadFromIntelHexFile(&expected, NULL, expFile, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	CHECK_EQUAL(expected.length, actual.length);

	CHECK_ARRAY_EQUAL(expected.data, actual.data, actual.length);

	bufDestroy(&expected);
	bufDestroy(&actual);
}

void testPatchPromFirmware(const char *expFile, const FirmwareInfo *fwInfo, uint8 port, uint8 tdo, uint8 tdi, uint8 tms, uint8 tck) {
	Buffer actual, expected, data, mask;
	FLStatus fStatus;
	BufferStatus bStatus;
	I2CStatus iStatus;
	bStatus = bufInitialise(&actual, 1024, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	fStatus = copyFirmwareAndRewriteIDs(
		fwInfo, 0x04B4, 0x8613,
		port, tdo, tdi, tms, tck,
		&actual, NULL);

	bStatus = bufInitialise(&expected, 0x4000, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	bStatus = bufInitialise(&data, 0x4000, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	bStatus = bufInitialise(&mask, 0x4000, 0x00, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	i2cInitialise(&expected, 0x0000, 0x0000, 0x0000, CONFIG_BYTE_400KHZ);
	bStatus = bufReadFromIntelHexFile(&data, &mask, expFile, NULL);
	CHECK(bStatus == BUF_SUCCESS);
	iStatus = i2cWritePromRecords(&expected, &data, &mask, NULL);
	CHECK(iStatus == I2C_SUCCESS);
	iStatus = i2cFinalise(&expected, NULL);
	CHECK(iStatus == I2C_SUCCESS);
	
	CHECK_EQUAL(expected.length, actual.length);

	//bStatus = bufWriteBinaryFile(&expected, "expected.bin", 0x00000000, expected.length, NULL);
	//CHECK(bStatus == BUF_SUCCESS);
	//bStatus = bufWriteBinaryFile(&actual, "actual.bin", 0x00000000, actual.length, NULL);
	//CHECK(bStatus == BUF_SUCCESS);

	CHECK_ARRAY_EQUAL(expected.data, actual.data, actual.length);

	bufDestroy(&mask);
	bufDestroy(&data);
	bufDestroy(&expected);
	bufDestroy(&actual);
}

TEST(FPGALink_testPatchFirmware) {
	// TDO=PD0, TDI=PD1, TMS=PD2, TCK=PD3
	testPatchRamFirmware("../gen_fw/ramFirmware1.hex", 1, 0, 1, 2, 3);
	testPatchPromFirmware("../gen_fw/eepromWithBootFirmware1.hex", &eepromWithBootFirmware, 1, 0, 1, 2, 3);
	testPatchPromFirmware("../gen_fw/eepromNoBootFirmware1.hex", &eepromNoBootFirmware, 1, 0, 1, 2, 3);

	// TDO=PC7, TDI=PC6, TMS=PC5, TCK=PC4
	testPatchRamFirmware("../gen_fw/ramFirmware2.hex", 0, 7, 6, 5, 4);
	testPatchPromFirmware("../gen_fw/eepromWithBootFirmware2.hex", &eepromWithBootFirmware, 0, 7, 6, 5, 4);
	testPatchPromFirmware("../gen_fw/eepromNoBootFirmware2.hex", &eepromNoBootFirmware, 0, 7, 6, 5, 4);
}

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <makestuff.h>
#include <libbuffer.h>
#include <libfx2loader.h>
#include <liberror.h>
#include "../firmware.h"

#define CHECK(condition, retCode) \
	if ( condition ) { \
		FAIL(retCode); \
	}

void dumpBytes(const uint8 *ptr, uint16 numBytes) {
	const uint8 *const end = ptr + numBytes;
	uint8 count = 1;
	printf("\t0x%02X", *ptr++);
	while ( ptr < end ) {
		if ( count == 0 ) {
			printf(",\n\t0x%02X", *ptr++);
		} else {
			printf(", 0x%02X", *ptr++);
		}
		count++;
		count &= 15;
	}
	printf("\n");
}
void dumpWords(const uint16 *ptr, uint16 numWords) {
	const uint16 *const end = ptr + numWords;
	uint8 count = 1;
	printf("\t0x%04X", *ptr++);
	while ( ptr < end ) {
		if ( count == 0 ) {
			printf(",\n\t0x%04X", *ptr++);
		} else {
			printf(", 0x%04X", *ptr++);
		}
		count++;
		count &= 7;
	}
	printf("\n");
}

int dumpCode(const char *progName, const char *name, const struct Buffer *buf) {
	int returnCode = 0;
	uint16 i;
	uint16 vp = 0;

	for ( i = 1; i < buf->length; i++ ) {
		if (
			buf->data[i] == 0xB4 && buf->data[i+1] == 0x04 &&
			buf->data[i+2] == 0x13 && buf->data[i+3] == 0x86 )
		{
			if ( vp ) {
				fprintf(stderr, "%s: Refusing to override VID:PID@%04X with %04X\n", progName, vp, i);
				FAIL(22);
			}
			vp = i;
			i += 3;
		}
	}
	if ( !vp ) {
		fprintf(stderr, "%s: Not enough occurrances of vp\n", progName);
		FAIL(44);
	}

	printf("/*\n * THIS FILE IS MACHINE-GENERATED! DO NOT EDIT IT!\n */\n");
	printf("#include \"../firmware.h\"\n\n");
	printf("static const uint8 data[] = {\n");
	dumpBytes(buf->data, buf->length);
	printf("};\n");
	printf("const struct FirmwareInfo %sFirmware = {\n", name);
	printf("\tdata, %d, 0x%04X\n", buf->length, vp);
	printf("};\n");
cleanup:
	return returnCode;
}

static void usage(const char *progName) {
	fprintf(stderr, "Synopsis: %s <fw.hex> <name> <bix|iic>\n", progName);
}

int main(int argc, const char *argv[]) {
	int returnCode = 0;
	struct Buffer data = {0,};
	struct Buffer mask = {0,};
	struct Buffer i2c = {0,};
	BufferStatus bStatus;
	I2CStatus iStatus;
	int dStatus;
	uint8 configByte;
	const char *error = NULL;

	if ( argc != 4 ) {
		usage(argv[0]);
		FAIL(1);
	}

	if ( strstr(argv[2], "WithBoot") ) {
		// Boot firmware explicitly connects
		configByte = CONFIG_BYTE_400KHZ | CONFIG_BYTE_DISCON;
	} else {
		// NonBoot firmwares are connected automatically
		configByte = CONFIG_BYTE_400KHZ;
	}

	bStatus = bufInitialise(&data, 0x4000, 0x00, &error);
	CHECK(bStatus, 2);
	bStatus = bufInitialise(&mask, 0x4000, 0x00, &error);
	CHECK(bStatus, 3);
	bStatus = bufReadFromIntelHexFile(&data, &mask, argv[1], &error);
	CHECK(bStatus, 4);

	if ( !strcmp("iic", argv[3]) ) {
		// Get i2c records
		bStatus = bufInitialise(&i2c, 0x4000, 0x00, &error);
		CHECK(bStatus, 8);
		i2cInitialise(&i2c, 0x0000, 0x0000, 0x0000, configByte);
		iStatus = i2cWritePromRecords(&i2c, &data, &mask, &error);
		CHECK(iStatus, 9);
		iStatus = i2cFinalise(&i2c, &error);
		CHECK(iStatus, 10);

		// Dump the code
		dStatus = dumpCode(argv[0], argv[2], &i2c);
		CHECK(dStatus, dStatus);
	} else if ( !strcmp("bix", argv[3]) ) {
		// Dump the code
		dStatus = dumpCode(argv[0], argv[2], &data);
		CHECK(dStatus, dStatus);
	} else {
		usage(argv[0]);
		FAIL(14);
	}

cleanup:
	if ( error ) {
		fprintf(stderr, "%s: %s\n", argv[0], error);
		errFree(error);
	}
	bufDestroy(&i2c);
	bufDestroy(&mask);
	bufDestroy(&data);
	return returnCode;
}

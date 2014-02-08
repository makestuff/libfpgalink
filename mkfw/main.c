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

void dumpBytes(const uint8 *ptr, uint32 numBytes) {
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

#define VID_MSB 0x1D
#define VID_LSB 0x50
#define PID_MSB 0x60
#define PID_LSB 0x2B

int dumpCode(const char *progName, const char *name, const struct Buffer *buf) {
	int retVal = 0;
	uint32 i;
	uint32 vp = 0;

	for ( i = 1; i < buf->length; i++ ) {
		if (
			buf->data[i] == VID_LSB && buf->data[i+1] == VID_MSB &&
			buf->data[i+2] == PID_LSB && buf->data[i+3] == PID_MSB )
		{
			if ( vp ) {
				fprintf(stderr, "%s: Refusing to override VID:PID@%04X with %04X\n", progName, vp, i);
				FAIL(9, cleanup);
			}
			vp = i;
			i += 3;
		}
	}
	if ( !vp ) {
		fprintf(stderr, "%s: Not enough occurrances of vp\n", progName);
		FAIL(10, cleanup);
	}

	printf("/*\n * THIS FILE IS MACHINE-GENERATED! DO NOT EDIT IT!\n */\n");
	printf("#include \"../firmware.h\"\n\n");
	printf("static const uint8 data[] = {\n");
	dumpBytes(buf->data, (uint32)buf->length);
	printf("};\n");
	printf("const struct FirmwareInfo %sFirmware = {\n", name);
	printf("\tdata, "PFSZD", 0x%04X\n", buf->length, vp);
	printf("};\n");
cleanup:
	return retVal;
}

static void usage(const char *progName) {
	fprintf(stderr, "Synopsis: %s <fw.hex> <name> <bix|iic>\n", progName);
}

int main(int argc, const char *argv[]) {
	int retVal = 0;
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
		FAIL(1, cleanup);
	}

	if ( strstr(argv[2], "WithBoot") ) {
		// Boot firmware explicitly connects
		configByte = CONFIG_BYTE_400KHZ | CONFIG_BYTE_DISCON;
	} else {
		// NonBoot firmwares are connected automatically
		configByte = CONFIG_BYTE_400KHZ;
	}

	bStatus = bufInitialise(&data, 0x4000, 0x00, &error);
	CHECK_STATUS(bStatus, 2, cleanup);
	bStatus = bufInitialise(&mask, 0x4000, 0x00, &error);
	CHECK_STATUS(bStatus, 3, cleanup);
	bStatus = bufReadFromIntelHexFile(&data, &mask, argv[1], &error);
	CHECK_STATUS(bStatus, 4, cleanup);

	if ( !strcmp("iic", argv[3]) ) {
		// Get i2c records
		bStatus = bufInitialise(&i2c, 0x4000, 0x00, &error);
		CHECK_STATUS(bStatus, 5, cleanup);
		i2cInitialise(&i2c, 0x0000, 0x0000, 0x0000, configByte);
		iStatus = i2cWritePromRecords(&i2c, &data, &mask, &error);
		CHECK_STATUS(iStatus, 6, cleanup);
		iStatus = i2cFinalise(&i2c, &error);
		CHECK_STATUS(iStatus, 7, cleanup);

		// Dump the code
		dStatus = dumpCode(argv[0], argv[2], &i2c);
		CHECK_STATUS(dStatus, dStatus, cleanup);
	} else if ( !strcmp("bix", argv[3]) ) {
		// Dump the code
		dStatus = dumpCode(argv[0], argv[2], &data);
		CHECK_STATUS(dStatus, dStatus, cleanup);
	} else {
		usage(argv[0]);
		FAIL(8, cleanup);
	}

cleanup:
	if ( error ) {
		fprintf(stderr, "%s: %s\n", argv[0], error);
		errFree(error);
	}
	bufDestroy(&i2c);
	bufDestroy(&mask);
	bufDestroy(&data);
	return retVal;
}

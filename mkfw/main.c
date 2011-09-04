/* 
 * Copyright (C) 2011 Chris McClelland
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <makestuff.h>
#include <libbuffer.h>
#include <libfx2loader.h>
#include <liberror.h>

void dumpCode(const char *name, const struct Buffer *buf) {
	if ( buf->length > 0 ) {
		const uint8 *ptr;
		const uint8 *end;
		uint8 count;
		printf("const unsigned int %sFirmwareSize = %du;\nconst unsigned char %sFirmwareData[] = {\n", name, buf->length, name);
		ptr = buf->data;
		end = buf->data + buf->length;
		count = 1;
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
		printf("\n};\n");
	}
}

int main(int argc, const char *argv[]) {
	int returnCode;
	struct Buffer data = {0,};
	struct Buffer mask = {0,};
	struct Buffer i2c = {0,};
	BufferStatus status;
	const char *error;

	if ( argc != 4 && argc != 5 ) {
		fprintf(stderr, "Synopsis: %s <firmware.hex> <name> <bix|iic> [<outfile.iic>]\n", argv[0]);
		FAIL(-1);
	}

	status = bufInitialise(&data, 0x4000, 0x00, &error);
	if ( status != BUF_SUCCESS ) {
		fprintf(stderr, "%s\n", error);
		errFree(error);
		FAIL(-2);
	}

	status = bufInitialise(&mask, 0x4000, 0x00, &error);
	if ( status != BUF_SUCCESS ) {
		fprintf(stderr, "%s\n", error);
		errFree(error);
		FAIL(-3);
	}

	status = bufReadFromIntelHexFile(&data, &mask, argv[1], &error);
	if ( status != BUF_SUCCESS ) {
		fprintf(stderr, "%s\n", error);
		errFree(error);
		FAIL(-4);
	}

	if ( !strcmp("iic", argv[3]) ) {
		status = bufInitialise(&i2c, 0x4000, 0x00, &error);
		if ( status != BUF_SUCCESS ) {
			fprintf(stderr, "%s\n", error);
			errFree(error);
			FAIL(-5);
		}
		i2cInitialise(&i2c, 0x0000, 0x0000, 0x0000, CONFIG_BYTE_400KHZ);
		if ( i2cWritePromRecords(&i2c, &data, &mask, &error) ) {
			fprintf(stderr, "%s\n", error);
			errFree(error);
			FAIL(-6);
		}
		if ( i2cFinalise(&i2c, &error) ) {
			fprintf(stderr, "%s\n", error);
			errFree(error);
			FAIL(-7);
		}
		dumpCode(argv[2], &i2c);
		if ( argc == 5 ) {
			if ( bufWriteBinaryFile(&i2c, argv[4], 0, i2c.length, &error) ) {
				fprintf(stderr, "%s\n", error);
				errFree(error);
				FAIL(-8);
			}
		}
		bufDestroy(&i2c);
	} else if ( !strcmp("bix", argv[3]) ) {
		dumpCode(argv[2], &data);
	} else {
		fprintf(stderr, "Synopsis: %s <firmware.hex> <name> <bix|iic> [<outfile.iic>]\n", argv[0]);
		FAIL(-8);
	}

	returnCode = 0;

cleanup:
	if ( i2c.data ) {
		bufDestroy(&i2c);
	}
	if ( mask.data ) {
		bufDestroy(&mask);
	}
	if ( data.data ) {
		bufDestroy(&data);
	}
	return returnCode;
}

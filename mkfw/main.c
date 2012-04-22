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

#define JTAG_PORT_1 1
#define TDO_BIT_1   0
#define TDI_BIT_1   1
#define TMS_BIT_1   2
#define TCK_BIT_1   3

#define JTAG_PORT_2 0
#define TDO_BIT_2   7
#define TDI_BIT_2   6
#define TMS_BIT_2   5
#define TCK_BIT_2   4

#define TDO_ADDR_1 (0xA0 + 16*JTAG_PORT_1 + TDO_BIT_1)
#define TDI_ADDR_1 (0xA0 + 16*JTAG_PORT_1 + TDI_BIT_1)
#define TMS_ADDR_1 (0xA0 + 16*JTAG_PORT_1 + TMS_BIT_1)
#define TCK_ADDR_1 (0xA0 + 16*JTAG_PORT_1 + TCK_BIT_1)

#define bmTDO_1 (1<<TDO_BIT_1)
#define bmTDI_1 (1<<TDI_BIT_1)
#define bmTMS_1 (1<<TMS_BIT_1)
#define bmTCK_1 (1<<TCK_BIT_1)

#define ALLBITS_1      (bmTDO_1|bmTDI_1|bmTMS_1|bmTCK_1)
#define ALLBITS_COMP_1 ((uint8)~ALLBITS_1)
#define OUTBITS_1      (bmTDI_1|bmTMS_1|bmTCK_1)
#define OUTBITS_COMP_1 ((uint8)~OUTBITS_1)

#define TDO_ADDR_2 (0xA0 + 16*JTAG_PORT_2 + TDO_BIT_2)
#define TDI_ADDR_2 (0xA0 + 16*JTAG_PORT_2 + TDI_BIT_2)
#define TMS_ADDR_2 (0xA0 + 16*JTAG_PORT_2 + TMS_BIT_2)
#define TCK_ADDR_2 (0xA0 + 16*JTAG_PORT_2 + TCK_BIT_2)

#define bmTDO_2 (1<<TDO_BIT_2)
#define bmTDI_2 (1<<TDI_BIT_2)
#define bmTMS_2 (1<<TMS_BIT_2)
#define bmTCK_2 (1<<TCK_BIT_2)

#define ALLBITS_2      (bmTDO_2|bmTDI_2|bmTMS_2|bmTCK_2)
#define ALLBITS_COMP_2 ((uint8)~ALLBITS_2)
#define OUTBITS_2      (bmTDI_2|bmTMS_2|bmTCK_2)
#define OUTBITS_COMP_2 ((uint8)~OUTBITS_2)

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

int dumpCode(
	const char *progName, const char *name, const struct Buffer *buf1, const struct Buffer *buf2)
{
	int returnCode = 0;
	uint16 i;
	uint16 d0A = 0;
	uint16 d0B = 0;
	uint16 vp = 0;
	uint16 outBits = 0;
	uint16 outBitsComp = 0;
	uint16 oeRegs[NUM_OE_BITS+1];
	uint16 oeRegsLen = 0;
	uint16 allBits[NUM_ALL_BITS+1];
	uint16 allBitsLen = 0;
	uint16 allBitsComp[NUM_ALL_BITS+1];
	uint16 allBitsCompLen = 0;
	uint16 tdoBit[NUM_TDO_BIT+1];
	uint16 tdoBitLen = 0;
	uint16 tdiBit[NUM_TDI_BIT+1];
	uint16 tdiBitLen = 0;
	uint16 tmsBit[NUM_TMS_BIT+1];
	uint16 tmsBitLen = 0;
	uint16 tckBit[NUM_TCK_BIT+1];
	uint16 tckBitLen = 0;

	if ( buf1->length != buf2->length ) {
		fprintf(stderr, "%s: File lengths differ (%d != %d)\n", progName, buf1->length, buf2->length);
		FAIL(20);
	}

	if ( *buf1->data != *buf2->data ) {
		fprintf(stderr, "%s: First bytes differ (0x%02X != 0x%02X)\n", progName, *buf1->data, *buf2->data);
		FAIL(21);
	}

	for ( i = 1; i < buf1->length; i++ ) {
		if (
			buf1->data[i] == 0xB4 && buf2->data[i] == 0xB4 &&
			buf1->data[i+1] == 0x04 && buf2->data[i+1] == 0x04 &&
			buf1->data[i+2] == 0x13 && buf2->data[i+2] == 0x13 &&
			buf1->data[i+3] == 0x86 && buf2->data[i+3] == 0x86 )
		{
			if ( vp ) {
				fprintf(stderr, "%s: Refusing to override VID:PID@%04X with %04X\n", progName, vp, i);
				FAIL(22);
			}
			vp = i;
			i += 3;
		} else if ( buf1->data[i] != buf2->data[i] ) {
			if ( buf1->data[i] == 0x0A && buf2->data[i] == 0x08 ) {
				if ( d0A ) {
					fprintf(stderr, "%s: Refusing to override 0A->08@%04X with %04X\n", progName, d0A, i);
					FAIL(23);
				}
				d0A = i;
			} else if ( buf1->data[i] == 0x0B && buf2->data[i] == 0x09 ) {
				if ( d0B ) {
					fprintf(stderr, "%s: Refusing to override 0B->09@%04X with %04X\n", progName, d0B, i);
					FAIL(24);
				}
				d0B = i;
			} else if ( buf1->data[i] == OUTBITS_1 && buf2->data[i] == OUTBITS_2 ) {
				if ( outBits ) {
					fprintf(stderr, "%s: Refusing to override outBits@%04X with %04X\n", progName, outBits, i);
					FAIL(25);
				}
				outBits = i;
			} else if ( buf1->data[i] == OUTBITS_COMP_1 && buf2->data[i] == OUTBITS_COMP_2 ) {
				if ( outBitsComp ) {
					fprintf(stderr, "%s: Refusing to override outBitsComp@%04X with %04X\n", progName, outBitsComp, i);
					FAIL(26);
				}
				outBitsComp = i;
			} else if ( buf1->data[i] == 0xB5 && buf2->data[i] == 0xB4 ) {
				if ( oeRegsLen > NUM_OE_BITS ) {
					fprintf(stderr, "%s: Too many occurrances of oeRegs@%04X\n", progName, i);
					FAIL(27);
				}
				oeRegs[oeRegsLen++] = i;
			} else if ( buf1->data[i] == ALLBITS_1 && buf2->data[i] == ALLBITS_2 ) {
				if ( allBitsLen > NUM_ALL_BITS ) {
					fprintf(stderr, "%s: Too many occurrances of allBits@%04X\n", progName, i);
					FAIL(28);
				}
				allBits[allBitsLen++] = i;
			} else if ( buf1->data[i] == ALLBITS_COMP_1 && buf2->data[i] == ALLBITS_COMP_2 ) {
				if ( allBitsCompLen > NUM_ALL_BITS ) {
					fprintf(stderr, "%s: Too many occurrances of allBitsComp@%04X\n", progName, i);
					FAIL(29);
				}
				allBitsComp[allBitsCompLen++] = i;
			} else if ( buf1->data[i] == TDO_ADDR_1 && buf2->data[i] == TDO_ADDR_2 ) {
				if ( tdoBitLen > NUM_TDO_BIT ) {
					fprintf(stderr, "%s: Too many occurrances of tdoBit@%04X\n", progName, i);
					FAIL(30);
				}
				tdoBit[tdoBitLen++] = i;
			} else if ( buf1->data[i] == TDI_ADDR_1 && buf2->data[i] == TDI_ADDR_2 ) {
				if ( tdiBitLen > NUM_TDI_BIT ) {
					fprintf(stderr, "%s: Too many occurrances of tdiBit@%04X\n", progName, i);
					FAIL(31);
				}
				tdiBit[tdiBitLen++] = i;
			} else if ( buf1->data[i] == TMS_ADDR_1 && buf2->data[i] == TMS_ADDR_2 ) {
				if ( tmsBitLen > NUM_TMS_BIT ) {
					fprintf(stderr, "%s: Too many occurrances of tmsBit@%04X\n", progName, i);
					FAIL(32);
				}
				tmsBit[tmsBitLen++] = i;
			} else if ( buf1->data[i] == TCK_ADDR_1 && buf2->data[i] == TCK_ADDR_2 ) {
				if ( tckBitLen > NUM_TCK_BIT ) {
					fprintf(stderr, "%s: Too many occurrances of tckBit@%04X\n", progName, i);
					FAIL(33);
				}
				tckBit[tckBitLen++] = i;
			} else {
				fprintf(stderr, "%s: Unsupported diff: %02X->%02X@%04X\n", progName, buf1->data[i], buf2->data[i], i);
				FAIL(34);
			}
		}
	}
	if ( oeRegsLen != NUM_OE_BITS ) {
		fprintf(stderr, "%s: Not enough occurrances of oeRegs\n", progName);
		FAIL(35);
	}
	if ( allBitsLen != NUM_ALL_BITS ) {
		fprintf(stderr, "%s: Not enough occurrances of allBits\n", progName);
		FAIL(36);
	}
	if ( allBitsCompLen != NUM_ALL_BITS ) {
		fprintf(stderr, "%s: Not enough occurrances of allBitsComp\n", progName);
		FAIL(37);
	}
	if ( tdoBitLen != NUM_TDO_BIT ) {
		fprintf(stderr, "%s: Not enough occurrances of tdoBit\n", progName);
		FAIL(38);
	}
	if ( tdiBitLen != NUM_TDI_BIT ) {
		fprintf(stderr, "%s: Not enough occurrances of tdiBit\n", progName);
		FAIL(39);
	}
	if ( tmsBitLen != NUM_TMS_BIT ) {
		fprintf(stderr, "%s: Not enough occurrances of tmsBit\n", progName);
		FAIL(40);
	}
	if ( tckBitLen != NUM_TCK_BIT ) {
		fprintf(stderr, "%s: Not enough occurrances of tckBit\n", progName);
		FAIL(41);
	}
	if ( !d0A ) {
		fprintf(stderr, "%s: Not enough occurrances of d0A\n", progName);
		FAIL(42);
	}
	if ( !d0B ) {
		fprintf(stderr, "%s: Not enough occurrances of d0B\n", progName);
		FAIL(43);
	}
	if ( !vp ) {
		fprintf(stderr, "%s: Not enough occurrances of vp\n", progName);
		FAIL(44);
	}
	if ( !outBits ) {
		fprintf(stderr, "%s: Not enough occurrances of outBits\n", progName);
		FAIL(45);
	}
	if ( !outBitsComp ) {
		fprintf(stderr, "%s: Not enough occurrances of outBitsComp\n", progName);
		FAIL(46);
	}

	oeRegs[oeRegsLen++] = 0x0000;
	allBits[allBitsLen++] = 0x0000;
	allBitsComp[allBitsCompLen++] = 0x0000;
	tdoBit[tdoBitLen++] = 0x0000;
	tdiBit[tdiBitLen++] = 0x0000;
	tmsBit[tmsBitLen++] = 0x0000;
	tckBit[tckBitLen++] = 0x0000;

	printf("/*\n * THIS FILE IS MACHINE-GENERATED! DO NOT EDIT IT!\n */\n");
	printf("#include \"../firmware.h\"\n\n");
	printf("static const uint8 data[] = {\n");
	dumpBytes(buf1->data, buf1->length);
	printf("};\n");
	printf("static const uint16 oeRegs[] = {\n");
	dumpWords(oeRegs, oeRegsLen);
	printf("};\n");
	printf("static const uint16 allBits[] = {\n");
	dumpWords(allBits, allBitsLen);
	printf("};\n");
	printf("static const uint16 allBitsComp[] = {\n");
	dumpWords(allBitsComp, allBitsCompLen);
	printf("};\n");
	printf("static const uint16 tdoBit[] = {\n");
	dumpWords(tdoBit, tdoBitLen);
	printf("};\n");
	printf("static const uint16 tdiBit[] = {\n");
	dumpWords(tdiBit, tdiBitLen);
	printf("};\n");
	printf("static const uint16 tmsBit[] = {\n");
	dumpWords(tmsBit, tmsBitLen);
	printf("};\n");
	printf("static const uint16 tckBit[] = {\n");
	dumpWords(tckBit, tckBitLen);
	printf("};\n");
	printf("const struct FirmwareInfo %sFirmware = {\n", name);
	printf("\tdata, %d,\n", buf1->length);
	printf("\t0x%04X, 0x%04X, 0x%04X,\n", vp, d0A, d0B);
	printf("\t0x%04X, 0x%04X,\n", outBits, outBitsComp);
	printf("\toeRegs,\n");
	printf("\tallBits, allBitsComp,\n");
	printf("\ttdoBit,\n");
	printf("\ttdiBit,\n");
	printf("\ttmsBit,\n");
	printf("\ttckBit\n");
	printf("};\n");
cleanup:
	return returnCode;
}

void usage(const char *progName) {
	fprintf(stderr, "Synopsis: %s <fw1.hex> <fw2.hex> <name> <bix|iic>\n", progName);
	//[<outfile.iic>]\n", argv[0]);
}

int main(int argc, const char *argv[]) {
	int returnCode = 0;
	struct Buffer data1 = {0,};
	struct Buffer mask1 = {0,};
	struct Buffer data2 = {0,};
	struct Buffer mask2 = {0,};
	struct Buffer i2c1 = {0,};
	struct Buffer i2c2 = {0,};
	BufferStatus bStatus;
	I2CStatus iStatus;
	int dStatus;
	const char *error = NULL;

	if ( argc != 5 ) { //&& argc != 6 ) {
		usage(argv[0]);
		FAIL(1);
	}

	bStatus = bufInitialise(&data1, 0x4000, 0x00, &error);
	CHECK(bStatus, 2);
	bStatus = bufInitialise(&mask1, 0x4000, 0x00, &error);
	CHECK(bStatus, 3);
	bStatus = bufReadFromIntelHexFile(&data1, &mask1, argv[1], &error);
	CHECK(bStatus, 4);

	bStatus = bufInitialise(&data2, 0x4000, 0x00, &error);
	CHECK(bStatus, 5);
	bStatus = bufInitialise(&mask2, 0x4000, 0x00, &error);
	CHECK(bStatus, 6);
	bStatus = bufReadFromIntelHexFile(&data2, &mask2, argv[2], &error);
	CHECK(bStatus, 7);

	if ( !strcmp("iic", argv[4]) ) {
		// Get i2c records from first build
		bStatus = bufInitialise(&i2c1, 0x4000, 0x00, &error);
		CHECK(bStatus, 8);
		i2cInitialise(&i2c1, 0x0000, 0x0000, 0x0000, CONFIG_BYTE_400KHZ);
		iStatus = i2cWritePromRecords(&i2c1, &data1, &mask1, &error);
		CHECK(iStatus, 9);
		iStatus = i2cFinalise(&i2c1, &error);
		CHECK(iStatus, 10);

		// Get i2c records from second build
		bStatus = bufInitialise(&i2c2, 0x4000, 0x00, &error);
		CHECK(bStatus, 11);
		i2cInitialise(&i2c2, 0x0000, 0x0000, 0x0000, CONFIG_BYTE_400KHZ);
		iStatus = i2cWritePromRecords(&i2c2, &data2, &mask2, &error);
		CHECK(iStatus, 12);
		iStatus = i2cFinalise(&i2c2, &error);
		CHECK(iStatus, 13);
		
		// Dump the code
		dStatus = dumpCode(argv[0], argv[3], &i2c1, &i2c2);
		CHECK(dStatus, dStatus);
	} else if ( !strcmp("bix", argv[4]) ) {
		// Dump the code
		dStatus = dumpCode(argv[0], argv[3], &data1, &data2);
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
	bufDestroy(&i2c2);
	bufDestroy(&i2c1);
	bufDestroy(&mask2);
	bufDestroy(&data2);
	bufDestroy(&mask1);
	bufDestroy(&data1);
	return returnCode;
}

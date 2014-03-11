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
#include <libfpgalink.h>
#include "args.h"

static const char *nibbles[] = {
	"0000",  // '0'
	"0001",  // '1'
	"0010",  // '2'
	"0011",  // '3'
	"0100",  // '4'
	"0101",  // '5'
	"0110",  // '6'
	"0111",  // '7'
	"1000",  // '8'
	"1001",  // '9'

	"XXXX",  // ':'
	"XXXX",  // ';'
	"XXXX",  // '<'
	"XXXX",  // '='
	"XXXX",  // '>'
	"XXXX",  // '?'
	"XXXX",  // '@'

	"1010",  // 'A'
	"1011",  // 'B'
	"1100",  // 'C'
	"1101",  // 'D'
	"1110",  // 'E'
	"1111"   // 'F'
};

int main(int argc, const char *argv[]) {
	int retVal;
	struct FLContext *handle = NULL;
	FLStatus status;
	const char *error = NULL;
	uint8 byte = 0x10;
	uint8 buf[256];
	uint8 flag;
	bool isNeroCapable, isCommCapable;
	size_t fileLen;
	uint8 *buffer = NULL;
	uint32 numDevices, scanChain[16], i;
	const char *vp = NULL, *ivp = NULL, *queryPort = NULL, *portConfig = NULL, *progConfig = NULL, *dataFile = NULL;
	const char *const prog = argv[0];

	printf("FPGALink \"C\" Example Copyright (C) 2011-2013 Chris McClelland\n\n");
	argv++;
	argc--;
	while ( argc ) {
		if ( argv[0][0] != '-' ) {
			unexpected(prog, *argv);
			FAIL(1, cleanup);
		}
		switch ( argv[0][1] ) {
		case 'h':
			usage(prog);
			FAIL(0, cleanup);
			break;
		case 'q':
			GET_ARG("q", queryPort, 2, cleanup);
			break;
		case 'd':
			GET_ARG("d", portConfig, 3, cleanup);
			break;
		case 'v':
			GET_ARG("v", vp, 4, cleanup);
			break;
		case 'i':
			GET_ARG("i", ivp, 5, cleanup);
			break;
		case 'p':
			GET_ARG("p", progConfig, 6, cleanup);
			break;
		case 'f':
			GET_ARG("f", dataFile, 7, cleanup);
			break;
		default:
			invalid(prog, argv[0][1]);
			FAIL(8, cleanup);
		}
		argv++;
		argc--;
	}
	if ( !vp ) {
		missing(prog, "v <VID:PID>");
		FAIL(9, cleanup);
	}

	status = flInitialise(0, &error);
	CHECK_STATUS(status, 10, cleanup);
	
	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	status = flOpen(vp, &handle, NULL);
	if ( status ) {
		if ( ivp ) {
			int count = 60;
			printf("Loading firmware into %s...\n", ivp);
			status = flLoadStandardFirmware(ivp, vp, &error);
			CHECK_STATUS(status, 11, cleanup);
			
			printf("Awaiting renumeration");
			flSleep(1000);
			do {
				printf(".");
				fflush(stdout);
				status = flIsDeviceAvailable(vp, &flag, &error);
				CHECK_STATUS(status, 12, cleanup);
				flSleep(250);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				fprintf(stderr, "FPGALink device did not renumerate properly as %s\n", vp);
				FAIL(13, cleanup);
			}
			
			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			status = flOpen(vp, &handle, &error);
			CHECK_STATUS(status, 14, cleanup);
		} else {
			fprintf(stderr, "Could not open FPGALink device at %s and no initial VID:PID was supplied\n", vp);
			FAIL(15, cleanup);
		}
	}
	
	if ( portConfig ) {
		uint32 readState;
		char hex[9];
		const uint8 *p = (const uint8 *)hex;
		printf("Configuring ports...\n");
		status = flMultiBitPortAccess(handle, portConfig, &readState, &error);
		CHECK_STATUS(status, 16, cleanup);
		sprintf(hex, "%08X", readState);
		printf("Readback:   28   24   20   16    12    8    4    0\n          %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf("  %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s\n", nibbles[*p++ - '0']);
		flSleep(100);
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle, 0x01);
	if ( queryPort ) {
		if ( isNeroCapable ) {
			status = jtagScanChain(handle, queryPort, &numDevices, scanChain, 16, &error);
			CHECK_STATUS(status, 17, cleanup);
			if ( numDevices ) {
				printf("The FPGALink device at %s scanned its JTAG chain, yielding:\n", vp);
				for ( i = 0; i < numDevices; i++ ) {
					printf("  0x%08X\n", scanChain[i]);
				}
			} else {
				printf("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices\n", vp);
			}
		} else {
			fprintf(stderr, "JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG\n", vp);
			FAIL(18, cleanup);
		}
	}

	if ( progConfig ) {
		printf("Executing programming configuration \"%s\" on FPGALink device %s...\n", progConfig, vp);
		if ( isNeroCapable ) {
			status = flProgram(handle, progConfig, NULL, &error);
			CHECK_STATUS(status, 19, cleanup);
		} else {
			fprintf(stderr, "Program operation requested but device at %s does not support NeroProg\n", vp);
			FAIL(20, cleanup);
		}
	}
	
	if ( dataFile ) {
		if ( isCommCapable ) {
			const uint8 *recvData;
			uint32 actualLength;
			uint32 j;
			uint16 checksum;
			printf("Selecting conduit 0x01...\n");
			status = flSelectConduit(handle, 0x01, &error);
			CHECK_STATUS(status, 21, cleanup);

			// Do some synchronous writes
			printf("Zeroing registers 1 & 2...\n");
			byte = 0xCA;
			status = flWriteChannel(handle, 0x01, 1, &byte, &error);
			CHECK_STATUS(status, 22, cleanup);
			byte = 0xFE;
			status = flWriteChannel(handle, 0x02, 1, &byte, &error);
			CHECK_STATUS(status, 23, cleanup);

			buffer = flLoadFile(dataFile, &fileLen);
			if ( !buffer ) {
				fprintf(stderr, "Unable to load file %s!\n", dataFile);
				FAIL(25, cleanup);
			}
			checksum = 0x0000;
			for ( i = 0; i < fileLen; i++ ) {
				checksum = (uint16)(checksum + buffer[i]);
			}
			
			for ( j = 0; j < 16; j++ ) {
				printf(
					"Writing %0.2f MiB (checksum 0x%04X) from %s to FPGALink device %s...\n",
					(double)fileLen/(1024*1024), checksum, dataFile, vp);

				// Write the file 16 times to the FPGA using the async write API
				for ( i = 0; i < 16; i++ ) {
					status = flWriteChannelAsync(handle, 0x00, fileLen, buffer, &error);
					CHECK_STATUS(status, 24, cleanup);
				}
				
				// Do some synchronous reads
				printf("Reading channel 0...");
				status = flReadChannel(handle, 0x00, 1, buf, &error);
				CHECK_STATUS(status, 26, cleanup);
				printf("got 0x%02X\n", buf[0]);
				printf("Reading channel 1...");
				status = flReadChannel(handle, 0x01, 1, buf, &error);
				CHECK_STATUS(status, 27, cleanup);
				printf("got 0x%02X\n", buf[0]);
				printf("Reading channel 2...");
				status = flReadChannel(handle, 0x02, 1, buf, &error);
				CHECK_STATUS(status, 28, cleanup);
				printf("got 0x%02X\n", buf[0]);

				// Submit a couple of async reads...
				status = flReadChannelAsyncSubmit(handle, 0x01, 65536, NULL, &error);
				CHECK_STATUS(status, 29, cleanup);
				status = flReadChannelAsyncSubmit(handle, 0x02, 65536, NULL, &error);
				CHECK_STATUS(status, 30, cleanup);
				
				// ...and then await their completion
				for ( i = 0; i < 2; i++ ) {
					status = flReadChannelAsyncAwait(
						handle, &recvData, &actualLength, &actualLength, &error);
					CHECK_STATUS(status, 31, cleanup);
					printf(
						"read[%d]: actualLength = %d, bytes = %02X %02X %02X %02X\n", i, actualLength,
						recvData[0], recvData[1], recvData[2], recvData[3]);
				}
			}
		} else {
			fprintf(stderr, "Data file load requested but device at %s does not support CommFPGA\n", vp);
			FAIL(29, cleanup);
		}
	}
	retVal = 0;
cleanup:
	if ( error ) {
		fprintf(stderr, "%s\n", error);
		flFreeError(error);
	}
	flFreeFile(buffer);
	flClose(handle);
	return retVal;
}

void usage(const char *prog) {
	printf("Usage: %s [-h] [-i <VID:PID>] -v <VID:PID> [-d <portConfig>]\n", prog);
	printf("          [-q <jtagPort>] [-p <progConfig>] [-f <dataFile>]\n\n");
	printf("Load FX2LP firmware, load the FPGA, interact with the FPGA.\n\n");
	printf("  -i <VID:PID>    initial vendor and product ID of the FPGALink device\n");
	printf("  -v <VID:PID>    renumerated vendor and product ID of the FPGALink device\n");
	printf("  -d <portConfig> configure the ports\n");
	printf("  -q <jtagPort>   scan the JTAG chain\n");
	printf("  -p <progConfig> configuration and programming file\n");
	printf("  -f <dataFile>   binary data to write to channel 0\n");
	printf("  -h              print this help and exit\n");
}

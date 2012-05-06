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

#define CHECK(x) \
	if ( status != FL_SUCCESS ) {       \
		returnCode = x;                   \
		fprintf(stderr, "%s\n", error); \
		flFreeError(error);             \
		goto cleanup;                   \
	}

int main(int argc, const char *argv[]) {
	int returnCode;
	struct FLContext *handle = NULL;
	FLStatus status;
	const char *error = NULL;
	uint8 byte = 0x10;
	uint8 buf[256];
	bool flag;
	bool isNeroCapable, isCommCapable;
	uint32 fileLen;
	uint8 *buffer = NULL;
	uint32 numDevices, scanChain[16], i;
	const char *vp = NULL, *ivp = NULL, *jtagPort = NULL, *xsvfFile = NULL, *dataFile = NULL;
	bool scan = false, usbPower = false;
	const char *const prog = argv[0];

	printf("FPGALink \"C\" Example Copyright (C) 2011 Chris McClelland\n\n");
	argv++;
	argc--;
	while ( argc ) {
		if ( argv[0][0] != '-' ) {
			unexpected(prog, *argv);
			FAIL(1);
		}
		switch ( argv[0][1] ) {
		case 'h':
			usage(prog);
			FAIL(0);
			break;
		case 's':
			scan = true;
			break;
		case 'p':
			usbPower = true;
			break;
		case 'v':
			GET_ARG("v", vp, 2);
			break;
		case 'i':
			GET_ARG("i", ivp, 3);
			break;
		case 'j':
			GET_ARG("j", jtagPort, 4);
			break;
		case 'x':
			GET_ARG("x", xsvfFile, 5);
			break;
		case 'f':
			GET_ARG("f", dataFile, 6);
			break;
		default:
			invalid(prog, argv[0][1]);
			FAIL(7);
		}
		argv++;
		argc--;
	}
	if ( !vp ) {
		missing(prog, "v <VID:PID>");
		FAIL(8);
	}

	if ( !ivp && jtagPort ) {
		fprintf(stderr, "You can't specify --j without -i");
		FAIL(9);
	}
	if ( !jtagPort ) {
		jtagPort = "D0234";
	}

	flInitialise();
	
	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	status = flOpen(vp, &handle, NULL);
	if ( status ) {
		if ( ivp ) {
			int count = 60;
			printf("Loading firmware into %s...\n", ivp);
			status = flLoadStandardFirmware(ivp, vp, jtagPort, &error);
			CHECK(10);
			
			printf("Awaiting renumeration");
			flSleep(1000);
			do {
				printf(".");
				fflush(stdout);
				flSleep(100);
				status = flIsDeviceAvailable(vp, &flag, &error);
				CHECK(11);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				fprintf(stderr, "FPGALink device did not renumerate properly as %s\n", vp);
				FAIL(12);
			}
			
			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			status = flOpen(vp, &handle, &error);
			CHECK(13);
		} else {
			fprintf(stderr, "Could not open FPGALink device at %s and no initial VID:PID was supplied\n", vp);
			FAIL(14);
		}
	}
	
	if ( usbPower ) {
		printf("Connecting USB power to FPGA...\n");
		status = flPortAccess(handle, 0x0080, 0x0080, NULL, &error);
		CHECK(15);
		flSleep(100);
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle);
	if ( scan ) {
		if ( isNeroCapable ) {
			status = flScanChain(handle, &numDevices, scanChain, 16, &error);
			CHECK(16);
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
			FAIL(17);
		}
	}

	if ( xsvfFile ) {
		printf("Playing \"%s\" into the JTAG chain on FPGALink device %s...\n", xsvfFile, vp);
		if ( isNeroCapable ) {
			status = flPlayXSVF(handle, xsvfFile, &error);
			CHECK(18);
		} else {
			fprintf(stderr, "XSVF play requested but device at %s does not support NeroJTAG\n", vp);
			FAIL(19);
		}
	}
	
	if ( dataFile && !isCommCapable ) {
		fprintf(stderr, "Data file load requested but device at %s does not support CommFPGA\n", vp);
		FAIL(20);
	}
	
	if ( isCommCapable ) {
		printf("Writing channel 0x01 to zero count...\n");
		byte = 0x01;
		status = flWriteChannel(handle, 1000, 0x01, 1, &byte, &error);
		CHECK(21);

		if ( dataFile ) {
			buffer = flLoadFile(dataFile, &fileLen);
			if ( buffer ) {
				uint16 checksum = 0x0000;
				uint32 i;
				for ( i = 0; i < fileLen; i++ ) {
					checksum += buffer[i];
				}
				printf(
					"Writing %0.2f MiB (checksum 0x%04X) from %s to FPGALink device %s...\n",
					(double)fileLen/(1024*1024), checksum, dataFile, vp);
				status = flWriteChannel(handle, 30000, 0x00, fileLen, buffer, &error);
				CHECK(22);
			} else {
				fprintf(stderr, "Unable to load file %s!\n", dataFile);
				FAIL(23);
			}
		}		
		printf("Reading channel...\n");
		status = flReadChannel(handle, 1000, 0x00, 2, buf, &error);
		CHECK(24);
		printf("Got 0x%02X\n", buf[0]);
		printf("Reading channel...\n");
		status = flReadChannel(handle, 1000, 0x00, 2, buf, &error);
		CHECK(25);
		printf("Got 0x%02X\n", buf[0]);
		printf("Reading channel...\n");
		status = flReadChannel(handle, 1000, 0x00, 2, buf, &error);
		CHECK(26);
		printf("Got 0x%02X\n", buf[0]);
	}

	returnCode = 0;

cleanup:
	flFreeFile(buffer);
	flClose(handle);
	return returnCode;
}

void usage(const char *prog) {
	printf("Usage: %s [-hps] -v <VID:PID> [-i <VID:PID>] [-x <xsvfFile>] [-f <dataFile>]\n\n", prog);
	printf("Load FX2 firmware, load the FPGA, interact with the FPGA.\n\n");
	printf("  -h             print this help and exit\n");
	printf("  -p             FPGA is powered from USB (Nexys2 only!)\n");
	printf("  -s             scan the JTAG chain\n");
	printf("  -v <VID:PID>   renumerated vendor and product ID of the FPGALink device\n");
	printf("  -i <VID:PID>   initial vendor and product ID of the FPGALink device\n");
	printf("  -j <jtagPort>  JTAG port config (e.g D0234)\n");
	printf("  -x <xsvfFile>  SVF, XSVF or CSVF file to play into the JTAG chain\n");
	printf("  -f <dataFile>  binary data to write to channel 0\n");
}

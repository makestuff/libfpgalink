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
#include <libfpgalink.h>

#define CHECK(x) \
	if ( status != FL_SUCCESS ) {       \
		exitCode = x;                   \
		fprintf(stderr, "%s\n", error); \
		flFreeError(error);             \
		goto cleanup;                   \
	}

int main(int argc, const char *argv[]) {
	int exitCode;
	struct FLContext *fpgaLink = NULL;
	FLStatus status;
	const char *error = NULL;
	uint8 byte = 0x10;
	uint8 buf[256];
	bool flag;
	bool isNeroCapable, isCommCapable;
	uint16 VID, PID, iVID, iPID;
	uint32 fileLen;
	uint8 *buffer = NULL;

	if ( argc != 4 && argc != 5 ) {
		fprintf(stderr, "Synopsis: %s <xsvfFile> <dataFile> <VID:PID> [<iVID:iPID>]\n", argv[0]);
		exitCode = 1;
		goto cleanup;
	}
	
	VID = (uint16)strtol(argv[3], NULL, 16);
	PID = (uint16)strtol(argv[3] + 5, NULL, 16);
	if ( argc == 5 ) {
		iVID = (uint16)strtol(argv[4], NULL, 16);
		iPID = (uint16)strtol(argv[4] + 5, NULL, 16);
	} else {
		iVID = VID;
		iPID = PID;
	}

	printf("Initialising...\n");
	flInitialise();
	
	printf("Attempting to open FPGALink connection...\n");
	status = flOpen(VID, PID, &fpgaLink, &error);
	if ( status ) {
		int count = 60;
		printf("Failed: %s\n", error);
		flFreeError(error);
		printf("Loading firmware...\n");
		//status = flLoadStandardFirmware(0x04B4, 0x8613, VID, PID, &error);
		status = flLoadStandardFirmware(iVID, iPID, VID, PID, &error);
		CHECK(1);
		
		printf("Awaiting renumeration...\n");
		do {
			flSleep(100);
			status = flIsDeviceAvailable(VID, PID, &flag, &error);
			CHECK(1);
			count--;
		} while ( !flag && count );
		if ( !flag ) {
			fprintf(stderr, "FPGALink device %04X:%04X did not renumerate properly\n", VID, PID);
			exitCode = 1;
			goto cleanup;
		}
		
		printf("Attempting to open FPGALink connection again...\n");
		status = flOpen(VID, PID, &fpgaLink, &error);
		CHECK(1);
	}
	
	isNeroCapable = flIsNeroCapable(fpgaLink);
	isCommCapable = flIsCommCapable(fpgaLink);
	printf(
		"Device %s NeroJTAG\nDevice %s CommFPGA\n",
		isNeroCapable ? "supports" : "does not support",
		isCommCapable ? "supports" : "does not support"
	);
	
	if ( isCommCapable ) {
		if ( isNeroCapable ) {
			status = flIsFPGARunning(fpgaLink, &flag, &error);
			CHECK(3);
			if ( !flag ) {
				printf("FPGA is not running - programming it now...\n");
				status = flPlayXSVF(fpgaLink, argv[1], &error);
				CHECK(3);
			} else {
				printf("FPGA is already running...\n");
			}
		}

		printf("Writing register 0x01 to zero count...\n");
		byte = 0x01;
		status = flWriteRegister(fpgaLink, 1000, 0x01, 1, &byte, &error);
		CHECK(8);

		printf("Reading %s\n", argv[2]);
		buffer = flLoadFile(argv[2], &fileLen);
		if ( buffer ) {
			uint16 checksum = 0x0000;
			uint32 i;
			for ( i = 0; i < fileLen; i++ ) {
				checksum += buffer[i];
			}
			printf(
				"Writing %0.2f Mbytes (checksum 0x%04X) to register...\n",
				(double)fileLen/(1024*1024), checksum);
			status = flWriteRegister(fpgaLink, 30000, 0x00, fileLen, buffer, &error);
			CHECK(8);
		} else {
			printf("Unable to load file!\n");
		}
		
		printf("Reading register...\n");
		status = flReadRegister(fpgaLink, 1000, 0x00, 2, buf, &error);
		CHECK(8);
		printf("Got 0x%02X\n", buf[0]);
		printf("Reading register...\n");
		status = flReadRegister(fpgaLink, 1000, 0x00, 2, buf, &error);
		CHECK(8);
		printf("Got 0x%02X\n", buf[0]);
		printf("Reading register...\n");
		status = flReadRegister(fpgaLink, 1000, 0x00, 2, buf, &error);
		CHECK(8);
		printf("Got 0x%02X\n", buf[0]);
	} else {
		printf("Programming FPGA...\n");
		status = flPlayXSVF(fpgaLink, argv[1], &error);
		CHECK(3);
	}
	exitCode = 0;

cleanup:
	flFreeFile(buffer);
	flClose(fpgaLink);
	return exitCode;
}

/* 
 * Copyright (C) 2012 Chris McClelland
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
#include <makestuff.h>
#include <libbuffer.h>
#include <libfpgalink.h>
#include <liberror.h>

#define CHECK(condition, retCode) \
	if ( condition ) { \
		FAIL(retCode); \
	}

int main(int argc, const char *argv[]) {
	int returnCode = 0;
	struct Buffer csvfBuf = {0,};
	BufferStatus bStatus;
	FLStatus fStatus;
	const char *error = NULL;
	uint32 csvfBufSize;
	if ( argc != 3 ) {
		fprintf(stderr, "Synopsis: %s <src.xsvf> <dst.csvf>\n", argv[0]);
		exit(1);
	}
	bStatus = bufInitialise(&csvfBuf, 10240, 0x00, &error);
	CHECK(bStatus, 1000+bStatus);
	fStatus = flLoadXsvfAndConvertToCsvf(argv[1], &csvfBuf, &csvfBufSize, NULL, &error);
	CHECK(fStatus, 2000+fStatus);
	printf("CSVF_BUF_SIZE = %d\n", csvfBufSize);
	bStatus = bufWriteBinaryFile(&csvfBuf, argv[2], 0, csvfBuf.length, &error);
	CHECK(bStatus, 3000+bStatus);
	return returnCode;
cleanup:
	bufDestroy(&csvfBuf);
	fprintf(stderr, "%s\n", error);
	bufFreeError(error);
	return returnCode;
}

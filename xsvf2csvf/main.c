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
#include <liberror.h>
#include "../private.h"

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
	uint32 csvfBufSize = 0;
	const char *srcFile, *dstFile;
	bool doCompress;
	const char *ext;
	if ( argc == 3 ) {
		srcFile = argv[1];
		dstFile = argv[2];
		doCompress = true;
	} else if ( argc == 4 && argv[1][0] == '-' && argv[1][1] == 'u' && argv[1][2] == '\0' ) {
		srcFile = argv[2];
		dstFile = argv[3];
		doCompress = false;
	} else {
		fprintf(stderr, "Synopsis: %s [-u] <src.xsvf|src.svf> <dst.csvf>\n", argv[0]);
		FAIL(1);
	}
	ext = srcFile + strlen(srcFile) - 5;
	bStatus = bufInitialise(&csvfBuf, 10240, 0x00, &error);
	CHECK(bStatus, 2);
	if ( strcmp(".svf", ext+1) == 0 ) {
		fStatus = flLoadSvfAndConvertToCsvf(srcFile, &csvfBuf, &csvfBufSize, &error);
	} else if ( strcmp(".xsvf", ext) == 0 ) {
		fStatus = flLoadXsvfAndConvertToCsvf(srcFile, &csvfBuf, &csvfBufSize, &error);
	} else {
		fprintf(stderr, "Source file should have .svf or .xsvf extension\n");
		FAIL(3);
	}
	CHECK(fStatus, 4);
	printf("CSVF_BUF_SIZE = %d\n", csvfBufSize);
	if ( doCompress ) {
		fStatus = flCompressCsvf(&csvfBuf, &error);
		CHECK(fStatus, 5);
	}
	bStatus = bufWriteBinaryFile(&csvfBuf, dstFile, 0, csvfBuf.length, &error);
	CHECK(bStatus, 6);

cleanup:
	bufDestroy(&csvfBuf);
	if ( error ) {
		fprintf(stderr, "%s\n", error);
		bufFreeError(error);
	}
	return returnCode;
}

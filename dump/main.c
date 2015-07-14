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
#include <makestuff.h>
#include <stdio.h>
#include <stdlib.h>
#include <libfpgalink.h>
#include "../xsvf.h"

#define bitsToBytes(x) ((x>>3) + (x&7 ? 1 : 0))
#define printAddrs() if ( wantAddrs ) printf("%08tX: ", p-buffer)

int main(int argc, const char *argv[]) {
	const uint8 *buffer, *p, *savePtr;
	size_t length;
	uint8 byte;
	uint32 xsdrSize = 0;
	uint32 numBytes, temp;
	bool wantAddrs = true;
	if ( argc != 2 && argc != 3 ) {
		fprintf(stderr, "Synopsis: %s [-n] <xsvfFile>\n", argv[0]);
		exit(1);
	}
	if ( argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0' ) {
		buffer = flLoadFile(argv[2], &length);
		wantAddrs = false;
	} else {
		buffer = flLoadFile(argv[1], &length);
	}
	p = buffer;
	byte = *p;
	while ( byte != XCOMPLETE ) {
		switch ( byte ) {
		case XTDOMASK:
			printAddrs();
			printf("XTDOMASK(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *++p);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRTDO:
			printAddrs();
			printf("XSDRTDO(");
			numBytes = temp = bitsToBytes(xsdrSize);
			savePtr = p + 1;
			while ( numBytes ) {
				printf("%02X", *++p);
				p++;
				numBytes--;
			}
			printf(", ");
			while ( temp ) {
				printf("%02X", *++savePtr);
				savePtr++;
				temp--;
			}
			printf(")\n");
			break;
		case XREPEAT:
			printAddrs();
			printf("XREPEAT(%02X)\n", *++p);
			break;
		case XRUNTEST:
			printAddrs();
			printf("XRUNTEST(%02X%02X%02X%02X)\n", p[1], p[2], p[3], p[4]);
			p += 4;
			break;
		case XSDRSIZE:
			printAddrs();
			xsdrSize = *++p;
			xsdrSize <<= 8;
			xsdrSize |= *++p;
			xsdrSize <<= 8;
			xsdrSize |= *++p;
			xsdrSize <<= 8;
			xsdrSize |= *++p;
			printf("XSDRSIZE(%08X)\n", xsdrSize);
			break;
		case XSIR:
			printAddrs();
			printf("XSIR(");
			byte = *++p;
			printf("%02X, ", byte);
			numBytes = bitsToBytes((uint32)byte);
			while ( numBytes ) {
				printf("%02X", *++p);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDR:
			printAddrs();
			printf("XSDR(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *++p);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRB:
			printAddrs();
			printf("XSDRB(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *++p);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRC:
			printAddrs();
			printf("XSDRC(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *++p);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRE:
			printAddrs();
			printf("XSDRE(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *++p);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSTATE:
			printAddrs();
			printf("XSTATE(%02X)\n", *++p);
			break;
		case XENDIR:
			printAddrs();
			printf("XENDIR(%02X)\n", *++p);
			break;
		case XENDDR:
			printAddrs();
			printf("XENDDR(%02X)\n", *++p);
			break;
		default:
			fprintf(stderr, "Unrecognised command %02X\n", byte);
			exit(1);
		}
		byte = *++p;
	}
	printf("XCOMPLETE\n");
	return 0;
}

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

#define bitsToBytes(x) ((x>>3) + (x&7 ? 1 : 0))

typedef enum {
	XCOMPLETE    = 0x00,
	XTDOMASK     = 0x01,
	XSIR         = 0x02,
	XSDR         = 0x03,
	XRUNTEST     = 0x04,
	XREPEAT      = 0x07,
	XSDRSIZE     = 0x08,
	XSDRTDO      = 0x09,
	XSETSDRMASKS = 0x0A,
	XSDRINC      = 0x0B,
	XSDRB        = 0x0C,
	XSDRC        = 0x0D,
	XSDRE        = 0x0E,
	XSDRTDOB     = 0x0F,
	XSDRTDOC     = 0x10,
	XSDRTDOE     = 0x11,
	XSTATE       = 0x12,
	XENDIR       = 0x13,
	XENDDR       = 0x14,
	XSIR2        = 0x15,
	XCOMMENT     = 0x16,
	XWAIT        = 0x17
} Command;


int main(int argc, const char *argv[]) {
	const uint8 *buffer, *p;
	uint32 length;
	uint8 byte;
	uint32 xsdrSize = 0;
	uint32 numBytes, temp;
	if ( argc != 2 ) {
		fprintf(stderr, "Synopsis: %s <xsvfFile>\n", argv[0]);
		exit(1);
	}
	buffer = flLoadFile(argv[1], &length);
	p = buffer;
	byte = *p++;
	while ( byte != XCOMPLETE ) {
		switch ( byte ) {
		case XTDOMASK:
			printf("XTDOMASK(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRTDO:
			printf("XSDRTDO(");
			numBytes = temp = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(", ");
			while ( temp ) {
				printf("%02X", *p++);
				temp--;
			}
			printf(")\n");
			break;
		case XREPEAT:
			printf("XREPEAT(%02X)\n", *p++);
			break;
		case XRUNTEST:
			printf("XRUNTEST(%02X%02X%02X%02X)\n", p[0], p[1], p[2], p[3]);
			p += 4;
			break;
		case XSDRSIZE:
			xsdrSize = *p++;
			xsdrSize <<= 8;
			xsdrSize |= *p++;
			xsdrSize <<= 8;
			xsdrSize |= *p++;
			xsdrSize <<= 8;
			xsdrSize |= *p++;
			printf("XSDRSIZE(%08X)\n", xsdrSize);
			break;
		case XSIR:
			byte = *p++;
			printf("XSIR(%02X, ", byte);
			numBytes = bitsToBytes(byte);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDR:
			printf("XSDR(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRB:
			printf("XSDRB(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRC:
			printf("XSDRC(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSDRE:
			printf("XSDRE(");
			numBytes = bitsToBytes(xsdrSize);
			while ( numBytes ) {
				printf("%02X", *p++);
				numBytes--;
			}
			printf(")\n");
			break;
		case XSTATE:
			printf("XSTATE(%02X)\n", *p++);
			break;
		case XENDIR:
			printf("XENDIR(%02X)\n", *p++);
			break;
		case XENDDR:
			printf("XENDDR(%02X)\n", *p++);
			break;
		default:
			fprintf(stderr, "Unrecognised command %02X\n", byte);
			exit(1);
		}
		byte = *p++;
	}
	return 0;
}

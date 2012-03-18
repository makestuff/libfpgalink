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
#include "csvfreader.h"

// -------------------------------------------------------------------------------------------------
// Declaration of private types & functions
// -------------------------------------------------------------------------------------------------

static uint8 getRawByte(struct Context *cp);
static uint32 readLength(struct Context *cp);

// -------------------------------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------------------------------

// Init the reader, return the header byte
//
uint8 csvfInitReader(struct Context *cp, const uint8 *csvfData, bool isCompressed) {
	uint8 thisByte;
	cp->data = csvfData;
	if ( isCompressed ) {
		cp->isCompressed = true;
		thisByte = getRawByte(cp);
		cp->count = readLength(cp);
	} else {
		cp->isCompressed = false;
		thisByte = 0x00;
		cp->count = 0;
	}
	cp->isReadingChunk = true;
	return thisByte;
}

// Get the next byte from the uncompressed stream. Uses m_count & m_isReadingChunk to keep state.
//
uint8 csvfGetByte(struct Context *cp) {
	if ( !cp->isCompressed ) {
		return getRawByte(cp);
	}
	if ( cp->isReadingChunk ) {
		// We're in the middle of reading a chunk.
		if ( cp->count ) {
			// There are still some bytes to copy verbatim into the uncompressed stream.
			cp->count--;
			return getRawByte(cp);
		} else {
			// We're at the end of this chunk; there will now be some zeros to insert into the
			// uncompressed stream.
			cp->count = readLength(cp);
			cp->isReadingChunk = false;
			return csvfGetByte(cp);
		}
	} else {
		// We're in the middle of a run of zeros.
		if ( cp->count ) {
			// There are still some zero bytes to write to the uncompressed stream.
			cp->count--;
			return 0x00;
		} else {
			// We're at the end of this run of zeros; there will now be a chunk of data to be copied
			// verbatim over to the uncompressed stream.
			cp->count = readLength(cp);
			cp->isReadingChunk = true;
			return csvfGetByte(cp);
		}
	}
}

// Get big-endian uint16 from the stream
//
uint16 csvfGetWord(struct Context *cp) {
	uint16 value;
	value = csvfGetByte(cp);
	value <<= 8;
	value |= csvfGetByte(cp);
	return value;
}

// Get big-endian uint32 from the stream
//
uint32 csvfGetLong(struct Context *cp) {
	uint32 value;
	value = csvfGetByte(cp);
	value <<= 8;
	value |= csvfGetByte(cp);
	value <<= 8;
	value |= csvfGetByte(cp);
	value <<= 8;
	value |= csvfGetByte(cp);
	return value;
}

// -------------------------------------------------------------------------------------------------
// Implementation of private functions
// -------------------------------------------------------------------------------------------------

// Read the next raw byte from the input stream
//
static uint8 getRawByte(struct Context *cp) {
	return *cp->data++;
}

// Read the length (of the chunk or the zero run). A short block (<256 bytes) length is encoded in a
// single byte. If that single byte is zero, we know it's a long block (256-65535 bytes), so read in
// the next two bytes as a big-endian uint16.
//
static uint32 readLength(struct Context *cp) {
	uint32 len = getRawByte(cp);
	if ( !len ) {
		len = getRawByte(cp);
		len <<= 8;
		len |= getRawByte(cp);
	}
	if ( !len ) {
		len = getRawByte(cp);
		len <<= 8;
		len |= getRawByte(cp);
		len <<= 8;
		len |= getRawByte(cp);
		len <<= 8;
		len |= getRawByte(cp);
	}
	return len;
}

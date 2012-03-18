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
#ifndef CSVF2XSVF_H
#define CSVF2XSVF_H

#include <makestuff.h>

#ifdef __cplusplus
extern "C" {
#endif

	struct Context {
		const uint8 *data;
		uint32 count;
		bool isReadingChunk;
		bool isCompressed;
	};

	uint8 csvfInitReader(struct Context *cp, const uint8 *data, bool isCompressed);
	uint8 csvfGetByte(struct Context *cp);
	uint16 csvfGetWord(struct Context *cp);
	uint32 csvfGetLong(struct Context *cp);

#ifdef __cplusplus
}
#endif

#endif

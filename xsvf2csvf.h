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
#ifndef XSVF2CSVF_H
#define XSVF2CSVF_H

#include <makestuff.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		X2C_SUCCESS = 0,
		X2C_BUF_INIT_ERR,
		X2C_BUF_APPEND_ERR,
		X2C_BUF_LOAD_ERR,
		X2C_UNSUPPORTED_CMD_ERR,
		X2C_UNSUPPORTED_DATA_ERR,
		X2C_UNSUPPORTED_SIZE_ERR
	} X2CStatus;

	// Forward declaration of Buffer struct
	struct Buffer;

	// Input: XSVF file
	// Output: CSVF buffer, max buffer size, an error string if necessary.
	//
	X2CStatus loadXsvfAndConvertToCsvf(
		const char *xsvfFile, struct Buffer *csvfBuf, uint16 *maxBufSize, const char **error
	) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif

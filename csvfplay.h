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
#ifndef CSVFPLAY_H
#define CSVFPLAY_H

#include <makestuff.h>

#ifdef __cplusplus
extern "C" {
#endif

	// Forward declaration of NeroHandle struct
	struct NeroHandle;

	// csvfPlay() may return any NeroStatus code, plus these
	#define CPLAY_COMPARE_ERR     100
	#define CPLAY_UNKNOWN_CMD_ERR 101
	#define CPLAY_HEADER_ERR      101

	// Play the uncompressed CSVF stream into the JTAG port.
	int csvfPlay(
		const uint8 *csvfData, bool isCompressed, struct NeroHandle *nero, const char **error
	) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif

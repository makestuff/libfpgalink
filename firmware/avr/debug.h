/*
 * Copyright (C) 2013 Chris McClelland
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
#ifndef DEBUG_H
#define DEBUG_H

#include <makestuff.h>

#if USART_DEBUG == 1
	void debugInit(void);
	void debugSendByte(uint8 byte);
	void debugSendByteHex(uint8 byte);
	void debugSendWordHex(uint16 word);
	void debugSendLongHex(uint32 word);
	void debugSendFlashString(const char *str);
	#define DEBUG_MASK (1<<2)
#endif

#endif

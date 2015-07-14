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
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>
#include "debug.h"

#if USART_DEBUG == 1
	void debugInit(void) {
		PORTC |= DEBUG_MASK;
		DDRC |= DEBUG_MASK;
	}

	#if F_CPU == 8000000
		#define DEBUG_COUNT 20
	#elif F_CPU == 16000000
		#define DEBUG_COUNT 44
	#else
		#error Unsupported XTAL frequency
	#endif

	void debugSendByte(uint8 byte) {
		uint8 i = 8;
		cli();
		PORTC &= ~DEBUG_MASK;  // PC2 clear
		_delay_loop_1(DEBUG_COUNT);
		while ( i-- ) {
			if ( byte & 0x01 ) {
				PORTC |= DEBUG_MASK;
			} else {
				PORTC &= ~DEBUG_MASK;
			}
			byte >>= 1;
			//_delay_loop_1(42);  42 43 44 45 46
			//_delay_loop_1(46);        ^^
			_delay_loop_1(DEBUG_COUNT);
		}
		PORTC |= DEBUG_MASK;
		_delay_loop_1(DEBUG_COUNT);
		sei();
	}

	// Send byte as two hex digits
	//
	void debugSendByteHex(uint8 byte) {
		uint8 ch;
		ch = byte >> 4;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = byte & 15;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
	}

	// Send long as four hex digits
	//
	void debugSendWordHex(uint16 word) {
		uint8 ch;
		ch = (word >> 12) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 8) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 4) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 0) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
	}

	// Send long as four hex digits
	//
	void debugSendLongHex(uint32 word) {
		uint8 ch;
		ch = (word >> 28) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 24) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 20) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 16) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 12) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 8) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 4) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
		ch = (word >> 0) & 0x0F;
		ch += (ch < 10 ) ? '0' : 'A' - 10;
		debugSendByte(ch);
	}

	void debugSendFlashString(const char *str) {
		uint8 ch = pgm_read_byte(str);
		while ( ch ) {
			debugSendByte(ch);
			str++;
			ch = pgm_read_byte(str);
		}
	}
#endif

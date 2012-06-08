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
#include <avr/io.h>
#include "jtag.h"
#include "desc.h"
//#include <usart.h>

static uint32 m_numBits = 0UL;
static uint8 m_flagByte = 0x00;
static bool m_enabled = false;

// Enable or disable the JTAG lines (i.e drive them or tristate them)
//
void jtagSetEnabled(bool enabled) {
	m_enabled = enabled;
	if ( enabled ) {
		DDRB |= (TCK | TMS | TDI);
	} else {
		DDRB &= ~(TCK | TMS | TDI);
	}
}               

bool jtagIsEnabled(void) {
	return m_enabled;
}

// Kick off a shift operation. Next time jtagExecuteShift() runs, it will execute the shift.
//
void jtagShiftBegin(uint32 numBits, uint8 flagByte) {
	m_numBits = numBits;
	m_flagByte = flagByte;
}

// See if a shift operation is pending
//
bool jtagIsShiftPending(void) {
	return (m_numBits != 0);
}

#define setTDI(x) if ( x ) { PORTB |= TDI; } else { PORTB &= ~TDI; }

// JTAG-clock the supplied byte into TDI, LSB first.
//
void shiftOut(uint8 tdiByte) {
	setTDI(tdiByte & 0x01);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x02);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x04);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x08);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x10);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x20);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x40);
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x80);
	PORTB |= TCK; PORTB &= ~TCK;
}

// JTAG-clock the supplied byte into TDI, MSB first. Return the byte clocked out of TDO.
//
uint8 shiftInOut(uint8 tdiByte) {
	uint8 tdoByte = 0x00;
	setTDI(tdiByte & 0x01);
	if ( PINB & TDO ) { tdoByte |= 0x01; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x02);
	if ( PINB & TDO ) { tdoByte |= 0x02; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x04);
	if ( PINB & TDO ) { tdoByte |= 0x04; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x08);
	if ( PINB & TDO ) { tdoByte |= 0x08; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x10);
	if ( PINB & TDO ) { tdoByte |= 0x10; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x20);
	if ( PINB & TDO ) { tdoByte |= 0x20; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x40);
	if ( PINB & TDO ) { tdoByte |= 0x40; }
	PORTB |= TCK; PORTB &= ~TCK;
	setTDI(tdiByte & 0x80);
	if ( PINB & TDO ) { tdoByte |= 0x80; }
	PORTB |= TCK; PORTB &= ~TCK;
	return tdoByte;
}

// The minimum number of bytes necessary to store x bits
//
#define bitsToBytes(x) ((x>>3) + (x&7 ? 1 : 0))

// Actually execute the shift operation initiated by jtagBeginShift(). This is done in a
// separate method because vendor commands cannot read & write to bulk endpoints.
//
void jtagShiftExecute(void) {
	// Are there any JTAG send/receive operations to execute?
	if ( (m_flagByte & bmSENDMASK) == bmSENDDATA ) {
		if ( m_flagByte & bmNEEDRESPONSE ) {
			// The host is giving us data, and is expecting a response
			uint8 buf[ENDPOINT_SIZE], *ptr, bytesRead, bytesRemaining;
			uint16 bitsRead, bitsRemaining;
			while ( m_numBits ) {
				bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
				bytesRead = bitsToBytes(bitsRead);
				Endpoint_SelectEndpoint(OUT_ENDPOINT_ADDR);
				Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
				ptr = buf;
				if ( bitsRead == m_numBits ) {
					// This is the last chunk
					uint8 tdoByte, tdiByte, leftOver, i;
					bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
					leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
					bytesRemaining = (bitsRemaining>>3);
					while ( bytesRemaining-- ) {
						*ptr = shiftInOut(*ptr);
						ptr++;
					}
					tdiByte = *ptr;  // Now do the bits in the final byte
					tdoByte = 0x00;
					i = 1;
					while ( i && leftOver ) {
						leftOver--;
						if ( (m_flagByte & bmISLAST) && !leftOver ) {
							PORTB |= TMS; // Exit Shift-DR state on next clock
						}
						setTDI(tdiByte & 0x01);
						tdiByte >>= 1;
						if ( PINB & TDO ) {
							tdoByte |= i;
						}
						PORTB |= TCK;
						PORTB &= ~TCK;
						i <<= 1;
					}
					*ptr = tdoByte;
				} else {
					// This is not the last chunk
					bytesRemaining = (bitsRead>>3);
					while ( bytesRemaining-- ) {
						*ptr = shiftInOut(*ptr);
						ptr++;
					}
				}
				Endpoint_SelectEndpoint(IN_ENDPOINT_ADDR);
				Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
				m_numBits -= bitsRead;
				Endpoint_ClearIN();
				Endpoint_ClearOUT();
			}
		} else {
			// The host is giving us data, but does not need a response
			uint8 buf[ENDPOINT_SIZE], *ptr, bytesRead, bytesRemaining, foo;
			uint16 bitsRead, bitsRemaining;
			Endpoint_SelectEndpoint(OUT_ENDPOINT_ADDR);
			while ( m_numBits ) {
				bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
				bytesRead = bitsToBytes(bitsRead);
				foo = Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
				ptr = buf;
				if ( bitsRead == m_numBits ) {
					// This is the last chunk
					uint8 tdiByte, leftOver, i;
					bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
					leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
					bytesRemaining = (bitsRemaining>>3);
					while ( bytesRemaining-- ) {
						shiftOut(*ptr++);
					}
					tdiByte = *ptr;  // Now do the bits in the final byte
					
					i = 1;
					while ( i && leftOver ) {
						leftOver--;
						if ( (m_flagByte & bmISLAST) && !leftOver ) {
							PORTB |= TMS; // Exit Shift-DR state on next clock
						}
						setTDI(tdiByte & 0x01);
						tdiByte >>= 1;
						PORTB |= TCK;
						PORTB &= ~TCK;
						i <<= 1;
					}
				} else {
					// This is not the last chunk
					bytesRemaining = (bitsRead>>3);
					while ( bytesRemaining-- ) {
						shiftOut(*ptr++);
					}
				}
				m_numBits -= bitsRead;
			}
			Endpoint_ClearOUT();
		}
	} else {
		if ( m_flagByte & bmNEEDRESPONSE ) {
			// The host is not giving us data, but is expecting a response
			uint8 buf[ENDPOINT_SIZE], *ptr, tdiByte, bytesRead, bytesRemaining;
			uint16 bitsRead, bitsRemaining;
			if ( (m_flagByte & bmSENDMASK) == bmSENDZEROS ) {
				tdiByte = 0x00;
			} else {
				tdiByte = 0xFF;
			}
			Endpoint_SelectEndpoint(IN_ENDPOINT_ADDR);
			while ( m_numBits ) {
				bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
				bytesRead = bitsToBytes(bitsRead);
				ptr = buf;
				if ( bitsRead == m_numBits ) {
					// This is the last chunk
					uint8 tdoByte, leftOver, i;
					bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
					leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
					bytesRemaining = (bitsRemaining>>3);
					while ( bytesRemaining-- ) {
						*ptr++ = shiftInOut(tdiByte);
					}
					tdoByte = 0x00;
					i = 1;
					setTDI(tdiByte & 0x01);
					while ( i && leftOver ) {
						leftOver--;
						if ( (m_flagByte & bmISLAST) && !leftOver ) {
							PORTB |= TMS; // Exit Shift-DR state on next clock
						}
						if ( PINB & TDO ) {
							tdoByte |= i;
						}
						PORTB |= TCK;
						PORTB &= ~TCK;
						i <<= 1;
					}
					*ptr = tdoByte;
				} else {
					// This is not the last chunk
					bytesRemaining = (bitsRead>>3);
					while ( bytesRemaining-- ) {
						*ptr++ = shiftInOut(tdiByte);
					}
				}
				Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
				m_numBits -= bitsRead;
			}
			Endpoint_ClearIN();
		} else {
			// The host is not giving us data, and does not need a response
			uint32 bitsRemaining, bytesRemaining;
			uint8 tdiByte, leftOver;
			if ( (m_flagByte & bmSENDMASK) == bmSENDZEROS ) {
				tdiByte = 0x00;
			} else {
				tdiByte = 0xFF;
			}
			bitsRemaining = (m_numBits-1) & 0xFFFFFFF8;    // Now an integer number of bytes
			leftOver = (uint8)(m_numBits - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				shiftOut(tdiByte);
			}
			setTDI(tdiByte & 0x01);
			while ( leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					PORTB |= TMS; // Exit Shift-DR state on next clock
				}
				PORTB |= TCK;
				PORTB &= ~TCK;
			}

			m_numBits = 0;
		}
	}
}

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
//
void jtagClockFSM(uint32 bitPattern, uint8 transitionCount) {
	while ( transitionCount-- ) {
		if ( bitPattern & 1 ) {
			PORTB |= TMS;
		} else {
			PORTB &= ~TMS;
		}
		PORTB |= TCK;
		PORTB &= ~TCK;
		bitPattern >>= 1;
	}
}

// Keep TMS and TDI as they are, and clock the JTAG state machine "numClocks" times.
//
void jtagClocks(uint32 numClocks) {
	while ( numClocks-- ) {
		PORTB |= TCK;
		PORTB &= ~TCK;
	}
}

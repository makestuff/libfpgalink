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
#include "prog.h"
#include "desc.h"
#include "usbio.h"
#include "debug.h"

static uint32 m_numBits = 0UL;
static ProgOp m_progOp = PROG_NOP;
static uint8 m_flagByte = 0x00;

#if PROG_HWSPI == 1
	// These are fixed for a given micro: the SPI pins
	#define PROG_PORT B
	#define bmTMS     0x01
	#define TCK_PORT  CONCAT(PORT, PROG_PORT)
	#define bmTCK     0x02
	#define TDI_PORT  CONCAT(PORT, PROG_PORT)
	#define bmTDI     0x04
	#define bmTDO     0x08
#else
	// These can be anything, depending on how JTAG is wired
	#define PROG_PORT B
	#define bmTMS     0x01
	//#define TCK_PORT  PORTB
	//#define bmTCK     0x02
	//#define TDI_PORT  PORTB
	//#define bmTDI     0x04
	#define bmTDO     0x08
#endif

// Make PORT{B,C,D...} & PIN{B,C,D...} macros
#define PORT CONCAT(PORT, PROG_PORT)
#define PIN CONCAT(PIN, PROG_PORT)

#define PAR_DDR DDRB
#define PAR_PORT PORTB

// Enable the parallel port
static inline void parEnable(void) {
	PAR_DDR = 0xFF;
}

// Disable the parallel port
static inline void parDisable(void) {
	PAR_DDR = 0x00;
}

// Send a byte over the parallel port
static inline void parSendByte(uint8 byte) {
	PAR_PORT = byte;
	TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
}

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
//
void progClockFSM(uint32 bitPattern, uint8 transitionCount) {
	while ( transitionCount-- ) {
		if ( bitPattern & 1 ) {
			PORT |= bmTMS;
		} else {
			PORT &= ~bmTMS;
		}
		TCK_PORT |= bmTCK;
		TCK_PORT &= ~bmTCK;
		bitPattern >>= 1;
	}
}

#define setTDI(x) if ( x ) { TDI_PORT |= bmTDI; } else { TDI_PORT &= ~bmTDI; }
#define jtagBit(x) setTDI(byte & x); TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK

// If the JTAG lines are wired to the right pins on the AVR SPI port, we can use
// it instead of bit-banging, which is much faster. Beware though, if SS is not used
// as an output, the SPI engine doesn't seem to work.
//
#if PROG_HWSPI == 1
	// Use SPI hardware!

	// TCK-clock the supplied byte into TDI, LSB first.
	static inline void jtagShiftOut(uint8 byte) {
		SPDR = byte;
		while ( !(SPSR & (1<<SPIF)) );
	}

	// JTAG-clock the supplied byte into TDI, LSB first. Return the byte clocked out of TDO.
	static inline uint8 jtagShiftInOut(uint8 byte) {
		SPDR = byte;
		while ( !(SPSR & (1<<SPIF)) );
		return SPDR;
	}

	// Enable the SPI port
	static inline void spiEnable(void) {
		SPSR = (1<<SPI2X);
		SPCR = (1<<SPE) | (1<<DORD) | (1<<MSTR) | (0<<SPR0);
	}

	// Disable the SPI port
	static inline void spiDisable(void) {
		SPSR = 0x00;
		SPCR = 0x00;
	}
#else
	// Use bit-banging!

	// TCK-clock the supplied byte into TDI, LSB first
	static void jtagShiftOut(uint8 byte) {
		jtagBit(0x01); jtagBit(0x02); jtagBit(0x04); jtagBit(0x08);
		jtagBit(0x10); jtagBit(0x20); jtagBit(0x40); jtagBit(0x80);
	}

	// JTAG-clock the supplied byte into TDI, LSB first. Return the byte clocked out of TDO.
	static uint8 jtagShiftInOut(uint8 byte) {
		uint8 tdoByte = 0x00;
		setTDI(byte & 0x01);
		if ( PIN & bmTDO ) { tdoByte |= 0x01; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x02);
		if ( PIN & bmTDO ) { tdoByte |= 0x02; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x04);
		if ( PIN & bmTDO ) { tdoByte |= 0x04; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x08);
		if ( PIN & bmTDO ) { tdoByte |= 0x08; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x10);
		if ( PIN & bmTDO ) { tdoByte |= 0x10; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x20);
		if ( PIN & bmTDO ) { tdoByte |= 0x20; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x40);
		if ( PIN & bmTDO ) { tdoByte |= 0x40; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		setTDI(byte & 0x80);
		if ( PIN & bmTDO ) { tdoByte |= 0x80; }
		TCK_PORT |= bmTCK; TCK_PORT &= ~bmTCK;
		return tdoByte;
	}

	// Enable the SPI port
	static inline void spiEnable(void) {
		// We're bit-banging, so nothing needs to be done
	}

	// Disable the SPI port
	static inline void spiDisable(void) {
		// We're bit-banging, so nothing needs to be done
	}
#endif

// Kick off a shift operation. Next time progExecuteShift() runs, it will execute the shift.
//
void progShiftBegin(uint32 numBits, ProgOp progOp, uint8 flagByte) {
	m_numBits = numBits;
	m_progOp = progOp;
	m_flagByte = flagByte;
}

// The minimum number of bytes necessary to store x bits
//
#define bitsToBytes(x) ((x>>3) + (x&7 ? 1 : 0))

static void jtagIsSendingIsReceiving(void) {
	// The host is giving us data and is expecting a response
	uint16 bitsRead, bitsRemaining;
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	spiEnable();
	while ( m_numBits ) {
		bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
		bytesRead = bitsToBytes(bitsRead);
		usbSelectEndpoint(OUT_ENDPOINT_ADDR);
		Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8 tdoByte, tdiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				*ptr = jtagShiftInOut(*ptr);
				ptr++;
			}
			spiDisable();
			tdiByte = *ptr;  // Now do the bits in the final byte
			tdoByte = 0x00;
			i = 1;
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					PORT |= bmTMS; // Exit Shift-DR state on next clock
				}
				setTDI(tdiByte & 0x01);
				tdiByte >>= 1;
				if ( PIN & bmTDO ) {
					tdoByte |= i;
				}
				TCK_PORT |= bmTCK;
				TCK_PORT &= ~bmTCK;
				i <<= 1;
			}
			*ptr = tdoByte;
		} else {
			// This is not the last chunk
			bytesRemaining = (bitsRead>>3);
			while ( bytesRemaining-- ) {
				*ptr = jtagShiftInOut(*ptr);
				ptr++;
			}
		}
		usbSelectEndpoint(IN_ENDPOINT_ADDR);
		Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
		m_numBits -= bitsRead;
		usbFlushPacket();
		usbAckPacket();
	}
	m_progOp = PROG_NOP;
}

static void jtagIsSendingNotReceiving(void) {
	// The host is giving us data, but does not need a response
	uint16 bitsRead, bitsRemaining;
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	spiEnable();
	while ( m_numBits ) {
		bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
		bytesRead = bitsToBytes(bitsRead);
		Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8 tdiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				jtagShiftOut(*ptr++);
			}
			spiDisable();
			tdiByte = *ptr;  // Now do the bits in the final byte
			i = 1;
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					PORT |= bmTMS; // Exit Shift-DR state on next clock
				}
				setTDI(tdiByte & 0x01);
				tdiByte >>= 1;
				TCK_PORT |= bmTCK;
				TCK_PORT &= ~bmTCK;
				i <<= 1;
			}
		} else {
			// This is not the last chunk
			bytesRemaining = (bitsRead>>3);
			while ( bytesRemaining-- ) {
				jtagShiftOut(*ptr++);
			}
		}
		m_numBits -= bitsRead;
	}
	usbAckPacket();
	m_progOp = PROG_NOP;
}

static void jtagNotSendingIsReceiving(void) {
	// The host is not giving us data, but is expecting a response
	uint16 bitsRead, bitsRemaining;
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	const uint8 tdiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	usbSelectEndpoint(IN_ENDPOINT_ADDR);
	spiEnable();
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
				*ptr++ = jtagShiftInOut(tdiByte);
			}
			spiDisable();
			tdoByte = 0x00;  // Now do the bits in the final byte
			i = 1;
			setTDI(tdiByte & 0x01);
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					PORT |= bmTMS; // Exit Shift-DR state on next clock
				}
				if ( PIN & bmTDO ) {
					tdoByte |= i;
				}
				TCK_PORT |= bmTCK;
				TCK_PORT &= ~bmTCK;
				i <<= 1;
			}
			*ptr = tdoByte;
		} else {
			// This is not the last chunk
			bytesRemaining = (bitsRead>>3);
			while ( bytesRemaining-- ) {
				*ptr++ = jtagShiftInOut(tdiByte);
			}
		}
		Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
		m_numBits -= bitsRead;
	}
	usbFlushPacket();
	m_progOp = PROG_NOP;
}

static void jtagNotSendingNotReceiving(void) {
	// The host is not giving us data, and does not need a response
	uint32 bitsRemaining, bytesRemaining;
	uint8 leftOver;
	const uint8 tdiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	bitsRemaining = (m_numBits-1) & 0xFFFFFFF8;    // Now an integer number of bytes
	leftOver = (uint8)(m_numBits - bitsRemaining); // How many bits in last byte (1-8)
	bytesRemaining = (bitsRemaining>>3);
	spiEnable();
	while ( bytesRemaining-- ) {
		jtagShiftOut(tdiByte);
	}
	spiDisable();
	setTDI(tdiByte & 0x01);
	while ( leftOver ) {
		leftOver--;
		if ( (m_flagByte & bmISLAST) && !leftOver ) {
			PORT |= bmTMS; // Exit Shift-DR state on next clock
		}
		TCK_PORT |= bmTCK;
		TCK_PORT &= ~bmTCK;
	}
	m_numBits = 0;
	m_progOp = PROG_NOP;
}

static void parSend(void) {
	uint8 byte;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	parEnable();
	while ( m_numBits-- ) {
		byte = usbFetchByte();
		parSendByte(byte);
	}
	parDisable();
	usbAckPacket();
	m_progOp = PROG_NOP;
}

static void spiSend(void) {
	uint8 bytesRead;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	spiEnable();
	while ( m_numBits ) {
		bytesRead = (m_numBits >= ENDPOINT_SIZE) ? ENDPOINT_SIZE : m_numBits;
		Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
		ptr = buf;
		m_numBits -= bytesRead;
		while ( bytesRead-- ) {
			jtagShiftOut(*ptr++);
		}
	}
	spiDisable();
	usbAckPacket();
	m_progOp = PROG_NOP;
}

static void spiSendRecv(void) {
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	spiEnable();
	while ( m_numBits ) {
		bytesRead = (m_numBits >= ENDPOINT_SIZE) ? ENDPOINT_SIZE : m_numBits;
		usbSelectEndpoint(OUT_ENDPOINT_ADDR);
		Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
		ptr = buf;
		bytesRemaining = bytesRead;
		while ( bytesRemaining-- ) {
			*ptr = jtagShiftInOut(*ptr);
			ptr++;
		}
		usbSelectEndpoint(IN_ENDPOINT_ADDR);
		Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
		m_numBits -= bytesRead;
		usbFlushPacket();
		usbAckPacket();
	}
	spiDisable();
	m_progOp = PROG_NOP;
}

static void spiRecv(void) {
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	spiEnable();
	usbSelectEndpoint(IN_ENDPOINT_ADDR);
	while ( m_numBits ) {
		bytesRead = (m_numBits >= ENDPOINT_SIZE) ? ENDPOINT_SIZE : m_numBits;
		ptr = buf;
		bytesRemaining = bytesRead;
		while ( bytesRemaining-- ) {
			*ptr = jtagShiftInOut(0x00);
			ptr++;
		}
		Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
		m_numBits -= bytesRead;
	}
	spiDisable();
	usbFlushPacket();
	m_progOp = PROG_NOP;
}

// Actually execute the shift operation initiated by progBeginShift(). This is done in a
// separate method because vendor commands cannot read & write to bulk endpoints.
//
void progShiftExecute(void) {
	switch ( m_progOp ) {
	case PROG_JTAG_ISSENDING_ISRECEIVING:
		// The host is giving us data, and is expecting a response (xdr)
		jtagIsSendingIsReceiving();
		break;
	case PROG_JTAG_ISSENDING_NOTRECEIVING:
		// The host is giving us data, but does not need a response (xdn)
		jtagIsSendingNotReceiving();
		break;
	case PROG_JTAG_NOTSENDING_ISRECEIVING:
		// The host is not giving us data, but is expecting a response (x0r)
		jtagNotSendingIsReceiving();
		break;
	case PROG_JTAG_NOTSENDING_NOTRECEIVING:
		jtagNotSendingNotReceiving();
		break;
	case PROG_SPI_SEND:
		spiSend();
		break;
	case PROG_SPI_SEND_RECV:
		spiSendRecv();
		break;
	case PROG_SPI_RECV:
		spiRecv();
		break;
	case PROG_PARALLEL:
		parSend();
		break;
	case PROG_NOP:
	default:
		break;
	}
}

// Keep TMS and TDI as they are, and clock the JTAG state machine "numClocks" times.
//
void progClocks(uint32 numClocks) {
	while ( numClocks-- ) {
		TCK_PORT |= bmTCK;
		TCK_PORT &= ~bmTCK;
	}
}

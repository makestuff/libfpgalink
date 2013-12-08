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
#include <avr/pgmspace.h>
#include "makestuff.h"
#include STR(boards/BSP.h)
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
	//#define PROG_PORT B
	//#define bmTMS     0x01
	//#define TCK_PORT  PORTB
	//#define bmTCK     0x02
	//#define TDI_PORT  PORTB
	//#define bmTDI     0x04
	//#define bmTDO     0x08
#endif

#define PORT(x) CONCAT(PORT_NUM_, x)
#define PIN(x) CONCAT(PIN_NUM_, x)

#define SPI_PORT 1
#define SS_BIT   0
#define SCK_BIT  1
#define MOSI_BIT 2
#define MISO_BIT 3

#define TDO_IN PIN(TDO_PORT)
#define bmTDO  (1<<TDO_BIT)
#define TDI_OUT PORT(TDI_PORT)
#define bmTDI  (1<<TDI_BIT)
#define TMS_OUT PORT(TMS_PORT)
#define bmTMS  (1<<TMS_BIT)
#define TCK_OUT PORT(TCK_PORT)
#define bmTCK  (1<<TCK_BIT)

#define PORT_NUM_0 PORTA
#define PORT_NUM_1 PORTB
#define PORT_NUM_2 PORTC
#define PORT_NUM_3 PORTD
#define PORT_NUM_4 PORTE
#define PIN_NUM_0 PINA
#define PIN_NUM_1 PINB
#define PIN_NUM_2 PINC
#define PIN_NUM_3 PIND
#define PIN_NUM_4 PINE

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
	TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
}

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
//
void progClockFSM(uint32 bitPattern, uint8 transitionCount) {
	while ( transitionCount-- ) {
		if ( bitPattern & 1 ) {
			TMS_OUT |= bmTMS;
		} else {
			TMS_OUT &= ~bmTMS;
		}
		TCK_OUT |= bmTCK;
		TCK_OUT &= ~bmTCK;
		bitPattern >>= 1;
	}
}

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
	#define tdiSet(x) if ( x ) { TDI_OUT |= bmTDI; } else { TDI_OUT &= ~bmTDI; }
	#define tdiBit(x) tdiSet(byte & x); TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK

	// TCK-clock the supplied byte into TDI, LSB first
	static void bbShiftOut(uint8 byte) {
		tdiBit(0x01); tdiBit(0x02); tdiBit(0x04); tdiBit(0x08);
		tdiBit(0x10); tdiBit(0x20); tdiBit(0x40); tdiBit(0x80);
	}

	// JTAG-clock the supplied byte into TDI, LSB first. Return the byte clocked out of TDO.
	static uint8 bbShiftInOut(uint8 byte) {
		uint8 tdoByte = 0x00;
		tdiSet(byte & 0x01);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x01; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x02);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x02; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x04);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x04; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x08);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x08; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x10);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x10; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x20);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x20; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x40);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x40; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		tdiSet(byte & 0x80);
		if ( TDO_IN & bmTDO ) { tdoByte |= 0x80; }
		TCK_OUT |= bmTCK; TCK_OUT &= ~bmTCK;
		return tdoByte;
	}

	// Enable the SPI port
	static inline void bbSpiEnable(void) {
		// We're bit-banging, so nothing needs to be done
	}

	// Disable the SPI port
	static inline void bbSpiDisable(void) {
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

// Bit-bang version of prog ops:
#define OP_HDR bb
#include "prog_ops.h"

static void nullFunc(void) {
	m_progOp = PROG_NOP;
}

typedef void (*FuncPtr)(void);
typedef struct {
	FuncPtr jtagIsSendingIsReceiving;
	FuncPtr jtagIsSendingNotReceiving;
	FuncPtr jtagNotSendingIsReceiving;
	FuncPtr jtagNotSendingNotReceiving;
	FuncPtr progSerSend;
	FuncPtr progSerRecv;
	FuncPtr progParSend;
} IndirectionTable;

static const IndirectionTable indirectionTable[] PROGMEM = {
	{
		nullFunc,  // NULL functions, just in case we get no mapping instructions
		nullFunc,
		nullFunc,
		nullFunc,
		nullFunc,
		nullFunc,
		nullFunc
	}, {
		bbIsSendingIsReceiving, // bit-bang implementations
		bbIsSendingNotReceiving,
		bbNotSendingIsReceiving,
		bbNotSendingNotReceiving,
		bbSerSend,
		bbSerRecv,
		bbParSend
	}
};
static uint8 m_funcIndex = 1;

static inline FuncPtr getFunc(uint8 index, uint8 offset) {
	const void *const baseAddr = &indirectionTable[index];
	return (FuncPtr)pgm_read_word(baseAddr + offset);
}

#define jtagIsSendingIsReceiving() (*getFunc(m_funcIndex, offsetof(IndirectionTable, jtagIsSendingIsReceiving)))()
#define jtagIsSendingNotReceiving() (*getFunc(m_funcIndex, offsetof(IndirectionTable, jtagIsSendingNotReceiving)))()
#define jtagNotSendingIsReceiving() (*getFunc(m_funcIndex, offsetof(IndirectionTable, jtagNotSendingIsReceiving)))()
#define jtagNotSendingNotReceiving() (*getFunc(m_funcIndex, offsetof(IndirectionTable, jtagNotSendingNotReceiving)))()
#define progSerSend() (*getFunc(m_funcIndex, offsetof(IndirectionTable, progSerSend)))()
#define progSerRecv() (*getFunc(m_funcIndex, offsetof(IndirectionTable, progSerRecv)))()
#define progParSend() (*getFunc(m_funcIndex, offsetof(IndirectionTable, progParSend)))()

// Actually execute the shift operation initiated by progBeginShift(). This is
// done in a separate function because vendor commands cannot read & write to
// bulk endpoints.
//
void progShiftExecute(void) {
	switch ( m_progOp ) {
	case PROG_JTAG_ISSENDING_ISRECEIVING:
		// The host is giving us data, and is expecting a response
		jtagIsSendingIsReceiving();
		break;
	case PROG_JTAG_ISSENDING_NOTRECEIVING:
		// The host is giving us data, but does not need a response
		jtagIsSendingNotReceiving();
		break;
	case PROG_JTAG_NOTSENDING_ISRECEIVING:
		// The host is not giving us data, but is expecting a response
		jtagNotSendingIsReceiving();
		break;
	case PROG_JTAG_NOTSENDING_NOTRECEIVING:
		// The host is neither giving us data, nor expecting a response
		jtagNotSendingNotReceiving();
		break;
	case PROG_SPI_SEND:
		progSerSend();
		break;
	case PROG_SPI_RECV:
		progSerRecv();
		break;
	case PROG_PARALLEL:
		progParSend();
		break;
	case PROG_NOP:
	default:
		break;
	}
}

// Keep TMS and TDI as they are, and clock the JTAG state machine "numClocks" times.
//
void progClocks(uint32 numClocks) {
	switch ( m_funcIndex ) {
	case 1:
		bbProgClocks(numClocks);
		return;
	}
}

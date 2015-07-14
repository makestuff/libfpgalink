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
#include "ports.h"

typedef enum {
	NONE,
	HW,
	BB
} PortMapping;
static PortMapping m_funcIndex = NONE;
static uint32 m_numBits = 0UL;
static ProgOp m_progOp = PROG_NOP;
static uint8 m_flagByte = 0x00;
static uint8 m_selectBM = 0x00;
static uint8 m_selectMask = 0x00;

#define PAR_DDR DDR(PAR_PORT)
#define PAR_IO PORT(PAR_PORT)

bool progPortMap(LogicalPort logicalPort, uint8 physicalPort, uint8 physicalBit) {
	switch ( logicalPort ) {
	case LP_CHOOSE:
		if ( (m_selectBM & m_selectMask) == m_selectMask ) {
			m_funcIndex = HW;
		} else if ( ((m_selectBM >> 4) & m_selectMask) == m_selectMask ) {
			m_funcIndex = BB;
		} else {
			m_selectBM = 0x00;
			m_selectMask = 0x00;
			return false;  // these choices cannot be accomodated by either HW or BB SPI
		}
		m_selectBM = 0x00;
		m_selectMask = 0x00;
		break;
	case LP_MISO:
		m_selectMask |= (1<<0);
		if ( physicalPort == HW_SPI_PORT && physicalBit == HW_MISO_BIT ) {
			m_selectBM |= (1<<0);  // this MISO choice is compatible with HW SPI
		}
		if ( physicalPort == BB_MISO_PORT && physicalBit == BB_MISO_BIT ) {
			m_selectBM |= (1<<4);  // this MISO choice is compatible with BB SPI
		}
		break;
	case LP_MOSI:
		m_selectMask |= (1<<1);
		if ( physicalPort == HW_SPI_PORT && physicalBit == HW_MOSI_BIT ) {
			m_selectBM |= (1<<1);  // this MOSI choice is compatible with HW SPI
		}
		if ( physicalPort == BB_MOSI_PORT && physicalBit == BB_MOSI_BIT ) {
			m_selectBM |= (1<<5);  // this MOSI choice is compatible with BB SPI
		}
		break;
	case LP_SS:
		m_selectMask |= (1<<2);
		if ( physicalPort == HW_SPI_PORT && physicalBit == HW_SS_BIT ) {
			m_selectBM |= (1<<2);  // this SS choice is compatible with HW SPI
		}
		if ( physicalPort == BB_SS_PORT && physicalBit == BB_SS_BIT ) {
			m_selectBM |= (1<<6);  // this SS choice is compatible with BB SPI
		}
		break;
	case LP_SCK:
		m_selectMask |= (1<<3);
		if ( physicalPort == HW_SPI_PORT && physicalBit == HW_SCK_BIT ) {
			m_selectBM |= (1<<3);  // this SCK choice is compatible with HW SPI
		}
		if ( physicalPort == BB_SCK_PORT && physicalBit == BB_SCK_BIT ) {
			m_selectBM |= (1<<7);  // this SCK choice is compatible with BB SPI
		}
		break;
	case LP_D8:
		break;
	default:
		return false;
	}
	return true;
}

// Enable the parallel port
static inline void parEnable(void) {
	PAR_DDR = 0xFF;
}

// Disable the parallel port
static inline void parDisable(void) {
	PAR_DDR = 0x00;
}

// If the JTAG lines are wired to the right pins on the AVR SPI port, we can use
// it instead of bit-banging, which is much faster. Beware though, if SS is not used
// as an output, the SPI engine doesn't seem to work.

// Enable the SPI port
static inline void hwSpiEnable(void) {
	SPSR = (1<<SPI2X);
	SPCR = (1<<SPE) | (1<<DORD) | (1<<MSTR) | (0<<SPR0);
}

// Disable the SPI port
static inline void hwSpiDisable(void) {
	SPSR = 0x00;
	SPCR = 0x00;
}

// Enable the SPI port
static inline void bbSpiEnable(void) {
	// We're bit-banging, so nothing needs to be done
}

// Disable the SPI port
static inline void bbSpiDisable(void) {
	// We're bit-banging, so nothing needs to be done
}

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
#define MISO_PORT BB_MISO_PORT
#define MOSI_PORT BB_MOSI_PORT
#define SS_PORT BB_SS_PORT
#define SCK_PORT BB_SCK_PORT
#define MISO_BIT  BB_MISO_BIT
#define MOSI_BIT  BB_MOSI_BIT
#define SS_BIT  BB_SS_BIT
#define SCK_BIT  BB_SCK_BIT
#define OP_HDR   bb
#include "prog_ops.h"

// SCK-clock the supplied byte into MOSI, LSB first
static inline void bbShiftOut(uint8 byte) {
	mosiBit(0x01); mosiBit(0x02); mosiBit(0x04); mosiBit(0x08);
	mosiBit(0x10); mosiBit(0x20); mosiBit(0x40); mosiBit(0x80);
}

// JTAG-clock the supplied byte into MOSI, LSB first. Return the byte clocked out of MISO.
static inline uint8 bbShiftInOut(uint8 byte) {
	uint8 misoByte = 0x00;
	mosiSet(byte & 0x01);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x01; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x02);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x02; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x04);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x04; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x08);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x08; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x10);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x10; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x20);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x20; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x40);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x40; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	mosiSet(byte & 0x80);
	if ( MISO_IN & bmMISO ) { misoByte |= 0x80; }
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
	return misoByte;
}

// Hardware SPI version of prog ops:
#undef  MISO_PORT
#undef  MOSI_PORT
#undef  SS_PORT
#undef  SCK_PORT
#undef  MISO_BIT
#undef  MOSI_BIT
#undef  SS_BIT
#undef  SCK_BIT
#define MISO_PORT HW_SPI_PORT
#define MOSI_PORT HW_SPI_PORT
#define SS_PORT HW_SPI_PORT
#define SCK_PORT HW_SPI_PORT
#define MISO_BIT  HW_MISO_BIT
#define MOSI_BIT  HW_MOSI_BIT
#define SS_BIT  HW_SS_BIT
#define SCK_BIT  HW_SCK_BIT
#define OP_HDR   hw
#include "prog_ops.h"

// SCK-clock the supplied byte into MOSI, LSB first.
static inline void hwShiftOut(uint8 byte) {
	SPDR = byte;
	while ( !(SPSR & (1<<SPIF)) );
}

// JTAG-clock the supplied byte into MOSI, LSB first. Return the byte clocked out of MISO.
static inline uint8 hwShiftInOut(uint8 byte) {
	SPDR = byte;
	while ( !(SPSR & (1<<SPIF)) );
	return SPDR;
}

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
		hwIsSendingIsReceiving, // bit-bang implementations
		hwIsSendingNotReceiving,
		hwNotSendingIsReceiving,
		hwNotSendingNotReceiving,
		hwSerSend,
		hwSerRecv,
		hwParSend
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
	case HW:
		hwProgClocks(numClocks);
		return;
	case BB:
		bbProgClocks(numClocks);
		return;
	default:
		return;
	}
}

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
//
void progClockFSM(uint32 bitPattern, uint8 transitionCount) {
	switch ( m_funcIndex ) {
	case HW:
		hwProgClockFSM(bitPattern, transitionCount);
		return;
	case BB:
		bbProgClockFSM(bitPattern, transitionCount);
		return;
	default:
		return;
	}
}

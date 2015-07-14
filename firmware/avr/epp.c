/*
 * Copyright (C) 2009-2013 Chris McClelland
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
#include "makestuff.h"
#include STR(boards/BSP.h)
#include "desc.h"
#include "usbio.h"
#include "ports.h"
#include "debug.h"

// Digilent
//#define bmEPP_ADDR (1<<4)
//#define bmEPP_DATA (1<<5)
//#define bmEPP_WRITE   (1<<6)
//#define bmEPP_WAIT    (1<<7)

// EPP definitions
#define CTRL_OUT  PORT(CTRL_PORT)
#define CTRL_IN   PIN(CTRL_PORT)
#define CTRL_DDR  DDR(CTRL_PORT)
#define DATA_OUT  PORT(DATA_PORT)
#define DATA_IN   PIN(DATA_PORT)
#define DATA_DDR  DDR(DATA_PORT)
#define bmADDRSTB (1<<ADDRSTB_BIT)
#define bmDATASTB (1<<DATASTB_BIT)
#define bmWRITE   (1<<WRITE_BIT)
#define bmWAIT    (1<<WAIT_BIT)

// Send an EPP data byte
static inline void eppSendDataByte(uint8 byte) {
	// Wait for FPGA to assert eppWait
	while ( CTRL_IN & bmWAIT );
	
	// Drive byte on data bus, strobe data write
	DATA_OUT = byte;
	CTRL_OUT &= ~bmDATASTB;
	
	// Wait for FPGA to deassert eppWait
	while ( !(CTRL_IN & bmWAIT) );
	
	// Signal FPGA that it's OK to end cycle
	CTRL_OUT |= bmDATASTB;
}

// Send an EPP address byte
static inline void eppSendAddrByte(uint8 byte) {
	// Wait for FPGA to assert eppWait
	while ( CTRL_IN & bmWAIT );
	
	// Drive byte on data bus, strobe address write
	DATA_OUT = byte;
	CTRL_OUT &= ~bmADDRSTB;
	
	// Wait for FPGA to deassert eppWait
	while ( !(CTRL_IN & bmWAIT) );
	
	// Signal FPGA that it's OK to end cycle
	CTRL_OUT |= bmADDRSTB;
}

// Receive an EPP data byte
static inline uint8 eppRecvDataByte(void) {
	uint8 byte;
	
	// Wait for FPGA to assert eppWait
	while ( CTRL_IN & bmWAIT );
	
	// Assert data strobe, to read a byte
	CTRL_OUT &= ~bmDATASTB;
	
	// Wait for FPGA to deassert eppWait
	while ( !(CTRL_IN & bmWAIT) );
	
	// Sample data lines
	byte = DATA_IN;
	
	// Deassert data strobe, telling FPGA that it's OK to end the cycle
	CTRL_OUT |= bmDATASTB;

	return byte;
}

// Set the data bus direction to AVR->FPGA
static inline void eppSending(void) {
	DATA_DDR = 0xFF;
	CTRL_OUT &= ~bmWRITE;
}

// Set the data bus direction to AVR<-FPGA
static inline void eppReceiving(void) {
	DATA_DDR = 0x00;
	CTRL_OUT |= bmWRITE;
}

// Execute pending EPP read/write operations
void eppExecute(void) {
	Endpoint_SelectEndpoint(OUT_ENDPOINT_ADDR);
	if ( usbOutPacketReady() ) {
		uint8 byte;
		uint16 count;
		do {
			// Read/write flag & channel
			byte = usbRecvByte(); eppSendAddrByte(byte);
			
			// Count high byte
			count = usbRecvByte();
			
			// Count low byte
			count <<= 8;
			count |= usbRecvByte();
			
			if ( byte & 0x80 ) {
				// The host is reading a channel
				usbAckPacket();                        // acknowledge the OUT packet
				usbSelectEndpoint(IN_ENDPOINT_ADDR);   // switch to the IN endpoint
				eppReceiving();                        // AVR reads from FPGA
				while ( !usbInPacketReady() );
				do {
					byte = eppRecvDataByte();
					if ( !usbReadWriteAllowed() ) {
						usbFlushPacket();
						while ( !usbInPacketReady() );
					}
					usbPutByte(byte);
					count--;
				} while ( count );
				eppSending();                          // AVR writes to FPGA again
				usbFlushPacket();                      // flush final packet
				usbSelectEndpoint(OUT_ENDPOINT_ADDR);  // ready for next command
				return;
			} else {
				// The host is writing a channel
				do {
					byte = usbRecvByte();
					eppSendDataByte(byte);
					count--;
				} while ( count );
			}
		} while ( usbReadWriteAllowed() );
		usbAckPacket();
	}
}

void eppEnable(void) {
	CTRL_OUT |= (bmADDRSTB | bmDATASTB | bmWAIT);
	CTRL_DDR &= ~bmWAIT; // WAIT is input
	CTRL_DDR |= (bmADDRSTB | bmDATASTB); // AS, DS are outputs
	eppSending();
	CTRL_DDR |= bmWRITE; // WR output low - FPGA in S_IDLE
}

void eppDisable(void) {
	CTRL_DDR &= ~(bmADDRSTB | bmDATASTB | bmWAIT | bmWRITE);
	CTRL_OUT &= ~(bmADDRSTB | bmDATASTB | bmWAIT | bmWRITE);
}

bool eppIsReady(void) {
	return (CTRL_IN & bmWAIT) == 0x00;
}

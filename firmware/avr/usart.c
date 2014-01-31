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

#define USART_OUT PORT(USART_PORT)
#define USART_DDR DDR(USART_PORT)
#define USART_PIN PIN(USART_PORT)
#define bmRX      (1<<RX_BIT)
#define bmTX      (1<<TX_BIT)
#define bmXCK     (1<<XCK_BIT)

// Send a USART byte.
static inline void usartSendByte(uint8 byte) {
	while ( !(UCSR1A & (1<<UDRE1)) );
	UDR1 = byte;
}

// Receive a USART byte.
static inline uint8 usartRecvByte(void) {
	while ( !(UCSR1A & (1<<RXC1)) );
	return UDR1;
}

// Execute pending USART read/write operations
void usartExecute(void) {
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	if ( usbOutPacketReady() ) {
		uint8 byte, chan;
		uint16 count;
		do {
			// Read/write flag & channel
			chan = usbRecvByte(); usartSendByte(chan);
			
			// Count high byte
			byte = usbRecvByte(); usartSendByte(byte);
			count = byte;
			
			// Count low byte
			byte = usbRecvByte(); usartSendByte(byte);
			count <<= 8;
			count |= byte;
			
			// Check to see if it's a read or a write
			if ( chan & 0x80 ) {
				// The host is reading a channel
				usbAckPacket();                        // acknowledge the OUT packet
				usbSelectEndpoint(IN_ENDPOINT_ADDR);   // switch to the IN endpoint
				#if USART_DEBUG == 1
					debugSendFlashString(PSTR("READ("));
					debugSendByteHex(count);
					debugSendByte(')');
					debugSendByte('\r');
				#endif
				while ( !(UCSR1A & (1<<TXC1)) );       // wait for send complete
				__asm volatile(
					"nop\nnop\nnop\nnop\n"              // give things a chance to settle
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					"nop\nnop\nnop\nnop\n"
					::);
				UCSR1B = (1<<RXEN1);                   // TX disabled, RX enabled
				while ( !usbInPacketReady() );
				USART_OUT &= ~bmTX;                 // TX low says "I'm ready"
				do {
					byte = usartRecvByte();
					if ( !usbReadWriteAllowed() ) {
						USART_OUT |= bmTX;            // TX high says "I'm not ready"
						usbFlushPacket();
						while ( !usbInPacketReady() );
						USART_OUT &= ~bmTX;           // TX low says "OK I'm ready now"
					}
					usbPutByte(byte);
					count--;
				} while ( count );
				UCSR1B = (1<<TXEN1);                   // TX enabled, RX disabled
				USART_OUT |= bmTX;                  // TX high says "I acknowledge receipt of your data"
				usbFlushPacket();                      // flush final packet
				usbSelectEndpoint(OUT_ENDPOINT_ADDR);  // ready for next command
				return;                                // there cannot be any more work to do
			} else {
				// The host is writing a channel
				#if USART_DEBUG == 1
					debugSendFlashString(PSTR("WRITE("));
					debugSendByteHex(count);
					debugSendByte(')');
					debugSendByte('\r');
				#endif
				do {
					byte = usbRecvByte();
					while ( PIND & bmRX );          // ensure RX is still low
					usartSendByte(byte);
					count--;
				} while ( count );
			}
		} while ( usbReadWriteAllowed() );
		usbAckPacket();
	}
}

void usartEnable(void) {
	USART_OUT |= bmTX;  // TX high
	USART_OUT &= ~bmXCK; // CK low
	USART_DDR |= (bmTX | bmXCK);  // TX & XCK are outputs
	USART_DDR &= ~bmRX;           // RX is input
	USART_OUT |= bmRX;            // RX pulled up
	UBRR1H = 0x00;
	UBRR1L = 0x00; // 8M sync
	UCSR1A = (1<<U2X1);
	UCSR1B = (1<<TXEN1);
	UCSR1C = (3<<UCSZ10) | (1<<UMSEL10);
}

void usartDisable(void) {
	USART_DDR &= ~(bmRX | bmTX | bmXCK);  // RX, TX & XCK all inputs (therefore float high).
	UCSR1A = 0x00;
	UCSR1B = 0x00;
	UCSR1C = 0x00;
}

bool usartIsReady(void) {
	return (USART_PIN & bmRX) == 0x00;  // ready when RX is low
}

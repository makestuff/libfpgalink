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
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/io.h>
#include <string.h>
#include <LUFA/Version.h>
#include <LUFA/Drivers/USB/USB.h>
#include "makestuff.h"
#include "desc.h"
#include "prog.h"
#include "../../vendorCommands.h"
#include "usbio.h"
#include "debug.h"
#include "date.h"

//#define EPP_ADDRSTB (1<<4)
//#define EPP_DATASTB (1<<5)
//#define EPP_WRITE   (1<<6)
//#define EPP_WAIT    (1<<7)

#define EPP_ADDRSTB (1<<1)
#define EPP_DATASTB (1<<5)
#define EPP_WRITE   (1<<3)
#define EPP_WAIT    (1<<2)

#define EPP_CTRL_PORT    PORTD
#define EPP_CTRL_PIN     PIND
#define EPP_CTRL_DDR     DDRD
#define EPP_DATA_PORT    PORTB
#define EPP_DATA_PIN     PINB
#define EPP_DATA_DDR     DDRB

#define SER_RX      (1<<2)
#define SER_TX      (1<<3)
#define SER_CK      (1<<5)

static uint8 m_fifoMode = 0;

// start LUFA DFU bootloader programmatically
void Jump_To_Bootloader(void);

// EPP Operations ----------------------------------------------------------------------------------

// Send an EPP data byte
static inline void eppSendDataByte(uint8 byte) {
	// Wait for FPGA to assert eppWait
	while ( EPP_CTRL_PIN & EPP_WAIT );
	
	// Drive byte on data bus, strobe data write
	EPP_DATA_PORT = byte;
	EPP_CTRL_PORT &= ~EPP_DATASTB;
	
	// Wait for FPGA to deassert eppWait
	while ( !(EPP_CTRL_PIN & EPP_WAIT) );
	
	// Signal FPGA that it's OK to end cycle
	EPP_CTRL_PORT |= EPP_DATASTB;
}

// Send an EPP address byte
static inline void eppSendAddrByte(uint8 byte) {
	// Wait for FPGA to assert eppWait
	while ( EPP_CTRL_PIN & EPP_WAIT );
	
	// Drive byte on data bus, strobe address write
	EPP_DATA_PORT = byte;
	EPP_CTRL_PORT &= ~EPP_ADDRSTB;
	
	// Wait for FPGA to deassert eppWait
	while ( !(EPP_CTRL_PIN & EPP_WAIT) );
	
	// Signal FPGA that it's OK to end cycle
	EPP_CTRL_PORT |= EPP_ADDRSTB;
}

// Receive an EPP data byte
static inline uint8 eppRecvDataByte(void) {
	uint8 byte;
	
	// Wait for FPGA to assert eppWait
	while ( EPP_CTRL_PIN & EPP_WAIT );
	
	// Assert data strobe, to read a byte
	EPP_CTRL_PORT &= ~EPP_DATASTB;
	
	// Wait for FPGA to deassert eppWait
	while ( !(EPP_CTRL_PIN & EPP_WAIT) );
	
	// Sample data lines
	byte = EPP_DATA_PIN;
	
	// Deassert data strobe, telling FPGA that it's OK to end the cycle
	EPP_CTRL_PORT |= EPP_DATASTB;

	return byte;
}

// Set the data bus direction to AVR->FPGA
static inline void eppSending(void) {
	EPP_DATA_DDR = 0xFF;
	EPP_CTRL_PORT &= ~EPP_WRITE;
}

// Set the data bus direction to AVR<-FPGA
static inline void eppReceiving(void) {
	EPP_DATA_DDR = 0x00;
	EPP_CTRL_PORT |= EPP_WRITE;
}

// Execute pending EPP read/write operations
void doEPP(void) {
	Endpoint_SelectEndpoint(OUT_ENDPOINT_ADDR);
	if ( usbOutPacketReady() ) {
		uint8 byte;
		uint32 count;
		do {
			// Read/write flag & channel
			byte = usbFetchByte(); eppSendAddrByte(byte);
			
			// Count high byte
			count = usbFetchByte();
			
			// Count high mid byte
			count <<= 8;
			count |= usbFetchByte();
			
			// Count low mid byte
			count <<= 8;
			count |= usbFetchByte();
			
			// Count low byte
			count <<= 8;
			count |= usbFetchByte();
			
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
					usbSendByte(byte);
					count--;
				} while ( count );
				eppSending();                          // AVR writes to FPGA again
				usbFlushPacket();                      // flush final packet
				usbSelectEndpoint(OUT_ENDPOINT_ADDR);  // ready for next command
				return;
			} else {
				// The host is writing a channel
				do {
					byte = usbFetchByte();
					eppSendDataByte(byte);
					count--;
				} while ( count );
			}
		} while ( usbReadWriteAllowed() );
		usbAckPacket();
	}
}

// Serial Operations -------------------------------------------------------------------------------

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
void doSerial(void) {
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	if ( usbOutPacketReady() ) {
		uint8 byte, chan;
		uint32 count;
		do {
			// Read/write flag & channel
			chan = usbFetchByte(); usartSendByte(chan);
			
			// Count high byte
			byte = usbFetchByte(); usartSendByte(byte);
			count = byte;
			
			// Count high mid byte
			byte = usbFetchByte(); usartSendByte(byte);
			count <<= 8;
			count |= byte;
			
			// Count low mid byte
			byte = usbFetchByte(); usartSendByte(byte);
			count <<= 8;
			count |= byte;
			
			// Count low byte
			byte = usbFetchByte(); usartSendByte(byte);
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
					::);
				UCSR1B = (1<<RXEN1);                   // TX disabled, RX enabled
				while ( !usbInPacketReady() );
				PORTD &= ~SER_TX;                      // TX low says "I'm ready"
				chan = 0;
				do {
					byte = usartRecvByte();
					if ( !usbReadWriteAllowed() ) {
						PORTD |= SER_TX;                 // TX high says "I'm not ready"
						usbFlushPacket();
						while ( !usbInPacketReady() );
						PORTD &= ~SER_TX;                // TX low says "OK I'm ready now"
					}
					usbSendByte(byte);
					count--;
				} while ( count );
				UCSR1B = (1<<TXEN1);                   // TX enabled, RX disabled
				PORTD |= SER_TX;                       // TX high says "I acknowledge receipt of your data"
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
					byte = usbFetchByte();
					while ( PIND & SER_RX );            // ensure RX is still low
					usartSendByte(byte);
					count--;
				} while ( count );
			}
		} while ( usbReadWriteAllowed() );
		usbAckPacket();
	}
}

// Conduit Operations ------------------------------------------------------------------------------

// Select the conduit to use for future read/write operations
void selectConduit(uint8 mode) {
	switch(mode) {
	case 0:
		// Undo previous settings (if any)
		if ( m_fifoMode == 1 ) {
			EPP_CTRL_DDR &= ~(EPP_ADDRSTB | EPP_DATASTB | EPP_WAIT | EPP_WRITE);
			EPP_CTRL_PORT &= ~(EPP_ADDRSTB | EPP_DATASTB | EPP_WAIT | EPP_WRITE);
		} else if ( m_fifoMode == 2 ) {
			UCSR1A = 0x00;
			UCSR1B = 0x00;
			UCSR1C = 0x00;
			DDRD &= ~(SER_RX | SER_TX | SER_CK);
			PORTD &= ~(SER_RX | SER_TX | SER_CK);
		}
		break;
	case 1:
		EPP_CTRL_PORT |= (EPP_ADDRSTB | EPP_DATASTB | EPP_WAIT);
		EPP_CTRL_DDR &= ~EPP_WAIT; // WAIT is input
		EPP_CTRL_DDR |= (EPP_ADDRSTB | EPP_DATASTB); // AS, DS are outputs
		eppSending();
		EPP_CTRL_DDR |= EPP_WRITE; // WR output low - FPGA in S_IDLE
		break;
	case 2:
		PORTD |= SER_TX;  // TX high
		PORTD &= ~SER_CK; // CK low
		DDRD |= (SER_TX | SER_CK);  // TX & XCK are outputs
		PORTD &= ~SER_TX;  // TX low - FPGA in S_RESET1
		PORTD |= SER_TX;  // TX high - FPGA in S_IDLE
		UBRR1H = 0x00;
		UBRR1L = 0x00; // 8M sync
		UCSR1A = (1<<U2X1);
		UCSR1B = (1<<TXEN1);
		UCSR1C = (3<<UCSZ10) | (1<<UMSEL10);
		break;
	}
	m_fifoMode = mode;
}

// Check to see if the FPGA is ready to do I/O on the selected conduit
bool isConduitReady(void) {
	switch ( m_fifoMode ) {
	case 1:
		return (EPP_CTRL_PIN & EPP_WAIT) == 0x00;
	case 2:
		return (PIND & SER_RX) == 0x00;  // ready when RX is low
	default:
		return false;
	}
}

// Main loop and control request callback ----------------------------------------------------------
//
int main(void) {
	#if REG_ENABLED == 0
		REGCR |= (1 << REGDIS);  // Disable regulator: using JTAG supply rail, which may be 3.3V.
	#endif
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	clock_prescale_set(clock_div_1);
	PORTB = 0x00;
	PORTC = 0x00;
	PORTD = 0x00;
	DDRB = 0x00;
	DDRC = 0x00;
	DDRD = 0x00;

	#include "init.inc"

	sei();
	#if USART_DEBUG == 1
	{
		// Read the Manufacturer and Product strings and print them on the debug console...
		uint16 size;
		const char *addr;
		debugInit();
		size = CALLBACK_USB_GetDescriptor((DTYPE_String << 8) + 0x01, 0x0000, (const void **)&addr);
		size = (size >> 1) - 1;
		addr += 2;
		while ( size-- ) {
			debugSendByte(pgm_read_byte(addr));
			addr += 2;
		}
		debugSendByte(' ');
		size = CALLBACK_USB_GetDescriptor((DTYPE_String << 8) + 0x02, 0x0000, (const void **)&addr);
		size = (size >> 1) - 1;
		addr += 2;
		while ( size-- ) {
			debugSendByte(pgm_read_byte(addr));
			addr += 2;
		}
		debugSendByte('\r');
	}
	#endif
	USB_Init();
	for ( ; ; ) {
		USB_USBTask();
		switch ( m_fifoMode ) {
		case 0:
			progShiftExecute();
			break;
		case 1:
			doEPP();
			break;
		case 2:
			doSerial();
			break;
		}
	}
}

#if USART_DEBUG == 1
	const char Op0[] PROGMEM = "PROG_NOP";
	const char Op1[] PROGMEM = "PROG_JTAG_ISSENDING_ISRECEIVING";
	const char Op2[] PROGMEM = "PROG_JTAG_ISSENDING_NOTRECEIVING";
	const char Op3[] PROGMEM = "PROG_JTAG_NOTSENDING_ISRECEIVING";
	const char Op4[] PROGMEM = "PROG_JTAG_NOTSENDING_NOTRECEIVING";
	const char Op5[] PROGMEM = "PROG_PARALLEL";
	const char Op6[] PROGMEM = "PROG_SERIAL";
	static const char *opNames[] PROGMEM = { Op0, Op1, Op2, Op3, Op4, Op5, Op6 };
#endif

#define SELECT_CONDUIT 0x0000

#define updatePort(port) \
	if ( CONCAT(DDR, port) | bitMask ) { \
		CONCAT(DDR, port)  = drive ? (CONCAT(DDR, port)  | bitMask) : (CONCAT(DDR, port)  & ~bitMask); \
		CONCAT(PORT, port) = high  ? (CONCAT(PORT, port) | bitMask) : (CONCAT(PORT, port) & ~bitMask); \
	} else { \
		CONCAT(PORT, port) = high  ? (CONCAT(PORT, port) | bitMask) : (CONCAT(PORT, port) & ~bitMask); \
		CONCAT(DDR, port)  = drive ? (CONCAT(DDR, port)  | bitMask) : (CONCAT(DDR, port)  & ~bitMask); \
	} \
	__asm volatile("nop\nnop\nnop\nnop"::);											\
	tempByte = (CONCAT(PIN, port) & bitMask) ? 0x01 : 0x00

// Called when a vendor command is received
//
void EVENT_USB_Device_ControlRequest(void) {
	switch ( USB_ControlRequest.bRequest ) {
	case CMD_MODE_STATUS:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			const uint16 param = USB_ControlRequest.wValue;
			const uint16 value = USB_ControlRequest.wIndex;
			if ( param == SELECT_CONDUIT ) {
				// Select conduit to use for future I/O
				Endpoint_ClearSETUP();
				selectConduit(value);
				#if USART_DEBUG == 1
					debugSendFlashString(PSTR("SELECT_CONDUIT("));
					debugSendByteHex(value);
					debugSendByte(')');
					debugSendByte('\r');
				#endif
				Endpoint_ClearStatusStage();
			}
		} else if ( USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_VENDOR) ) {
			uint8 statusBuffer[16];
			Endpoint_ClearSETUP();
			statusBuffer[0] = 'N';                     // Magic bytes (my cat's name)
			statusBuffer[1] = 'E';
			statusBuffer[2] = 'M';
			statusBuffer[3] = 'I';
			statusBuffer[4] = 0x00;                    // Last operation diagnostic code
			statusBuffer[5] = isConduitReady() ? 0x01 : 0x00;
			statusBuffer[6] = 0x24;                    // NeroJTAG endpoints
			statusBuffer[7] = 0x24;                    // CommFPGA endpoints
			statusBuffer[8] = 0xAA;                    // Firmware ID MSB
			statusBuffer[9] = 0xAA;                    // Firmware ID LSB
			statusBuffer[10] = (uint8)(DATE>>24);      // Version MSB
			statusBuffer[11] = (uint8)(DATE>>16);      // Version
			statusBuffer[12] = (uint8)(DATE>>8);       // Version
			statusBuffer[13] = (uint8)DATE;            // Version LSB
			statusBuffer[14] = 0x00;                   // Reserved
			statusBuffer[15] = 0x00;                   // Reserved
			Endpoint_Write_Control_Stream_LE(statusBuffer, 16);
			Endpoint_ClearStatusStage();
		}
		break;

	case CMD_JTAG_CLOCK_DATA:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			uint32 numBits;
			const ProgOp operation = (ProgOp)USB_ControlRequest.wIndex;
			const uint8 flagByte = (uint8)USB_ControlRequest.wValue;
			Endpoint_ClearSETUP();
			Endpoint_Read_Control_Stream_LE(&numBits, 4);
			#if USART_DEBUG == 1
				debugSendFlashString(PSTR("CMD_JTAG_CLOCK_DATA("));
				debugSendLongHex(numBits);
				debugSendByte(',');
				debugSendFlashString(
					(const char*)pgm_read_word(&opNames[operation]));
				debugSendByte(',');
				debugSendByteHex(flagByte);
				debugSendByte(')');
				debugSendByte('\r');
			#endif
			progShiftBegin(numBits, operation, flagByte);
			Endpoint_ClearStatusStage();
			// Now that numBits & flagByte are set, this operation will continue in mainLoop()...
		}
		break;

	case CMD_JTAG_CLOCK_FSM:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			uint32 bitPattern;
			const uint8 transitionCount = (uint8)USB_ControlRequest.wValue;
			Endpoint_ClearSETUP();
			Endpoint_Read_Control_Stream_LE(&bitPattern, 4);
			#if USART_DEBUG == 1
				debugSendFlashString(PSTR("CMD_JTAG_CLOCK_FSM("));
				debugSendByteHex(transitionCount);
				debugSendByte(',');
				debugSendLongHex(bitPattern);
				debugSendByte(')');
				debugSendByte('\r');
			#endif
			Endpoint_ClearStatusStage();
			progClockFSM(bitPattern, transitionCount);
		}
		break;

	case CMD_JTAG_CLOCK:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			uint32 numCycles = USB_ControlRequest.wIndex;
			Endpoint_ClearSETUP();
			numCycles <<= 16;
			numCycles |= USB_ControlRequest.wValue;
			progClocks(numCycles);
			Endpoint_ClearStatusStage();
		}
		break;

	case CMD_PORT_BIT_IO:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_VENDOR) ) {
			const uint8 portNumber = USB_ControlRequest.wValue & 0x00FF;
			const uint8 bitMask = (1 << (USB_ControlRequest.wValue >> 8));
			const uint8 drive = USB_ControlRequest.wIndex & 0x00FF;
			const uint8 high = USB_ControlRequest.wIndex >> 8;
			uint8 tempByte;
			switch ( portNumber ) {
			#ifdef HAS_PORTA
				case 0:
					updatePort(A);
					break;
			#endif
			case 1:
				updatePort(B);
				break;
			case 2:
				updatePort(C);
				break;
			case 3:
				updatePort(D);
				break;
			#ifdef HAS_PORTE
				case 4:
					updatePort(E);
					break;
			#endif
			default:
				return;
			}
			// Get the state of the port lines:
			Endpoint_ClearSETUP();
			Endpoint_Write_Control_Stream_LE(&tempByte, 1);
			Endpoint_ClearStatusStage();
		}
		break;

	case CMD_PORT_MAP:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			Endpoint_ClearSETUP();
			Endpoint_ClearStatusStage();
		}
		break;

	case CMD_BOOTLOADER:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			Endpoint_ClearSETUP();
			Endpoint_ClearStatusStage();
			Jump_To_Bootloader();
		}
		break;
	}
}

void EVENT_USB_Device_Connect(void) {
	// Connected
}

void EVENT_USB_Device_Disconnect(void) {
	// Disconnected
}

void EVENT_USB_Device_ConfigurationChanged(void) {
	if ( !(Endpoint_ConfigureEndpoint(ENDPOINT_DIR_OUT | OUT_ENDPOINT_ADDR,
	                                  EP_TYPE_BULK,
	                                  ENDPOINT_SIZE,
	                                  1)) )
	{
	}
	if ( !(Endpoint_ConfigureEndpoint(ENDPOINT_DIR_IN | IN_ENDPOINT_ADDR,
	                                  EP_TYPE_BULK,
	                                  ENDPOINT_SIZE,
	                                  1)) )
	{
	}
}

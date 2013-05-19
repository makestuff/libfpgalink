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
#include "debug.h"

#define EPP_ADDRSTB (1<<4)
#define EPP_DATASTB (1<<5)
#define EPP_WRITE   (1<<6)
#define EPP_WAIT    (1<<7)

static uint8 m_fifoMode = 0;

void fifoSetEnabled(uint8 mode) {
	m_fifoMode = mode;
	switch(mode) {
	case 0:
		// Port C and D inputs, pull-ups off
		DDRC = 0x00;
		DDRD = 0x00;
		PORTC = 0x00;
		PORTD = 0x00;
		break;
	case 1:
		// EPP_WAIT pulled-up input, other port C lines outputs, high
		PORTC |= (EPP_ADDRSTB | EPP_DATASTB | EPP_WRITE | EPP_WAIT);
		DDRC &= ~EPP_WAIT;
		DDRC |= (EPP_ADDRSTB | EPP_DATASTB | EPP_WRITE);
		break;
	}
}

void doComms(void) {
	Endpoint_SelectEndpoint(OUT_ENDPOINT_ADDR);
	if ( Endpoint_IsOUTReceived() ) {
		uint8 buf[64];
		uint32 num64;
		uint8 mod64;
		uint8 i;
		Endpoint_Read_Stream_LE(buf, 5, NULL);
		Endpoint_ClearOUT();
		i = buf[0];
		num64 = buf[1];
		num64 <<= 8;
		num64 |= buf[2];
		num64 <<= 8;
		num64 |= buf[3];
		num64 <<= 8;
		num64 |= buf[4];
		num64 >>= 6;
		mod64 = buf[4] & 0x3F;

		// Wait for FPGA to assert eppWait
		while ( PINC & EPP_WAIT );

		// Put addr on port D, strobe an addr write
		PORTC &= ~EPP_WRITE;    // AVR writes to FPGA
		PORTD = i;              // set value of data lines
		DDRD = 0xFF;            // drive data lines
		PORTC &= ~EPP_ADDRSTB;  // assert address strobe
		
		// Wait for FPGA to deassert eppWait
		while ( !(PINC & EPP_WAIT) );
		
		// Deassert address strobe, telling FPGA that it's OK to end the cycle
		PORTC |= EPP_ADDRSTB;
		
		if ( i & 0x80 ) {
			// The host is reading a channel ----------------------------------------------------------

			DDRD = 0x00;         // stop driving data lines
			PORTC |= EPP_WRITE;  // FPGA writes to AVR

			// We're reading from the FPGA and sending to the host
			Endpoint_SelectEndpoint(IN_ENDPOINT_ADDR);
			
			while ( num64-- ) {
				// Fetch 64 bytes from FPGA
				for ( i = 0; i < 64; i++ ) {
					// Wait for FPGA to assert eppWait
					while ( PINC & EPP_WAIT );
					
					// Assert data strobe, to read a byte
					PORTC &= ~EPP_DATASTB;
					
					// Wait for FPGA to deassert eppWait
					while ( !(PINC & EPP_WAIT) );

					// Sample data lines
					buf[i] = PIND;
					
					// Deassert data strobe, telling FPGA that it's OK to end the cycle
					PORTC |= EPP_DATASTB;
				}

				// Send the data packet to host
				while ( !Endpoint_IsINReady() );
				Endpoint_Write_Stream_LE(buf, 64, NULL);
				Endpoint_ClearIN();
			}
			
			if ( mod64 ) {
				// Get last few bytes of data from FPGA
				for ( i = 0; i < mod64; i++ ) {
					// Wait for FPGA to assert eppWait
					while ( PINC & EPP_WAIT );
					
					// Put byte on port D, strobe a data read
					PORTC &= ~EPP_DATASTB;
					
					// Wait for FPGA to deassert eppWait
					while ( !(PINC & EPP_WAIT) );
					
					// Sample data lines
					buf[i] = PIND;
					
					// Signal FPGA that it's OK to end cycle
					PORTC |= EPP_DATASTB;
				}
				
				// And send it to the host
				while ( !Endpoint_IsINReady() );
				Endpoint_Write_Stream_LE(buf, mod64, NULL);
				Endpoint_ClearIN();
			}
		} else {
			// The host is writing a channel ----------------------------------------------------------

			// Get num64 packets from host, sending each one in turn to the FPGA
			while ( num64-- ) {
				// Get a data packet from host
				Endpoint_Read_Stream_LE(buf, 64, NULL);
				Endpoint_ClearOUT();

				for ( i = 0; i < 64; i++ ) {
					// Wait for FPGA to assert eppWait
					while ( PINC & EPP_WAIT );
					
					// Put byte on port D, strobe a data write
					PORTD = buf[i];
					PORTC &= ~EPP_DATASTB;
					
					// Wait for FPGA to deassert eppWait
					while ( !(PINC & EPP_WAIT) );
					
					// Signal FPGA that it's OK to end cycle
					PORTC |= EPP_DATASTB;
				}
			}
			
			// Get last few bytes of data from host
			if ( mod64 ) {
				Endpoint_Read_Stream_LE(buf, mod64, NULL);
				Endpoint_ClearOUT();
				for ( i = 0; i < mod64; i++ ) {
					// Wait for FPGA to assert eppWait
					while ( PINC & EPP_WAIT );
					
					// Put byte on port D, strobe a data write
					PORTD = buf[i];
					PORTC &= ~EPP_DATASTB;
					
					// Wait for FPGA to deassert eppWait
					while ( !(PINC & EPP_WAIT) );
					
					// Signal FPGA that it's OK to end cycle
					PORTC |= EPP_DATASTB;
				}
			}

			DDRD = 0x00;         // stop driving port D
			PORTC |= EPP_WRITE;  // FPGA writes to AVR
		}
	}
}

// Called once at startup
//
int main(void) {
	REGCR |= (1 << REGDIS);  // Disable regulator: using JTAG supply rail, which may be 3.3V.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	clock_prescale_set(clock_div_1);
	PORTB = 0x00;
	PORTC = 0x00;
	PORTD = 0x00;
	DDRB = 0x00;
	DDRC = 0x00;
	DDRD = 0x00;

	sei();
	#ifdef DEBUG
		debugInit();
		debugSendFlashString(PSTR("MakeStuff FPGALink/AVR v1.1...\r"));
	#endif
	USB_Init();
	for ( ; ; ) {
		USB_USBTask();
		switch ( m_fifoMode ) {
		case 0:
			progShiftExecute();
			break;
		case 1:
			doComms();
			break;
		}
	}
}

#define MODE_FIFO (1<<1)

#define updateRegister(reg, val) tempByte = reg; tempByte &= ~mask; tempByte |= val; reg = tempByte

uint8 portAccess(uint8 portSelect, uint8 mask, uint8 ddrWrite, uint8 portWrite) {
	uint8 tempByte = 0x00;
	switch ( portSelect ) {

#ifdef HAS_PORTA
	case 0:
		updateRegister(PORTA, portWrite);
		updateRegister(DDRA, ddrWrite);
		tempByte = PINA;
		break;
#endif
	case 1:
		updateRegister(PORTB, portWrite);
		updateRegister(DDRB, ddrWrite);
		tempByte = PINB;
		break;
	case 2:
		updateRegister(PORTC, portWrite);
		updateRegister(DDRC, ddrWrite);
		tempByte = PINC;
		break;
	case 3:
		updateRegister(PORTD, portWrite);
		updateRegister(DDRD, ddrWrite);
		tempByte = PIND;
		break;
#ifdef HAS_PORTE
	case 4:
		updateRegister(PORTE, portWrite);
		updateRegister(DDRE, ddrWrite);
		tempByte = PINE;
		break;
#endif
	}
	return tempByte;
}

// Called when a vendor command is received
//
void EVENT_USB_Device_ControlRequest(void) {
	switch ( USB_ControlRequest.bRequest ) {
	case CMD_MODE_STATUS:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			// Enable/disable JTAG mode
			const uint16 wBits = USB_ControlRequest.wValue;
			const uint16 wMask = USB_ControlRequest.wIndex;
			if ( wMask & MODE_FIFO ) {
				// Enable or disable FIFO mode
				Endpoint_ClearSETUP();
				fifoSetEnabled(wBits & MODE_FIFO ? 1 : 0);
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
			statusBuffer[5] = (PINC & EPP_WAIT)?0x00:0x01;                    // Flags
			statusBuffer[6] = 0x24;                    // NeroJTAG endpoints
			statusBuffer[7] = 0x24;                    // CommFPGA endpoints
			statusBuffer[8] = 0x00;                    // Reserved
			statusBuffer[9] = 0x00;                    // Reserved
			statusBuffer[10] = 0x00;                   // Reserved
			statusBuffer[11] = 0x00;                   // Reserved
			statusBuffer[12] = 0x00;                   // Reserved
			statusBuffer[13] = 0x00;                   // Reserved
			statusBuffer[14] = 0x00;                   // Reserved
			statusBuffer[15] = 0x00;                   // Reserved
			Endpoint_Write_Control_Stream_LE(statusBuffer, 16);
			Endpoint_ClearStatusStage();
		}
		break;

	case CMD_JTAG_CLOCK_DATA:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			uint32 numBits;
			Endpoint_ClearSETUP();
			Endpoint_Read_Control_Stream_LE(&numBits, 4);
			#ifdef DEBUG
				debugSendFlashString(PSTR("CMD_JTAG_CLOCK_DATA("));
				debugSendLongHex(numBits);
				debugSendByte(',');
				debugSendWordHex(USB_ControlRequest.wIndex);
				debugSendByte(',');
				debugSendWordHex(USB_ControlRequest.wValue);
				debugSendByte(')');
				debugSendByte('\r');
			#endif
			progShiftBegin(numBits, (ProgOp)USB_ControlRequest.wIndex, (uint8)USB_ControlRequest.wValue);
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

	case CMD_PORT_IO:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_VENDOR) ) {
			const uint8 portSelect = USB_ControlRequest.wIndex & 0x00FF;
			const uint8 mask = USB_ControlRequest.wIndex >> 8;
			uint8 ddrWrite = USB_ControlRequest.wValue & 0x00FF;
			uint8 portWrite = USB_ControlRequest.wValue >> 8;
			uint8 response;
			if (
				portSelect >= 
					#ifdef HAS_PORTA
						0
					#else
						1
					#endif
				&& portSelect <=
					#ifdef HAS_PORTE
						4
					#else
						3
					#endif
			) {
				portWrite &= mask;
				ddrWrite &= mask;

				// Get the state of the port lines:
				Endpoint_ClearSETUP();
				response = portAccess(portSelect, mask, ddrWrite, portWrite);
				Endpoint_Write_Control_Stream_LE(&response, 1);
				Endpoint_ClearStatusStage();
			}
		}
		break;

	case CMD_PORT_MAP:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			Endpoint_ClearSETUP();
			Endpoint_ClearStatusStage();
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

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
#include <string.h>
#include <LUFA/Version.h>
#include <LUFA/Drivers/USB/USB.h>
#include "makestuff.h"
#include "desc.h"
#include "jtag.h"
#include "sync.h"
#include "../../vendorCommands.h"
//#include <usart.h>

#define EPP_ADDRSTB (1<<4)
#define EPP_DATASTB (1<<5)
#define EPP_WRITE   (1<<6)
#define EPP_WAIT    (1<<7)

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
	PORTC = 0x00;
	DDRC = 0x00;
	DDRD = 0x00;
	jtagSetEnabled(false);

	// Initialise EPP control outputs, enable eppWait pull-up
	PORTC |= (EPP_ADDRSTB | EPP_DATASTB | EPP_WRITE | EPP_WAIT);
	
	// Drive EPP control outputs
	DDRC |= (EPP_ADDRSTB | EPP_DATASTB | EPP_WRITE);
	
	//usartInit(115200);
	//usartSendFlashString(PSTR("MakeStuff FPGALink/AVR v1.0...\r"));

	sei();
	USB_Init();
	for ( ; ; ) {
		USB_USBTask();
		if ( jtagIsShiftPending() ) {
			jtagShiftExecute();
		} else if ( syncIsEnabled() ) {
			syncExecute();
		} else if ( !jtagIsEnabled() ) {
			doComms();
		}
	}
}

// Called when a vendor command is received
//
void EVENT_USB_Device_ControlRequest(void) {
	switch ( USB_ControlRequest.bRequest ) {
	case CMD_MODE_STATUS:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			// Enable sync mode if wValue is nonzero
			uint16 wBits = USB_ControlRequest.wValue;
			uint16 wMask = USB_ControlRequest.wIndex;
			Endpoint_ClearSETUP();
			if ( wMask & MODE_SYNC ) {
				// Sync mode does a loopback, so endpoints can be sync'd with the host software
				syncSetEnabled(wBits & MODE_SYNC ? true : false);
			} else if ( wMask & MODE_JTAG ) {
				// When in JTAG mode, the JTAG lines are driven; tristate otherwise
				jtagSetEnabled(wBits & MODE_JTAG ? true : false);
			}
			Endpoint_ClearStatusStage();
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
			jtagShiftBegin(numBits, (uint8)USB_ControlRequest.wValue);
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
			jtagClockFSM(bitPattern, transitionCount);
		}
		break;

	case CMD_JTAG_CLOCK:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			uint32 numCycles = USB_ControlRequest.wIndex;
			Endpoint_ClearSETUP();
			numCycles <<= 16;
			numCycles |= USB_ControlRequest.wValue;
			jtagClocks(numCycles);
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
	if ( !(Endpoint_ConfigureEndpoint(OUT_ENDPOINT_ADDR,
	                                  EP_TYPE_BULK,
	                                  ENDPOINT_DIR_OUT,
	                                  ENDPOINT_SIZE,
	                                  ENDPOINT_BANK_SINGLE)) )
	{
	}
	if ( !(Endpoint_ConfigureEndpoint(IN_ENDPOINT_ADDR,
	                                  EP_TYPE_BULK,
	                                  ENDPOINT_DIR_IN,
	                                  ENDPOINT_SIZE,
	                                  ENDPOINT_BANK_SINGLE)) )
	{
	}
}

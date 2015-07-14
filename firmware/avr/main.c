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
#include <string.h>
#include <LUFA/Version.h>
#include <LUFA/Drivers/USB/USB.h>
#include "makestuff.h"
#include STR(boards/BSP.h)
#include "desc.h"
#include "prog.h"
#include "../../vendorCommands.h"
#include "usbio.h"
#include "debug.h"
#include "date.h"
#include "epp.h"
#include "usart.h"

// Which conduit to use
static uint8 m_fifoMode = 0;

// start LUFA DFU bootloader programmatically
void Jump_To_Bootloader(void);

// Select the conduit to use for future read/write operations
void selectConduit(uint8 mode) {
	switch(mode) {
	case 0:
		// Undo previous settings (if any)
		if ( m_fifoMode == 1 ) {
			eppDisable();
		} else if ( m_fifoMode == 2 ) {
			usartDisable();
		}
		break;
	case 1:
		eppEnable();
		break;
	case 2:
		usartEnable();
		break;
	}
	m_fifoMode = mode;
}

// Check to see if the FPGA is ready to do I/O on the selected conduit
bool isConduitReady(void) {
	switch ( m_fifoMode ) {
	case 1:
		return eppIsReady();
	case 2:
		return usartIsReady();
	default:
		return false;
	}
}

// Main loop and control request callback ----------------------------------------------------------
//
int main(void) {
	#if REG_ENABLED == 0
		#ifdef __AVR_at90usb162__
			REGCR |= (1 << REGDIS);  // Disable regulator: using JTAG supply rail, which may be 3.3V.
		#endif
	#else
		#ifdef __AVR_atmega16u4__
			UHWCON |= (1 << UVREGE);  // Enable regulator.
		#endif
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

	#ifdef BSP_INIT
		#include BSP_INIT
	#endif

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
			eppExecute();
			break;
		case 2:
			usartExecute();
			break;
		}
	}
}

#if USART_DEBUG == 1
	const char progOp0[] PROGMEM = "PROG_NOP";
	const char progOp1[] PROGMEM = "PROG_JTAG_ISSENDING_ISRECEIVING";
	const char progOp2[] PROGMEM = "PROG_JTAG_ISSENDING_NOTRECEIVING";
	const char progOp3[] PROGMEM = "PROG_JTAG_NOTSENDING_ISRECEIVING";
	const char progOp4[] PROGMEM = "PROG_JTAG_NOTSENDING_NOTRECEIVING";
	const char progOp5[] PROGMEM = "PROG_PARALLEL";
	const char progOp6[] PROGMEM = "PROG_SPI_SEND";
	const char progOp7[] PROGMEM = "PROG_SPI_RECV";
	static const char *const progOpName[] PROGMEM = { progOp0, progOp1, progOp2, progOp3, progOp4, progOp5, progOp6, progOp7 };
	const char lp0[] PROGMEM = "LP_CHOOSE";
	const char lp1[] PROGMEM = "LP_MISO";
	const char lp2[] PROGMEM = "LP_MOSI";
	const char lp3[] PROGMEM = "LP_SS";
	const char lp4[] PROGMEM = "LP_SCK";
	const char lp5[] PROGMEM = "LP_D8";
	static const char *const logicalPortName[] PROGMEM = { lp0, lp1, lp2, lp3, lp4, lp5 };
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

	case CMD_PROG_CLOCK_DATA:
		if ( USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR) ) {
			uint32 numBits;
			const ProgOp operation = (ProgOp)USB_ControlRequest.wIndex;
			const uint8 flagByte = (uint8)USB_ControlRequest.wValue;
			Endpoint_ClearSETUP();
			Endpoint_Read_Control_Stream_LE(&numBits, 4);
			#if USART_DEBUG == 1
				debugSendFlashString(PSTR("CMD_PROG_CLOCK_DATA("));
				debugSendLongHex(numBits);
				debugSendByte(',');
				debugSendFlashString(
					(const char*)pgm_read_word(&progOpName[operation]));
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
			#if USART_DEBUG == 1
				debugSendFlashString(PSTR("CMD_JTAG_CLOCK("));
				debugSendLongHex(numCycles);
				debugSendByte(')');
				debugSendByte('\r');
			#endif
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
			const LogicalPort logicalPort = (LogicalPort)(USB_ControlRequest.wIndex & 0x00FF);
			const uint8 physicalPort = USB_ControlRequest.wIndex >> 8;
			const uint8 physicalBit = USB_ControlRequest.wValue & 0x00FF;
			#if USART_DEBUG == 1
				debugSendFlashString(PSTR("CMD_PORT_MAP("));
				debugSendFlashString(
					(const char*)pgm_read_word(&logicalPortName[logicalPort]));
				debugSendByte(',');
				debugSendByteHex(physicalPort);
				debugSendByte(',');
				debugSendByteHex(physicalBit);
				debugSendByte(')');
				debugSendByte('\r');
			#endif
			if ( progPortMap(logicalPort, physicalPort, physicalBit) ) {
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();
			}
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

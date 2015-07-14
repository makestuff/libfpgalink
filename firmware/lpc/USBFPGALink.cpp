/*
 * Copyright (C) 2013 Frank Buss
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
#include "stdint.h"
#include "USBFPGALink.h"

#define OUT_ENDPOINT EP2OUT
#define IN_ENDPOINT EP4IN

#define DEFAULT_CONFIGURATION (1)

#define FIFO_MODE 0x0000

static uint32_t checksum = 0;
static uint8_t buf[64];

enum FpgaState_t {
	FpgaStateReadChannel,
	FpgaStateReadSize,
	FpgaStateHostToDevice,
	FpgaStateDeviceToHost,
};

static FpgaState_t fpgaState = FpgaStateReadChannel;
static uint8_t fpgaChannel;
static uint8_t fpgaSizeCounter;
static uint32_t fpgaSize;

USBFPGALink::USBFPGALink(uint16_t vendor_id, uint16_t product_id, uint16_t product_release): USBDevice(vendor_id, product_id, product_release) {
	m_fifoMode = 0;
	m_numBits = 0UL;
	m_progOp = PROG_NOP;
	m_flagByte = 0x00;
	setPinsToInput();
}

void USBFPGALink::jtagBit(bool set) {
	setTDI(set);
	jtagClock();
}

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
//
void USBFPGALink::progClockFSM(uint32_t bitPattern, uint8_t transitionCount) {
	while ( transitionCount-- ) {
		setTMS(bitPattern & 1);
		jtagClock();
		bitPattern >>= 1;
	}
}

// TCK-clock the supplied byte into TDI, LSB first.
//
void USBFPGALink::jtagShiftOut(uint8_t byte) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		jtagBit(byte & 1);
		byte >>= 1;
	}
}

// JTAG-clock the supplied byte into TDI, MSB first. Return the byte clocked out of TDO.
//
uint8_t USBFPGALink::shiftInOut(uint8_t byte) {
	uint8_t tdoByte = 0;
	uint8_t i;
	for (i = 0; i < 8; i++) {
		tdoByte >>= 1;
		if (getTDO()) tdoByte |= 128;
		jtagBit(byte & 1);
		byte >>= 1;
	}
	return tdoByte;
}

// The minimum number of bytes necessary to store x bits
//
#define bitsToBytes(x) ((x>>3) + (x&7 ? 1 : 0))

void USBFPGALink::jtagIsSendingIsReceiving(uint8_t* buf, uint32_t len) {
	// The host is giving us data and is expecting a response
	uint16_t bitsRead, bitsRemaining;
	uint8_t bytesRead, bytesRemaining;
	uint8_t* ptr;
	if (m_numBits) {
		bitsRead = len * 8;
		if (bitsRead > m_numBits) {
			//printf("bitsRead > m_numBits\r\n");
			return;
		}
		bytesRead = bitsToBytes(bitsRead);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8_t tdoByte, tdiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8_t)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
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
					setTMS(true); // Exit Shift-DR state on next clock
				}
				setTDI(tdiByte & 0x01);
				tdiByte >>= 1;
				if ( getTDO() ) {
					tdoByte |= i;
				}
				jtagClock();
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
		write(buf, bytesRead);
		m_numBits -= bitsRead;
	}

	if (!m_numBits) {
		m_progOp = PROG_NOP;
	}
}

void USBFPGALink::jtagIsSendingNotReceiving(uint8_t* buf, uint32_t len) {
	// The host is giving us data, but does not need a response
	uint16_t bitsRead, bitsRemaining;
	uint8_t bytesRemaining;
	uint8_t* ptr;
	if (m_numBits) {
		bitsRead = len * 8;
		if (bitsRead > m_numBits) {
			//printf("bitsRead > m_numBits\r\n");
			return;
		}
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8_t tdiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8_t)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				jtagShiftOut(*ptr++);
			}
			tdiByte = *ptr;  // Now do the bits in the final byte
			
			i = 1;
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					setTMS(true); // Exit Shift-DR state on next clock
				}
				setTDI(tdiByte & 0x01);
				tdiByte >>= 1;
				jtagClock();
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

	if (!m_numBits) {
		m_progOp = PROG_NOP;
	}
}

void USBFPGALink::jtagNotSendingIsReceiving() {
	// The host is not giving us data, but is expecting a response
	uint16_t bitsRead, bitsRemaining;
	uint8_t bytesRead, bytesRemaining;
	uint8_t buf[MAX_PACKET_SIZE_EPBULK], *ptr;
	const uint8_t tdiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	if (m_numBits) {
		bitsRead = (m_numBits >= (MAX_PACKET_SIZE_EPBULK<<3)) ? MAX_PACKET_SIZE_EPBULK<<3 : m_numBits;
		bytesRead = bitsToBytes(bitsRead);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8_t tdoByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8_t)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				uint8_t b = shiftInOut(tdiByte);
				*ptr++ = b;
			}
			tdoByte = 0x00;
			i = 1;
			setTDI(tdiByte & 0x01);
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					setTMS(true); // Exit Shift-DR state on next clock
				}
				if ( getTDO() ) {
					tdoByte |= i;
				}
				jtagClock();
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
		for (int i = 0; i < bytesRead; i++) {
			//printf("read: %02x\r\n", buf[i]);
		}
		write(buf, bytesRead);
		m_numBits -= bitsRead;
	}

	if (!m_numBits) {
		m_progOp = PROG_NOP;
	}
}

void USBFPGALink::jtagNotSendingNotReceiving(void) {
	// The host is not giving us data, and does not need a response
	uint32_t bitsRemaining, bytesRemaining;
	uint8_t leftOver;
	const uint8_t tdiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	bitsRemaining = (m_numBits-1) & 0xFFFFFFF8;    // Now an integer number of bytes
	leftOver = (uint8_t)(m_numBits - bitsRemaining); // How many bits in last byte (1-8)
	bytesRemaining = (bitsRemaining>>3);
	while (bytesRemaining--) {
		jtagShiftOut(tdiByte);
	}
	setTDI(tdiByte & 0x01);
	while (leftOver) {
		leftOver--;
		if ( (m_flagByte & bmISLAST) && !leftOver ) {
			setTMS(true); // Exit Shift-DR state on next clock
		}
		jtagClock();
	}
	m_numBits = 0;
	m_progOp = PROG_NOP;
}

// Keep TMS and TDI as they are, and clock the JTAG state machine "numClocks" times.
//
void USBFPGALink::progClocks(uint32_t numClocks) {
	while ( numClocks-- ) {
		jtagClock();
	}
}

void USBFPGALink::write(uint8_t* data, uint32_t size) {
	endpointWrite(IN_ENDPOINT, data, size);
}


void USBFPGALink::fpgaDeviceToHost() {
	if (fpgaSize == 0) return;
	uint32_t c = fpgaSize;
	if (c > 64) c = 64;
		
	// read max 64 bytes from FPGA
	for (uint8_t i = 0; i < c; i++) {
		buf[i] = spiTransfer(0);
		//printf("in: %02x\r\n", buf[i]);
	}

	// and send it to the host
	write(buf, c);
	
	// all bytes transfered?
	fpgaSize -= c;
	if (fpgaSize == 0) {
		// then stop SPI communication
		setCS(true);
		fpgaState = FpgaStateReadChannel;
	}
}

bool USBFPGALink::EP2_OUT_callback() {
	uint32_t len;
	readEP(OUT_ENDPOINT, buf, &len, 64);
	//printf("EP2_OUT_callback, m_fifoMode: %i, fpgaState: %i, len: %i\r\n", m_fifoMode, fpgaState, len);
	if (m_fifoMode) {
		for (uint32_t i = 0; i < len; i++) {
			switch (fpgaState) {
				case FpgaStateReadChannel:
					fpgaChannel = buf[i];
					fpgaState = FpgaStateReadSize;
					fpgaSizeCounter = 4;
					fpgaSize = 0;
					break;
				case FpgaStateReadSize:
					fpgaSize <<= 8;
					fpgaSize |= buf[i];
					fpgaState = FpgaStateReadSize;
					if (--fpgaSizeCounter == 0) {
						// start SPI communication
						setSCLK(false);
						setCS(false);
				
						// send channel number
						spiTransfer(fpgaChannel);

						if (fpgaChannel & 0x80) {
							// send dummy wait state byte
							spiTransfer(0);
							
							// start sending bytes to the host
							fpgaDeviceToHost();
							
							fpgaState = FpgaStateDeviceToHost;
						} else {
							fpgaState = FpgaStateHostToDevice;
						}
					}
					break;
				case FpgaStateHostToDevice:
					if (fpgaSize) {
						spiTransfer(buf[i]);
						//printf("out: %02x\r\n", buf[i]);
					}
					fpgaSize--;
					if (fpgaSize == 0) {
						// stop SPI communication
						setCS(true);
						fpgaState = FpgaStateReadChannel;
					}
					break;
				default:
					printf("statemachine error: %i\r\n", fpgaState);
					break;
			}
		}
	} else {
		switch (m_progOp) {
			case PROG_JTAG_ISSENDING_ISRECEIVING:
				// The host is giving us data, and is expecting a response (xdr)
		//		printf("PROG_JTAG_ISSENDING_ISRECEIVING\r\n");
				jtagIsSendingIsReceiving(buf, len);
				break;
			case PROG_JTAG_ISSENDING_NOTRECEIVING:
				// The host is giving us data, but does not need a response (xdn)
		//		printf("PROG_JTAG_ISSENDING_NOTRECEIVING\r\n");
				jtagIsSendingNotReceiving(buf, len);
				break;
/*			case PROG_JTAG_FAST:
				// The host is giving us data for JTAG programming without testing
				//printf("EP2 out: PROG_JTAG_FAST\r\n");
				jtagFast(buf, len);
				break;*/
			case PROG_NOP:
				break;
			default:
				printf("out: PROG_ undefined\r\n");
				break;
		}
	}
	
	readStart(OUT_ENDPOINT, MAX_PACKET_SIZE_EPBULK);

	return true;
}

bool USBFPGALink::EP4_IN_callback() {
	if (m_fifoMode) {
		if (fpgaState == FpgaStateDeviceToHost) {
			// read next packet from FPGA and send it to the host
			fpgaDeviceToHost();
		}
	} else {
		switch (m_progOp) {
			case PROG_JTAG_NOTSENDING_ISRECEIVING:
				// The host is not giving us data, but is expecting a response (x0r)
		//		printf("PROG_JTAG_NOTSENDING_ISRECEIVING\r\n");
				jtagNotSendingIsReceiving();
				break;
			case PROG_NOP:
				break;
			default:
				printf("in: PROG_ undefined\r\n");
				break;
		}
	}
	
	return true;
}



// Called in ISR context
// Set configuration. Return false if the
// configuration is not supported.
bool USBFPGALink::USBCallback_setConfiguration(uint8_t configuration) {
    if (configuration != DEFAULT_CONFIGURATION) {
        return false;
    }

    // Configure endpoints > 0
    addEndpoint(IN_ENDPOINT, MAX_PACKET_SIZE_EPBULK);
    addEndpoint(OUT_ENDPOINT, MAX_PACKET_SIZE_EPBULK);

    // We activate the endpoint to be able to receive data
    readStart(OUT_ENDPOINT, MAX_PACKET_SIZE_EPBULK);
    return true;
}


uint8_t * USBFPGALink::stringImanufacturerDesc() {
    static uint8_t stringImanufacturerDescriptor[] = {
        0x16,                           //bLength
        STRING_DESCRIPTOR,              //bDescriptorType 0x03
//        'A',0,'u',0,'d',0,'i',0,'o',0   //bString iInterface - Audio
        'F',0,'r',0,'a',0,'n',0,'k',0,' ',0,'B',0,'u',0,'s',0,'s',0   //bString iManufacturer - Frank Buss
    };
    return stringImanufacturerDescriptor;
}

uint8_t * USBFPGALink::stringIproductDesc() {
    static uint8_t stringIproductDescriptor[] = {
        0x24,                                                       //bLength
        STRING_DESCRIPTOR,                                          //bDescriptorType 0x03
//        'M',0,'b',0,'e',0,'d',0,' ',0,'A',0,'u',0,'d',0,'i',0,'o',0 //bString iProduct - Mbed Audio
        'F',0,'P',0,'G',0,'A',0,'L',0,'i',0,'n',0,'k',0,'/',0,'L',0,'P',0,'C',0,' ',0,'v',0,'0',0,'.',0,'1',0  //bString iProduct - FPGALink/LPC v0.1
    };
    return stringIproductDescriptor;
}

#define TOTAL_DESCRIPTOR_LENGTH ((1 * CONFIGURATION_DESCRIPTOR_LENGTH) \
                               + (1 * INTERFACE_DESCRIPTOR_LENGTH) \
                               + (2 * ENDPOINT_DESCRIPTOR_LENGTH))

uint8_t * USBFPGALink::configurationDesc() {
    static uint8_t configurationDescriptor[] = {
        CONFIGURATION_DESCRIPTOR_LENGTH,// bLength
        CONFIGURATION_DESCRIPTOR,       // bDescriptorType
        LSB(TOTAL_DESCRIPTOR_LENGTH),   // wTotalLength (LSB)
        MSB(TOTAL_DESCRIPTOR_LENGTH),   // wTotalLength (MSB)
        0x01,                           // bNumInterfaces
        DEFAULT_CONFIGURATION,          // bConfigurationValue
        0x00,                           // iConfiguration
        C_RESERVED | C_SELF_POWERED,    // bmAttributes
        C_POWER(100),                   // bMaxPower

        INTERFACE_DESCRIPTOR_LENGTH,    // bLength
        INTERFACE_DESCRIPTOR,           // bDescriptorType
        0x00,                           // bInterfaceNumber
        0x00,                           // bAlternateSetting
        0x02,                           // bNumEndpoints
        0xff,                           // bInterfaceClass
        0xff,                           // bInterfaceSubClass
        0xff,                           // bInterfaceProtocol
        0x00,                           // iInterface

        ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
        ENDPOINT_DESCRIPTOR,            // bDescriptorType
        PHY_TO_DESC(IN_ENDPOINT),       // bEndpointAddress
        E_BULK,                         // bmAttributes
        LSB(MAX_PACKET_SIZE_EPBULK),    // wMaxPacketSize (LSB)
        MSB(MAX_PACKET_SIZE_EPBULK),    // wMaxPacketSize (MSB)
        1,                              // bInterval (milliseconds)

        ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
        ENDPOINT_DESCRIPTOR,            // bDescriptorType
        PHY_TO_DESC(OUT_ENDPOINT),      // bEndpointAddress
        E_BULK,                         // bmAttributes
        LSB(MAX_PACKET_SIZE_EPBULK),    // wMaxPacketSize (LSB)
        MSB(MAX_PACKET_SIZE_EPBULK),    // wMaxPacketSize (MSB)
        1,                              // bInterval (milliseconds)
    };
    return configurationDescriptor;
}

uint8_t * USBFPGALink::deviceDesc() {
    static uint8_t deviceDescriptor[] = {
        DEVICE_DESCRIPTOR_LENGTH,       /* bLength */
        DEVICE_DESCRIPTOR,              /* bDescriptorType */
        LSB(USB_VERSION_2_0),           /* bcdUSB (LSB) */
        MSB(USB_VERSION_2_0),           /* bcdUSB (MSB) */
        0xff,                           /* bDeviceClass (vendor specific) */
        0xff,                           /* bDeviceSubClass (vendor specific) */
        0xff,                           /* bDeviceprotocol (vendor specific) */
        MAX_PACKET_SIZE_EP0,            /* bMaxPacketSize0 */
        (uint8_t)(LSB(VENDOR_ID)),                 /* idVendor (LSB) */
        (uint8_t)(MSB(VENDOR_ID)),                 /* idVendor (MSB) */
        (uint8_t)(LSB(PRODUCT_ID)),                /* idProduct (LSB) */
        (uint8_t)(MSB(PRODUCT_ID)),                /* idProduct (MSB) */
        (uint8_t)(LSB(PRODUCT_RELEASE)),           /* bcdDevice (LSB) */
        (uint8_t)(MSB(PRODUCT_RELEASE)),           /* bcdDevice (MSB) */
        STRING_OFFSET_IMANUFACTURER,    /* iManufacturer */
        STRING_OFFSET_IPRODUCT,         /* iProduct */
        STRING_OFFSET_ISERIAL,          /* iSerialNumber */
        0x01                            /* bNumConfigurations */
    };
    return deviceDescriptor;
}


// Called in ISR context
// Called by USBDevice on Endpoint0 request
// This is used to handle extensions to standard requests and class specific requests.
// Return true if class handles this request
bool USBFPGALink::USBCallback_request() {
	CONTROL_TRANSFER * transfer = getTransferPtr();
	const uint16_t wValue = transfer->setup.wValue;
	const uint16_t wIndex = transfer->setup.wIndex;
	static uint8_t response = 0;
	
	// the parent class handles all non-vendor requests
//	printf("type: %i, bRequest: %i\r\n", transfer->setup.bmRequestType.Type, transfer->setup.bRequest);
	
	if (transfer->setup.bmRequestType.Type != VENDOR_TYPE) return false;

	// get direction	
	bool deviceToHost = (transfer->setup.bmRequestType.dataTransferDirection == DEVICE_TO_HOST);
	bool hostToDevice = !deviceToHost;
	//printf("ctl, bRequest: %02x, deviceToHost: %i, wValue: %i, wIndex, %i\r\n", transfer->setup.bRequest, deviceToHost, transfer->setup.wValue, transfer->setup.wIndex);
	
	// evaluate command
	switch (transfer->setup.bRequest) {
		case CMD_MODE_STATUS:
			if (deviceToHost) {
				static uint8_t statusBuffer[] = {
					'N',   // Magic bytes (Chris' cat name)
					'E',
					'M',
					'I',
					0x00,  // Last operation diagnostic code
					1,     // TODO: FPGA running flag
					0x24,  // NeroJTAG endpoints (0x24)
					0x24,  // CommFPGA endpoints
					0x00,  // Reserved
					0x00,  // Reserved
					0x00,  // Reserved
					0x00,  // Reserved
					0x00,  // Reserved
					0x00,  // Reserved
					0x00,  // Reserved
					0x00,  // Reserved
				};
				transfer->remaining = sizeof(statusBuffer);
				transfer->ptr = (uint8_t*) statusBuffer;
				transfer->direction = DEVICE_TO_HOST;
				return true;
			} else {
				// Enable or disable FIFO mode
				const uint16_t param = transfer->setup.wValue;
				const uint16_t value = transfer->setup.wIndex;
				if (param == FIFO_MODE) {
					fifoSetEnabled(value);
				}
				return true;
			}
			break;

		case CMD_JTAG_CLOCK_DATA:
			if (hostToDevice) {
//				printf("jtag clock data\r\n");
				transfer->remaining = 4;
				transfer->direction = HOST_TO_DEVICE;
				transfer->notify = true;
				return true;
			}
			break;
	
		case CMD_JTAG_CLOCK_FSM:
			if (hostToDevice) {
//				printf("jtag clock fsm\r\n");
				transfer->remaining = 4;
				transfer->direction = HOST_TO_DEVICE;
				transfer->notify = true;
				return true;
			}
			break;
	
		case CMD_JTAG_CLOCK:
			if (hostToDevice) {
				uint32_t numCycles = wIndex;
				numCycles <<= 16;
				numCycles |= wValue;
				progInitPortDirections();
				progClocks(numCycles);
				return true;
			}
			break;
	
		case CMD_PORT_BIT_IO:
			if (deviceToHost) {
				const uint8_t portNumber = wValue & 0xFF;
				const uint8_t bitNumber = wValue >> 8;
				const uint8_t drive = wIndex & 0xFF;
				const uint8_t high = wIndex >> 8;
				if (drive) {
					writeGpio(portNumber, bitNumber, high);
					configurePinAsGpioOutput(portNumber, bitNumber);
				} else {
					configurePinAsGpioInput(portNumber, bitNumber);
				}
				response = readGpio(portNumber, bitNumber);
				transfer->remaining = 1;
				transfer->ptr = &response;
				transfer->direction = DEVICE_TO_HOST;
				return true;
			}
			break;
	
		case CMD_PORT_MAP:
			if (hostToDevice) {
				//printf("CMD_PORT_MAP\r");
				return true;
			}
			break;

		default:
			break;
	}
	return false;
}


// Kick off a shift operation. Next time progShiftExecute() runs, it will execute the shift.
//
void USBFPGALink::progShiftBegin(uint32_t numBits, ProgOp progOp, uint8_t flagByte) {
	m_numBits = numBits;
	m_progOp = progOp;
	m_flagByte = flagByte;
	
	if (m_progOp == PROG_JTAG_NOTSENDING_NOTRECEIVING) {
		//printf("PROG_JTAG_NOTSENDING_NOTRECEIVING\r\n");
		jtagNotSendingNotReceiving();
	}
	switch (m_progOp) {
		case PROG_JTAG_NOTSENDING_NOTRECEIVING:
			//printf("PROG_JTAG_NOTSENDING_NOTRECEIVING\r\n");
			jtagNotSendingNotReceiving();
			break;
			
		case PROG_JTAG_NOTSENDING_ISRECEIVING:
			// The host is not giving us data, but is expecting a response (x0r)
			//printf("PROG_JTAG_NOTSENDING_ISRECEIVING\r\n");
			jtagNotSendingIsReceiving();
			break;

/*		case PROG_JTAG_FAST:
			// The host is giving us data for JTAG programming without testing
			printf("shift begin: PROG_JTAG_FAST\r\n");
			checksum = 0;
			break;*/

		default:
			printf("shift begin: PROG_ undefined\r\n");
			break;
	}
}

// Called in ISR context when a data OUT stage has been performed
void USBFPGALink::USBCallback_requestCompleted(uint8_t* buf, uint32_t length) {
	//printf("xfer compl\r\n");
	
	CONTROL_TRANSFER* transfer = getTransferPtr();
	const uint16_t wValue = transfer->setup.wValue;
	const uint16_t wIndex = transfer->setup.wIndex;
	
	// evaluate command
	switch (transfer->setup.bRequest) {
		case CMD_JTAG_CLOCK_DATA:
		{
			//printf("jtag clock data complete\r\n");
			uint32_t numBits = *((uint32_t*) buf);
			progInitPortDirections();
			progShiftBegin(numBits, (ProgOp)wIndex, (uint8_t)wValue);
			break;
		}
	
		case CMD_JTAG_CLOCK_FSM:
		{
			//printf("jtag clock fsm complete\r\n");
			uint32_t bitPattern = *((uint32_t*) buf);
			const uint8_t transitionCount = (uint8_t) wValue;
			progInitPortDirections();
			progClockFSM(bitPattern, transitionCount);
			break;
		}
	
		default:
			break;
	}
}



// FPGALink logic

void USBFPGALink::fifoSetEnabled(uint8_t mode) {
	m_fifoMode = mode;
	switch (m_fifoMode) {
		case 0:
			//printf("FPGA FIFO mode off\r\n");
			setPinsToInput();
			break;
		case 1:
			//printf("FPGA FIFO mode on\r\n");
			enableSpiPins();
			fpgaState = FpgaStateReadChannel;
			break;
	}
}

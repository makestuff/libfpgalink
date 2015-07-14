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

#define CPLD_MOSI_PORT 0
#define CPLD_MOSI_BIT 21

#define CPLD_MISO_PORT 1
#define CPLD_MISO_BIT 21

#define CPLD_SCLK_PORT 1
#define CPLD_SCLK_BIT 20

#define CPLD_CS_PORT 1
#define CPLD_CS_BIT 23

#define TDI_PORT 0
#define TDI_BIT 22

#define TMS_PORT 1
#define TMS_BIT 22

#define TCK_PORT 1
#define TCK_BIT 15

#define TDO_PORT 1
#define TDO_BIT 19

void USBFPGALink::setPinFunction(uint8_t port, uint8_t pin, uint8_t function) {
	uint32_t* reg = (uint32_t*) (LPC_IOCON_BASE + 4 * pin + 0x60 * port);
	*reg = (*reg & ~0x7) | (function & 0x7);
}

void USBFPGALink::setPinDirection(uint8_t port, uint8_t pin, PinDirection direction) {
	if (direction == PIN_INPUT) {
       		LPC_GPIO->DIR[port] &= ~(1 << pin);
       	} else {
       		LPC_GPIO->DIR[port] |= 1 << pin;
       	}
}

void USBFPGALink::writeGpio(uint8_t port, uint8_t pin, bool value) {
	if (value) {
        	LPC_GPIO->SET[port] = 1 << pin;
        } else {
        	LPC_GPIO->CLR[port] = 1 << pin;
        }
}

bool USBFPGALink::readGpio(uint8_t port, uint8_t pin) {
       	return (LPC_GPIO->PIN[port] & (1 << pin)) ? true : false;
}

void USBFPGALink::configurePinAsGpioInput(uint8_t port, uint8_t pin) {
	setPinFunction(port, pin, 0);
	setPinDirection(port, pin, PIN_INPUT);
}

void USBFPGALink::configurePinAsGpioOutput(uint8_t port, uint8_t pin) {
	setPinFunction(port, pin, 0);
	setPinDirection(port, pin, PIN_OUTPUT);
}

void USBFPGALink::progInitPortDirections() {
	// TDI out
	writeGpio(TDI_PORT, TDI_BIT, false);
	configurePinAsGpioOutput(TDI_PORT, TDI_BIT);

	// TMS out
	writeGpio(TMS_PORT, TMS_BIT, false);
	configurePinAsGpioOutput(TMS_PORT, TMS_BIT);

	// TCK out
	writeGpio(TCK_PORT, TCK_BIT, false);
	configurePinAsGpioOutput(TCK_PORT, TCK_BIT);

	// TDO in
	configurePinAsGpioInput(TDO_PORT, TDO_BIT);
}

void USBFPGALink::setTDI(bool set) {
	writeGpio(TDI_PORT, TDI_BIT, set);
}

bool USBFPGALink::getTDO() {
	return readGpio(TDO_PORT, TDO_BIT);
}

void USBFPGALink::setTMS(bool set) {
	writeGpio(TMS_PORT, TMS_BIT, set);
}

void USBFPGALink::setTCK(bool set) {
	writeGpio(TCK_PORT, TCK_BIT, set);
}

void USBFPGALink::setPinsToInput() {
	// set all available pins to input and GPIO function, except these pins:
	// PIO0_0: reset
	// PIO0_3: VBUS
	// PIO0_6: USB_CONNECT
	// PIO0_18/PIO0_19: LEDs
	// PIO1_13/PIO1_14: serial debug port
	//printf("setPinsToInput\r\n");
	for (int i = 0; i < 32; i++) {
		if (i != 0 && i != 3 && i != 6 && i != 18 && i != 19 && i < 28) {
			configurePinAsGpioInput(0, i);
		}
		if (i != 13 && i != 14) {
			configurePinAsGpioInput(1, i);
		}
	}
}

void USBFPGALink::setMOSI(bool set) {
	writeGpio(CPLD_MOSI_PORT, CPLD_MOSI_BIT, set);
}

void USBFPGALink::setSCLK(bool set) {
	writeGpio(CPLD_SCLK_PORT, CPLD_SCLK_BIT, set);
}

void USBFPGALink::setCS(bool set) {
	writeGpio(CPLD_CS_PORT, CPLD_CS_BIT, set);
}

bool USBFPGALink::getMISO() {
	return readGpio(CPLD_MISO_PORT, CPLD_MISO_BIT);
}

// SCLK-clock the supplied byte into MOSI, MSB first, and returns the received byte from MISO.
uint8_t USBFPGALink::spiTransfer(uint8_t byte) {
	// TODO: use hardware implementation of mbed
	uint8_t result = 0;
	for (int i = 0; i < 8; i++) {
		setMOSI((byte & 0x80) > 0);
		setSCLK(true);
		byte <<= 1;
		result <<= 1;
		setSCLK(false);
		if (getMISO()) result |= 1;
	}
	return result;
}

void USBFPGALink::enableSpiPins() {
	#if 0
	// TODO: use hardware implementation of mbed
	setPinFunction(CPLD_MOSI_PORT, CPLD_MOSI_BIT, 2);  // MOSI1
	setPinFunction(CPLD_MISO_PORT, CPLD_MISO_BIT, 2);  // MISO1
	setPinFunction(CPLD_SCLK_PORT, CPLD_SCLK_BIT, 2);  // SCK1
	setPinFunction(CPLD_CS_PORT, CPLD_CS_BIT, 0);  // CS as GPIO, software controlled
	setPinDirection(CPLD_MOSI_PORT, CPLD_MOSI_BIT, PIN_OUTPUT);
	setPinDirection(CPLD_MISO_PORT, CPLD_MISO_BIT, PIN_INPUT);
	setPinDirection(CPLD_SCLK_PORT, CPLD_SCLK_BIT, PIN_OUTPUT);
	setPinDirection(CPLD_CS_PORT, CPLD_CS_BIT, PIN_OUTPUT);
	#endif

	configurePinAsGpioOutput(CPLD_MOSI_PORT, CPLD_MOSI_BIT);
	configurePinAsGpioInput(CPLD_MISO_PORT, CPLD_MISO_BIT);
	configurePinAsGpioOutput(CPLD_SCLK_PORT, CPLD_SCLK_BIT);
	configurePinAsGpioOutput(CPLD_CS_PORT, CPLD_CS_BIT);
	setCS(true);
}

void USBFPGALink::jtagClock() {
	// add delays, if too fast for your CPU
	setTCK(true);
	setTCK(false);
}

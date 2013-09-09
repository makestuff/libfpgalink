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
#ifndef USBFPGALINK_H
#define USBFPGALINK_H

/* These headers are included for child class. */
#include "USBEndpoints.h"
#include "USBDescriptor.h"
#include "USBDevice_Types.h"
#include "USBDevice.h"

#include "../../vendorCommands.h"

/** 
* USBFPGALink example
*
* @code
* #include "mbed.h"
*
* USBFPGALink fpgaLink;
*
* int main() {            
*    while (1) {    
* TODO
*    }
* }
* @endcode
*/
class USBFPGALink: public USBDevice {
public:

    /**
    * Constructor
    *
    * @param vendor_id 0x1D50, openmoko vendor id
    * @param product_id 0x602B, allocated by http://wiki.openmoko.org/wiki/USB_Product_IDs
    * @param product_release 1
    */
    USBFPGALink(uint16_t vendor_id = 0x1D50, uint16_t product_id = 0x602B, uint16_t product_release = 0x0001);
    
protected:
    /*
    * Callback called when a packet is received
    */
    virtual bool EP2_OUT_callback();

    /*
    * Callback called when a packet has been sent
    */
    virtual bool EP4_IN_callback();

    virtual bool USBCallback_setConfiguration(uint8_t configuration);
    /*
    * Get string product descriptor
    *
    * @returns pointer to the string product descriptor
    */
    virtual uint8_t * stringIproductDesc();
    
    /*
    * Get string manufacturer descriptor
    *
    * @returns pointer to the string manufacturer descriptor
    */
    virtual uint8_t * stringImanufacturerDesc();
    
    /*
    * Get configuration descriptor
    *
    * @returns pointer to the configuration descriptor
    */
    virtual uint8_t * configurationDesc();

    /*
    * Get device descriptor
    *
    * @returns pointer to the device descriptor
    */
    virtual uint8_t * deviceDesc();

    virtual bool USBCallback_request();
    virtual void USBCallback_requestCompleted(uint8_t * buf, uint32_t length);

private:
	uint8_t m_fifoMode;

	void setPinFunction(uint8_t port, uint8_t pin, uint8_t function);
	void setPinDirection(uint8_t port, uint8_t pin, PinDirection direction);
	void writeGpio(uint8_t port, uint8_t pin, bool value);
	bool readGpio(uint8_t port, uint8_t pin);
	void configurePinAsGpioInput(uint8_t port, uint8_t pin);
	void configurePinAsGpioOutput(uint8_t port, uint8_t pin);
	void progInitPortDirections();
	void setTDI(bool set);
	bool getTDO();
	void setTMS(bool set);
	void setTCK(bool set);
	void jtagClock();
	void jtagBit(bool set);
	void progClockFSM(uint32_t bitPattern, uint8_t transitionCount);
	void jtagShiftOut(uint8_t byte);
	uint8_t shiftInOut(uint8_t byte);
	void jtagIsSendingIsReceiving(uint8_t* buf, uint32_t len);
	void jtagIsSendingNotReceiving(uint8_t* buf, uint32_t len);
	void jtagFastByte(uint8_t b);
	void jtagFast(uint8_t* buf, uint32_t len);
	void jtagNotSendingIsReceiving();
	void jtagNotSendingNotReceiving();
	void progClocks(uint32_t numClocks);
	void progShiftBegin(uint32_t numBits, ProgOp progOp, uint8_t flagByte);

	void write(uint8_t* data, uint32_t size);
	void fifoSetEnabled(uint8_t mode);
	uint8_t portBitAccess(uint8_t portNumber, uint8_t bitNumber, uint8_t inputOutput, uint8_t clearSet);
	void setPinsToInput();
	void enableSpiPins();
	
	void fpgaDeviceToHost();

	void setMOSI(bool set);
	void setSCLK(bool set);
	void setCS(bool set);
	bool getMISO();
	uint8_t spiTransfer(uint8_t byte);
	
	uint32_t m_numBits;
	ProgOp m_progOp;
	uint8_t m_flagByte;
};

#endif

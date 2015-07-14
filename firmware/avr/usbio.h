/*
 * Copyright (C) 2013 Chris McClelland
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
#ifndef USBIO_H
#define USBIO_H

// Select a different endpoint.
static inline void usbSelectEndpoint(uint8 epNum) {
	UENUM = epNum;
}

// Check if the selected OUT endpoint has received a packet.
static inline bool usbOutPacketReady(void) {
	return ((UEINTX & (1 << RXOUTI)) ? true : false);
}

// Check if the selected IN endpoint is ready for another packet.
static inline bool usbInPacketReady(void) {
	return ((UEINTX & (1 << TXINI)) ? true : false);
}

// Acknowledge this OUT packet, making room for another.
static inline void usbAckPacket(void) {
	UEINTX &= ~((1 << RXOUTI) | (1 << FIFOCON));
}

// Send the current IN packet to the host.
static inline void usbFlushPacket(void) {
	UEINTX &= ~((1 << TXINI) | (1 << FIFOCON));
}

// Is there space in the IN buffer, or bytes in the OUT buffer?
static inline bool usbReadWriteAllowed(void) {
	return ((UEINTX & (1 << RWAL)) ? true : false);
}

// Get the next byte from the OUT buffer.
static inline uint8 usbGetByte(void) {
	return UEDATX;
}

// Put another byte in the IN buffer.
static inline void usbPutByte(uint8 byte) {
	UEDATX = byte;
}

// Get another OUT byte, acknowledging this packet and waiting for another, if necessary.
static inline uint8 usbRecvByte(void) {
	while ( !usbReadWriteAllowed() ) {
		usbAckPacket();
		while ( !usbOutPacketReady() );
	}
	return usbGetByte();
}

// Flush the current IN packet if necessary, then put a byte in the IN buffer.
static inline void usbSendByte(uint8 byte) {
	if ( !usbReadWriteAllowed() ) {
		usbFlushPacket();
		while ( !usbInPacketReady() );
	}
	usbPutByte(byte);
}

#endif

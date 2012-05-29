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
#ifndef DESC_H
#define DESC_H

#include <LUFA/Drivers/USB/USB.h>
#include <avr/pgmspace.h>

typedef USB_Descriptor_Device_t USBDeviceDescriptor;
typedef USB_Descriptor_String_t USBStringDescriptor;
typedef USB_Descriptor_Configuration_Header_t USBConfigurationDescriptorHeader;
typedef USB_Descriptor_Interface_t USBInterfaceDescriptor;
typedef USB_Descriptor_Endpoint_t USBEndpointDescriptor;

typedef struct {
	USBConfigurationDescriptorHeader ConfigurationHeader;
	USBInterfaceDescriptor           Interface;
	USBEndpointDescriptor            InEndpoint;
	USBEndpointDescriptor            OutEndpoint;
} USBConfigurationDescriptor;

#define OUT_ENDPOINT_ADDR 2
#define IN_ENDPOINT_ADDR  4
#define ENDPOINT_SIZE 64

#endif

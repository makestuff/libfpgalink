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
#ifndef FIRMWARE_H
#define FIRMWARE_H

#include <makestuff.h>

#ifdef __cplusplus
extern "C" {
#endif

	struct FirmwareInfo {
		const uint8 *const data;
		const uint16 length;
		const uint16 vp;
		const uint16 d0A;  // offset to change to 08 when JTAG on port C
		const uint16 d0B;  // offset to change to 09 when JTAG on port C
		const uint16 outBits;
		const uint16 outBitsComp;
		const uint16 *oeRegs;
		const uint16 *allBits;
		const uint16 *allBitsComp;
		const uint16 *tdoBit;
		const uint16 *tdiBit;
		const uint16 *tmsBit;
		const uint16 *tckBit;
	};
	
	extern const struct FirmwareInfo ramFirmware;
	extern const struct FirmwareInfo eepromNoBootFirmware;
	extern const struct FirmwareInfo eepromWithBootFirmware;
	
	#define NUM_OE_BITS  2
	#define NUM_ALL_BITS 4
	#define NUM_TDO_BIT  11
	#define NUM_TDI_BIT  38
	#define NUM_TMS_BIT  7
	#define NUM_TCK_BIT  81

#ifdef __cplusplus
}
#endif

#endif

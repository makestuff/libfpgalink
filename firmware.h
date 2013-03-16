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
	};
	
	extern const struct FirmwareInfo ramFirmware;
	extern const struct FirmwareInfo eepromNoBootFirmware;
	
#ifdef __cplusplus
}
#endif

#endif

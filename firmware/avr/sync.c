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
#include <LUFA/Drivers/USB/USB.h>
#include <makestuff.h>
#include "sync.h"
#include "desc.h"

static bool m_enabled = false;

void syncSetEnabled(bool enable) {
	m_enabled = enable;
}

bool syncIsEnabled(void) {
	return m_enabled;
}

void syncExecute(void) {
	Endpoint_SelectEndpoint(OUT_ENDPOINT_ADDR);
	if ( Endpoint_IsOUTReceived() ) {
		uint8 buf[4], i;
		Endpoint_Read_Stream_LE(buf, 4, NULL);
		Endpoint_ClearOUT();
		for ( i = 0; i < 4; i++ ) {
			if ( buf[i] >= 'a' && buf[i] <= 'z' ) {
				buf[i] = buf[i] & 0xDF;
			}
		}
		Endpoint_SelectEndpoint(IN_ENDPOINT_ADDR);
		while ( !Endpoint_IsINReady() );
		Endpoint_Write_Stream_LE(buf, 4, NULL);
		Endpoint_ClearIN();
	}
}

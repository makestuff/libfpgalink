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
#include "mbed.h"
#include "USBFPGALink.h"

DigitalInOut greenLed(P0_18);
DigitalInOut redLed(P0_19);

USBFPGALink fpgaLink;

int main()
{
	// start message
	printf("MakeStuff FPGALink/LPC v0.1\r\n");
	
	// set LED pins to output
	greenLed.output();
	redLed.output();
	
	// red LED on, green LED off
	greenLed = 1;
	redLed = 0;
	
	// try to connect
	printf("USB connect...\r\n");
	fpgaLink.connect();
	printf("connected\r\n");

	// red LED off
	redLed = 1;

	// rest in interrupt
	while (1) {
		//greenLed = 0;
		wait_ms(500);
		//greenLed = 1;
		wait_ms(500);
		//printf("test\r\n");
	}
}

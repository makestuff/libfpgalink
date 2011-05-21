/* 
 * Copyright (C) 2009 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <cstdio>
#include <UnitTest++.h>

using namespace std;

int main() {
	printf("\n\nLibFPGALink Integration Tests\n");
	printf("-----------------------------\n");
	printf("If this is Linux, remember to run as root.\n");
	printf("Ensure that the S3BOARD is connected with the EEPROM jumper open, and a NeroJTAG/AVR connected with nothing attached.\n");
	printf("Press <Enter> to begin testing.\n");
	getchar();
	return UnitTest::RunAllTests();
}

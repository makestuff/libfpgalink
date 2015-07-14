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
#ifndef PROG_H
#define PROG_H

#include <makestuff.h>
#include "../../vendorCommands.h"
#include "../../libfpgalink.h"

// Kick off a shift operation. Next time progExecuteShift() runs, it will execute the shift.
void progShiftBegin(uint32 numBits, ProgOp progOp, uint8 flagByte);

// Actually execute the shift operation initiated by progBeginShift(). This is done in a
// separate method because vendor commands cannot read & write to bulk endpoints.
void progShiftExecute(void);

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
void progClockFSM(uint32 bitPattern, uint8 transitionCount);

// Keep TMS and TDI as they are, and clock the JTAG state machine "numClocks" times.
void progClocks(uint32 numClocks);

// Map the ports, to select either a hardware SPI or a bit-bang SPI
bool progPortMap(LogicalPort logicalPort, uint8 physicalPort, uint8 physicalBit);

#endif

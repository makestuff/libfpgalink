//
// Copyright (C) 2009-2012 Chris McClelland
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
module
	top_level(
		// FX2 interface
		input  wire      fx2Clk_in,     // 48MHz clock from FX2
		output wire[1:0] fx2Addr_out,   // select FIFO: "10" for EP6OUT, "11" for EP8IN
		inout  wire[7:0] fx2Data_io,    // 8-bit data to/from FX2
		output wire      fx2Read_out,   // asserted (active-low) when reading from FX2
		output wire      fx2OE_out,     // asserted (active-low) to tell FX2 to drive bus
		input  wire      fx2GotData_in, // asserted (active-high) when FX2 has data for us
		output wire      fx2Write_out,  // asserted (active-low) when writing to FX2
		input  wire      fx2GotRoom_in, // asserted (active-high) when FX2 has room for more data from us
		output wire      fx2PktEnd_out, // asserted (active-low) when a host read needs to be committed early

		// Onboard peripherals
		output wire[7:0] sseg_out,      // seven-segment display cathodes (one for each segment)
		output wire[3:0] anode_out,     // seven-segment display anodes (one for each digit)
		output wire[7:0] led_out,       // eight LEDs
		input  wire[7:0] sw_in          // eight switches
	);

	// Channel read/write interface:
	wire[6:0]  chanAddr;         // the selected channel (0-127)
	wire[7:0]  chanDataIn;       // data lines used when the host reads from a channel
	wire       chanRead;         // '1' means "on the next clock rising edge, put your next byte of data on chanData_in"
	wire       chanGotData;      // channel logic can drive this low to say "I don't have data ready for you"
	wire[7:0]  chanDataOut;      // data lines used when the host writes to a channel
	wire       chanWrite;        // '1' means "on the next clock rising edge, please accept the data on chanData_out"
	wire       chanGotRoom;      // channel logic can drive this low to say "I'm not ready for more data yet"

	// Needed so that the comm_fpga module can drive both fx2Read_out and fx2OE_out
	wire       fx2Read;

	// Flags for display on the 7-seg decimal points
	wire[3:0]  flags;

	// FIFOs
	wire[15:0] fifoCount;        // MSB=writeFifo, LSB=readFifo

	// Write FIFO:
	wire[7:0]  writeFifoDataIn;  // producer: data
	wire       writeFifoWrite;   // producer: write enable
	wire       writeFifoFull;    // producer: full flag
	wire[7:0]  writeFifoDataOut; // consumer: data
	wire       writeFifoRead;    // consumer: write enable
	wire       writeFifoEmpty;   // consumer: full flag

	// Read FIFO:
	wire[7:0]  readFifoDataIn;   // producer: data
	wire       readFifoWrite;    // producer: write enable
	wire       readFifoFull;     // producer: full flag
	wire[7:0]  readFifoDataOut;  // consumer: data
	wire       readFifoRead;     // consumer: write enable
	wire       readFifoEmpty;    // consumer: full flag

	// Counter which endlessly puts items in read FIFO for the host to read
	reg[7:0]   count = 8'h00;
	wire[7:0]  count_next;
	
	// Infer registers
	always @(posedge fx2Clk_in)
	begin
		count <= count_next;
	end

	// Wire up write FIFO to channel 0:
	assign writeFifoDataIn = chanDataOut;
	assign writeFifoWrite = (chanWrite == 1'b1 && chanAddr == 7'b0000000) ? 1'b1 : 1'b0;
	assign chanGotRoom = (writeFifoFull == 1'b1 && chanAddr == 7'b0000000) ? 1'b0 : 1'b1;

	// Wire up read FIFO to channel 0:
	assign readFifoDataIn = count;
	assign readFifoRead = (chanRead == 1'b1 && chanAddr == 7'b0000000) ? 1'b1 : 1'b0;
	assign chanGotData = (readFifoEmpty == 1'b1 && chanAddr == 7'b0000000) ? 1'b0 : 1'b1;
	assign count_next = (readFifoWrite == 1'b1) ? count + 1 : count;
	
	// Select values to return for each channel when the host is reading
	assign chanDataIn =
		(chanAddr == 7'b0000000) ? readFifoDataOut :  // get data from the read FIFO
		(chanAddr == 7'b0000001) ? fifoCount[15:8] :  // read the current depth of the write FIFO
		(chanAddr == 7'b0000010) ? fifoCount[7:0]  :  // read the current depth of the write FIFO
		8'h00;
	
	// CommFPGA module
	assign fx2Read_out = fx2Read;
	assign fx2OE_out = fx2Read;
	assign fx2Addr_out[1] = 1'b1;  // Use EP6OUT/EP8IN, not EP2OUT/EP4IN.
	comm_fpga comm_fpga(
		// FX2 interface
		.fx2Clk_in(fx2Clk_in),
		.fx2FifoSel_out(fx2Addr_out[0]),
		.fx2Data_io(fx2Data_io),
		.fx2Read_out(fx2Read),
		.fx2GotData_in(fx2GotData_in),
		.fx2Write_out(fx2Write_out),
		.fx2GotRoom_in(fx2GotRoom_in),
		.fx2PktEnd_out(fx2PktEnd_out),

		// Channel read/write interface
		.chanAddr_out(chanAddr),
		.chanData_in(chanDataIn),
		.chanRead_out(chanRead),
		.chanGotData_in(chanGotData),
		.chanData_out(chanDataOut),
		.chanWrite_out(chanWrite),
		.chanGotRoom_in(chanGotRoom)
	);

	// Write FIFO: written by host, read by leds
	fifo write_fifo(
		.clk(fx2Clk_in),
		.data_count(fifoCount[15:8]),

		// Production end
		.din(writeFifoDataIn),
		.wr_en(writeFifoWrite),
		.full(writeFifoFull),

		// Consumption end
		.dout(writeFifoDataOut),
		.rd_en(writeFifoRead),
		.empty(writeFifoEmpty)
	);
	
	// Read FIFO: written by counter, read by host
	fifo read_fifo(
		.clk(fx2Clk_in),
		.data_count(fifoCount[7:0]),

		// Production end
		.din(readFifoDataIn),
		.wr_en(readFifoWrite),
		.full(readFifoFull),

		// Consumption end
		.dout(readFifoDataOut),
		.rd_en(readFifoRead),
		.empty(readFifoEmpty)
	);

	// Producer timer
	timer producer_timer(
		.clk_in(fx2Clk_in),
		.ceiling_in(sw_in[3:0]),
		.tick_out(readFifoWrite)
	);

	// Consumer timer
	timer consumer_timer(
		.clk_in(fx2Clk_in),
		.ceiling_in(sw_in[7:4]),
		.tick_out(writeFifoRead)
	);

	// LEDs and 7-seg display
	assign led_out = writeFifoDataOut;
	assign flags = {1'b0, writeFifoEmpty, 1'b0, readFifoFull};
	seven_seg seven_seg(
		.clk_in(fx2Clk_in),
		.data_in(fifoCount),
		.dots_in(flags),
		.segs_out(sseg_out),
		.anodes_out(anode_out)
	);
endmodule

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
		// EPP interface -----------------------------------------------------------------------------
		input  wire      eppClk_in,
		inout  wire[7:0] eppData_io,
		input  wire      eppAddrStb_in,
		input  wire      eppDataStb_in,
		input  wire      eppWrite_in,
		output wire      eppWait_out,

		// Onboard peripherals -----------------------------------------------------------------------
		output wire[7:0] sseg_out,      // seven-segment display cathodes (one for each segment)
		output wire[3:0] anode_out,     // seven-segment display anodes (one for each digit)
		output wire[7:0] led_out,       // eight LEDs
		input  wire[7:0] sw_in          // eight switches
	);

	// Channel read/write interface -----------------------------------------------------------------
	wire[6:0]  chanAddr;  // the selected channel (0-127)

	// Host >> FPGA pipe:
	wire[7:0]  h2fData;   // data lines used when the host writes to a channel
	wire       h2fValid;  // '1' means "on the next clock rising edge, please accept the data on h2fData_out"
	wire       h2fReady;  // channel logic can drive this low to say "I'm not ready for more data yet"

	// Host << FPGA pipe:
	wire[7:0]  f2hData;   // data lines used when the host reads from a channel
	wire       f2hValid;  // channel logic can drive this low to say "I don't have data ready for you"
	wire       f2hReady;  // '1' means "on the next clock rising edge, put your next byte of data on f2hData_in"
	// ----------------------------------------------------------------------------------------------
	
	// Flags for display on the 7-seg decimal points
	wire[3:0]  flags;

	// FIFOs
	wire[15:0] fifoCount;        // MSB=writeFifo, LSB=readFifo

	// Write FIFO:
	wire[7:0]  writeFifoInputData;    // producer: data
	wire       writeFifoInputValid;   //           valid flag
	wire       writeFifoInputReady;   //           ready flag
	wire[7:0]  writeFifoOutputData;   // consumer: data
	wire       writeFifoOutputValid;  //           valid flag
	wire       writeFifoOutputReady;  //           ready flag

	// Read FIFO:
	wire[7:0]  readFifoInputData;     // producer: data
	wire       readFifoInputValid;    //           valid flag
	wire       readFifoInputReady;    //           ready flag
	wire[7:0]  readFifoOutputData;    // consumer: data
	wire       readFifoOutputValid;   //           valid flag
	wire       readFifoOutputReady;   //           ready flag

	// Counter which endlessly puts items into the read FIFO for the host to read
	reg[7:0]   count = 8'h00;
	wire[7:0]  count_next;

	// Producer and consumer timers
	wire[3:0]  producerSpeed;
	wire[3:0]  consumerSpeed;
	
	// Infer registers
	always @(posedge eppClk_in)
		count <= count_next;

	// Wire up write FIFO to channel 0 writes:
	//   flags(2) driven by writeFifoOutputValid
	//   writeFifoOutputReady driven by consumer_timer
	//   LEDs driven by writeFifoOutputData
	assign writeFifoInputData = h2fData;
	assign writeFifoInputValid = (h2fValid == 1'b1 && chanAddr == 7'b0000000) ? 1'b1 : 1'b0;
	assign h2fReady = (writeFifoInputReady == 1'b0 && chanAddr == 7'b0000000) ? 1'b0 : 1'b1;

	// Wire up read FIFO to channel 0 reads:
	//   readFifoInputValid driven by producer_timer
	//   flags(0) driven by readFifoInputReady
	assign count_next = (readFifoInputValid == 1'b1) ? count + 1'b1 : count;
	assign readFifoInputData = count;
	assign f2hValid = (readFifoOutputValid == 1'b0 && chanAddr == 7'b0000000) ? 1'b0 : 1'b1;
	assign readFifoOutputReady = (f2hReady == 1'b1 && chanAddr == 7'b0000000) ? 1'b1 : 1'b0;
	
	// Select values to return for each channel when the host is reading
	assign f2hData =
		(chanAddr == 7'b0000000) ? readFifoOutputData :  // get data from the read FIFO
		(chanAddr == 7'b0000001) ? fifoCount[15:8] :  // read the current depth of the write FIFO
		(chanAddr == 7'b0000010) ? fifoCount[7:0]  :  // read the current depth of the write FIFO
		8'h00;
	
	// CommFPGA module
	comm_fpga_epp comm_fpga_epp(
		// EPP interface
		.eppClk_in(eppClk_in),
		.eppData_io(eppData_io),
		.eppAddrStb_in(eppAddrStb_in),
		.eppDataStb_in(eppDataStb_in),
		.eppWrite_in(eppWrite_in),
		.eppWait_out(eppWait_out),

		// Channel read/write interface
		.chanAddr_out(chanAddr),
		.h2fData_out(h2fData),
		.h2fValid_out(h2fValid),
		.h2fReady_in(h2fReady),
		.f2hData_in(f2hData),
		.f2hValid_in(f2hValid),
		.f2hReady_out(f2hReady)
	);

	// Write FIFO: written by host, read by LEDs
	fifo_wrapper write_fifo(
		.clk_in(eppClk_in),
		.depth_out(fifoCount[15:8]),

		// Production end
		.inputData_in(writeFifoInputData),
		.inputValid_in(writeFifoInputValid),
		.inputReady_out(writeFifoInputReady),

		// Consumption end
		.outputData_out(writeFifoOutputData),
		.outputValid_out(writeFifoOutputValid),
		.outputReady_in(writeFifoOutputReady)
	);
	
	// Read FIFO: written by counter, read by host
	fifo_wrapper read_fifo(
		.clk_in(eppClk_in),
		.depth_out(fifoCount[7:0]),

		// Production end
		.inputData_in(readFifoInputData),
		.inputValid_in(readFifoInputValid),
		.inputReady_out(readFifoInputReady),

		// Consumption end
		.outputData_out(readFifoOutputData),
		.outputValid_out(readFifoOutputValid),
		.outputReady_in(readFifoOutputReady)
	);

	// Producer timer: how fast stuff is put into the read FIFO
	assign producerSpeed = ~(sw_in[3:0]);
	timer producer_timer(
		.clk_in(eppClk_in),
		.ceiling_in(producerSpeed),
		.tick_out(readFifoInputValid)
	);

	// Consumer timer: how fast stuff is drained from the write FIFO
	assign consumerSpeed = ~(sw_in[7:4]);
	timer consumer_timer(
		.clk_in(eppClk_in),
		.ceiling_in(consumerSpeed),
		.tick_out(writeFifoOutputReady)
	);

	// LEDs and 7-seg display
	assign led_out = writeFifoOutputData;
	assign flags = {1'b0, writeFifoOutputValid, 1'b0, readFifoInputReady};
	seven_seg seven_seg(
		.clk_in(eppClk_in),
		.data_in(fifoCount),
		.dots_in(flags),
		.segs_out(sseg_out),
		.anodes_out(anode_out)
	);
endmodule

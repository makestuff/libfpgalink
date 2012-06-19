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
		// FX2 interface -----------------------------------------------------------------------------
		input  wire      fx2Clk_in,     // 48MHz clock from FX2
		output wire[1:0] fx2Addr_out,   // select FIFO: "10" for EP6OUT, "11" for EP8IN
		inout  wire[7:0] fx2Data_io,    // 8-bit data to/from FX2

		// When EP6OUT selected:
		output wire      fx2Read_out,   // asserted (active-low) when reading from FX2
		output wire      fx2OE_out,     // asserted (active-low) to tell FX2 to drive bus
		input  wire      fx2GotData_in, // asserted (active-high) when FX2 has data for us

		// When EP8IN selected:
		output wire      fx2Write_out,  // asserted (active-low) when writing to FX2
		input  wire      fx2GotRoom_in, // asserted (active-high) when FX2 has room for more data from us
		output wire      fx2PktEnd_out, // asserted (active-low) when a host read needs to be committed early

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

	// Needed so that the comm_fpga_fx2 module can drive both fx2Read_out and fx2OE_out
	wire       fx2Read;

	// Flags for display on the 7-seg decimal points
	wire[3:0]  flags;

	// Registers implementing the channels
	reg[15:0]  checksum        = 16'h0000;
	wire[15:0] checksum_next;
	reg[7:0]   reg0              = 8'h00;
	wire[7:0]  reg0_next;
	reg[7:0]   reg1              = 8'h00;
	wire[7:0]  reg1_next;
	reg[7:0]   reg2              = 8'h00;
	wire[7:0]  reg2_next;
	reg[7:0]   reg3              = 8'h00;
	wire[7:0]  reg3_next;
                                                                          //BEGIN_SNIPPET(registers)
	// Infer registers
	always @(posedge fx2Clk_in)
	begin
		checksum <= checksum_next;
		reg0 <= reg0_next;
		reg1 <= reg1_next;
		reg2 <= reg2_next;
		reg3 <= reg3_next;
	end

	// Drive register inputs for each channel when the host is writing
	assign checksum_next =
		(chanAddr == 7'b0000000 && h2fValid == 1'b1) ?
			checksum + h2fData :
		(chanAddr == 7'b0000001 && h2fValid == 1'b1 && h2fData[0] == 1'b1) ?
			16'h0000 :
		checksum;
	assign reg0_next =
		(chanAddr == 7'b0000000 && h2fValid == 1'b1) ? h2fData : reg0;
	assign reg1_next =
		(chanAddr == 7'b0000001 && h2fValid == 1'b1) ? h2fData : reg1;
	assign reg2_next =
		(chanAddr == 7'b0000010 && h2fValid == 1'b1) ? h2fData : reg2;
	assign reg3_next =
		(chanAddr == 7'b0000011 && h2fValid == 1'b1) ? h2fData : reg3;
	
	// Select values to return for each channel when the host is reading
	assign f2hData =
		(chanAddr == 7'b0000000) ? sw_in :
		(chanAddr == 7'b0000001) ? reg1 :
		(chanAddr == 7'b0000010) ? reg2 :
		(chanAddr == 7'b0000011) ? reg3 :
		8'h00;

	// Assert that there's always data for reading, and always room for writing
	assign f2hValid = 1'b1;
	assign h2fReady = 1'b1;                                                  //END_SNIPPET(registers)
	
	// CommFPGA module
	assign fx2Read_out = fx2Read;
	assign fx2OE_out = fx2Read;
	assign fx2Addr_out[1] = 1'b1;  // Use EP6OUT/EP8IN, not EP2OUT/EP4IN.
	comm_fpga_fx2 comm_fpga_fx2(
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
		.h2fData_out(h2fData),
		.h2fValid_out(h2fValid),
		.h2fReady_in(h2fReady),
		.f2hData_in(f2hData),
		.f2hValid_in(f2hValid),
		.f2hReady_out(f2hReady)
	);

	// LEDs and 7-seg display
	assign led_out = reg0;
	assign flags = {3'b000, f2hReady};
	seven_seg seven_seg(
		.clk_in(fx2Clk_in),
		.data_in(checksum),
		.dots_in(flags),
		.segs_out(sseg_out),
		.anodes_out(anode_out)
	);
endmodule

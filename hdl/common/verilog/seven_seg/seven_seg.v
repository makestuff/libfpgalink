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
	seven_seg#(
		parameter
		// This can be overridden to change the refresh rate. The anode pattern will change at a
		// frequency given by F(clk_in) / (2**COUNTER_WIDTH). So for a 50MHz clk_in and
		// COUNTER_WIDTH=18, the anode pattern changes at ~191Hz, which means each digit gets
		// refreshed at ~48Hz.
		COUNTER_WIDTH = 18
	)(
		input  wire       clk_in,
		input  wire[15:0] data_in,
		input  wire[3:0]  dots_in,
		output wire[7:0]  segs_out,
		output wire[3:0]  anodes_out
	);

	reg[COUNTER_WIDTH-1:0]  count         = 0;
	wire[COUNTER_WIDTH-1:0] count_next;
	wire[1:0]               anodeSelect;
	wire[3:0]               nibble;
	wire[6:0]               segs;
	wire                    dot;

	// Infer counter register
	always @(posedge clk_in)
		count <= count_next;

	// Increment counter and derive anode select from top two bits
	assign count_next = count + 1'b1;
	assign anodeSelect = count[COUNTER_WIDTH-1:COUNTER_WIDTH-2];

	// Drive anodes
	assign anodes_out =
		(anodeSelect == 2'b00) ? 4'b0111 :
		(anodeSelect == 2'b01) ? 4'b1011 :
		(anodeSelect == 2'b10) ? 4'b1101 :
		4'b1110;

	// Select the appropriate bit from dots_in
	assign dot =
		(anodeSelect == 2'b00) ? ~(dots_in[3]) :
		(anodeSelect == 2'b01) ? ~(dots_in[2]) :
		(anodeSelect == 2'b10) ? ~(dots_in[1]) :
		~(dots_in[0]);

	// Choose a nibble to display
	assign nibble =
		(anodeSelect == 2'b00) ? data_in[15:12] :
		(anodeSelect == 2'b01) ? data_in[11:8] :
		(anodeSelect == 2'b10) ? data_in[7:4] :
		data_in[3:0];
	
	// Decode chosen nibble
	assign segs =
		(nibble == 4'b0000) ? 7'b1000000 :
		(nibble == 4'b0001) ? 7'b1111001 :
		(nibble == 4'b0010) ? 7'b0100100 :
		(nibble == 4'b0011) ? 7'b0110000 :
		(nibble == 4'b0100) ? 7'b0011001 :
		(nibble == 4'b0101) ? 7'b0010010 :
		(nibble == 4'b0110) ? 7'b0000010 :
		(nibble == 4'b0111) ? 7'b1111000 :
		(nibble == 4'b1000) ? 7'b0000000 :
		(nibble == 4'b1001) ? 7'b0010000 :
		(nibble == 4'b1010) ? 7'b0001000 :
		(nibble == 4'b1011) ? 7'b0000011 :
		(nibble == 4'b1100) ? 7'b1000110 :
		(nibble == 4'b1101) ? 7'b0100001 :
		(nibble == 4'b1110) ? 7'b0000110 :
		7'b0001110;

	// Drive segs_out
	assign segs_out = {dot, segs};
endmodule

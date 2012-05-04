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
	timer#(
		parameter
		// This gives the number of bits to be counted when ceiling_in is zero
		COUNTER_WIDTH = 25,
		// This gives the number of bits in the ceiling value
		CEILING_WIDTH = 4
	)(
		input  wire                    clk_in,
		input  wire[CEILING_WIDTH-1:0] ceiling_in,
		output reg                     tick_out
	);

	localparam TOP_BIT = 2**CEILING_WIDTH - 1;
	function[TOP_BIT:0] reverse(input[COUNTER_WIDTH:0] fwd);
		integer i;
		for ( i = 0; i <= TOP_BIT; i = i + 1 )
			reverse[i] = fwd[COUNTER_WIDTH-i];
	endfunction
	reg[COUNTER_WIDTH:0] count_next, count = 0;
	wire[TOP_BIT:0] revCount;

	// Infer registers
	always @(posedge clk_in)
		count <= count_next;

	assign revCount = reverse(count);
	
	always @*
	begin
		if ( revCount[ceiling_in] == 1'b0 )
			begin
				count_next = count + 1'b1;
				tick_out = 1'b0;
			end
		else
			begin
				count_next = 0;
				tick_out = 1'b1;
			end
	end
endmodule

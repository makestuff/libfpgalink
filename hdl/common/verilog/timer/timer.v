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
		// This gives the number of bits in the counter
		COUNTER_WIDTH = 25,
		// This gives the number of bits in the ceiling value
		CEILING_WIDTH = 4
	)(
		input wire                    clk_in,
		input wire[CEILING_WIDTH-1:0] ceiling_in,
		output reg                    tick_out
	);

	reg[COUNTER_WIDTH:0] count_next, count = 0;
	integer index;

	// Infer registers
	always @(posedge clk_in)
		count <= count_next;

	always @*
	begin
		index = COUNTER_WIDTH - ceiling_in;
		if ( count[index] == 1'b0 )
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

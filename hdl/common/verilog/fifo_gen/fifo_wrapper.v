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
	fifo_wrapper(
		// Clock and depth
		input  wire      clk_in,
		output wire[7:0] depth_out,

		// Data is clocked into the FIFO on each clock edge where both valid & ready are high
		input  wire[7:0] inputData_in,
		input  wire      inputValid_in,
		output wire      inputReady_out,

		// Data is clocked out of the FIFO on each clock edge where both valid & ready are high
		output wire[7:0] outputData_out,
		output wire      outputValid_out,
		input wire       outputReady_in
	);

	wire inputFull;
	wire outputEmpty;

	// Invert "full/empty" signals to give "ready/valid" signals
	assign inputReady_out = ~inputFull;
	assign outputValid_out = ~outputEmpty;

	// The encapsulated FIFO
	fifo fifo(
		.clk(clk_in),
		.data_count(depth_out),

		// Production end
		.din(inputData_in),
		.wr_en(inputValid_in),
		.full(inputFull),

		// Consumption end
		.dout(outputData_out),
		.empty(outputEmpty),
		.rd_en(outputReady_in)
	);
endmodule

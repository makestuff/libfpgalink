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
	comm_fpga_epp(
		// EPP interface -----------------------------------------------------------------------------
		input wire eppClk_in,
		inout wire[7:0] eppData_io,
		input wire eppAddrStb_in,
		input wire eppDataStb_in,
		input wire eppWrite_in,
		output wire eppWait_out,

		// Channel read/write interface --------------------------------------------------------------
		output wire[6:0] chanAddr_out,    // the selected channel (0-127)

		// Host >> FPGA pipe:
		output reg[7:0]  h2fData_out,     // data lines used when the host writes to a channel
		output reg       h2fValid_out,    // '1' means "on the next clock rising edge, please accept the data on h2fData_out"
		input  wire      h2fReady_in,     // channel logic can drive this low to say "I'm not ready for more data yet"

		// Host << FPGA pipe:
		input  wire[7:0] f2hData_in,      // data lines used when the host reads from a channel
		input  wire      f2hValid_in,     // channel logic can drive this low to say "I don't have data ready for you"
		output reg       f2hReady_out     // '1' means "on the next clock rising edge, put your next byte of data on f2hData_in"
	);

	localparam[2:0] S_IDLE             = 3'h0;
	localparam[2:0] S_ADDR_WRITE_WAIT  = 3'h1;
	localparam[2:0] S_DATA_WRITE_EXEC  = 3'h2;
	localparam[2:0] S_DATA_WRITE_WAIT  = 3'h3;
	localparam[2:0] S_DATA_READ_EXEC   = 3'h4;
	localparam[2:0] S_DATA_READ_WAIT   = 3'h5;

	// State and next-state
	reg[2:0] state_next, state         = S_IDLE;
	
	// Synchronised versions of asynchronous inputs
	reg      eppAddrStb_sync           = 1'b1;
	reg      eppDataStb_sync           = 1'b1;
	reg      eppWrite_sync             = 1'b1;
	
	// Registers
	reg      eppWait_next, eppWait     = 1'b0;
	reg[6:0] chanAddr_next, chanAddr   = 7'b0000000;
	reg[7:0] eppData_next, eppData     = 8'h00;

	// Infer registers
	always @(posedge eppClk_in)
	begin
		state <= state_next;
		chanAddr <= chanAddr_next;
		eppData <= eppData_next;
		eppWait <= eppWait_next;
		eppAddrStb_sync <= eppAddrStb_in;
		eppDataStb_sync <= eppDataStb_in;
		eppWrite_sync <= eppWrite_in;
	end

	// Next state logic
	always @*
	begin
		state_next = state;
		chanAddr_next = chanAddr;
		eppWait_next = eppWait;
		eppData_next = eppData;
		h2fData_out = 8'h00;
		f2hReady_out = 1'b0;
		h2fValid_out = 1'b0;

		case ( state )
			// Finish the address update cycle
			S_ADDR_WRITE_WAIT:
				begin
					if ( eppAddrStb_sync == 1'b1 )
						begin
							eppWait_next = 1'b0;
							state_next = S_IDLE;
						end
				end

			// Host writes a byte to the FPGA
			S_DATA_WRITE_EXEC:
				begin
					h2fData_out = eppData_io;
					h2fValid_out = 1'b1;
					if ( h2fReady_in == 1'b1 )
						begin
							eppWait_next = 1'b1;
							state_next = S_DATA_WRITE_WAIT;
						end
				end
			S_DATA_WRITE_WAIT:
				begin
					if ( eppDataStb_sync == 1'b1 )
						begin
							eppWait_next = 1'b0;
							state_next = S_IDLE;
						end
				end

			// Host reads a byte from the FPGA
			S_DATA_READ_EXEC:
				begin
					eppData_next = f2hData_in;
					f2hReady_out = 1'b1;
					if ( f2hValid_in == 1'b1 )
						begin
							eppWait_next = 1'b1;
							state_next = S_DATA_READ_WAIT;
						end
				end
			S_DATA_READ_WAIT:
				begin
					if ( eppDataStb_sync == 1'b1 )
						begin
							eppWait_next = 1'b0;
							state_next = S_IDLE;
						end
				end

			// S_IDLE and others
			default:
				begin
					eppWait_next = 1'b0;
					if ( eppAddrStb_sync == 1'b0 )
						begin
							// Address can only be written, not read
							if ( eppWrite_sync == 1'b0 )
								begin
									eppWait_next = 1'b1;
									chanAddr_next = eppData_io[6:0];
									state_next = S_ADDR_WRITE_WAIT;
								end
						end
					else if ( eppDataStb_sync == 1'b0 )
						begin
							// Register read or write
							if ( eppWrite_sync == 1'b0 )
								state_next = S_DATA_WRITE_EXEC;
							else
								state_next = S_DATA_READ_EXEC;
						end
				end
		endcase
	end

	// Drive stateless signals
	assign chanAddr_out = chanAddr;
	assign eppWait_out = eppWait;
	assign eppData_io = (eppWrite_in == 1'b1) ? eppData : 8'hZZ;
endmodule

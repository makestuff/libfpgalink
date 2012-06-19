--
-- Copyright (C) 2009-2012 Chris McClelland
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--
library ieee;

use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity comm_fpga_fx2 is
	port(
		-- FX2 interface -----------------------------------------------------------------------------
		fx2Clk_in      : in    std_logic;                     -- 48MHz clock from FX2
		fx2FifoSel_out : out   std_logic;                     -- select FIFO: '0' for EP6OUT, '1' for EP8IN
		fx2Data_io     : inout std_logic_vector(7 downto 0);  -- 8-bit data to/from FX2

		-- When EP6OUT selected:
		fx2Read_out    : out   std_logic;                     -- asserted (active-low) when reading from FX2
		fx2GotData_in  : in    std_logic;                     -- asserted (active-high) when FX2 has data for us

		-- When EP8IN selected:
		fx2Write_out   : out   std_logic;                     -- asserted (active-low) when writing to FX2
		fx2GotRoom_in  : in    std_logic;                     -- asserted (active-high) when FX2 has room for more data from us
		fx2PktEnd_out  : out   std_logic;                     -- asserted (active-low) when a host read needs to be committed early

		-- Channel read/write interface --------------------------------------------------------------
		chanAddr_out   : out   std_logic_vector(6 downto 0);  -- the selected channel (0-127)

		-- Host >> FPGA pipe:
		h2fData_out    : out   std_logic_vector(7 downto 0);  -- data lines used when the host writes to a channel
		h2fValid_out   : out   std_logic;                     -- '1' means "on the next clock rising edge, please accept the data on h2fData_out"
		h2fReady_in    : in    std_logic;                     -- channel logic can drive this low to say "I'm not ready for more data yet"

		-- Host << FPGA pipe:
		f2hData_in     : in    std_logic_vector(7 downto 0);  -- data lines used when the host reads from a channel
		f2hValid_in    : in    std_logic;                     -- channel logic can drive this low to say "I don't have data ready for you"
		f2hReady_out   : out   std_logic                      -- '1' means "on the next clock rising edge, put your next byte of data on f2hData_in"
	);
end comm_fpga_fx2;

architecture behavioural of comm_fpga_fx2 is
	-- The read/write nomenclature here refers to the FPGA reading and writing the FX2 FIFOs, and is therefore
	-- of the opposite sense to the host's read and write. So host reads are fulfilled in the S_WRITE state, and
	-- vice-versa. Apologies for the confusion.
	type StateType is (
		S_IDLE,                  -- wait for requst from host & register chanAddr & isWrite
		S_GET_COUNT0,            -- register most significant byte of message length
		S_GET_COUNT1,            -- register next byte of message length
		S_GET_COUNT2,            -- register next byte of message length
		S_GET_COUNT3,            -- register least significant byte of message length
		S_BEGIN_WRITE,           -- switch direction of FX2 data bus
		S_WRITE,                 -- write data to FX2 EP8IN FIFO, one byte at a time
		S_END_WRITE_ALIGNED,     -- end an aligned write (do not assert fx2PktEnd_out)
		S_END_WRITE_NONALIGNED,  -- end a nonaligned write (assert fx2PktEnd_out)
		S_READ                   -- read data from FX2 EP6OUT FIFO, one byte at a time
	);
	constant FIFO_READ               : std_logic_vector(1 downto 0) := "10";             -- assert fx2Read_out (active-low)
	constant FIFO_WRITE              : std_logic_vector(1 downto 0) := "01";             -- assert fx2Write_out (active-low)
	constant FIFO_NOP                : std_logic_vector(1 downto 0) := "11";             -- assert nothing
	constant OUT_FIFO                : std_logic                    := '0';              -- EP6OUT
	constant IN_FIFO                 : std_logic                    := '1';              -- EP8IN
	signal state, state_next         : StateType                    := S_IDLE;
	signal fifoOp                    : std_logic_vector(1 downto 0) := FIFO_NOP;
	signal count, count_next         : unsigned(31 downto 0)        := (others => '0');  -- read/write count
	signal chanAddr, chanAddr_next   : std_logic_vector(6 downto 0) := (others => '0');  -- channel being accessed (0-127)
	signal isWrite, isWrite_next     : std_logic                    := '0';              -- is this access is an FX2 FIFO write or a read?
	signal isAligned, isAligned_next : std_logic                    := '0';              -- is this FX2 FIFO write block-aligned?
	signal dataOut                   : std_logic_vector(7 downto 0);                     -- data to be driven on fx2Data_io
	signal driveBus                  : std_logic;                                        -- whether or not to drive fx2Data_io
begin
	-- Infer registers
	process(fx2Clk_in)
	begin
		if ( rising_edge(fx2Clk_in) ) then
			state <= state_next;
			count <= count_next;
			chanAddr <= chanAddr_next;
			isWrite <= isWrite_next;
			isAligned <= isAligned_next;
		end if;
	end process;

	-- Next state logic
	process(
		state, fx2Data_io, fx2GotData_in, fx2GotRoom_in, count, isAligned, isWrite, chanAddr,
		f2hData_in, f2hValid_in, h2fReady_in)
	begin
		state_next <= state;
		count_next <= count;
		chanAddr_next <= chanAddr;
		isWrite_next <= isWrite;      -- is the FPGA writing to the FX2?
		isAligned_next <= isAligned;  -- does this FIFO write end on a block (512-byte) boundary?
		dataOut <= (others => '0');
		driveBus <= '0';              -- don't drive fx2Data_io by default
		fifoOp <= FIFO_READ;          -- read the FX2 FIFO by default
		fx2PktEnd_out <= '1';         -- inactive: FPGA does not commit a short packet.
		f2hReady_out <= '0';
		h2fValid_out <= '0';

		case state is
			when S_GET_COUNT0 =>
				fx2FifoSel_out <= OUT_FIFO;  -- Reading from FX2
				if ( fx2GotData_in = '1' ) then
					-- The count high word high byte will be available on the next clock edge.
					count_next(31 downto 24) <= unsigned(fx2Data_io);
					state_next <= S_GET_COUNT1;
				end if;

			when S_GET_COUNT1 =>
				fx2FifoSel_out <= OUT_FIFO;  -- Reading from FX2
				if ( fx2GotData_in = '1' ) then
					-- The count high word low byte will be available on the next clock edge.
					count_next(23 downto 16) <= unsigned(fx2Data_io);
					state_next <= S_GET_COUNT2;
				end if;

			when S_GET_COUNT2 =>
				fx2FifoSel_out <= OUT_FIFO;  -- Reading from FX2
				if ( fx2GotData_in = '1' ) then
					-- The count low word high byte will be available on the next clock edge.
					count_next(15 downto 8) <= unsigned(fx2Data_io);
					state_next <= S_GET_COUNT3;
				end if;

			when S_GET_COUNT3 =>
				fx2FifoSel_out <= OUT_FIFO;  -- Reading from FX2
				if ( fx2GotData_in = '1' ) then
					-- The count low word low byte will be available on the next clock edge.
					count_next(7 downto 0) <= unsigned(fx2Data_io);
					if ( isWrite = '1' ) then
						state_next <= S_BEGIN_WRITE;
					else
						state_next <= S_READ;
					end if;
				end if;

			when S_BEGIN_WRITE =>
				fx2FifoSel_out <= IN_FIFO;   -- Writing to FX2
				fifoOp <= FIFO_NOP;
				if ( count(8 downto 0) = "000000000" ) then
					isAligned_next <= '1';
				else
					isAligned_next <= '0';
				end if;
				state_next <= S_WRITE;

			when S_WRITE =>
				fx2FifoSel_out <= IN_FIFO;   -- Writing to FX2
				if ( fx2GotRoom_in = '1' ) then
					f2hReady_out <= '1';
				end if;
				if ( fx2GotRoom_in = '1' and f2hValid_in = '1' ) then
					fifoOp <= FIFO_WRITE;
					dataOut <= f2hData_in;
					driveBus <= '1';
					count_next <= count - 1;
					if ( count = 1 ) then
						if ( isAligned = '1' ) then
							state_next <= S_END_WRITE_ALIGNED;  -- don't assert fx2PktEnd
						else
							state_next <= S_END_WRITE_NONALIGNED;  -- assert fx2PktEnd to commit small packet
						end if;
					end if;
				else
					fifoOp <= FIFO_NOP;
				end if;

			when S_END_WRITE_ALIGNED =>
				fx2FifoSel_out <= IN_FIFO;   -- Writing to FX2
				fifoOp <= FIFO_NOP;
				state_next <= S_IDLE;

			when S_END_WRITE_NONALIGNED =>
				fx2FifoSel_out <= IN_FIFO;   -- Writing to FX2
				fifoOp <= FIFO_NOP;
				fx2PktEnd_out <= '0';        -- Active: FPGA commits the packet early.
				state_next <= S_IDLE;

			when S_READ =>
				fx2FifoSel_out <= OUT_FIFO;  -- Reading from FX2
				if ( fx2GotData_in = '1' and h2fReady_in = '1') then
					-- A data byte will be available on the next clock edge
					h2fValid_out <= '1';
					count_next <= count - 1;
					if ( count = 1 ) then
						state_next <= S_IDLE;
					end if;
				else
					fifoOp <= FIFO_NOP;
				end if;

			-- S_IDLE and others
			when others =>
				fx2FifoSel_out <= OUT_FIFO;  -- Reading from FX2
				if ( fx2GotData_in = '1' ) then
					-- The read/write flag and a seven-bit channel address will be available on the
					-- next clock edge.
					chanAddr_next <= fx2Data_io(6 downto 0);
					isWrite_next <= fx2Data_io(7);
					state_next <= S_GET_COUNT0;
				end if;
		end case;
	end process;

	-- Drive stateless signals
	fx2Read_out <= fifoOp(0);
	fx2Write_out <= fifoOp(1);
	chanAddr_out <= chanAddr;
	h2fData_out <= fx2Data_io;
	fx2Data_io <= dataOut when driveBus = '1' else (others => 'Z');
end behavioural;

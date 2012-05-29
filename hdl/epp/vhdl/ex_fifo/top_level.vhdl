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

entity top_level is
	port(
		-- EPP interface -----------------------------------------------------------------------------
		eppClk_in     : in    std_logic;
		eppData_io    : inout std_logic_vector(7 downto 0);
		eppAddrStb_in : in    std_logic;
		eppDataStb_in : in    std_logic;
		eppWrite_in   : in    std_logic;
		eppWait_out   : out   std_logic;

		-- Onboard peripherals -----------------------------------------------------------------------
		sseg_out      : out   std_logic_vector(7 downto 0); -- seven-segment display cathodes (one for each segment)
		anode_out     : out   std_logic_vector(3 downto 0); -- seven-segment display anodes (one for each digit)
		led_out       : out   std_logic_vector(7 downto 0); -- eight LEDs
		sw_in         : in    std_logic_vector(7 downto 0)  -- eight switches
	);
end top_level;

architecture behavioural of top_level is
	-- Channel read/write interface -----------------------------------------------------------------
	signal chanAddr  : std_logic_vector(6 downto 0);  -- the selected channel (0-127)

	-- Host >> FPGA pipe:
	signal h2fData   : std_logic_vector(7 downto 0);  -- data lines used when the host writes to a channel
	signal h2fValid  : std_logic;                     -- '1' means "on the next clock rising edge, please accept the data on h2fData"
	signal h2fReady  : std_logic;                     -- channel logic can drive this low to say "I'm not ready for more data yet"

	-- Host << FPGA pipe:
	signal f2hData   : std_logic_vector(7 downto 0);  -- data lines used when the host reads from a channel
	signal f2hValid  : std_logic;                     -- channel logic can drive this low to say "I don't have data ready for you"
	signal f2hReady  : std_logic;                     -- '1' means "on the next clock rising edge, put your next byte of data on f2hData"
	-- ----------------------------------------------------------------------------------------------

	-- Flags for display on the 7-seg decimal points
	signal flags             : std_logic_vector(3 downto 0);

	-- FIFOs
	signal fifoCount         : std_logic_vector(15 downto 0); -- MSB=writeFifo, LSB=readFifo

	-- Write FIFO:
	signal writeFifoDataIn   : std_logic_vector(7 downto 0);  -- producer: data
	signal writeFifoWrite    : std_logic;                     -- producer: write enable
	signal writeFifoFull     : std_logic;                     -- producer: full flag
	signal writeFifoDataOut  : std_logic_vector(7 downto 0);  -- consumer: data
	signal writeFifoRead     : std_logic;                     -- consumer: write enable
	signal writeFifoEmpty    : std_logic;                     -- consumer: full flag

	-- Read FIFO:
	signal readFifoDataIn    : std_logic_vector(7 downto 0);  -- producer: data
	signal readFifoWrite     : std_logic;                     -- producer: write enable
	signal readFifoFull      : std_logic;                     -- producer: full flag
	signal readFifoDataOut   : std_logic_vector(7 downto 0);  -- consumer: data
	signal readFifoRead      : std_logic;                     -- consumer: write enable
	signal readFifoEmpty     : std_logic;                     -- consumer: full flag

	-- Counter which endlessly puts items in read FIFO for the host to read
	signal count, count_next : std_logic_vector(7 downto 0) := (others => '0');
begin
	-- Infer registers
	process(eppClk_in)
	begin
		if ( rising_edge(eppClk_in) ) then
			count <= count_next;
		end if;
	end process;

	-- Wire up write FIFO to channel 0:
	writeFifoDataIn <= h2fData;
	writeFifoWrite <=
		'1' when h2fValid = '1' and chanAddr = "0000000"
		else '0';
	h2fReady <=
		'0' when writeFifoFull = '1' and chanAddr = "0000000"
		else '1';

	-- Wire up read FIFO to channel 0:
	readFifoDataIn <= count;
	readFifoRead <=
		'1' when f2hReady = '1' and chanAddr = "0000000"
		else '0';
	f2hValid <=
		'0' when readFifoEmpty = '1' and chanAddr = "0000000"
		else '1';
	count_next <=
		std_logic_vector(unsigned(count) + 1) when readFifoWrite = '1'
		else count;
	
	-- Select values to return for each channel when the host is reading
	with chanAddr select f2hData <=
		readFifoDataOut        when "0000000",  -- get data from the read FIFO
		fifoCount(15 downto 8) when "0000001",  -- read the current depth of the write FIFO
		fifoCount(7 downto 0)  when "0000010",  -- read the current depth of the write FIFO
		x"00" when others;
	
	-- CommFPGA module
	comm_fpga : entity work.comm_fpga
		port map(
			-- EPP interface
			eppClk_in      => eppClk_in,
			eppData_io     => eppData_io,
			eppAddrStb_in  => eppAddrStb_in,
			eppDataStb_in  => eppDataStb_in,
			eppWrite_in    => eppWrite_in,
			eppWait_out    => eppWait_out,

			-- Channel read/write interface
			chanAddr_out   => chanAddr,
			f2hData_in     => f2hData,
			f2hReady_out   => f2hReady,
			f2hValid_in    => f2hValid,
			h2fData_out    => h2fData,
			h2fValid_out   => h2fValid,
			h2fReady_in    => h2fReady
		);

	-- Write FIFO: written by host, read by leds
	write_fifo : entity work.fifo
		port map(
			clk        => eppClk_in,
			data_count => fifoCount(15 downto 8),

			-- Production end
			din        => writeFifoDataIn,
			wr_en      => writeFifoWrite,
			full       => writeFifoFull,

			-- Consumption end
			dout       => writeFifoDataOut,
			rd_en      => writeFifoRead,
			empty      => writeFifoEmpty
		);
	
	-- Read FIFO: written by counter, read by host
	read_fifo : entity work.fifo
		port map(
			clk        => eppClk_in,
			data_count => fifoCount(7 downto 0),

			-- Production end
			din        => readFifoDataIn,
			wr_en      => readFifoWrite,
			full       => readFifoFull,

			-- Consumption end
			dout       => readFifoDataOut,
			rd_en      => readFifoRead,
			empty      => readFifoEmpty
		);

	-- Producer timer
	producer_timer : entity work.timer
		port map(
			clk_in     => eppClk_in,
			ceiling_in => sw_in(3 downto 0),
			tick_out   => readFifoWrite
		);

	-- Consumer timer
	consumer_timer : entity work.timer
		port map(
			clk_in     => eppClk_in,
			ceiling_in => sw_in(7 downto 4),
			tick_out   => writeFifoRead
		);

	-- LEDs and 7-seg display
	led_out <= writeFifoDataOut;
	flags <= '0' & writeFifoEmpty & '0' & readFifoFull;
	seven_seg : entity work.seven_seg
		port map(
			clk_in     => eppClk_in,
			data_in    => fifoCount,
			dots_in    => flags,
			segs_out   => sseg_out,
			anodes_out => anode_out
		);
end behavioural;

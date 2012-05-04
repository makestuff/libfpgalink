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
		-- FX2 interface
		fx2Clk_in     : in    std_logic;                    -- 48MHz clock from FX2
		fx2Addr_out   : out   std_logic_vector(1 downto 0); -- select FIFO: "10" for EP6OUT, "11" for EP8IN
		fx2Data_io    : inout std_logic_vector(7 downto 0); -- 8-bit data to/from FX2
		fx2Read_out   : out   std_logic;                    -- asserted (active-low) when reading from FX2
		fx2OE_out     : out   std_logic;                    -- asserted (active-low) to tell FX2 to drive bus
		fx2GotData_in : in    std_logic;                    -- asserted (active-high) when FX2 has data for us
		fx2Write_out  : out   std_logic;                    -- asserted (active-low) when writing to FX2
		fx2GotRoom_in : in    std_logic;                    -- asserted (active-high) when FX2 has room for more data from us
		fx2PktEnd_out : out   std_logic;                    -- asserted (active-low) when a host read needs to be committed early

		-- Onboard peripherals
		sseg_out      : out   std_logic_vector(7 downto 0); -- seven-segment display cathodes (one for each segment)
		anode_out     : out   std_logic_vector(3 downto 0); -- seven-segment display anodes (one for each digit)
		led_out       : out   std_logic_vector(7 downto 0); -- eight LEDs
		sw_in         : in    std_logic_vector(7 downto 0)  -- eight switches
	);
end top_level;

architecture behavioural of top_level is
	-- Channel read/write interface:
	signal chanAddr                : std_logic_vector(6 downto 0);  -- the selected channel (0-127)
	signal chanDataIn              : std_logic_vector(7 downto 0);  -- data lines used when the host reads from a channel
	signal chanRead                : std_logic;                     -- '1' means "on the next clock rising edge, put your next byte of data on chanData_in"
	signal chanGotData             : std_logic;                     -- channel logic can drive this low to say "I don't have data ready for you"
	signal chanDataOut             : std_logic_vector(7 downto 0);  -- data lines used when the host writes to a channel
	signal chanWrite               : std_logic;                     -- '1' means "on the next clock rising edge, please accept the data on chanData_out"
	signal chanGotRoom             : std_logic;                     -- channel logic can drive this low to say "I'm not ready for more data yet"

	-- Needed so that the comm_fpga module can drive both fx2Read_out and fx2OE_out
	signal fx2Read                 : std_logic;

	-- Flags for display on the 7-seg decimal points
	signal flags                   : std_logic_vector(3 downto 0);

	-- FIFOs
	signal fifoCount               : std_logic_vector(15 downto 0); -- MSB=writeFifo, LSB=readFifo

	-- Write FIFO:
	signal writeFifoDataIn         : std_logic_vector(7 downto 0);  -- producer: data
	signal writeFifoWrite          : std_logic;                     -- producer: write enable
	signal writeFifoFull           : std_logic;                     -- producer: full flag
	signal writeFifoDataOut        : std_logic_vector(7 downto 0);  -- consumer: data
	signal writeFifoRead           : std_logic;                     -- consumer: write enable
	signal writeFifoEmpty          : std_logic;                     -- consumer: full flag

	-- Read FIFO:
	signal readFifoDataIn          : std_logic_vector(7 downto 0);  -- producer: data
	signal readFifoWrite           : std_logic;                     -- producer: write enable
	signal readFifoFull            : std_logic;                     -- producer: full flag
	signal readFifoDataOut         : std_logic_vector(7 downto 0);  -- consumer: data
	signal readFifoRead            : std_logic;                     -- consumer: write enable
	signal readFifoEmpty           : std_logic;                     -- consumer: full flag

	-- Counter which endlessly puts items in read FIFO for the host to read
	signal count, count_next : std_logic_vector(7 downto 0) := (others => '0');
begin
	-- Infer registers
	process(fx2Clk_in)
	begin
		if ( rising_edge(fx2Clk_in) ) then
			count <= count_next;
		end if;
	end process;

	-- Wire up write FIFO to channel 0:
	writeFifoDataIn <= chanDataOut;
	writeFifoWrite <=
		'1' when chanWrite = '1' and chanAddr = "0000000"
		else '0';
	chanGotRoom <=
		'0' when writeFifoFull = '1' and chanAddr = "0000000"
		else '1';

	-- Wire up read FIFO to channel 0:
	readFifoDataIn <= count;
	readFifoRead <=
		'1' when chanRead = '1' and chanAddr = "0000000"
		else '0';
	chanGotData <=
		'0' when readFifoEmpty = '1' and chanAddr = "0000000"
		else '1';
	count_next <=
		std_logic_vector(unsigned(count) + 1) when readFifoWrite = '1'
		else count;
	
	-- Select values to return for each channel when the host is reading
	with chanAddr select chanDataIn <=
		readFifoDataOut        when "0000000",  -- get data from the read FIFO
		fifoCount(15 downto 8) when "0000001",  -- read the current depth of the write FIFO
		fifoCount(7 downto 0)  when "0000010",  -- read the current depth of the write FIFO
		x"00" when others;
	
	-- CommFPGA module
	fx2Read_out <= fx2Read;
	fx2OE_out <= fx2Read;
	fx2Addr_out(1) <= '1';  -- Use EP6OUT/EP8IN, not EP2OUT/EP4IN.
	comm_fpga : entity work.comm_fpga
		port map(
			-- FX2 interface
			fx2Clk_in      => fx2Clk_in,
			fx2FifoSel_out => fx2Addr_out(0),
			fx2Data_io     => fx2Data_io,
			fx2Read_out    => fx2Read,
			fx2GotData_in  => fx2GotData_in,
			fx2Write_out   => fx2Write_out,
			fx2GotRoom_in  => fx2GotRoom_in,
			fx2PktEnd_out  => fx2PktEnd_out,

			-- Channel read/write interface
			chanAddr_out   => chanAddr,
			chanData_in    => chanDataIn,
			chanRead_out   => chanRead,
			chanGotData_in => chanGotData,
			chanData_out   => chanDataOut,
			chanWrite_out  => chanWrite,
			chanGotRoom_in => chanGotRoom
		);

	-- Write FIFO: written by host, read by leds
	write_fifo : entity work.fifo
		port map(
			clk        => fx2Clk_in,
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
			clk        => fx2Clk_in,
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
			clk_in     => fx2Clk_in,
			ceiling_in => sw_in(3 downto 0),
			tick_out   => readFifoWrite
		);

	-- Consumer timer
	consumer_timer : entity work.timer
		port map(
			clk_in     => fx2Clk_in,
			ceiling_in => sw_in(7 downto 4),
			tick_out   => writeFifoRead
		);

	-- LEDs and 7-seg display
	led_out <= writeFifoDataOut;
	flags <= '0' & writeFifoEmpty & '0' & readFifoFull;
	seven_seg : entity work.seven_seg
		port map(
			clk_in     => fx2Clk_in,
			data_in    => fifoCount,
			dots_in    => flags,
			segs_out   => sseg_out,
			anodes_out => anode_out
		);
end behavioural;

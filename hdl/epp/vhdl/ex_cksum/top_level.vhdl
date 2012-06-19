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
	signal flags                   : std_logic_vector(3 downto 0);

	-- Registers implementing the channels
	signal checksum, checksum_next : std_logic_vector(15 downto 0) := x"0000";
	signal reg0, reg0_next         : std_logic_vector(7 downto 0)  := x"00";
	signal reg1, reg1_next         : std_logic_vector(7 downto 0)  := x"00";
	signal reg2, reg2_next         : std_logic_vector(7 downto 0)  := x"00";
	signal reg3, reg3_next         : std_logic_vector(7 downto 0)  := x"00";
begin
	-- Infer registers
	process(eppClk_in)
	begin
		if ( rising_edge(eppClk_in) ) then
			checksum <= checksum_next;
			reg0 <= reg0_next;
			reg1 <= reg1_next;
			reg2 <= reg2_next;
			reg3 <= reg3_next;
		end if;
	end process;

	-- Drive register inputs for each channel when the host is writing
	checksum_next <=
		std_logic_vector(unsigned(checksum) + unsigned(h2fData)) when chanAddr = "0000000" and h2fValid = '1'
		else x"0000" when chanAddr = "0000001" and h2fValid = '1' and h2fData(0) = '1'
		else checksum;
	reg0_next <= h2fData when chanAddr = "0000000" and h2fValid = '1' else reg0;
	reg1_next <= h2fData when chanAddr = "0000001" and h2fValid = '1' else reg1;
	reg2_next <= h2fData when chanAddr = "0000010" and h2fValid = '1' else reg2;
	reg3_next <= h2fData when chanAddr = "0000011" and h2fValid = '1' else reg3;
	
	-- Select values to return for each channel when the host is reading
	with chanAddr select f2hData <=
		sw_in when "0000000",
		reg1  when "0000001",
		reg2  when "0000010",
		reg3  when "0000011",
		x"00" when others;

	-- Assert that there's always data for reading, and always room for writing
	f2hValid <= '1';
	h2fReady <= '1';

	-- CommFPGA module
	comm_fpga_epp : entity work.comm_fpga_epp
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
			h2fData_out    => h2fData,
			h2fValid_out   => h2fValid,
			h2fReady_in    => h2fReady,
			f2hData_in     => f2hData,
			f2hValid_in    => f2hValid,
			f2hReady_out   => f2hReady
		);

	-- LEDs and 7-seg display
	led_out <= reg0;
	flags <= "000" & f2hReady;
	seven_seg : entity work.seven_seg
		port map(
			clk_in     => eppClk_in,
			data_in    => checksum,
			dots_in    => flags,
			segs_out   => sseg_out,
			anodes_out => anode_out
		);
end behavioural;

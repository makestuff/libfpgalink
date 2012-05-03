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
		fx2Clk_in     : in    std_logic;
		fx2Data_io    : inout std_logic_vector(7 downto 0);
		fx2GotData_in : in    std_logic;                    -- FLAGC=EF (active-low), so '1' when there's data
		fx2GotRoom_in : in    std_logic;                    -- FLAGB=FF (active-low), so '1' when there's room
		fx2Read_out   : out   std_logic;                    -- PA2
		fx2OE_out     : out   std_logic;
		fx2Write_out  : out   std_logic;
		fx2Addr_out   : out   std_logic_vector(1 downto 0); -- PA4 & PA5
		fx2PktEnd_out : out   std_logic;                    -- PA6

		-- Onboard peripherals
		sseg_out      : out std_logic_vector(7 downto 0);
		anode_out     : out std_logic_vector(3 downto 0);
		led_out       : out std_logic_vector(7 downto 0);
		sw_in         : in std_logic_vector(7 downto 0)
	);
end top_level;

architecture behavioural of top_level is
	signal checksum, checksum_next : std_logic_vector(15 downto 0) := x"0000";
	signal r0, r0_next             : std_logic_vector(7 downto 0) := x"00";
	signal r1, r1_next             : std_logic_vector(7 downto 0) := x"00";
	signal r2, r2_next             : std_logic_vector(7 downto 0) := x"00";
	signal r3, r3_next             : std_logic_vector(7 downto 0) := x"00";
	signal chanAddr                : std_logic_vector(6 downto 0);
	signal chanDataIn              : std_logic_vector(7 downto 0);
	signal chanDataOut             : std_logic_vector(7 downto 0);
	signal chanWrite               : std_logic;
	signal chanRead                : std_logic;
	signal chanGotData             : std_logic;
	signal chanGotRoom             : std_logic;
	signal fx2Read                 : std_logic;
	signal flags                   : std_logic_vector(3 downto 0);
begin
	-- Infer registers
	process(fx2Clk_in)
	begin
		if ( rising_edge(fx2Clk_in) ) then
			checksum <= checksum_next;
			r0 <= r0_next;
			r1 <= r1_next;
			r2 <= r2_next;
			r3 <= r3_next;
		end if;
	end process;

	-- Drive register inputs for when the host is writing
	checksum_next <=
		std_logic_vector(unsigned(checksum) + unsigned(chanDataOut)) when chanAddr = "0000000" and chanWrite = '1'
		else x"0000" when chanAddr = "0000001" and chanWrite = '1' and chanDataOut(0) = '1'
		else checksum;
	r0_next <= chanDataOut when chanAddr = "0000000" and chanWrite = '1' else r0;
	r1_next <= chanDataOut when chanAddr = "0000001" and chanWrite = '1' else r1;
	r2_next <= chanDataOut when chanAddr = "0000010" and chanWrite = '1' else r2;
	r3_next <= chanDataOut when chanAddr = "0000011" and chanWrite = '1' else r3;
	
	-- Select values to return for each channel when the host is reading
	with chanAddr select chanDataIn <=
		sw_in when "0000000",
		r1    when "0000001",
		r2    when "0000010",
		r3    when "0000011",
		x"00" when others;

	-- Assert that there's always data for reading, and always room for writing
	chanGotData <= '1';
	chanGotRoom <= '1';

	-- CommFPGA module
	fx2Read_out <= fx2Read;
	fx2OE_out <= fx2Read;
	fx2Addr_out(1) <= '1';  -- Use EP6OUT/EP8IN, not EP2OUT/EP4IN.
	comm_fpga : entity work.comm_fpga
		port map(
			-- FX2 interface
			fx2Clk_in      => fx2Clk_in,
			fx2Data_io     => fx2Data_io,
			fx2GotData_in  => fx2GotData_in,
			fx2GotRoom_in  => fx2GotRoom_in,
			fx2Read_out    => fx2Read,
			fx2Write_out   => fx2Write_out,
			fx2FifoSel_out => fx2Addr_out(0),
			fx2PktEnd_out  => fx2PktEnd_out,

			-- Channel read/write interface
			chanAddr_out   => chanAddr,
			chanData_in    => chanDataIn,
			chanData_out   => chanDataOut,
			chanRead_out   => chanRead,
			chanWrite_out  => chanWrite,
			chanGotData_in => chanGotData,
			chanGotRoom_in => chanGotROom
		);

	-- LEDs and 7-seg display
	led_out <= r0;
	flags <= "000" & chanRead;
	seven_seg : entity work.seven_seg
		port map(
			clk_in     => fx2Clk_in,
			data_in    => checksum,
			dots_in    => flags,
			segs_out   => sseg_out,
			anodes_out => anode_out
		);
end behavioural;

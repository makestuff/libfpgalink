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

entity seven_seg is
	generic (
		-- This can be overridden to change the refresh rate. The anode pattern will change at a
		-- frequency given by F(clk_in) / (2**COUNTER_WIDTH). So for a 50MHz clk_in and
		-- COUNTER_WIDTH=18, the anode pattern changes at ~191Hz, which means each digit gets
		-- refreshed at ~48Hz.
		COUNTER_WIDTH : integer := 18
	);
	port(
		clk_in     : in  std_logic;
		data_in    : in  std_logic_vector(15 downto 0);
		dots_in    : in  std_logic_vector(3 downto 0);
		segs_out   : out std_logic_vector(7 downto 0);
		anodes_out : out std_logic_vector(3 downto 0)
	);
end seven_seg;

architecture behavioural of seven_seg is
	signal count       : unsigned(COUNTER_WIDTH-1 downto 0) := (others => '0');
	signal count_next  : unsigned(COUNTER_WIDTH-1 downto 0);
	signal anodeSelect : std_logic_vector(1 downto 0);
	signal nibble      : std_logic_vector(3 downto 0);
	signal segs        : std_logic_vector(6 downto 0);
	signal dot         : std_logic;
begin
	-- Infer counter register
	process(clk_in)
	begin
		if ( rising_edge(clk_in) ) then
			count <= count_next;
		end if;
	end process;

	-- Increment counter and derive anode select from top two bits
	count_next <= count + 1;
	anodeSelect <= std_logic_vector(count(COUNTER_WIDTH-1 downto COUNTER_WIDTH-2));

	-- Drive anodes
	with anodeSelect select anodes_out <= 
		"0111" when "00",
		"1011" when "01",
		"1101" when "10",
		"1110" when others;

	-- Select the appropriate bit from dots_in
	with anodeSelect select dot <=
		not(dots_in(3)) when "00",
		not(dots_in(2)) when "01",
		not(dots_in(1)) when "10",
		not(dots_in(0)) when others;

	-- Choose a nibble to display
	with anodeSelect select nibble <= 
		data_in(15 downto 12) when "00",
		data_in(11 downto 8)  when "01",
		data_in(7 downto 4)   when "10",
		data_in(3 downto 0)   when others;
	
	-- Decode chosen nibble
	with nibble select segs <=
		"1000000" when "0000",
		"1111001" when "0001",
		"0100100" when "0010",
		"0110000" when "0011",
		"0011001" when "0100",
		"0010010" when "0101",
		"0000010" when "0110",
		"1111000" when "0111",
		"0000000" when "1000",
		"0010000" when "1001",
		"0001000" when "1010",
		"0000011" when "1011",
		"1000110" when "1100",
		"0100001" when "1101",
		"0000110" when "1110",
		"0001110" when others;

	-- Drive segs_out
	segs_out <= dot & segs;
end behavioural;

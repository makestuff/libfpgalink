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

entity timer is
	generic (
		-- This gives the number of bits to be counted when ceiling_in is zero
		COUNTER_WIDTH : integer := 25;
		-- This gives the number of bits in the ceiling value
		CEILING_WIDTH : integer := 4
	);
	port(
		clk_in     : in  std_logic;
		ceiling_in : in  std_logic_vector(CEILING_WIDTH-1 downto 0);
		tick_out   : out std_logic
	);
end timer;

architecture behavioural of timer is
	constant TOP_BIT : natural := 2**CEILING_WIDTH - 1;
	function reverse(fwd : in std_logic_vector) return std_logic_vector is
		variable result : std_logic_vector(TOP_BIT downto 0);
	begin
		for i in result'range loop
			result(i) := fwd(COUNTER_WIDTH-i);
		end loop;
		return result;
	end;
	signal count, count_next : std_logic_vector(COUNTER_WIDTH downto 0) := (others => '0');
	signal revCount : std_logic_vector(TOP_BIT downto 0);
begin
	-- Infer registers
	process(clk_in)
	begin
		if ( rising_edge(clk_in) ) then
			count <= count_next;
		end if;
	end process;

	revCount <= reverse(count);
	
	process(count, revCount, ceiling_in)
	begin
		if ( revCount(to_integer(unsigned(ceiling_in))) = '0' ) then
			count_next <= std_logic_vector(unsigned(count) + 1);
			tick_out <= '0';
		else
			count_next <= (others => '0');
			tick_out <= '1';
		end if;
	end process;
end behavioural;

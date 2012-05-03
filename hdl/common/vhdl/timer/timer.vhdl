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
		-- This should be overridden by the inferring hardware or testbench!
		COUNTER_WIDTH : integer := 25;
		CEILING_WIDTH : integer := 4
	);
	port(
		clk_in     : in  std_logic;
		ceiling_in : in  std_logic_vector(CEILING_WIDTH-1 downto 0);
		tick_out   : out std_logic
	);
end timer;

architecture behavioural of timer is
	signal count, count_next : std_logic_vector(COUNTER_WIDTH downto 0) := (others => '0');
begin
	-- Infer registers
	process(clk_in)
	begin
		if ( rising_edge(clk_in) ) then
			count <= count_next;
		end if;
	end process;

	process(count, ceiling_in)
		variable index : integer range COUNTER_WIDTH-(2**CEILING_WIDTH) to COUNTER_WIDTH;
	begin
		index := COUNTER_WIDTH - to_integer(unsigned(ceiling_in));
		if ( count(index) = '0' ) then
			count_next <= std_logic_vector(unsigned(count) + 1);
			tick_out <= '0';
		else
			count_next <= (others => '0');
			tick_out <= '1';
		end if;
	end process;
end behavioural;

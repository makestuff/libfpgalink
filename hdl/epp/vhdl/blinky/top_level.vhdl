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
	generic (
		-- This can be overridden to change the refresh rate. The anode pattern will change at a
		-- frequency given by F(clk_in) / (2**COUNTER_WIDTH). So for a 50MHz clk_in and
		-- COUNTER_WIDTH=18, the anode pattern changes at ~191Hz, which means each digit gets
		-- refreshed at ~48Hz.
		COUNTER_WIDTH : integer := 27
	);
	port(
		sysClk_in : in  std_logic;
		led_out   : out std_logic_vector(1 downto 0)
	);
end top_level;

architecture behavioural of top_level is
	signal count       : unsigned(COUNTER_WIDTH-1 downto 0) := (others => '0');
	signal count_next  : unsigned(COUNTER_WIDTH-1 downto 0);
begin
	-- Infer registers
	process(sysClk_in)
	begin
		if ( rising_edge(sysClk_in) ) then
			count <= count_next;
		end if;
	end process;

	count_next <= count + 1;
	led_out <= std_logic_vector(count(COUNTER_WIDTH-1 downto COUNTER_WIDTH-2));

end behavioural;

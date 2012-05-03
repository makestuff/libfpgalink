--
-- Copyright (C) 2012 Chris McClelland
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--
library ieee;

use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_textio.all;
use std.textio.all;

entity seven_seg_tb is
end seven_seg_tb;

architecture behavioural of seven_seg_tb is
	constant COUNTER_WIDTH : integer := 4;
	signal sysClk          : std_logic;
	signal dispClk         : std_logic;  -- display version of sysClk, which leads it by 4ns
	signal data            : std_logic_vector(15 downto 0);
	signal dots            : std_logic_vector(3 downto 0);
	signal segs            : std_logic_vector(7 downto 0);
	signal anodes          : std_logic_vector(3 downto 0);
	signal dot             : std_logic;
begin
	-- Instantiate seven_seg for testing
	uut: entity work.seven_seg
		generic map(
			COUNTER_WIDTH => COUNTER_WIDTH
		)
		port map(
			clk_in     => sysClk,
			data_in    => data,
			dots_in    => dots,
			segs_out   => segs,
			anodes_out => anodes
		);

	-- Drive the clocks. In simulation, sysClk lags 4ns behind dispClk, to give a visual hold time for
	-- signals in GTKWave.
	process
	begin
		sysClk <= '0';
		dispClk <= '1';
		wait for 10 ns;
		dispClk <= '0';
		wait for 10 ns;		
		loop
			dispClk <= '1';
			wait for 4 ns;
			sysClk <= '1';
			wait for 6 ns;
			dispClk <= '0';
			wait for 4 ns;
			sysClk <= '0';
			wait for 6 ns;
		end loop;
	end process;

	-- Drive the seven_seg
	process
	begin
		data <= x"ABCD";
		dots <= "1010";
		wait for 324 ns;
		data <= x"FEDC";
		dots <= "0101";
		wait;
	end process;

	dot <= segs(7);

end architecture;

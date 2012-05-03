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

entity timer_tb is
end timer_tb;

architecture behavioural of timer_tb is
	constant COUNTER_WIDTH : integer := 4;
	constant CEILING_WIDTH : integer := 2;
	signal sysClk          : std_logic;
	signal dispClk         : std_logic;  -- display version of sysClk, which leads it by 4ns
	signal ceiling         : std_logic_vector(CEILING_WIDTH-1 downto 0);
	signal tick            : std_logic;
begin
	-- Instantiate timer for testing
	uut: entity work.timer
		generic map(
			COUNTER_WIDTH => COUNTER_WIDTH,
			CEILING_WIDTH => CEILING_WIDTH
		)
		port map(
			clk_in     => sysClk,
			ceiling_in => ceiling,
			tick_out   => tick
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

	-- Drive the timer
	process
	begin
		ceiling <= "00";
		wait until rising_edge(tick); wait until falling_edge(tick);
		wait until rising_edge(tick); wait until falling_edge(tick);
		ceiling <= "01";
		wait until rising_edge(tick); wait until falling_edge(tick);
		wait until rising_edge(tick); wait until falling_edge(tick);
		ceiling <= "10";
		wait until rising_edge(tick); wait until falling_edge(tick);
		wait until rising_edge(tick); wait until falling_edge(tick);
		ceiling <= "11";
		wait until rising_edge(tick); wait until falling_edge(tick);
		wait until rising_edge(tick); wait until falling_edge(tick);
		ceiling <= "00";
		wait;
	end process;

end architecture;

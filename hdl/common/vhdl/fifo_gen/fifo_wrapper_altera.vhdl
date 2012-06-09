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

entity fifo_wrapper is
	port(
		-- Clock and depth
		clk_in          : in  std_logic;
		depth_out       : out std_logic_vector(7 downto 0);

		-- Data is clocked into the FIFO on each clock edge where both valid & ready are high
		inputData_in    : in  std_logic_vector(7 downto 0);
		inputValid_in   : in  std_logic;
		inputReady_out  : out std_logic;

		-- Data is clocked out of the FIFO on each clock edge where both valid & ready are high
		outputData_out  : out std_logic_vector(7 downto 0);
		outputValid_out : out std_logic;
		outputReady_in  : in  std_logic
	);
end fifo_wrapper;

architecture behavioural of fifo_wrapper is
	signal inputFull   : std_logic;
	signal outputEmpty : std_logic;
begin
	-- Invert "full/empty" signals to give "ready/valid" signals
	inputReady_out <= not(inputFull);
	outputValid_out <= not(outputEmpty);

	-- The encapsulated FIFO
	fifo : entity work.fifo
		port map(
			clock      => clk_in,
			usedw      => depth_out,

			-- Production end
			data       => inputData_in,
			wrreq      => inputValid_in,
			full       => inputFull,

			-- Consumption end
			q          => outputData_out,
			empty      => outputEmpty,
			rdreq      => outputReady_in
		);
	
end behavioural;

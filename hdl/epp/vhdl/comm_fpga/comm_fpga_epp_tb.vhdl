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

entity comm_fpga_epp_tb is
end comm_fpga_epp_tb;

architecture behavioural of comm_fpga_epp_tb is
	-- Clocks
	signal sysClk     : std_logic;  -- main system clock
	signal dispClk    : std_logic;  -- display version of sysClk, which leads it by 4ns

	-- External interface ---------------------------------------------------------------------------
	signal eppClk     : std_logic;
	signal eppData    : std_logic_vector(7 downto 0);
	signal eppAddrStb : std_logic;
	signal eppDataStb : std_logic;
	signal eppWrite   : std_logic;
	signal eppWait    : std_logic;

	-- Channel read/write interface -----------------------------------------------------------------
	signal chanAddr   : std_logic_vector(6 downto 0);  -- comm_fpga_epp selects one of 128 channels to access

	-- Host >> FPGA pipe:
	signal h2fData    : std_logic_vector(7 downto 0);  -- data to be read from the selected channel
	signal h2fValid   : std_logic;  -- comm_fpga_epp drives h2fValid='1' when it wants to write to the selected channel
	signal h2fReady   : std_logic;  -- this must be driven high if the selected channel has room for data to be written to it

	-- Host << FPGA pipe:
	signal f2hData    : std_logic_vector(7 downto 0);  -- data to be written to the selected channel
	signal f2hValid   : std_logic;  -- this must be asserted if the selected channel has data available for reading
	signal f2hReady   : std_logic;  -- comm_fpga_epp drives f2hReady='1' when it wants to read from the selected channel
begin
	-- Instantiate comm_fpga_epp for testing
	uut: entity work.comm_fpga_epp
		port map(
			-- EPP interface --------------------------------------------------------------------------
			eppClk_in      => sysClk,
			eppData_io     => eppData,
			eppAddrStb_in  => eppAddrStb,
			eppDataStb_in  => eppDataStb,
			eppWrite_in    => eppWrite,
			eppWait_out    => eppWait,

			-- Channel read/write interface -----------------------------------------------------------
			chanAddr_out   => chanAddr,    -- which channel to connect the pipes to

			f2hData_in     => f2hData,     -- \
			f2hValid_in    => f2hValid,    --  Host >> FPGA pipe
			f2hReady_out   => f2hReady,    -- /

			h2fData_out    => h2fData,     -- \
			h2fValid_out   => h2fValid,    --  Host << FPGA pipe
			h2fReady_in    => h2fReady     -- /
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

	-- Drive the EPP side
	process
	begin
		eppData <= (others => 'Z');
		eppAddrStb <= '1';
		eppDataStb <= '1';
		eppWrite <= '1';
		wait for 55 ns;

		-- Do address write
		eppData <= x"55";
		eppWrite <= '0';
		wait for 5 ns;
		eppAddrStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppAddrStb <= '1';
		wait for 5 ns;
		eppWrite <= '1';
		eppData <= (others => 'Z');
		wait until eppWait = '0';
		wait for 5 ns;

		-- Do data write 1
		eppData <= x"12";
		eppWrite <= '0';
		wait for 5 ns;
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait for 5 ns;
		eppWrite <= '1';
		eppData <= (others => 'Z');
		wait until eppWait = '0';
		wait for 5 ns;
		
		-- Do data write 2
		eppData <= x"34";
		eppWrite <= '0';
		wait for 5 ns;
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait for 5 ns;
		eppWrite <= '1';
		eppData <= (others => 'Z');
		wait until eppWait = '0';
		wait for 5 ns;
		
		-- Do data write 3
		eppData <= x"56";
		eppWrite <= '0';
		wait for 5 ns;
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait for 5 ns;
		eppWrite <= '1';
		eppData <= (others => 'Z');
		wait until eppWait = '0';
		wait for 5 ns;
		
		-- Do data write 4
		eppData <= x"78";
		eppWrite <= '0';
		wait for 5 ns;
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait for 5 ns;
		eppWrite <= '1';
		eppData <= (others => 'Z');
		wait until eppWait = '0';
		wait for 5 ns;

		-- Do data read 1
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait until eppWait = '0';
		wait for 5 ns;

		-- Do data read 2
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait until eppWait = '0';
		wait for 5 ns;

		-- Do data read 3
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait until eppWait = '0';
		wait for 5 ns;

		-- Do data read 4
		eppDataStb <= '0';
		wait until eppWait = '1';
		wait for 5 ns;
		eppDataStb <= '1';
		wait until eppWait = '0';

		wait;
	end process;

	-- Drive the internal side
	process
	begin
		f2hValid <= '0';
		h2fReady <= '1';
		f2hData <= (others => 'Z');

		wait until h2fValid = '1';
		wait until h2fValid = '0';
		h2fReady <= '0';
		wait for 120 ns;
		h2fReady <= '1';

		wait for 400 ns;
		f2hValid <= '1';
		f2hData <= x"87";
		
		wait until f2hReady = '1';
		f2hData <= x"65";
		wait until f2hReady = '0';

		wait until f2hReady = '1';
		f2hData <= x"43";
		wait until f2hReady = '0';

		wait until f2hReady = '1';
		f2hData <= x"21";
		wait until f2hReady = '0';

		wait until f2hReady = '1';
		f2hData <= (others => 'Z');
		f2hValid <= '0';
		wait;
	end process;

end architecture;

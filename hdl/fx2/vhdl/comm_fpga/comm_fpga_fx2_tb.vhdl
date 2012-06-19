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

entity comm_fpga_fx2_tb is
end comm_fpga_fx2_tb;

architecture behavioural of comm_fpga_fx2_tb is
	-- Clocks
	signal sysClk     : std_logic;  -- main system clock
	signal dispClk    : std_logic;  -- display version of sysClk, which leads it by 4ns

	-- External interface ---------------------------------------------------------------------------
	signal fx2FifoSel : std_logic;  -- comm_fpga_fx2 drives fx2FifoSel='0' to read from EP6OUT, and fx2FifoSel='1' to write to EP8IN
	signal fx2Data    : std_logic_vector(7 downto 0);  -- data to/from the FX2

	-- When EP6OUT selected:
	signal fx2Read    : std_logic;  -- comm_fpga_fx2 drives fx2Read='1' telling FX2 to commit data out of the EP6OUT FIFO
	signal fx2GotData : std_logic;  -- FX2 drives '1' when fx2FifoSel='0' and there is some data in the EP6OUT FIFO

	-- When EP8IN selected:
	signal fx2Write   : std_logic;  -- comm_fpga_fx2 drives fx2Write='1' telling FX2 to commit data into the EP8IN FIFO
	signal fx2GotRoom : std_logic;  -- FX2 drives '1' when fx2FifoSel='1' and there is room in the EP8IN FIFO
	signal fx2PktEnd  : std_logic;  -- comm_fpga_fx2 drives fx2PktEnd='1' to commit an EP8IN packet early

	-- Channel read/write interface -----------------------------------------------------------------
	signal chanAddr   : std_logic_vector(6 downto 0);  -- comm_fpga_fx2 selects one of 128 channels to access

	-- Host >> FPGA pipe:
	signal h2fData    : std_logic_vector(7 downto 0);  -- data to be read from the selected channel
	signal h2fValid   : std_logic;  -- comm_fpga_fx2 drives h2fValid='1' when it wants to write to the selected channel
	signal h2fReady   : std_logic;  -- this must be driven high if the selected channel has room for data to be written to it

	-- Host << FPGA pipe:
	signal f2hData    : std_logic_vector(7 downto 0);  -- data to be written to the selected channel
	signal f2hValid   : std_logic;  -- this must be asserted if the selected channel has data available for reading
	signal f2hReady   : std_logic;  -- comm_fpga_fx2 drives f2hReady='1' when it wants to read from the selected channel
begin
	-- Instantiate comm_fpga_fx2 for testing
	uut: entity work.comm_fpga_fx2
		port map(
			-- FX2 interface --------------------------------------------------------------------------
			fx2Clk_in      => sysClk,      
			fx2FifoSel_out => fx2FifoSel,  
			fx2Data_io     => fx2Data,     

			fx2Read_out    => fx2Read,		 -- when EP6OUT selected
			fx2GotRoom_in  => fx2GotRoom,  -- /

			fx2Write_out   => fx2Write,    -- \
			fx2GotData_in  => fx2GotData,  --  when EP8IN selected
			fx2PktEnd_out  => fx2PktEnd,	 -- /

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

	-- Drive the FX2 side
	process
	begin
		fx2Data <= (others => 'Z');
		fx2GotData <= '0';
		fx2GotRoom <= '1';
		wait until sysClk = '1';
		wait until sysClk = '0';
		wait until sysClk = '1';
		fx2Data <= x"55";
		fx2GotData <= '1';
		wait until rising_edge(sysClk);
		fx2Data <= x"00";
		wait until rising_edge(sysClk);
		fx2Data <= x"FF"; wait for 1 ps; fx2Data <= x"00";
		wait until rising_edge(sysClk);
		fx2Data <= x"FF"; wait for 1 ps; fx2Data <= x"00";
		wait until rising_edge(sysClk);
		fx2Data <= x"04";
		wait until rising_edge(sysClk);
		fx2Data <= x"12";
		wait until rising_edge(sysClk);
		fx2Data <= x"34";
		wait until rising_edge(sysClk);
		fx2Data <= (others => 'Z');
		wait until rising_edge(sysClk);
		fx2Data <= x"56";
		wait until rising_edge(sysClk);
		fx2Data <= x"78";
		wait until rising_edge(sysClk);
		fx2Data <= (others => 'Z');
		fx2GotData <= '0';

		wait until rising_edge(sysClk);
		wait until rising_edge(sysClk);
		wait until rising_edge(sysClk);
		wait until rising_edge(sysClk);
		fx2Data <= x"AA";
		fx2GotData <= '1';
		wait until rising_edge(sysClk);
		fx2Data <= x"00";
		wait until rising_edge(sysClk);
		fx2Data <= x"FF"; wait for 1 ps; fx2Data <= x"00";
		wait until rising_edge(sysClk);
		fx2Data <= x"FF"; wait for 1 ps; fx2Data <= x"00";
		wait until rising_edge(sysClk);
		fx2Data <= x"04";
		wait until rising_edge(sysClk);
		fx2GotData <= '0';
		fx2Data <= (others => 'Z');
		wait;
	end process;

	-- Drive the internal side
	process
	begin
		-- Host << FPGA data is invalid for now
		f2hData <= (others => '0');
		f2hValid <= '0';

		-- We're ready for Host >> FPGA data
		h2fReady <= '1';

		-- Wait until some Host >> FPGA data arrives, then deassert h2fReady to test throttling
		wait until h2fValid = '1';
		wait for 40 ns;
		h2fReady <= '0';
		wait for 20 ns;
		h2fReady <= '1';

		-- Wait for the Host << FPGA pipe to become ready, then drive some data, with a hole in the
		-- middle again to test the throttling.
		wait until f2hReady = '1';
		f2hData <= x"87";
		f2hValid <= '1';
		wait for 20 ns;
		f2hData <= x"65";
		wait for 20 ns;
		f2hData <= (others => 'Z');
		f2hValid <= '0';
		wait for 20 ns;
		f2hData <= x"43";
		f2hValid <= '1';
		wait for 20 ns;
		f2hData <= x"21";
		wait for 20 ns;
		f2hData <= (others => '0');
		f2hValid <= '0';
		wait;
	end process;

end architecture;

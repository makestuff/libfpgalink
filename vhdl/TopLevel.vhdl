--
-- Copyright (C) 2009-2011 Chris McClelland
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

entity TopLevel is
	port(
		reset_in     : in std_logic;
		ifclk_in     : in std_logic;

		-- Unused connections must be configured as inputs
		flagA_in     : in std_logic;
		flagB_in     : in std_logic;
		int0_in      : in std_logic;                     -- PA0
		int1_in      : in std_logic;                     -- PA1
		pa3_in       : in std_logic;                     -- PA3
		pa7_in       : in std_logic;                     -- PA7
		clkout_in    : in std_logic;
		pd_in        : in std_logic_vector(7 downto 0);
		dummy_out    : out std_logic;                    -- Dummy output, not connected to FX2

		-- Data & control from the FX2
		fifodata_io  : inout std_logic_vector(7 downto 0);
		gotdata_in   : in std_logic;                     -- FLAGC=EF (active-low), so '1' when there's data

		-- Control to the FX2
		sloe_out     : out std_logic;                    -- PA2
		slrd_out     : out std_logic;
		slwr_out     : out std_logic;
		fifoaddr_out : out std_logic_vector(1 downto 0); -- PA4 & PA5
		pktend_out   : out std_logic;                    -- PA6

		-- Onboard peripherals
		sseg_out     : out std_logic_vector(7 downto 0);
		anode_out    : out std_logic_vector(3 downto 0);
		led_out      : out std_logic_vector(7 downto 0);
		sw_in        : in std_logic_vector(7 downto 0)
	);
end TopLevel;

architecture Behavioural of TopLevel is
	type StateType is (
		STATE_IDLE,
		STATE_GET_COUNT0,
		STATE_GET_COUNT1,
		STATE_GET_COUNT2,
		STATE_GET_COUNT3,
		STATE_BEGIN_WRITE,
		STATE_WRITE,
		STATE_END_WRITE,
		STATE_READ
	);
	signal state, state_next        : StateType;
	signal wcount, wcount_next      : unsigned(31 downto 0);  -- Read/Write count
	signal addr, addr_next          : std_logic_vector(1 downto 0);
	signal dowrite, dowrite_next    : std_logic;
	signal checksum, checksum_next  : std_logic_vector(15 downto 0);
	signal r0, r1, r0_next, r1_next : std_logic_vector(7 downto 0);
	signal r2, r3, r2_next, r3_next : std_logic_vector(7 downto 0);
	constant OUT_FIFO               : std_logic_vector(1 downto 0) := "10"; -- EP6OUT
	constant IN_FIFO                : std_logic_vector(1 downto 0) := "11"; -- EP8IN
begin
	process(ifclk_in, reset_in)
	begin
		if ( reset_in = '1' ) then
			state    <= STATE_IDLE;
			wcount   <= (others => '0');
			addr     <= (others => '0');
			dowrite  <= '0';
			checksum <= (others => '0');
			r0       <= (others => '0');
			r1       <= (others => '0');
			r2       <= (others => '0');
			r3       <= (others => '0');
		elsif ( ifclk_in'event and ifclk_in = '1' ) then
			state    <= state_next;
			wcount   <= wcount_next;
			addr     <= addr_next;
			dowrite  <= dowrite_next;
			checksum <= checksum_next;
			r0       <= r0_next;
			r1       <= r1_next;
			r2       <= r2_next;
			r3       <= r3_next;
		end if;
	end process;

	-- Next state logic
	process(state, fifodata_io, gotdata_in, wcount, addr, r0, r1, r2, r3, checksum, dowrite, sw_in)
	begin
		state_next    <= state;
		wcount_next   <= wcount;
		addr_next     <= addr;
		dowrite_next  <= dowrite;
		checksum_next <= checksum;
		r0_next       <= r0;
		r1_next       <= r1;
		r2_next       <= r2;
		r3_next       <= r3;
		fifodata_io   <= (others => 'Z');  -- Tristated unless explicitly driven
		case state is

			when STATE_IDLE =>
				fifoaddr_out <= OUT_FIFO;  -- Reading from FX2LP
				sloe_out     <= '0';  	   -- Active: FX2LP should drive fifodata_io
				slrd_out     <= '0';  	   -- Active: If there's data available, commit the read.
				slwr_out     <= '1';  	   -- Inactive: FPGA does not write.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				if ( gotdata_in = '1' ) then
					-- The read/write flag and a seven-bit register address will be available on the
					-- next clock edge.
					addr_next    <= fifodata_io(1 downto 0);
					dowrite_next <= fifodata_io(7);
					state_next   <= STATE_GET_COUNT0;
				end if;

			when STATE_GET_COUNT0 =>
				fifoaddr_out <= OUT_FIFO;  -- Reading from FX2LP
				sloe_out     <= '0';  	   -- Active: FX2LP should drive fifodata_io
				slrd_out     <= '0';  	   -- Active: If there's data available, commit the read.
				slwr_out     <= '1';  	   -- Inactive: FPGA does not write.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				if ( gotdata_in = '1' ) then
					-- The count high word high byte will be available on the next clock edge.
					wcount_next(31 downto 24) <= unsigned(fifodata_io);
					state_next <= STATE_GET_COUNT1;
				end if;

			when STATE_GET_COUNT1 =>
				fifoaddr_out <= OUT_FIFO;  -- Reading from FX2LP
				sloe_out     <= '0';  	   -- Active: FX2LP should drive fifodata_io
				slrd_out     <= '0';  	   -- Active: If there's data available, commit the read.
				slwr_out     <= '1';  	   -- Inactive: FPGA does not write.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				if ( gotdata_in = '1' ) then
					-- The count high word low byte will be available on the next clock edge.
					wcount_next(23 downto 16) <= unsigned(fifodata_io);
					state_next <= STATE_GET_COUNT2;
				end if;

			when STATE_GET_COUNT2 =>
				fifoaddr_out <= OUT_FIFO;  -- Reading from FX2LP
				sloe_out     <= '0';  	   -- Active: FX2LP should drive fifodata_io
				slrd_out     <= '0';  	   -- Active: If there's data available, commit the read.
				slwr_out     <= '1';  	   -- Inactive: FPGA does not write.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				if ( gotdata_in = '1' ) then
					-- The count low word high byte will be available on the next clock edge.
					wcount_next(15 downto 8) <= unsigned(fifodata_io);
					state_next <= STATE_GET_COUNT3;
				end if;

			when STATE_GET_COUNT3 =>
				fifoaddr_out <= OUT_FIFO;  -- Reading from FX2LP
				sloe_out     <= '0';  	   -- Active: FX2LP should drive fifodata_io
				slrd_out     <= '0';  	   -- Active: If there's data available, commit the read.
				slwr_out     <= '1';  	   -- Inactive: FPGA does not write.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				if ( gotdata_in = '1' ) then
					-- The count low word low byte will be available on the next clock edge.
					wcount_next(7 downto 0) <= unsigned(fifodata_io);
					if ( dowrite = '1' ) then
						state_next <= STATE_BEGIN_WRITE;
					else
						state_next <= STATE_READ;
					end if;
				end if;

			when STATE_BEGIN_WRITE =>
				fifoaddr_out <= IN_FIFO;   -- Writing to FX2LP
				sloe_out     <= '1';  	   -- Inactive: FX2LP should not drive fifodata_io
				slrd_out     <= '1';  	   -- Inactive: FPGA is not reading.
				slwr_out     <= '1';  	   -- Inactive: FPGA is not writing.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				state_next   <= STATE_WRITE;

			when STATE_WRITE =>
				fifoaddr_out <= IN_FIFO;   -- Writing to FX2LP
				sloe_out     <= '1';  	   -- Inactive: FX2LP should not drive fifodata_io
				slrd_out     <= '1';  	   -- Inctive: FPGA is not reading.
				slwr_out     <= '0';  	   -- Active: FPGA is writing.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				case addr is
					when "00" =>
						fifodata_io <= sw_in; -- Reading from R0 returns the state of the switches
					when "01" =>
						fifodata_io <= r1;
					when "10" =>
						fifodata_io <= r2;
					when others =>
						fifodata_io <= r3;
				end case;
				wcount_next  <= wcount - 1;
				if ( wcount = 1 ) then
					state_next <= STATE_END_WRITE;
				end if;

			when STATE_END_WRITE =>
				fifoaddr_out <= IN_FIFO;   -- Writing to FX2LP
				sloe_out     <= '1';  	   -- Inactive: FX2LP should not drive fifodata_io
				slrd_out     <= '1';  	   -- Inactive: FPGA is not reading.
				slwr_out     <= '1';  	   -- Inactive: FPGA is not writing.
				pktend_out   <= '0';  	   -- Active: FPGA commits the packet.
				state_next   <= STATE_IDLE;

			when STATE_READ =>
				fifoaddr_out <= OUT_FIFO;  -- Reading from FX2LP
				sloe_out     <= '0';  	   -- Active: FX2LP should drive fifodata_io
				slrd_out     <= '0';  	   -- Active: FPGA is reading.
				slwr_out     <= '1';  	   -- Inactive: FPGA is not writing.
				pktend_out   <= '1';  	   -- Inactive: FPGA does not commit a short packet.
				if ( gotdata_in = '1' ) then
					-- A data byte will be available on the next clock edge
					case addr is
						when "00" =>
							r0_next <= fifodata_io;
							checksum_next <= std_logic_vector(
								unsigned(checksum) + unsigned(fifodata_io));  -- Update R0 checksum
						when "01" =>
							r1_next <= fifodata_io;  -- Writing a '1' to bit 0 of R1 clears R0 checksum
							if ( fifodata_io(0) = '1' ) then
								checksum_next <= (others => '0');
							end if;
						when "10" =>
							r2_next <= fifodata_io;
						when others =>
							r3_next <= fifodata_io;
					end case;
					wcount_next  <= wcount - 1;
					if ( wcount = 1 ) then
						state_next <= STATE_IDLE;
					end if;
				end if;
		end case;
	end process;
	
	-- Mop up all the unused inputs to prevent synthesis warnings
	dummy_out <=
		clkout_in and flagA_in and flagB_in and int0_in and int1_in and pa3_in and pa7_in
		and pd_in(0) and pd_in(1) and pd_in(2) and pd_in(3)
		and pd_in(4) and pd_in(5) and pd_in(6) and pd_in(7);
	
	led_out     <= r0;
	sseg_out(7) <= '1';  -- Decimal point off

	sevenSeg : entity work.SevenSeg
		port map(
			clk    => ifclk_in,
			data   => checksum,
			segs   => sseg_out(6 downto 0),
			anodes => anode_out
		);
end Behavioural;

-- Infer registers
process(clk_in)
begin
   if ( rising_edge(clk_in) ) then
      if ( reset_in = '1' ) then
         count <= (others => '0');
      else
         count <= count_next;
      end if;
   end if;
end process;

-- Wire up write FIFO to channel 0 writes:
--   flags(2) driven by writeFifoOutputValid
--   writeFifoOutputReady driven by consumer_timer
--   LEDs driven by writeFifoOutputData
writeFifoInputData <=
   h2fData_in;
writeFifoInputValid <=
   '1' when h2fValid_in = '1' and chanAddr_in = "0000000"
   else '0';
h2fReady_out <=
   '0' when writeFifoInputReady = '0' and chanAddr_in = "0000000"
   else '1';

-- Wire up read FIFO to channel 0 reads:
--   readFifoInputValid driven by producer_timer
--   flags(0) driven by readFifoInputReady
count_next <=
   std_logic_vector(unsigned(count) + 1) when readFifoInputValid = '1'
   else count;
readFifoInputData <=
   count;
f2hValid_out <=
   '0' when readFifoOutputValid = '0' and chanAddr_in = "0000000"
   else '1';
readFifoOutputReady <=
   '1' when f2hReady_in = '1' and chanAddr_in = "0000000"
   else '0';

-- Select values to return for each channel when the host is reading
with chanAddr_in select f2hData_out <=
   readFifoOutputData     when "0000000",  -- get from read FIFO
   fifoCount(15 downto 8) when "0000001",  -- get depth of write FIFO
   fifoCount(7 downto 0)  when "0000010",  -- get depth of read FIFO
   x"00"                  when others;

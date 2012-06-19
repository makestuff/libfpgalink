-- Infer registers
process(fx2Clk_in)
begin
   if ( rising_edge(fx2Clk_in) ) then
      checksum <= checksum_next;
      reg0 <= reg0_next;
      reg1 <= reg1_next;
      reg2 <= reg2_next;
      reg3 <= reg3_next;
   end if;
end process;

-- Drive register inputs for each channel when the host is writing
checksum_next <=
   std_logic_vector(unsigned(checksum) + unsigned(h2fData))
      when chanAddr = "0000000" and h2fValid = '1'
   else x"0000"
      when chanAddr = "0000001" and h2fValid = '1' and h2fData(0) = '1'
   else checksum;
reg0_next <= h2fData when chanAddr = "0000000" and h2fValid = '1' else reg0;
reg1_next <= h2fData when chanAddr = "0000001" and h2fValid = '1' else reg1;
reg2_next <= h2fData when chanAddr = "0000010" and h2fValid = '1' else reg2;
reg3_next <= h2fData when chanAddr = "0000011" and h2fValid = '1' else reg3;

-- Select values to return for each channel when the host is reading
with chanAddr select f2hData <=
   sw_in when "0000000",
   reg1  when "0000001",
   reg2  when "0000010",
   reg3  when "0000011",
   x"00" when others;

-- Assert that there's always data for reading, and always room for writing
f2hValid <= '1';
h2fReady <= '1';

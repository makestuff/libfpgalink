-- Infer registers
process(clk_in)
begin
   if ( rising_edge(clk_in) ) then
      if ( reset_in = '1' ) then
         reg0 <= (others => '0');
         checksum <= (others => '0');
      else
         reg0 <= reg0_next;
         checksum <= checksum_next;
      end if;
   end if;
end process;

-- Drive register inputs for each channel when the host is writing
reg0_next <=
   h2fData_in when chanAddr_in = "0000000" and h2fValid_in = '1'
   else reg0;
checksum_next <=
   std_logic_vector(unsigned(checksum) + unsigned(h2fData_in))
      when chanAddr_in = "0000000" and h2fValid_in = '1'
   else h2fData_in & checksum(7 downto 0)
      when chanAddr_in = "0000001" and h2fValid_in = '1'
   else checksum(15 downto 8) & h2fData_in
      when chanAddr_in = "0000010" and h2fValid_in = '1'
   else checksum;

-- Select values to return for each channel when the host is reading
with chanAddr_in select f2hData_out <=
   sw_in                 when "0000000",
   checksum(15 downto 8) when "0000001",
   checksum(7 downto 0)  when "0000010",
   x"00" when others;

-- Assert that there's always data for reading, and always room for writing
f2hValid_out <= '1';
h2fReady_out <= '1';

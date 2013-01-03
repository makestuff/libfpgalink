// Infer registers
always @(posedge clk_in)
begin
   if ( reset_in == 1'b1 )
      begin
         reg0 <= 8'h00;
         checksum <= 16'h0000;
      end
   else
      begin
         reg0 <= reg0_next;
         checksum <= checksum_next;
      end
end

// Drive register inputs for each channel when the host is writing
assign reg0_next =
   (chanAddr_in == 7'b0000000 && h2fValid_in == 1'b1) ? h2fData_in :
   reg0;
assign checksum_next =
   (chanAddr_in == 7'b0000000 && h2fValid_in == 1'b1) ?
      checksum + h2fData_in :
   (chanAddr_in == 7'b0000001 && h2fValid_in == 1'b1) ?
      {h2fData_in, checksum[7:0]} :
   (chanAddr_in == 7'b0000010 && h2fValid_in == 1'b1) ?
      {checksum[15:8], h2fData_in} :
   checksum;

// Select values to return for each channel when the host is reading
assign f2hData_out =
   (chanAddr_in == 7'b0000000) ? sw_in :
   (chanAddr_in == 7'b0000001) ? checksum[15:8] :
   (chanAddr_in == 7'b0000010) ? checksum[7:0] :
   8'h00;

// Assert that there's always data for reading, and always room for writing
assign f2hValid_out = 1'b1;
assign h2fReady_out = 1'b1;

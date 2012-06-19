// Infer registers
always @(posedge fx2Clk_in)
begin
   checksum <= checksum_next;
   reg0 <= reg0_next;
   reg1 <= reg1_next;
   reg2 <= reg2_next;
   reg3 <= reg3_next;
end

// Drive register inputs for each channel when the host is writing
assign checksum_next =
   (chanAddr == 7'b0000000 && h2fValid == 1'b1) ?
      checksum + h2fData :
   (chanAddr == 7'b0000001 && h2fValid == 1'b1 && h2fData[0] == 1'b1) ?
      16'h0000 :
   checksum;
assign reg0_next =
   (chanAddr == 7'b0000000 && h2fValid == 1'b1) ? h2fData : reg0;
assign reg1_next =
   (chanAddr == 7'b0000001 && h2fValid == 1'b1) ? h2fData : reg1;
assign reg2_next =
   (chanAddr == 7'b0000010 && h2fValid == 1'b1) ? h2fData : reg2;
assign reg3_next =
   (chanAddr == 7'b0000011 && h2fValid == 1'b1) ? h2fData : reg3;

// Select values to return for each channel when the host is reading
assign f2hData =
   (chanAddr == 7'b0000000) ? sw_in :
   (chanAddr == 7'b0000001) ? reg1 :
   (chanAddr == 7'b0000010) ? reg2 :
   (chanAddr == 7'b0000011) ? reg3 :
   8'h00;

// Assert that there's always data for reading, and always room for writing
assign f2hValid = 1'b1;
assign h2fReady = 1'b1;

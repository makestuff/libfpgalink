// Infer registers
always @(posedge clk_in)
begin
   if ( reset_in == 1'b1 )
      count <= 8'h00;
   else
      count <= count_next;
end

// Wire up write FIFO to channel 0 writes:
//   flags(2) driven by writeFifoOutputValid
//   writeFifoOutputReady driven by consumer_timer
//   LEDs driven by writeFifoOutputData
assign writeFifoInputData =
   h2fData_in;
assign writeFifoInputValid =
   (h2fValid_in == 1'b1 && chanAddr_in == 7'b0000000) ? 1'b1 : 1'b0;
assign h2fReady_out =
   (writeFifoInputReady == 1'b0 && chanAddr_in == 7'b0000000) ? 1'b0 : 1'b1;

// Wire up read FIFO to channel 0 reads:
//   readFifoInputValid driven by producer_timer
//   flags(0) driven by readFifoInputReady
assign count_next =
   (readFifoInputValid == 1'b1) ? count + 1'b1 : count;
assign readFifoInputData =
   count;
assign f2hValid_out =
   (readFifoOutputValid == 1'b0 && chanAddr_in == 7'b0000000) ? 1'b0 : 1'b1;
assign readFifoOutputReady =
   (f2hReady_in == 1'b1 && chanAddr_in == 7'b0000000) ? 1'b1 : 1'b0;

// Select values to return for each channel when the host is reading
assign f2hData_out =
   (chanAddr_in == 7'b0000000) ? readFifoOutputData :  // get from read FIFO
   (chanAddr_in == 7'b0000001) ? fifoCount[15:8] :  // get depth of write FIFO
   (chanAddr_in == 7'b0000010) ? fifoCount[7:0]  :  // get depth of write FIFO
   8'h00;

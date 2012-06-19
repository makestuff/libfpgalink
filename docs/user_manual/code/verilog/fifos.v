// Infer registers
always @(posedge fx2Clk_in)
   count <= count_next;

// Wire up write FIFO to channel 0 writes:
//   flags(2) driven by writeFifoOutputValid
//   writeFifoOutputReady driven by consumer_timer
//   LEDs driven by writeFifoOutputData
assign writeFifoInputData = h2fData;
assign writeFifoInputValid =
   (h2fValid == 1'b1 && chanAddr == 7'b0000000) ? 1'b1 : 1'b0;
assign h2fReady =
   (writeFifoInputReady == 1'b0 && chanAddr == 7'b0000000) ? 1'b0 : 1'b1;

// Wire up read FIFO to channel 0 reads:
//   readFifoInputValid driven by producer_timer
//   flags(0) driven by readFifoInputReady
assign count_next = (readFifoInputValid == 1'b1) ? count + 1'b1 : count;
assign readFifoInputData = count;
assign f2hValid =
   (readFifoOutputValid == 1'b0 && chanAddr == 7'b0000000) ? 1'b0 : 1'b1;
assign readFifoOutputReady =
   (f2hReady == 1'b1 && chanAddr == 7'b0000000) ? 1'b1 : 1'b0;

// Select values to return for each channel when the host is reading
assign f2hData =
   (chanAddr == 7'b0000000) ? readFifoOutputData :  // get data from read FIFO
   (chanAddr == 7'b0000001) ? fifoCount[15:8] :  // read depth of the write FIFO
   (chanAddr == 7'b0000010) ? fifoCount[7:0]  :  // read depth of the write FIFO
   8'h00;

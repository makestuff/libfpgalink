#!/bin/bash

echo Build starting at $(date)...

export HDL=/home/chris/makestuff/libs/libfpgalink/hdl
rm -rf gen_xsvf
mkdir gen_xsvf
for i in atlys lx9 nexys2-1200 nexys2-500 nexys3 s3board xylo-l; do
	echo Cleaning for ${i}...
	cd ${HDL}/sync/verilog/ex_cksum
	make clean
	cd ${HDL}/sync/verilog/ex_fifo/
	make clean allclean
	cd ${HDL}/sync/vhdl/ex_cksum
	make clean
	cd ${HDL}/sync/vhdl/ex_fifo/
	make clean allclean

	echo Building ${i}...
	cd ${HDL}/sync/verilog/ex_cksum
	make PLATFORM=${i}
	cp top_level.xsvf ${HDL}/gen_xsvf/ex_cksum_${i}_verilog.xsvf
	cd ${HDL}/sync/verilog/ex_fifo/
	make PLATFORM=${i}
	cp top_level.xsvf ${HDL}/gen_xsvf/ex_fifo_${i}_verilog.xsvf
	cd ${HDL}/sync/vhdl/ex_cksum
	make PLATFORM=${i}
	cp top_level.xsvf ${HDL}/gen_xsvf/ex_cksum_${i}_vhdl.xsvf
	cd ${HDL}/sync/vhdl/ex_fifo/
	make PLATFORM=${i}
	cp top_level.xsvf ${HDL}/gen_xsvf/ex_fifo_${i}_vhdl.xsvf
done

echo Build finished at $(date)

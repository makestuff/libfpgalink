#!/bin/bash

if [ $# != 1 ]; then
	echo "Synopsis: $0 <xsvf2csvf path>"
	exit 1
fi

export X2C=$(pwd)

echo HDL build starting at $(date)...

export HDL=$(dirname $0)
rm -rf ../gen_xsvf ../gen_csvf
mkdir ../gen_xsvf ../gen_csvf
for p in atlys lx9 nexys2-1200 nexys2-500 nexys3 s3board xylo-l; do
	echo Building for platform: ${p}...
	for l in vhdl verilog; do
		# cksum example
		cd ${HDL}/sync/${l}/ex_cksum
		make clean
		make PLATFORM=${p}
		cp top_level.xsvf ${HDL}/../gen_xsvf/ex_cksum_${p}_${l}.xsvf
		${X2C} ${HDL}/../gen_xsvf/ex_cksum_${p}_${l}.xsvf ${HDL}/../gen_csvf/ex_cksum_${p}_${l}.csvf
		make clean

		# fifo example
		cd ${HDL}/sync/${l}/ex_fifo
		make clean allclean
		make PLATFORM=${p}
		cp top_level.xsvf ${HDL}/../gen_xsvf/ex_fifo_${p}_${l}.xsvf
		${X2C} ${HDL}/../gen_xsvf/ex_fifo_${p}_${l}.xsvf ${HDL}/../gen_csvf/ex_fifo_${p}_${l}.csvf
		make clean allclean
	done
done

echo HDL build finished at $(date)

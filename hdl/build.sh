#!/bin/bash

if [ $# != 1 ]; then
	echo "Synopsis: $0 <xsvf2csvf path>"
	exit 1
fi

export HDL=$(pwd)
export X2C=${HDL}/../$1

echo HDL build starting at $(date)...

rm -rf ../gen_svf ../gen_xsvf ../gen_csvf
mkdir ../gen_svf ../gen_xsvf ../gen_csvf
for p in atlys lx9 nexys2-1200 nexys2-500 nexys3 s3board xylo-l; do
	echo Building for FX2 platform: ${p}...
	for l in vhdl verilog; do
		# cksum example
		cd ${HDL}/fx2/${l}/ex_cksum
		make clean
		make PLATFORM=${p}
		cp top_level.svf ${HDL}/../gen_svf/ex_cksum_${p}_fx2_${l}.svf
		if [ -e top_level.xsvf ]; then
			cp top_level.xsvf ${HDL}/../gen_xsvf/ex_cksum_${p}_fx2_${l}.xsvf
		fi
		${X2C} ${HDL}/../gen_svf/ex_cksum_${p}_fx2_${l}.svf ${HDL}/../gen_csvf/ex_cksum_${p}_fx2_${l}.csvf
		make clean

		# fifo example
		cd ${HDL}/fx2/${l}/ex_fifo
		make clean fifoclean
		make PLATFORM=${p}
		cp top_level.svf ${HDL}/../gen_svf/ex_fifo_${p}_fx2_${l}.svf
		if [ -e top_level.xsvf ]; then
			cp top_level.xsvf ${HDL}/../gen_xsvf/ex_fifo_${p}_fx2_${l}.xsvf
		fi
		${X2C} ${HDL}/../gen_svf/ex_fifo_${p}_fx2_${l}.svf ${HDL}/../gen_csvf/ex_fifo_${p}_fx2_${l}.csvf
		make clean fifoclean
	done
done

for p in nexys2-1200 ep2c5; do
	echo Building for EPP platform: ${p}...
	for l in vhdl verilog; do
		# cksum example
		cd ${HDL}/epp/${l}/ex_cksum
		make clean
		make PLATFORM=${p}
		cp top_level.svf ${HDL}/../gen_svf/ex_cksum_${p}_epp_${l}.svf
		if [ -e top_level.xsvf ]; then
			cp top_level.xsvf ${HDL}/../gen_xsvf/ex_cksum_${p}_epp_${l}.xsvf
		fi
		${X2C} ${HDL}/../gen_svf/ex_cksum_${p}_epp_${l}.svf ${HDL}/../gen_csvf/ex_cksum_${p}_epp_${l}.csvf
		make clean

		# fifo example
		cd ${HDL}/epp/${l}/ex_fifo
		make clean fifoclean
		make PLATFORM=${p}
		cp top_level.svf ${HDL}/../gen_svf/ex_fifo_${p}_epp_${l}.svf
		if [ -e top_level.xsvf ]; then
			cp top_level.xsvf ${HDL}/../gen_xsvf/ex_fifo_${p}_epp_${l}.xsvf
		fi
		${X2C} ${HDL}/../gen_svf/ex_fifo_${p}_epp_${l}.svf ${HDL}/../gen_csvf/ex_fifo_${p}_epp_${l}.csvf
		make clean fifoclean
	done
done

echo HDL build finished at $(date)

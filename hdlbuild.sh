#!/bin/bash

if [ $# != 1 ]; then
	echo "Synopsis: $0 <xsvf2csvf path>"
	exit 1
fi

cd $(dirname $0)
export HDL=$(pwd)/hdl
export X2C=$(pwd)/$1

echo HDL build starting at $(date)...

if [ ! -e hdl ]; then
	echo "Fetching hdlmake from GitHub..."
	wget -qO- https://github.com/makestuff/hdlmake/archive/master.tar.gz | tar zxf -
	mv hdlmake-master hdl
fi

export PATH=${HDL}/bin:${PATH}

cd hdl/apps
hdlmake.py -g makestuff/swled
cd makestuff/swled

for j in cksum/vhdl cksum/verilog fifo/vhdl fifo/verilog; do
	cd $j

	mkdir -p svf
	mkdir -p xsvf
	mkdir -p csvf
	mkdir -p bin

	export HDL=$(basename $(pwd))

	for i in atlys nexys2-1200 nexys2-500 nexys3 s3board xylo-l; do
		hdlmake.py -c
		hdlmake.py -t ../../templates/fx2all/${HDL} -b $i
		cp top_level.svf svf/fx2all-$i.svf
		cp top_level.xsvf xsvf/fx2all-$i.xsvf
		${X2C} top_level.svf csvf/fx2all-$i.csvf
	done

	for i in aes220; do
		hdlmake.py -c
		hdlmake.py -t ../../templates/fx2all/${HDL} -b $i
		cp top_level.bin bin/fx2all-$i.bin
	done

	for i in lx9; do
		hdlmake.py -c
		hdlmake.py -t ../../templates/fx2min/${HDL} -b $i
		cp top_level.svf svf/fx2min-$i.svf
		cp top_level.xsvf xsvf/fx2min-$i.xsvf
		${X2C} top_level.svf csvf/fx2min-$i.csvf
	done

	for i in ep2c5 nexys2-1200; do
		hdlmake.py -c
		hdlmake.py -t ../../templates/epp/${HDL} -b $i
		cp top_level.svf svf/epp-$i.svf
		if [ -e top_level.xsvf ]; then
			cp top_level.xsvf xsvf/epp-$i.xsvf
		fi
		${X2C} top_level.svf csvf/epp-$i.csvf
	done

	hdlmake.py -c

	cd ../..
done

for i in $(find . -name "fifo.batch"); do
	cd $(dirname $i)
	hdlmake.py -z -f
	cd -
done

echo HDL build finished at $(date)

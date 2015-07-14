#!/bin/bash

export HDL=../../hdl

rm -rf code/vhdl
mkdir -p code/vhdl
./snippet.pl ${HDL}/apps/makestuff/swled/cksum/vhdl/cksum_rtl.vhdl registers > code/vhdl/registers.vhdl
./snippet.pl ${HDL}/apps/makestuff/swled/fifo/vhdl/fifo_rtl.vhdl fifos > code/vhdl/fifos.vhdl

rm -rf code/verilog
mkdir -p code/verilog
./snippet.pl ${HDL}/apps/makestuff/swled/cksum/verilog/cksum_rtl.v registers > code/verilog/registers.v
./snippet.pl ${HDL}/apps/makestuff/swled/fifo/verilog/fifo_rtl.v fifos > code/verilog/fifos.v

cat top_level_verilog.tex | sed 's/Verilog/VHDL/g;s/verilog/vhdl/g;s/\.v/\.vhdl/g' > top_level_vhdl.tex

make clean
rm top_level.tex
ln -s top_level_verilog.tex top_level.tex
make
cp paper.pdf ../verilog_paper.pdf
cp mobile.pdf ../verilog_mobile.pdf
make clean
rm top_level.tex
ln -s top_level_vhdl.tex top_level.tex
make
cp paper.pdf ../vhdl_paper.pdf
cp mobile.pdf ../vhdl_mobile.pdf
rm -f top_level.tex top_level_vhdl.tex
rm -rf code/vhdl code/verilog

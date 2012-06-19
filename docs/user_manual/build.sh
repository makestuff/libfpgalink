#!/bin/bash

export HDL=../../hdl

rm -rf code/vhdl
mkdir -p code/vhdl
./snippet.pl ${HDL}/fx2/vhdl/ex_cksum/top_level.vhdl registers > code/vhdl/registers.vhdl
./snippet.pl ${HDL}/fx2/vhdl/ex_fifo/top_level.vhdl fifos > code/vhdl/fifos.vhdl

rm -rf code/verilog
mkdir -p code/verilog
./snippet.pl ${HDL}/fx2/verilog/ex_cksum/top_level.v registers > code/verilog/registers.v
./snippet.pl ${HDL}/fx2/verilog/ex_fifo/top_level.v fifos > code/verilog/fifos.v

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

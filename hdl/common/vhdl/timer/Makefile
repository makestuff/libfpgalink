#
# Copyright (C) 2012 Chris McClelland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
MODULE := timer
SIGNALS := \
	dispClk \
	- \
	uut.ceiling_in \
	uut.tick_out \
	- \
	uut.count
HDLS := \
	${MODULE}.vhdl

all: synthesis simulation

synthesis: ${HDLS}
	mkdir -p synthesis
	for i in $+; do echo "vhdl work \"$$i\""; done > synthesis/$(MODULE).prj
	echo "set -tmpdir \"xst/projnav.tmp\"" > synthesis/platform.xst
	echo "set -xsthdpdir \"xst\"" >> synthesis/platform.xst
	echo "run" >> synthesis/platform.xst
	echo "-ifn ${MODULE}.prj" >> synthesis/platform.xst
	echo "-ifmt mixed" >> synthesis/platform.xst
	echo "-ofn ${MODULE}" >> synthesis/platform.xst
	echo "-ofmt NGC" >> synthesis/platform.xst
	echo "-p xc3s200-4-ft256" >> synthesis/platform.xst
	echo "-top ${MODULE}" >> synthesis/platform.xst
	echo "-opt_mode Speed" >> synthesis/platform.xst
	echo "-opt_level 1" >> synthesis/platform.xst
	cp *.vhdl synthesis/
	mkdir -p synthesis/xst/projnav.tmp
	cd synthesis && xst -intstyle ise -ifn platform.xst -ofn ${MODULE}.syr
	grep WARNINGS synthesis/* | grep -Eq "XST_NUMBER_OF_WARNINGS\" value=\"0\""
	touch $@

simulation: ${HDLS} ${MODULE}_tb.vhdl
	mkdir -p simulation
	ghdl -i --ieee=synopsys --std=93c --vital-checks --warn-binding --warn-reserved --warn-library --warn-vital-generic --warn-delayed-checks --warn-body --warn-specs --warn-unused --warn-error --workdir=simulation --work=work *.vhdl
	ghdl -m --ieee=synopsys --std=93c --vital-checks --warn-binding --warn-reserved --warn-library --warn-vital-generic --warn-delayed-checks --warn-body --warn-specs --warn-unused --warn-error --workdir=simulation --work=work ${MODULE}_tb
	mv ${MODULE}_tb simulation
	./simulation/${MODULE}_tb --stop-time=1400ns --wave=simulation/${MODULE}_tb.ghw
	touch $@

waves: simulation
	@for i in ${SIGNALS}; do if [ $$i = "-" ]; then echo "gtkwave::/Edit/Insert_Blank"; else echo "gtkwave::addSignalsFromList $$i"; fi; done > simulation/startup.tcl
	@echo "gtkwave::setZoomFactor -27" >> simulation/startup.tcl
	@echo "gtkwave::setMarker 600ns" >> simulation/startup.tcl
	@echo "for { set i 0 } { \$$i <= [ gtkwave::getVisibleNumTraces ] } { incr i } { gtkwave::setTraceHighlightFromIndex \$$i off }" >> simulation/startup.tcl
	@echo "gtkwave::setLeftJustifySigs on" >> simulation/startup.tcl
	cat simulation/startup.tcl
	gtkwave -T simulation/startup.tcl simulation/${MODULE}_tb.ghw

clean: FORCE
	rm -rf synthesis simulation results.txt

FORCE:

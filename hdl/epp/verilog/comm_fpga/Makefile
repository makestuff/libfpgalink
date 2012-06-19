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
MODULE := comm_fpga_epp
HDLS := \
	${MODULE}.v

all: synthesis

synthesis: ${HDLS}
	mkdir -p synthesis
	for i in ${HDLS}; do if [ "$${i##*.}" = "vhdl" ]; then echo "vhdl work \"$$i\""; elif [ "$${i##*.}" = "v" ]; then echo "verilog work \"$$i\""; fi; done > synthesis/$(MODULE).prj
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
	cp ${HDLS} synthesis/
	mkdir -p synthesis/xst/projnav.tmp
	cd synthesis && xst -intstyle ise -ifn platform.xst -ofn ${MODULE}.syr
	grep WARNINGS synthesis/* | grep -Eq "XST_NUMBER_OF_WARNINGS\" value=\"0\""
	touch $@

clean: FORCE
	rm -rf synthesis

FORCE:

#
# Copyright (C) 2009-2012 Chris McClelland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
report: $(TOP_LEVEL).twr

# This assumes that the "XILINX" environment variable is set 
$(TOP_LEVEL).svf: ../../platforms/$(PLATFORM)/platform.batch $(TOP_LEVEL).bit
	echo "setPreference -pref KeepSVF:True" > temp.batch
	cat $< | sed s#\$${XILINX}#$(subst \,/,$(XILINX))#g >> temp.batch
	impact -batch temp.batch
	rm -f temp.batch

$(TOP_LEVEL).bit: ../../platforms/$(PLATFORM)/platform.ut $(TOP_LEVEL).ncd
	bitgen -intstyle ise -f $+

$(TOP_LEVEL).twr: $(TOP_LEVEL).ncd ../../platforms/$(PLATFORM)/platform.ucf
	trce -intstyle ise -v 3 -s 4 -n 3 -fastpaths -xml $(TOP_LEVEL).twx $< -o $@ $(TOP_LEVEL).pcf -ucf ../../platforms/$(PLATFORM)/platform.ucf

$(TOP_LEVEL).ncd: $(TOP_LEVEL)_map.ncd
	par -w -intstyle ise -ol high $(PARFLAGS) $< $@ $(TOP_LEVEL).pcf

$(TOP_LEVEL)_map.ncd: $(TOP_LEVEL).ngd
	map -intstyle ise -p $(FPGA) $(MAPFLAGS) -ir off -pr off -c 100 -o $@ $< $(TOP_LEVEL).pcf

$(TOP_LEVEL).ngd: $(TOP_LEVEL).ngc $(EXTRAS) ../../platforms/$(PLATFORM)/platform.ucf
	ngdbuild -intstyle ise -dd _ngo -nt timestamp -uc ../../platforms/$(PLATFORM)/platform.ucf -p $(FPGA) $< $@

$(TOP_LEVEL).edif: $(TOP_LEVEL).ngc
	rm -f $@
	ngc2edif -intstyle ise -bd square $(TOP_LEVEL).ngc $(TOP_LEVEL).edif

$(TOP_LEVEL).ngc: ../../platforms/$(PLATFORM)/platform.xst $(TOP_LEVEL).prj
	mkdir -p xst/projnav.tmp
	xst -intstyle ise -ifn $< -ofn $(TOP_LEVEL).syr

$(TOP_LEVEL).prj: $(HDLS)
	for i in $+; do if [ "$${i##*.}" = "vhdl" ]; then echo "vhdl work \"$$i\""; elif [ "$${i##*.}" = "v" ]; then echo "verilog work \"$$i\""; fi; done > $@

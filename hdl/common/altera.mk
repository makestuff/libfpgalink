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
ifeq ($(HDL),vhdl)
	HDL_EXT := vhdl
else ifeq ($(HDL),verilog)
	HDL_EXT := v
endif

$(TOP_LEVEL).qsf: ../../platforms/$(PLATFORM)/platform.qsf $(HDLS)
	cp $< $@
	for i in $+; do if [ "$${i##*.}" = "vhdl" ]; then echo "set_global_assignment -name VHDL_FILE $$i"; elif [ "$${i##*.}" = "v" ]; then echo "set_global_assignment -name VERILOG_FILE $$i"; fi; done >> $@

$(TOP_LEVEL).svf: $(TOP_LEVEL).qsf $(TOP_LEVEL).fit.rpt
	quartus_asm --read_settings_files=on --write_settings_files=off $(TOP_LEVEL) -c $(TOP_LEVEL)

$(TOP_LEVEL).fit.rpt: $(TOP_LEVEL).map.rpt
	quartus_fit --parallel=1 --read_settings_files=on --write_settings_files=off $(TOP_LEVEL) -c $(TOP_LEVEL)

$(TOP_LEVEL).sdc: ../../platforms/$(PLATFORM)/platform.sdc
	cp $< $@

$(TOP_LEVEL).map.rpt: $(TOP_LEVEL).$(HDL_EXT) $(TOP_LEVEL).sdc
	echo '{ "Warning" "WCPT_FEATURE_DISABLED_POST" "LogicLock " "Warning (292013): Feature LogicLock is only available with a valid subscription license. You can purchase a software subscription to gain full access to this feature." {  } {  } 0 292013 "Feature %1!s! is only available with a valid subscription license. You can purchase a software subscription to gain full access to this feature." 1 0 "" 0 -1}' > $(TOP_LEVEL).srf
	quartus_map --parallel=1 --read_settings_files=on --write_settings_files=off $(TOP_LEVEL) -c $(TOP_LEVEL)

FORCE:

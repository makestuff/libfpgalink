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
ROOT             := $(realpath ../..)
DEPS             := error usbwrap fx2loader nero sync buffer
TYPE             := dll
SUBDIRS          := tests-unit
PRE_BUILD        := $(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw
POST_BUILD       := tools gen_csvf
EXTRA_CC_SRCS    := gen_fw/ramFirmware.c gen_fw/eepromWithBootFirmware.c gen_fw/eepromNoBootFirmware.c
EXTRA_CLEAN      := gen_svf gen_xsvf gen_csvf gen_fw
EXTRA_CLEAN_DIRS := vhdl mkfw firmware xsvf2csvf dump

-include $(ROOT)/common/top.mk

MKFW := mkfw/$(PLATFORM)/rel/mkfw$(EXE)
X2C := xsvf2csvf/$(PLATFORM)/rel/xsvf2csvf$(EXE)

$(MKFW):
	make -C mkfw rel

tools:
	make -C xsvf2csvf rel
	make -C dump rel

gen_fw: $(MKFW)
	mkdir -p gen_fw
	make -C firmware clean
	make -C firmware
	LD_LIBRARY_PATH=$(dir $(MKFW)) $(MKFW) firmware/firmware.hex ram bix > gen_fw/ramFirmware.c
	cp -p firmware/firmware.hex gen_fw/ramFirmware.hex
	make -C firmware clean
	make -C firmware FLAGS="-DEEPROM -DBOOT"
	LD_LIBRARY_PATH=$(dir $(MKFW)) $(MKFW) firmware/firmware.hex eepromWithBoot iic gen_fw/eepromWithBootFirmware.iic > gen_fw/eepromWithBootFirmware.c
	make -C firmware clean
	make -C firmware FLAGS="-DEEPROM"
	LD_LIBRARY_PATH=$(dir $(MKFW)) $(MKFW) firmware/firmware.hex eepromNoBoot iic gen_fw/eepromNoBootFirmware.iic > gen_fw/eepromNoBootFirmware.c
	make -C firmware clean

gen_csvf:
	mkdir -p gen_svf
	mkdir -p gen_xsvf
	mkdir -p gen_csvf
	make -C vhdl clean
	make -C vhdl PLATFORM=nexys2-500 X2C=$(X2C) TopLevel.xsvf
	cp -rp vhdl/TopLevel.svf gen_svf/nexys2-500.svf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/nexys2-500.xsvf
	$(X2C) vhdl/TopLevel.xsvf gen_csvf/nexys2-500.csvf
	make -C vhdl clean
	make -C vhdl PLATFORM=nexys2-1200 X2C=$(X2C) TopLevel.xsvf
	cp -rp vhdl/TopLevel.svf gen_svf/nexys2-1200.svf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/nexys2-1200.xsvf
	$(X2C) vhdl/TopLevel.xsvf gen_csvf/nexys2-1200.csvf
	make -C vhdl clean
	make -C vhdl PLATFORM=s3board X2C=$(X2C) TopLevel.xsvf
	cp -rp vhdl/TopLevel.svf gen_svf/s3board.svf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/s3board.xsvf
	$(X2C) vhdl/TopLevel.xsvf gen_csvf/s3board.csvf
	make -C vhdl clean
	make -C vhdl PLATFORM=atlys X2C=$(X2C) TopLevel.xsvf
	cp -rp vhdl/TopLevel.svf gen_svf/atlys.svf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/atlys.xsvf
	$(X2C) vhdl/TopLevel.xsvf gen_csvf/atlys.csvf
	make -C vhdl clean
	make -C vhdl PLATFORM=nexys3 X2C=$(X2C) TopLevel.xsvf
	cp -rp vhdl/TopLevel.svf gen_svf/nexys3.svf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/nexys3.xsvf
	$(X2C) vhdl/TopLevel.xsvf gen_csvf/nexys3.csvf
	make -C vhdl clean

$(ROOT)/3rd/fx2lib/lib/fx2.lib: $(ROOT)/3rd/fx2lib
	make -C $<

tests: FORCE
	make -C tests rel

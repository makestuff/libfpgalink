#
# Copyright (C) 2011 Chris McClelland
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
ROOT             := $(realpath ../..)
DEPS             := error usbwrap fx2loader nero sync buffer
TYPE             := dll
SUBDIRS          := tests
PRE_BUILD        := $(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw gen_xsvf
EXTRA_CC_SRCS    := gen_fw/ramFirmware.c gen_fw/eepromWithBootFirmware.c gen_fw/eepromNoBootFirmware.c
EXTRA_CLEAN      := gen_xsvf gen_fw
EXTRA_CLEAN_DIRS := vhdl mkfw firmware

-include $(ROOT)/common/top.mk

MKFW := mkfw/$(PLATFORM)/rel/mkfw$(EXE)

$(MKFW):
	make -C mkfw rel

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

gen_xsvf:
	mkdir -p gen_xsvf
	make -C vhdl clean
	make -C vhdl PLATFORM=nexys2-500 TopLevel.xsvf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/nexys2-500.xsvf
	make -C vhdl clean
	make -C vhdl PLATFORM=nexys2-1200 TopLevel.xsvf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/nexys2-1200.xsvf
	make -C vhdl clean
	make -C vhdl PLATFORM=s3board TopLevel.xsvf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/s3board.xsvf
	make -C vhdl clean
	make -C vhdl PLATFORM=atlys TopLevel.xsvf
	cp -rp vhdl/TopLevel.xsvf gen_xsvf/atlys.xsvf
	make -C vhdl clean

$(ROOT)/3rd/fx2lib/lib/fx2.lib: $(ROOT)/3rd/fx2lib
	make -C $<

tests: FORCE
	make -C tests rel

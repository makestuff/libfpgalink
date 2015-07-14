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
DEPS             := error usbwrap fx2loader buffer
TYPE             := dll
SUBDIRS          := tests-unit
EXTRA_CC_SRCS    := gen_fw/ramFirmware.c gen_fw/eepromNoBootFirmware.c
#PRE_BUILD        := $(ROOT)/3rd/fx2lib/lib/fx2.lib
PRE_BUILD        += $(EXTRA_CC_SRCS)
POST_BUILD       := tools
#EXTRA_CLEAN      := gen_fw
EXTRA_CLEAN_DIRS := mkfw firmware/fx2 xsvf2csvf dump
AS8051           := sdas8051
#AS8051           := asx8051

-include $(ROOT)/common/top.mk

MKFW := mkfw/$(PM)/rel/mkfw$(EXE)
X2C := xsvf2csvf/$(PM)/rel/xsvf2csvf$(EXE)

$(MKFW):
	make -C mkfw rel

tools:
	make -C xsvf2csvf rel
	make -C dump rel

gen_fw/ramFirmware.c: gen_fw/ramFirmware.hex $(MKFW) gen_fw
	$(MKFW) $< ram bix > $@

gen_fw/eepromNoBootFirmware.c: gen_fw/eepromNoBootFirmware.hex $(MKFW) gen_fw
	$(MKFW) $< eepromNoBoot iic > $@

gen_fw/ramFirmware.hex:
	@echo Building RAM firmware...
	mkdir -p gen_fw
	make -C firmware/fx2 clean
	make -C firmware/fx2
	cp firmware/fx2/firmware.hex gen_fw/ramFirmware.hex
	make -C firmware/fx2 clean

gen_fw/eepromNoBootFirmware.hex:
	@echo Building EEPROM firmware...
	mkdir -p gen_fw
	make -C firmware/fx2 AS8051=$(AS8051) clean
	make -C firmware/fx2 AS8051=$(AS8051) FLAGS="-DEEPROM"
	cp firmware/fx2/firmware.hex gen_fw/eepromNoBootFirmware.hex
	make -C firmware/fx2 clean

hdl:
	./hdlbuild.sh $(X2C)

fx2lib: $(ROOT)/3rd/fx2lib/lib/fx2.lib

$(ROOT)/3rd/fx2lib/lib/fx2.lib: $(ROOT)/3rd/fx2lib
	make AS8051=$(AS8051) -C $<

tests: FORCE
	make -C tests-unit rel
	make -C tests-integration rel

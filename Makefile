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
DEPS             := error usbwrap fx2loader nero buffer
TYPE             := dll
#SUBDIRS          := tests-unit
PRE_BUILD        := $(ROOT)/3rd/fx2lib/lib/fx2.lib gen_fw
POST_BUILD       := tools #gen_csvf
EXTRA_CC_SRCS    := gen_fw/ramFirmware.c gen_fw/eepromWithBootFirmware.c gen_fw/eepromNoBootFirmware.c
EXTRA_CLEAN      := gen_svf gen_xsvf gen_csvf gen_fw
EXTRA_CLEAN_DIRS := mkfw firmware/fx2 xsvf2csvf dump

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
	make -C firmware/fx2 clean
	make -C firmware/fx2 FLAGS="-DJTAG_PORT=3 -DTDO_BIT=0 -DTDI_BIT=1 -DTMS_BIT=2 -DTCK_BIT=3"
	cp firmware/fx2/firmware.hex gen_fw/ramFirmware1.hex
	make -C firmware/fx2 clean
	make -C firmware/fx2 FLAGS="-DJTAG_PORT=2 -DTDO_BIT=7 -DTDI_BIT=6 -DTMS_BIT=5 -DTCK_BIT=4"
	cp firmware/fx2/firmware.hex gen_fw/ramFirmware2.hex
	make -C firmware/fx2 clean
	make -C firmware/fx2 FLAGS="-DJTAG_PORT=3 -DTDO_BIT=0 -DTDI_BIT=1 -DTMS_BIT=2 -DTCK_BIT=3 -DEEPROM -DBOOT"
	cp firmware/fx2/firmware.hex gen_fw/eepromWithBootFirmware1.hex
	make -C firmware/fx2 clean
	make -C firmware/fx2 FLAGS="-DJTAG_PORT=2 -DTDO_BIT=7 -DTDI_BIT=6 -DTMS_BIT=5 -DTCK_BIT=4 -DEEPROM -DBOOT"
	cp firmware/fx2/firmware.hex gen_fw/eepromWithBootFirmware2.hex
	make -C firmware/fx2 clean
	make -C firmware/fx2 FLAGS="-DJTAG_PORT=3 -DTDO_BIT=0 -DTDI_BIT=1 -DTMS_BIT=2 -DTCK_BIT=3 -DEEPROM"
	cp firmware/fx2/firmware.hex gen_fw/eepromNoBootFirmware1.hex
	make -C firmware/fx2 clean
	make -C firmware/fx2 FLAGS="-DJTAG_PORT=2 -DTDO_BIT=7 -DTDI_BIT=6 -DTMS_BIT=5 -DTCK_BIT=4 -DEEPROM"
	cp firmware/fx2/firmware.hex gen_fw/eepromNoBootFirmware2.hex
	make -C firmware/fx2 clean
	$(MKFW) gen_fw/ramFirmware1.hex gen_fw/ramFirmware2.hex ram bix > gen_fw/ramFirmware.c
	$(MKFW) gen_fw/eepromWithBootFirmware1.hex gen_fw/eepromWithBootFirmware2.hex eepromWithBoot iic > gen_fw/eepromWithBootFirmware.c
	$(MKFW) gen_fw/eepromNoBootFirmware1.hex gen_fw/eepromNoBootFirmware2.hex eepromNoBoot iic > gen_fw/eepromNoBootFirmware.c

gen_csvf:
	cd hdl && ./build.sh $(X2C)

$(ROOT)/3rd/fx2lib/lib/fx2.lib: $(ROOT)/3rd/fx2lib
	make -C $<

tests: FORCE
	make -C tests rel

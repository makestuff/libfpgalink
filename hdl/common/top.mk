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
THISDIR := $(dir $(lastword $(MAKEFILE_LIST)))
ifeq ($(PLATFORM),s3board)
	VENDOR   := xilinx
	FPGA     := xc3s200-ft256-4
	MAPFLAGS := -cm area
	PARFLAGS := -t 1
else ifeq ($(PLATFORM),nexys2-500)
	VENDOR   := xilinx
	FPGA     := xc3s500e-fg320-4
	MAPFLAGS := -cm area
	PARFLAGS := -t 1
else ifeq ($(PLATFORM),nexys2-1200)
	VENDOR   := xilinx
	FPGA     := xc3s1200e-fg320-4
	MAPFLAGS := -cm area
	PARFLAGS := -t 1
else ifeq ($(PLATFORM),atlys)
	VENDOR   := xilinx
	FPGA     := xc6slx45-csg324-3
	MAPFLAGS :=
	PARFLAGS :=
else ifeq ($(PLATFORM),nexys3)
	VENDOR   := xilinx
	FPGA     := xc6slx16-csg324-3
	MAPFLAGS :=
	PARFLAGS :=
else ifeq ($(PLATFORM),xylo-l)
	VENDOR   := xilinx
	FPGA     := xc3s500e-pq208-4
	MAPFLAGS := -cm area
	PARFLAGS := -t 1
else ifeq ($(PLATFORM),lx9)
	VENDOR   := xilinx
	FPGA     := xc6slx9-tqg144-2
	MAPFLAGS :=
	PARFLAGS :=
endif

include $(THISDIR)/$(VENDOR).mk

X2C := xsvf2csvf

$(TOP_LEVEL).csvf: $(TOP_LEVEL).xsvf
	$(X2C) $(TOP_LEVEL).xsvf $(TOP_LEVEL).csvf

FORCE:

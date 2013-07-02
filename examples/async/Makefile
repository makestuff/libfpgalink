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
ROOT    := $(realpath ../../../..)
DEPS    := fpgalink usbwrap error
TYPE    := exe
SUBDIRS := tests

ifeq ($(OS),Windows_NT)
	LIBUSB_VERSION := 1.0.14
	ifdef PROCESSOR_ARCHITEW6432
		LINK_EXTRALIBS_REL := $(ROOT)/3rd/libusbx-$(LIBUSB_VERSION)-win/MS64/dll/libusb-1.0.lib
		EXTRADLLS_REL := $(ROOT)/3rd/libusbx-$(LIBUSB_VERSION)-win/MS64/dll/libusb-1.0.dll
	else
		LINK_EXTRALIBS_REL := $(ROOT)/3rd/libusbx-$(LIBUSB_VERSION)-win/MS32/dll/libusb-1.0.lib
		EXTRADLLS_REL := $(ROOT)/3rd/libusbx-$(LIBUSB_VERSION)-win/MS32/dll/libusb-1.0.dll
	endif
	LINK_EXTRALIBS_DBG := $(LINK_EXTRALIBS_REL)
	EXTRADLLS_DBG := $(EXTRADLLS_REL)
	EXTRA_INCS := -I$(ROOT)/3rd/libusbx-$(LIBUSB_VERSION)-win/include
	PRE_BUILD := $(ROOT)/3rd/libusbx-$(LIBUSB_VERSION)-win
else
	LINK_EXTRALIBS_REL := -lusb-1.0
	LINK_EXTRALIBS_DBG := $(LINK_EXTRALIBS_REL)
endif

-include $(ROOT)/common/top.mk

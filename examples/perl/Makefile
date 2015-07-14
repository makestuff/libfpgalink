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
DEPS    := fpgalink
TYPE    := dll
SUBDIRS :=
LOCALNAME := fl
EXTRA_CC_SRCS := $(LOCALNAME)_wrap.c
PRE_BUILD := $(EXTRA_CC_SRCS)
EXTRA_CLEAN := $(EXTRA_CC_SRCS) $(LOCALNAME).pm
ifeq ($(OS),Windows_NT)
	MACHINE := $(shell echo $${MACHINE})
	PLATFORM := $(shell echo $${PLATFORM})
	ifeq ($(PLATFORM),mingw)
		CSTD := gnu99
		ALIASING := -fno-strict-aliasing
		ifeq ($(MACHINE),x64)
			EXTRA_INCS := -I/c/Perl64/lib/CORE
			LINK_EXTRALIBS_REL := /c/Perl64/lib/CORE/libperl518.a
		else ifeq ($(MACHINE),x86)
			EXTRA_INCS := -I/c/Perl/lib/CORE
			LINK_EXTRALIBS_REL := /c/Perl/lib/CORE/libperl518.a
		endif
	else
		ifeq ($(MACHINE),x64)
			EXTRA_INCS := -I/c/Perl64/lib/CORE
			LINK_EXTRALIBS_REL := /c/Perl64/lib/CORE/perl516.lib
		else ifeq ($(MACHINE),x86)
			EXTRA_INCS := -I/c/Perl/lib/CORE
			LINK_EXTRALIBS_REL := /c/Perl/lib/CORE/perl516.lib
		endif
	endif
	LINK_EXTRALIBS_DBG := $(LINK_EXTRALIBS_REL)
	SWIG := $HOME/3rd/swigwin-2.0.12/swig.exe
else
	EXTRA_INCS := -I/usr/lib/perl/5.18.2/CORE
	EXTRA_CFLAGS := -DLINUX=2 -D_REENTRANT -D_LARGEFILE64_SOURCE
	CSTD := gnu99
	ALIASING := -fno-strict-aliasing
	SWIG := swig2.0
endif

-include $(ROOT)/common/top.mk

$(LOCALNAME)_wrap.c: $(LOCALNAME).i
	$(SWIG) -perl5 $(DEBUG) $<
	cat extra.pm >> $(LOCALNAME).pm

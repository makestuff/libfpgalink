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
EXTRAS := fifo.ngc $(COMMON)/$(HDL)/fifo_gen/$(PLATFORM)/fifo.v
HDLS += $(COMMON)/$(HDL)/fifo_gen/fifo_wrapper_xilinx.v $(COMMON)/$(HDL)/fifo_gen/$(PLATFORM)/fifo.v

fifo.ngc: $(COMMON)/$(HDL)/fifo_gen/$(PLATFORM)/fifo.ngc
	cp -p $< $@

$(COMMON)/$(HDL)/fifo_gen/$(PLATFORM)/fifo.v $(COMMON)/$(HDL)/fifo_gen/$(PLATFORM)/fifo.ngc: $(COMMON)/$(HDL)/fifo_gen/$(PLATFORM)/fifo_gen.batch
	cd $(COMMON)/$(HDL)/fifo_gen/$(PLATFORM) && coregen -b fifo_gen.batch

fifoclean: FORCE
	cd $(COMMON)/$(HDL)/fifo_gen && for i in $$(find . -maxdepth 1 -mindepth 1 -type d); do mv $$i/fifo_gen.batch .; rm -rf $$i; mkdir $$i; mv fifo_gen.batch $$i; done

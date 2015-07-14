INTRODUCTION
************

Justification
=============

Development kits for Field Programmable Gate Arrays (FPGAs) are ubiquitous, with offerings from a plethora of manufacturers, with prices ranging from the tens of dollars to well into the thousands, and featured FPGAs ranging from a few thousand logic cells to a few million. Whereas the high-end boards tend to be PCIx plug-in cards, the cheaper boards tend either to be designed around a USB interface chip (e.g Cypress FX2LP, Atmel AVR, Microchip PIC or FTDI chip), or lack direct host interfacing altogether, requiring a standalone JTAG cable for programming. Unfortunately, even for those boards designed around a USB interface, there is a general lack of good integrated solutions for exchanging arbitrary data between the host computer and the FPGA, once it has been programmed.

Overview
========

FPGALink is an end-to-end, open-source, cross-platform solution designed to do a few simple jobs, and do them well:

* Directly program the FPGA during development, using JTAG or one of the vendor-specific serial and parallel programming algorithms.

* Program onboard flash chips used by the FPGA to boot on power-on.

* Allow the host and/or microcontroller to exchange arbitrary binary data with the FPGA, using a variety of connection methods.

It provides a host-side API, firmware for several USB interface microcontrollers, and 128 addressable eight-bit read/write FIFOs on the FPGA side.

* On the host side there is a dynamic-link library with a straightforward API. The libraries and example application code will build on MacOSX (x64 & x86), Windows (x64 & x86) and Linux (x64, x86, ARM & PowerPC). Bindings are provided for C/C++, Python, Perl, Java and Excel/VBA; binding other languages is straightforward.

* For the USB interface there are firmwares for the `Cypress FX2LP <http://www.cypress.com/?id=193>`_ (used on most Digilent, Aessent, KNJN, ZTEX and Opal Kelly boards) and `Atmel AVR <http://en.wikipedia.org/wiki/Atmel_AVR>`_ (used by some AVNet, Digilent, Embedded Micro and Lattice boards). There's also an `NXP LPC <http://en.wikipedia.org/wiki/NXP_LPC>`_ firmware which unfortunately lacks an active maintainer.

* The Cypress FX2LP firmware supports a synchronous FIFO interface with a sustained bandwidth of around 42MiB/s. The Atmel AVR firmware supports an asynchronous parallel interface (using the `IEEE1284 EPP Protocol <http://www.fapo.com/eppmode.htm>`_) and a synchronous serial interface, both of which have a sustained bandwidth of around 500KiB/s.

* On the FPGA side there are several simple interface modules with separate implementations in VHDL and Verilog. Each module gives the host a FIFO-style read/write interface into your FPGA design, supporting up to 128 separate logical "channels" with flow-control. A couple of fully-functional example designs are provided to get you started.

Everything is licensed under the `GNU Lesser General Public Licence <http://www.gnu.org/copyleft/lesser.html>`_; you are therefore free to distribute unmodified copies of FPGALink with your products. The library has no commercial or hardware usage restrictions, so you can prototype your design with an inexpensive devkit, and then use the same software tools on your custom-built PCBs. In this way you can easily distribute updated FPGA designs to your customers just as you would with regular firmware updates, with no special programming cables required, making your FPGA truly "field-programmable".

Conventions
===========

* 1MB = 1 megabyte = 10\ :sup:`6` bytes = 1,000,000 bytes.

* 1MiB = 1 mebibyte = 2\ :sup:`20` bytes = 1,048,576 bytes.

* 1Mb = 1 megabit = 10\ :sup:`6` bits = 1,000,000 bits.

* 1Mib = 1 mebibit = 2\ :sup:`20` bits = 1,048,576 bits.

How to get help
===============

The only place you’re *guaranteed* to get a response to FPGALink-related queries is the `FPGALink Users Group <http://groups.google.com/group/fpgalink-users>`_.

Licences & Disclaimers
======================

The FPGALink library, firmware & HDL code is licensed under the LGPLv3:

  Copyright © 2009-2014 Chris McClelland

  FPGALink is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

  FPGALink is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the `GNU Lesser General Public License <http://www.gnu.org/copyleft/lesser.html>`_ for more details.

What does it mean to apply a free software licence like the LGPLv3 to FPGALink's VHDL and Verilog modules? Broadly, the intent is to enable you to use them *unmodified* in your own (perhaps commercial & proprietary) products without invoking the copyleft clause. However, if you *modify* any FPGALink VHDL or Verilog source file, the result is a derived work which must only be distributed under the LGPLv3 licence. In practice it's sufficient to email your modifications to the `fpgalink-users <https://groups.google.com/forum/#!forum/fpgalink-users>`_ mailing list. In summary, I will never ask you to reveal *your* intellectual property; I only ask that you reveal your improvements to *my* intellectual property.

The FLCLI utility is licensed under the GPLv3:

  Copyright © 2009-2014 Chris McClelland

  FLCLI is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

  FLCLI is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the `GNU General Public License <http://www.gnu.org/copyleft/gpl.html>`_ for more details.

The Gordon flash tool is conceptually based on ideas from the `flashrom project <http://flashrom.org>`_ and is thus licensed under the GPLv2:

  Copyright © 2013-2014 Chris McClelland

  Gordon is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 of the License.

  Gordon is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the `GNU General Public License <http://www.gnu.org/licenses/gpl-2.0.html>`_ for more details.

If you have specific licensing concerns, just ask on the `mailing list <https://groups.google.com/forum/#!forum/fpgalink-users>`_. Don't worry, I won't bite!

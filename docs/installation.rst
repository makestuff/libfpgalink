INSTALLATION
************

Linux Installation
==================

Prerequisites
-------------

You'll need a gcc compiler and the development packages :command:`ABCDEFGHIJKLMNOPQRSTUVWXYZ` for :command:`libusb` and :command:`libreadline`. For example, on a Debian-derived Linux distribution (e.g Debian, Ubuntu & Mint), thus:

.. code-block:: console

  $ sudo apt-get install build-essential libreadline-dev libusb-1.0-0-dev python-yaml

If you want to use the Atmel AVR firmware, you'll also need the AVR toolchain:

.. code-block:: console

  $ sudo apt-get install gcc-avr avr-libc dfu-programmer

The Cypress FX2LP firmware is provided as a pre-built :command:`.hex` file, so you don't *need* to build it from source, but if you want to do so, you'll need :command:`sdcc`:

.. code-block:: console

  $ sudo apt-get install sdcc

For building the VHDL and/or Verilog examples for loading into your FPGA you'll need to install the FPGA vendor tools. For Xilinx FPGAs you'll need `ISE WebPACK <http://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/design-tools.html>`_ and for Altera FPGAs you'll need `Quartus II Web Edition <http://dl.altera.com/?edition=web>`_.

.. note::

  Pay close attention to the supported device families before you download the software: Altera especially have a habit of dropping tooling support for devices fairly rapidly, so unless your FPGA is very recent you may need to download an older software release.

On Debian/testing amd64 I did this:

If you want to run the VHDL in simulation you'll need GHDL and GTKWave. You can get a recent GHDL from Joris van Rantwijk:

.. code-block:: console

  $ wget http://jorisvr.nl/debian/ghdl/ghdl_0.30~svn20130213-2_amd64.deb
  $ sudo dpkg --install ghdl_0.30~svn20130213-2_amd64.deb
  $ sudo apt-get -f install  # fix dependencies 

And you can install GTKWave from the standard repository:

.. code-block:: console

  $ sudo apt-get install gtkwave

Blah
====

Blah blah blah.

  Copyright (C) 2009-2014 Chris McClelland
  
  This program is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.

Blah.


FooBar
======

.. note::

  Blah note!

Foo bar!

Code:

.. code-block:: html
 :linenos:

 <h1>code block example</h1>

Foo:

.. code-block:: c
  :linenos:

  // 456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
  #include <stdio.h>

  int main(void) {
      printf("Hello World\n");
  }

See?

.. code-block:: vhdl
  :linenos:

  package mem_ctrl_pkg is
      type MCCmdType is (
          MC_NOP,
          MC_RD,
          MC_WR,
          MC_REF
      );
      component mem_ctrl is
          generic (
              INIT_COUNT     : unsigned(12 downto 0);  -- cycles to wait during initialisation
              REFRESH_DELAY  : unsigned(12 downto 0);  -- gap between refresh cycles
              REFRESH_LENGTH : unsigned(12 downto 0)   -- length of a refresh cycle
          );
          port(
              clk_in         : in    std_logic;
              reset_in       : in    std_logic;
  
              -- Client interface
              mcAutoMode_in  : in    std_logic;
              mcCmd_in       : in    MCCmdType;
              mcAddr_in      : in    std_logic_vector(22 downto 0);
              mcData_in      : in    std_logic_vector(15 downto 0);
              mcData_out     : out   std_logic_vector(15 downto 0);
              mcRDV_out      : out   std_logic;
              mcReady_out    : out   std_logic;
  
              -- SDRAM interface
              ramCmd_out     : out   std_logic_vector(2 downto 0);
              ramBank_out    : out   std_logic_vector(1 downto 0);
              ramAddr_out    : out   std_logic_vector(11 downto 0);
              ramData_io     : inout std_logic_vector(15 downto 0);
              ramLDQM_out    : out   std_logic;
              ramUDQM_out    : out   std_logic
          );
      end component;
  end package;

See?

.. image:: images/fx2minHarness.png

And that's all.

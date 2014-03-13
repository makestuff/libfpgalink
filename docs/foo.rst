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

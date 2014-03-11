Blah
====

Blah blah blah.

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

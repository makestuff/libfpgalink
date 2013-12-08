#ifndef MINIMUS_H
#define MINIMUS_H

// Platform definitions
#define MCU at90usb162
#define XTAL 16000000

// Bootloader definitions
#define FLASH_SIZE_BYTES          16384
#define BOOTLOADER_SEC_SIZE_BYTES 4096

// Hardware SPI bits
#define SPI_PORT 1
#define SS_BIT   0
#define SCK_BIT  1
#define MOSI_BIT 2
#define MISO_BIT 3

// JTAG/SPI definitions (all unused - we use hardware SPI instead)
#define TDO_PORT 1
#define TDI_PORT 1
#define TMS_PORT 1
#define TCK_PORT 1
#define TDO_BIT  3
#define TDI_BIT  2
#define TMS_BIT  0
#define TCK_BIT  1

// EPP definitions
#define EPP_CTRL_NAME C
#define EPP_DATA_NAME D
#define EPP_ADDRSTB   4
#define EPP_DATASTB   5
#define EPP_WRITE     6
#define EPP_WAIT      7

// Serial port definitions
#define SER_NAME D
#define SER_RX 2
#define SER_TX 3
#define SER_CK 5

// Parallel port definitions
#define PAR_PORT 3

// Miscellaneous
#define REG_ENABLED 0

#endif

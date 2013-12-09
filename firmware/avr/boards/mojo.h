#ifndef MOJO_H
#define MOJO_H

// Platform definitions
#define MCU at90usb162
#define XTAL 8000000

// Bootloader definitions
#define FLASH_SIZE_BYTES          16384
#define BOOTLOADER_SEC_SIZE_BYTES 4096

// Hardware SPI bits
#define SPI_PORT 1
#define SS_BIT   0
#define SCK_BIT  1
#define MOSI_BIT 2
#define MISO_BIT 3

// JTAG/SPI definitions
#define TDO_PORT 1 // unused
#define TDI_PORT 1
#define TMS_PORT 1 // unused
#define TCK_PORT 3
#define TDO_BIT  0 // unused
#define TDI_BIT  0
#define TMS_BIT  0 // unused
#define TCK_BIT  0

// EPP definitions
#define CTRL_PORT   3
#define DATA_PORT   1
#define ADDRSTB_BIT 1
#define DATASTB_BIT 5
#define WRITE_BIT   3
#define WAIT_BIT    2

// Serial port definitions
#define SER_NAME D
#define SER_RX 2
#define SER_TX 3
#define SER_CK 5

// Parallel port definitions
#define PAR_PORT 1

// Miscellaneous
#define REG_ENABLED 0

#endif

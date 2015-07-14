#ifndef MINIMUS_H
#define MINIMUS_H

// Platform definitions
#define MCU at90usb162
#define XTAL 16000000

// Miscellaneous
#define REG_ENABLED  1

// Bootloader definitions
#define FLASH_SIZE_BYTES          16384
#define BOOTLOADER_SEC_SIZE_BYTES 4096

// Hardware SPI bits
#define HW_SPI_PORT  1
#define HW_MISO_BIT  3
#define HW_MOSI_BIT  2
#define HW_SS_BIT    0
#define HW_SCK_BIT   1

//#define TEST_DUAL
#ifdef TEST_DUAL
	// Test dual-JTAG (bit-bang on B6B5B4B7, HW-SPI on B3B2B0B1)
	#define BB_MISO_PORT 1
	#define BB_MOSI_PORT 1
	#define BB_SS_PORT   1
	#define BB_SCK_PORT  1
	#define BB_MISO_BIT  6
	#define BB_MOSI_BIT  5
	#define BB_SS_BIT    4
	#define BB_SCK_BIT   7
#else
	// Bit-bang SPI definitions (same as hardware SPI but MISO/MOSI swapped)
	#define BB_MISO_PORT HW_SPI_PORT
	#define BB_MOSI_PORT HW_SPI_PORT
	#define BB_SS_PORT   HW_SPI_PORT
	#define BB_SCK_PORT  HW_SPI_PORT
	#define BB_MISO_BIT  HW_MOSI_BIT
	#define BB_MOSI_BIT  HW_MISO_BIT
	#define BB_SS_BIT    HW_SS_BIT
	#define BB_SCK_BIT   HW_SCK_BIT
#endif

// EPP definitions
#define CTRL_PORT    2
#define DATA_PORT    3
#define ADDRSTB_BIT  4
#define DATASTB_BIT  5
#define WRITE_BIT    6
#define WAIT_BIT     7

// Serial port definitions
#define USART_PORT   3
#define RX_BIT       2
#define TX_BIT       3
#define XCK_BIT      5

// SelectMap programming port
#define PAR_PORT     3

#endif

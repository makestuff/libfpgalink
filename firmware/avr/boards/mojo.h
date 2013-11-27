#ifndef MOJO_H
#define MOJO_H

#define MCU at90usb162
#define XTAL 8000000

#define FLASH_SIZE_BYTES          16384
#define BOOTLOADER_SEC_SIZE_BYTES 4096

#define TCK_PORT    PORTD
#define bmTCK       0x01
#define TDI_PORT    PORTB
#define bmTDI       0x80
#define EPP_CTRL    D
#define EPP_DATA    B
#define EPP_ADDRSTB 1
#define EPP_DATASTB 5
#define EPP_WRITE   3
#define EPP_WAIT    2

#define PROG_HWSPI  0
#define REG_ENABLED 0

#endif

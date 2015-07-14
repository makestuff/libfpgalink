// This code modified slightly from:
//   http://www.fourwalledcubicle.com/files/LUFA/Doc/130901/html/_page__software_bootloader_start.html
//   Copyright (C) 2013 Dean Camera
//
#include <avr/wdt.h>
#include <avr/io.h>
#include <util/delay.h>

#include <LUFA/Common/Common.h>
#include <LUFA/Drivers/USB/USB.h>

#include <makestuff.h>
#include STR(boards/BSP.h)

uint32_t Boot_Key ATTR_NO_INIT;

#define MAGIC_BOOT_KEY 0xDC42ACCA
#define BOOTLOADER_START_ADDRESS (FLASH_SIZE_BYTES - BOOTLOADER_SEC_SIZE_BYTES)

void Bootloader_Jump_Check(void) ATTR_INIT_SECTION(3);
void Bootloader_Jump_Check(void)
{
	// If the reset source was the bootloader and the key is correct, clear it and jump to the bootloader
	if (
		//(MCUSR & (1 << WDRF)) &&
		(Boot_Key == MAGIC_BOOT_KEY)
	) {
		Boot_Key = 0;
		__asm volatile("jmp " STR(BOOTLOADER_START_ADDRESS)::);
	}
}

void Jump_To_Bootloader(void)
{
	Boot_Key = MAGIC_BOOT_KEY;

	// Wait for any in-progress request to complete
	Delay_MS(100);

	// If USB is used, detach from the bus and reset it
	USB_Disable();
	
	// Disable all interrupts
	cli();
	
	// Wait a second for the USB detachment to register on the host
	Delay_MS(2000);
	
	// Set the bootloader key to the magic value and force a reset
	wdt_enable(WDTO_250MS);
	for (;;);
}

#include "mbed.h"
#include "USBFPGALink.h"

DigitalInOut greenLed(P0_18);
DigitalInOut redLed(P0_19);

USBFPGALink fpgaLink;

int main()
{
	// start message
	printf("MakeStuff FPGALink/LPC v0.1\r\n");
	
	// set LED pins to output
	greenLed.output();
	redLed.output();
	
	// red LED on, green LED off
	greenLed = 1;
	redLed = 0;
	
	// try to connect
	printf("USB connect...\r\n");
	fpgaLink.connect();
	printf("connected\r\n");

	// red LED off
	redLed = 1;

	// rest in interrupt
	while (1) {
		//greenLed = 0;
		wait_ms(500);
		//greenLed = 1;
		wait_ms(500);
		//printf("test\r\n");
	}
}

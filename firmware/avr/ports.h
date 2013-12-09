#ifndef PORTS_H
#define PORTS_H

#define PORT(x) CONCAT(PORT_NUM_, x)
#define PIN(x) CONCAT(PIN_NUM_, x)
#define DDR(x) CONCAT(DDR_NUM_, x)

#define PORT_NUM_0 PORTA
#define PORT_NUM_1 PORTB
#define PORT_NUM_2 PORTC
#define PORT_NUM_3 PORTD
#define PORT_NUM_4 PORTE
#define PIN_NUM_0 PINA
#define PIN_NUM_1 PINB
#define PIN_NUM_2 PINC
#define PIN_NUM_3 PIND
#define PIN_NUM_4 PINE
#define DDR_NUM_0 DDRA
#define DDR_NUM_1 DDRB
#define DDR_NUM_2 DDRC
#define DDR_NUM_3 DDRD
#define DDR_NUM_4 DDRE

#endif

#include <avr/io.h>
#include <util/delay.h>
#include "ota.h"


int main(void){

WDT_clear();
OSC_CAL();
UART_init();
_delay_ms(20);
Page_write_handler();

}
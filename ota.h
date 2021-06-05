
/*INCLUDE LIBRARIES*/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <string.h>

//+------------------------------------------------------------------------------------+
//|DEFINE    NAME                        VALUE                   COMMENTS              |
//+------------------------------------------------------------------------------------+

#define     TIMEOUT_MS                  500                     //USER CONFIGURABLE
#define     PROCESSOR_RX_WAIT           (2*TIMEOUT_MS)          //DO NOT CHANGE

#define     BOOT_READY_FOR_ENCRYPT_KEY  27                      //DO NOT CHANGE
#define     BOOT_RETRY                  25                      //DO NOT CHANGE
#define     BOOT_READY_FOR_DATA         24                      //DO NOT CHANGE
#define     BOOT_PAGE_COMPLETE          23                      //DO NOT CHANGE
#define     BOOT_COMPLETE               22                      //DO NOT CHANGE
#define     BOOT_EXIT                   21                      //DO NOT CHANGE


#define     BOOT_PAGE_STRT_ADDR         224                     //DO NOT CHANGE
#define     BOOT_PAGE_SIZE_BYTES        128                     //DO NOT CHANGE

#define     JUMP_APP()                  {asm("jmp 0x0000");}    //DO NOT CHANGE   
#define     WDT_clear()                 cli();MCUSR =0x00;WDTCSR=0x18;WDTCSR=0x00;
#define     WDT_reset()                 WDTCSR=0x18;WDTCSR=(1<<WDE);while(1);
#define     UART_init()                 UBRR0H=0x00;UBRR0L=0x0B;TCCR1B=0x05;\
                                        UCSR0B=0x18;UCSR0C=0x06;
#define     OSC_FREQUENCY_CAL           3686     //3.686 MHz
#define     OSC_FREQUENCY_TOLERENCE     40       //40 kHz 

//UBRR0L=0x17  9600   BPS
//UBRR0L=0x0B  19200  BPS
//UBRR0L=0x05  38400  BPS
//UBRR0L=0x01  115200 BPS


uint8_t  ENCRYPT_KEY[8]="1234ABCD",RCVD_ENCRPT_KEY[11];
uint8_t  BOOT_buffer[BOOT_PAGE_SIZE_BYTES];
uint8_t  allow_erase=0,error=0;
uint16_t INCOMING_PAGE=0;

void CLEAR_peripherals(void){
UBRR0H=0x00;UBRR0L=0x00;UCSR0B=0x00;UCSR0C=0x00;
TCCR1A=0x00;TCCR1B=0x00;TCCR1C=0x00;TCNT1 =0x00;
TIMSK2=0x00;ASSR  =0x00;TCNT2 =0x00;TCCR2A=0x00;
TCCR2B=0x00;TIFR2 =0x00;
}



void UART_TX_SINGLE(uint8_t x){
while(!(UCSR0A & (1<<UDRE0)));
UDR0=x;
while(!(UCSR0A & (1<<TXC0)));
}

void boot_program_page (uint32_t page, uint8_t *buf){

    uint16_t i;
    uint8_t sreg;
    sreg = SREG;
    cli();
    eeprom_busy_wait ();
    boot_page_erase (page);
    boot_spm_busy_wait (); 
	
    for (i=0; i<SPM_PAGESIZE; i+=2)
    {
        uint16_t w = *buf++;
        w += (*buf++) << 8;
        boot_page_fill (page + i, w);
    }
	
    boot_page_write (page); 
    boot_spm_busy_wait();   
    boot_rww_enable ();
    SREG = sreg;
}


void chip_erase(uint8_t strt, uint8_t stop){
uint8_t erase_buf[BOOT_PAGE_SIZE_BYTES];
for(uint8_t i=0;i<BOOT_PAGE_SIZE_BYTES;i++){
    erase_buf[i]=0xFF;
	}
for(uint8_t page_erase=strt;page_erase<stop;page_erase++){
    boot_program_page((page_erase*BOOT_PAGE_SIZE_BYTES),erase_buf);
	}
}


uint8_t UART_rx(void){
TCNT1=0;
while((UCSR0A & (1<<RXC0))==0)
   {
     if(TCNT1>PROCESSOR_RX_WAIT)
	 {
	   if(allow_erase==1){chip_erase(0,BOOT_PAGE_STRT_ADDR);}
	   CLEAR_peripherals();
	   JUMP_APP();
	  }
	}
return UDR0;
}


void Page_write_handler(void)
{
    allow_erase=0;
    UART_TX_SINGLE(BOOT_READY_FOR_ENCRYPT_KEY);

    for(uint8_t i=0;i<11;i++){RCVD_ENCRPT_KEY[i]=UART_rx();}
    for(uint8_t i=0;i<8;i++){if(ENCRYPT_KEY[i]!=RCVD_ENCRPT_KEY[i]){error=1;}}
    INCOMING_PAGE=RCVD_ENCRPT_KEY[8]*100+RCVD_ENCRPT_KEY[9]*10+RCVD_ENCRPT_KEY[10];

    if(error!=1)
       {
        
        if(INCOMING_PAGE!=0)
           {
             UART_TX_SINGLE(BOOT_READY_FOR_DATA);
             allow_erase=1;
             for(uint16_t page=0;page<INCOMING_PAGE;page++)
	           {
                 
                 for(uint16_t i=0;i<BOOT_PAGE_SIZE_BYTES;i++)
		           {
		             BOOT_buffer[i]=UART_rx();
		            }
		         boot_program_page((page*BOOT_PAGE_SIZE_BYTES),BOOT_buffer);
		         UART_TX_SINGLE(BOOT_PAGE_COMPLETE);
	            }
			 error=0;
             chip_erase(INCOMING_PAGE,BOOT_PAGE_STRT_ADDR);
             UART_TX_SINGLE(BOOT_COMPLETE);
             while(UART_rx()!=BOOT_EXIT);
	        }
        }
		
    else
	{
        UART_TX_SINGLE(BOOT_RETRY);
    }

CLEAR_peripherals();
JUMP_APP();
}


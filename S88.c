#include "S88.h"
#include <string.h>
#include <util/delay.h>
#include <avr/eeprom.h> 
#include <stddef.h>

#define EEREFR (uint16_t*)0x0080

#ifdef ATMEGA32u4
// PIN OUT S88N
#define START_TIMER1 TCCR1B |= (1<<CS10)|(1<<CS11)|(1<<CS12)
#define STOP_TIMER1  TCCR1B &= 0B11111000 

#define CLK PB6
#define PS PB4
#define DATA PB0
#define RST PB5
#define PWR PB3

#define S88PORT PORTB
#define S88PIN PINB
#define S88DDR DDRB
#else
#define START_TIMER1 TCCR0B |= (1<<CS01)
#define STOP_TIMER1  TCCR0B &= 0B11111000 

#define CLK PA0
#define PS PA1
#define DATA PA3
#define RST PA2
#define PWR PA4

#define S88PORT PORTA
#define S88PIN PINA
#define S88DDR DDRA
#endif

volatile S88_t* _S88;
volatile S88_State_t State;

#ifdef ATMEGA32u4
	ISR(TIM1_COMPA_vect) {
#else
	ISR(TIM0_COMPA_vect) {
#endif
  asm("sbi %0, %1" : :"I" (_SFR_IO_ADDR(S88PIN)), "I" (PA5));
  if (State.state == CLOCK) {
    //asm("sbi 0x03, 6");
	//  S88PORT ^= (1<<CLK); // Toggle the pin
	asm("sbi %[s88pin], %[CLKBIT] ; Toggle the clock pin\n\t"
		:
		: [s88pin] "I" (_SFR_IO_ADDR(S88PIN)), 
	      [CLKBIT]"I" (CLK));
    State.CLKC--;
    if  (!(S88PIN & (1 << CLK))) // Down flank
    { 
      State.bit = (State.bit >> 1);
      if (State.bit == 0) {
		_S88->Config.data[_S88->Config.activeData][State.module] = State.currentByte;
		State.currentByte = 0;  
        State.module++;
        State.bit = 128;
      }
/*      asm("sbi %[s88pin], %[CLKBIT] ; Toggle the clock pin\n\t"
//  		"lds __tmp_reg__, %a[CB]\n\t"
  		"sbic %[s88pin], %[DATABIT] ; Skip next instruction if dataline low \n\t"
  		"sbr %[r1], %[BIT]\n\t"
 			: [r1] "+r" (State.currentByte) 
	  		: [s88pin] "I" (_SFR_IO_ADDR(S88PIN)), 
			  [CLKBIT] "I" (CLK), 
			  [DATABIT] "I" (DATA), 
			  [BIT] "r" (State.bit));*/
	  asm("sbic %[s88pin], %[DATABIT] ; Skip setting the bit if data line low\n\t"
  		"or %[cb], %[bit]\n\t"
  		: [cb] "+r" (State.currentByte) 
  		: [s88pin] "I" (_SFR_IO_ADDR(S88PIN)), 
  		  [DATABIT] "I" (DATA),
		  [bit] "r" (State.bit));
    }
    if (State.CLKC == 0) 
    {
      State.state = SENDDATA;
      State.module = 0; // Used again for sending the data
//      State.timeout = 0;
    }
  } else if (State.state == PRERESET) 
    { // Pulse reset
      S88PORT ^= (1<<RST); // Toggle the pin
      State.state = RESET;
//    	PINA = 0xf;
  } else if (State.state == RESET) {
    S88PORT ^= (1<<RST); // Toggle the pin
    State.state = POSTLOAD;
    State.module = 0;
  } else if (State.state == STARTREAD) { // && !(S88PIN & (1 << PS))) {
    State.state = PRELOADCLK;
	S88PORT &= ! ((1<<CLK) | (1<<PS) | (1<<RESET));
    S88PORT ^= (1<<PS);
  } else if (State.state == PRELOADCLK) { // && (S88PIN & (1 << PS))) {
    State.CLKC--;
    S88PORT ^= (1<<CLK); // Toggle the pin (up)
    State.state = POSTLOADCLK;
  } else if (State.state == POSTLOADCLK) { //} && (S88PIN & (1 << CLK))) {
    State.CLKC--;
    State.bit = 128;
    asm("sbi %[s88pin], %[CLKBIT] ; Toggle the clock pin\n\t"
		"sbic %[s88pin], %[DATABIT] ; Skip setting the bit if data line low\n\t"
		"sbr %[cb], 0\n\t"
		: [cb] "+r" (State.currentByte) 
		: [s88pin] "I" (_SFR_IO_ADDR(S88PIN)), 
		  [CLKBIT]"I" (CLK), 
		  [DATABIT] "I" (DATA));
		 //_S88->Config.data[_S88->Config.activeData][State.module] |= ((S88PIN & (1<<DATA)) ? 1 : 0) ;
    State.state = PRERESET;
  } else if (State.state == POSTLOAD) {// && !(S88PIN & (1 << CLK))) {
    asm("sbi %0, %1" : :"I" (_SFR_IO_ADDR(S88PIN)), "I" (PS));
    State.state = CLOCK;
  }
  asm("sbi %0, %1" : :"I" (_SFR_IO_ADDR(S88PIN)), "I" (PA5));
}

/**
* Set the number of S88 modules on a bus.
* 
**/
void SetNoModules(S88_t* S88, uint8_t modules, uint8_t bus) {
      S88->Config.modules[bus] = modules;
      S88->Config.maxModules = (S88->Config.maxModules > S88->Config.modules[bus]) ? S88->Config.maxModules : S88->Config.modules[bus];
      S88->Config.totalModules = S88->Config.modules[0] + S88->Config.modules[1] + S88->Config.modules[2];
      S88->Config.autoTimeout = (S88->Config.maxModules * 2 * 16) + 10; // 32 Timer1 compares per module and 10 for the RESET and LOAD/PS header.	
}

int8_t IsReady(S88_t* S88){
	if (S88->State.state == SENDDATA) {
		return 1;
	} else {
		return 0;
	}
}

void SwapAndClearS88Data(S88_t* S88)
{ 
  S88->Config.activeData = (S88->Config.activeData == 0) ? 1 : 0;
  uint8_t i = 0;
  for (i=0; i < S88->Config.maxModules; i++) {
    S88->Config.data[S88->Config.activeData][i] = (uint16_t)0;
  }
}

void InitForTest(S88_t* S88) {
  // Make sure S88 bus read is done
  while (S88->State.state != IDLE) {};
  
  // Stop reading the bus
  STOP_TIMER1;
  
  // Pull RST, LOAD and CLK HIGH while power cycling the S88 bus. This causes compatible decoder to emit a predefined pattern
  S88PORT |= ((1 << RST) | (1 << PS) | (1 << CLK));
  S88PORT &= ~(1 << PWR);
  _delay_ms(1000);
  S88PORT |= (1 << PWR);
  _delay_ms(1000);
  S88PORT &= ~((1 << RST) | (1 << PS) | (1 << CLK));
  
  // Compatible decoder should now start emitting a predefined pattern, the host computer can read the bus as usual
  START_TIMER1;
}

void StartS88Read(volatile S88_t* S88, reportstate full) {
//  State.reportState = full;
  State.state = STARTREAD;
  State.CLKC = 2*8*S88->Config.maxModules; // up and downflank, 16 bits per module
  State.module = 0;
  State.bit = 128;
  _S88 = S88; // Make the S88 struct available to the interupt routine
  START_TIMER1;
}

uint16_t GetClock(S88_t* S88) {
  return eeprom_read_word(EEREFR);
}

void SetClock(S88_t* S88, uint16_t* clk, int store) {
  OCR0A = *clk;
  if (store)
    eeprom_write_word(EEREFR, *clk);
}
      
void SetupS88Hardware(S88_t* S88) {
  // Set direction register for S88 Bus
  S88DDR |= ((1 << PS)|(1 << RST)|(1 << CLK)|(1<<PA5));//|(1 << PWR));
  S88DDR &= ~(1<<DATA); // Make sure DATA is input
   
  // Unset pull up for data
  S88PORT &= ~(1 << DATA);
  // Setup S88 Clock
#ifdef ATMEGA32u4
  
  TIMSK1 = _BV(OCIE1A); // Interrupt on T0CNT == COMPA
  TCCR1B = _BV(WGM12);  // Timer clear on Compare match
  
  OCR1A = 200;
#else
  TCCR0A = 0;
  TCCR0B = 0;
  
  TIMSK0 |= (1 << OCIE0A); // Interrupt on T0CNT == COMPA
  TCCR0A |= (1 << WGM01); // Timer clear on Compare match
//  TIFR0 |= (1<<OCF0A);
  OCR0A = 200;
#endif
  //OCR1A = eeprom_read_word(EEREFR); //T0CNT; // Set the compare value
}

void S88Reset(S88_t* S88) 
{
  STOP_TIMER1;
  S88->State.state = IDLE;
  S88->Config.modules[0] = 0;
  S88->Config.modules[1] = 0;
  S88->Config.modules[2] = 0;
}



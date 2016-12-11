#include "S88.h"
#include <string.h>
#include <util/delay.h>
#include <avr/eeprom.h> 


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

#ifdef ATMEGA32u4
	ISR(TIM1_COMPA_vect) {
#else
	ISR(TIM0_COMPA_vect) {
#endif
	//	;
  if (_S88->State.state == CLOCK) {
    //asm("sbi 0x03, 6");
	  S88PORT ^= (1<<CLK); // Toggle the pin
    _S88->State.CLKC--;
    if  (!(S88PIN & (1 << CLK))) // Down flank
    { 
      _S88->State.bit++;
      if (_S88->State.bit > 15) {
        _S88->State.module++;
        _S88->State.bit = 0;
      }
      uint8_t t;
      t = (S88PIN & (1<<DATA)) ? 1 : 0 ;
      _S88->Config.data[_S88->Config.activeData][_S88->State.module] |= (t << _S88->State.bit);
    }
    if (_S88->State.CLKC == 0) 
    {
      _S88->State.state = SENDDATA;
      _S88->State.module = 0; // Used again for sending the data
      _S88->State.timeout = 0;
    }
  } else if (_S88->State.state == IDLE)  {
    if (_S88->State.timeout++ > _S88->Config.autoTimeout){
      StartS88Read(_S88, DIFF);
    }
  } else if (_S88->State.state == PRERESET) 
    { // Pulse reset
      S88PORT ^= (1<<RST); // Toggle the pin
      _S88->State.state = RESET;
//    	PINA = 0xf;
  } else if (_S88->State.state == RESET) {
    S88PORT ^= (1<<RST); // Toggle the pin
    _S88->State.state = POSTLOAD;
    _S88->State.module = 0;
  } else if (_S88->State.state == STARTREAD) { // && !(S88PIN & (1 << PS))) {
    _S88->State.state = PRELOADCLK;
    S88PORT ^= (1<<PS);
  } else if (_S88->State.state == PRELOADCLK) { // && (S88PIN & (1 << PS))) {
    _S88->State.CLKC--;
    S88PORT ^= (1<<CLK); // Toggle the pin (up)
    _S88->State.state = POSTLOADCLK;
  } else if (_S88->State.state == POSTLOADCLK) { //} && (S88PIN & (1 << CLK))) {
    _S88->State.CLKC--;
    S88PORT ^= (1<<CLK); // Toggle the pin (down)
    _S88->State.bit = 0;
    _S88->Config.data[_S88->Config.activeData][_S88->State.module] |= ((S88PIN & (1<<DATA)) ? 1 : 0) ;
    _S88->State.state = PRERESET;
  } else if (_S88->State.state == POSTLOAD) {// && !(S88PIN & (1 << CLK))) {
    S88PORT ^= (1<<PS);
    _S88->State.state = CLOCK;
  }
}

void SetNoModules(S88_t* S88, uint8_t modules, uint8_t bus) {
      S88->Config.modules[bus] = modules;
      S88->State.maxModules = (S88->State.maxModules > S88->Config.modules[bus]) ? S88->State.maxModules : S88->Config.modules[bus];
      S88->State.totalModules = S88->Config.modules[0] + S88->Config.modules[1] + S88->Config.modules[2];
      S88->Config.autoTimeout = (S88->State.maxModules * 2 * 16) + 10; // 32 Timer1 compares per module and 10 for the RESET and LOAD/PS header.	
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
  for (i=0; i < S88->State.maxModules; i++) {
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
  S88->State.reportState = full;
  S88->State.state = STARTREAD;
  S88->State.CLKC = 2*16*S88->State.maxModules; // up and downflank, 16 bits per module
  S88->State.module = 0;
  S88->State.bit = 0;
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
  S88DDR |= ((1 << PS)|(1 << RST)|(1 << CLK));//|(1 << PWR));
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



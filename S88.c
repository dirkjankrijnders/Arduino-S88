#include "S88.h"
#include <string.h>
#include <util/delay.h>
#include <avr/eeprom.h> 

#define START_TIMER1 TCCR1B |= (1<<CS00)|(1<<CS01) 
#define STOP_TIMER1  TCCR1B &= 0B11111000 

#define EEREFR (uint16_t*)0x0080

// PIN OUT S88N
#define CLK PB6
#define PS PB4
#define DATA PB0
#define RST PB5
#define PWR PB3

#define S88PORT PORTB
#define S88PIN PINB
#define S88DDR DDRB

#define cTERM  't'
#define cINIT  's'
#define cFULL  'm'
#define cVERS  'v'
#define cREFR  'r'
#define cTEST  'c'

/*
char commandBuffer[8]; // Max 5 in binary mode, 8 in terminal mode
int commandBufferIndex = 0;
*/

volatile S88_t* _S88;

ISR(TIMER1_COMPA_vect) {
  if (_S88->State.state == CLOCK) {
    asm("sbi 0x03, 6");
    //S88PORT ^= (1<<CLK); // Toggle the pin
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
  } else if (_S88->State.state == STARTREAD) 
    { // Pulse reset
      S88PORT ^= (1<<RST); // Toggle the pin
      _S88->State.state = RESET;
  } else if (_S88->State.state == RESET) {
    S88PORT ^= (1<<RST); // Toggle the pin
    _S88->State.state = PRELOAD;
    _S88->State.module = 0;
  } else if (_S88->State.state == PRELOAD) { // && !(S88PIN & (1 << PS))) {
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
    _S88->State.state = POSTLOAD;
  } else if (_S88->State.state == POSTLOAD) {// && !(S88PIN & (1 << CLK))) {
    S88PORT ^= (1<<PS);
    _S88->State.state = CLOCK;
  }
}

void cmdDispatcher(S88_t* S88, char cmd[8] ) {
  if (S88->State.state == IDLE) {
    switch (cmd[0]) {
    case 't': //cTERM: // Mode change, ignored only binary supported, reply "t0"
      strncpy(S88->State.responseBuffer, "t0\r\0", 4);
      S88->State.state = IDLE;
      break;
    case cINIT:
      S88->Config.modules[0]=(uint8_t)cmd[1];
      S88->State.maxModules = S88->Config.modules[0];
      S88->Config.modules[1]=(uint8_t)cmd[2];
      S88->State.maxModules = (S88->State.maxModules > S88->Config.modules[1]) ? S88->State.maxModules : S88->Config.modules[1];
      S88->Config.modules[2]=(uint8_t)cmd[3];
      S88->State.maxModules = (S88->State.maxModules > S88->Config.modules[2]) ? S88->State.maxModules : S88->Config.modules[2];
      S88->State.totalModules = S88->Config.modules[0] + S88->Config.modules[1] + S88->Config.modules[2];
      strncpy(S88->State.responseBuffer, "s0\r\0", 4);
      S88->State.responseBuffer[1] = S88->State.totalModules;
      S88->Config.autoTimeout = (S88->State.maxModules * 2 * 16) + 10; // 32 Timer1 compares per module and 10 for the RESET and LOAD/PS header.
      StartS88Read(S88, FULL);
      break;
    case cFULL:
      StartS88Read(S88, FULL);
      break;
    case cVERS:
      strncpy(S88->State.responseBuffer , "CmpRR-88 0.1\r\0", 14);
      S88->State.state = IDLE;
      break;
    case cREFR:
      SetClock(S88, (uint16_t*)&(cmd[1]), 1);
      strncpy(S88->State.responseBuffer , "rxx\r\0", 5);
      uint16_t clk = GetClock(S88);
      S88->State.responseBuffer[1] = (int8_t)clk;
      S88->State.responseBuffer[2] = (int8_t)(clk >> 8);
  
      break;
    case cTEST:
      InitForTest(S88);
      break;
    default:
      strncpy(S88->State.responseBuffer, "Error\r", 7);
      S88->State.state = IDLE;
    }
//    S88->State.responseBuffer[1] = cmd[0];
  }
};

int8_t IsReady(S88_t* S88){
	if (S88->State.state == SENDDATA) {
		return 1;
	} else {
		return 0;
	}
}

int SendableResponse(S88_t* S88)
{
  if (S88->State.state == SENDDATA) {
    if (((S88->State.reportState == FULL) && (S88->State.module == S88->State.totalModules)) |
        ((S88->State.reportState == DIFF) && (S88->State.diffModules > 0) && (S88->State.module == S88->State.diffModules))) {
      S88->State.state = IDLE;
      strncpy(S88->State.responseBuffer, "\r\0", 3);
      SwapAndClearS88Data(S88);
      return STRING;
    } else {
      if (S88->State.reportState == FULL) {
        if (S88->State.module == 0) {
          strncpy(S88->State.responseBuffer, "mmm12\0", 6);
          S88->State.responseBuffer[1] = S88->State.totalModules;
          S88->State.responseBuffer[3] = (S88->Config.data[S88->Config.activeData][S88->State.module] >> 8);
          S88->State.responseBuffer[4] = S88->Config.data[S88->Config.activeData][S88->State.module];
          S88->State.responseBuffer[2] = ++S88->State.module;
          return 5;
        } else {
          strncpy(S88->State.responseBuffer, "m12\0", 4);
          S88->State.responseBuffer[1] = (S88->Config.data[S88->Config.activeData][S88->State.module] >> 8);
          S88->State.responseBuffer[2] = S88->Config.data[S88->Config.activeData][S88->State.module];
          S88->State.responseBuffer[0] = ++S88->State.module;
          return THREEBYTEDATA;   
        }
      } else if (S88->State.reportState == DIFF) {
        uint8_t otherBuffer = (S88->Config.activeData == 0) ? 1 : 0;
        if (S88->State.module == 0) {
          uint8_t i = 0;
          S88->State.diffModules = 0;
          for (i =0 ; i<S88->State.totalModules; i++ )
          {
            if (S88->Config.data[S88->Config.activeData][i] != S88->Config.data[otherBuffer][i]){
              S88->Config.data[otherBuffer][S88->State.diffModules] = S88->Config.data[S88->Config.activeData][i];
              S88->Config.changedModules[S88->State.diffModules] = i + 1;
              S88->State.diffModules++;
            }
          }
          if (S88->State.diffModules > 0) {
            strncpy(S88->State.responseBuffer, "imm12\0", 6);
            S88->State.responseBuffer[1] = S88->State.diffModules;
            S88->State.responseBuffer[3] = (S88->Config.data[otherBuffer][S88->State.module] >> 8);
            S88->State.responseBuffer[4] = S88->Config.data[otherBuffer][S88->State.module];
            S88->State.responseBuffer[2] = S88->Config.changedModules[S88->State.module];
            S88->State.module++;
            return 5;
          } else {
            S88->State.state = IDLE;
            SwapAndClearS88Data(S88);
            return NONE;
          }
        } else {
          strncpy(S88->State.responseBuffer, "m12\0", 4);
          S88->State.responseBuffer[1] = (S88->Config.data[otherBuffer][S88->State.module] >> 8);
          S88->State.responseBuffer[2] = S88->Config.data[otherBuffer][S88->State.module];
          S88->State.responseBuffer[0] = ++S88->State.module;
          return THREEBYTEDATA;
        }
        
      }   
    }
  } else if ((S88->State.responseBuffer[0] != '\0')) {
    return STRING;
  }
  return NONE;
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
  PORTD |= ((1 << RST) | (1 << PS) | (1 << CLK));
  PORTD &= ~(1 << PWR);
  _delay_ms(1000);
  PORTD |= (1 << PWR);
  _delay_ms(1000);
  PORTD &= ~((1 << RST) | (1 << PS) | (1 << CLK));
  
  // Compatible decoder should now start emitting a predefined pattern, the host computer can read the bus as usual
  START_TIMER1;
}

void StartS88Read(S88_t* S88, reportstate full) {
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
  OCR1A = *clk;
  if (store)
    eeprom_write_word(EEREFR, *clk);
}
      
void SetupS88Hardware(void) {
  // Set direction register for S88 Bus
  S88DDR |= ((1 << PS)|(1 << RST)|(1 << CLK));//|(1 << PWR));
  // Set pull up for data
//    S88PORT |= (1 << DATA);
  // Setup S88 Clock
  TIMSK1 = _BV(OCIE1A); // Interrupt on T0CNT == COMPA
  TCCR1B = _BV(WGM12);  // Timer clear on Compare match
  
//  OCR1A = 200;
  OCR1A = eeprom_read_word(EEREFR); //T0CNT; // Set the compare value
}

void S88Reset(S88_t* S88) 
{
  STOP_TIMER1;
  S88->State.state = IDLE;
  S88->Config.modules[0] = 0;
  S88->Config.modules[1] = 0;
  S88->Config.modules[2] = 0;
}



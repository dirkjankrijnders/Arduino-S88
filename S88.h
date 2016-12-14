#ifndef _S88_h
#define _S88_h

#include <stdint.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h> 

#define S88_MAX_BUSSES 3
#define S88_MAX_MODULES 62

#ifdef __cplusplus
extern "C" {
#endif

enum states {
  SENDDATA = 0, // Prob. not neccesary
  CLOCK,
  POSTLOAD, 
  RESET, 
  PRERESET,
  POSTLOADCLK, 
  PRELOADCLK,
  STARTREAD,
  IDLE
};

typedef enum  {
  FULL = 0,
  DIFF
} reportstate;

typedef struct {
    uint8_t modules[S88_MAX_BUSSES];
    uint8_t data[2][S88_MAX_MODULES];
    uint16_t autoTimeout;
    uint8_t activeData;
    uint8_t maxModules;
    uint8_t totalModules;
  } S88_Config_t;

typedef struct {
    uint8_t module;
    uint8_t bit;
    //uint8_t diffModules;
    uint8_t CLKC;
    uint8_t state;
    //reportstate reportState;
    //uint16_t timeout;
    uint8_t currentByte;
  } S88_State_t;

typedef struct {
    S88_Config_t Config;
    S88_State_t State;
  } S88_t;


void SetupS88Hardware(S88_t* S88);
void SetNoModules(S88_t* S88, uint8_t modules, uint8_t bus);
void InitForTest(S88_t* S88);
void SetClock(S88_t* S88, uint16_t* clk, int store);
uint16_t GetClock(S88_t* S88);
 
void SwapAndClearS88Data(S88_t* S88);
void StartS88Read(volatile S88_t* S88, reportstate full);
int8_t IsReady(S88_t* S88);

void S88Reset(S88_t* S88);

#ifdef __cplusplus
}
#endif
#endif


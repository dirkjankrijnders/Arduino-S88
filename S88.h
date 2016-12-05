#ifndef _S88_h
#define _S88_h

#include <stdint.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h> 

#define S88_MAX_BUSSES 3
#define S88_MAX_MODULES 31

#ifdef __cplusplus
extern "C" {
#endif

enum states {
  IDLE = 0,
  STARTREAD,
  PRERESET,
  RESET, 
  PRELOAD,
  PRELOADCLK,
  POSTLOADCLK, 
  POSTLOAD, 
  CLOCK,
  SENDDATA
};

typedef enum  {
  FULL = 0,
  DIFF
} reportstate;

typedef struct {
    uint8_t modules[S88_MAX_BUSSES];
    uint16_t data[2][S88_MAX_MODULES];
    uint8_t changedModules[S88_MAX_MODULES];
    uint16_t autoTimeout;
    uint8_t activeData;
  } S88_Config_t;

typedef struct {
    uint8_t module;
    uint16_t bit;
    uint8_t maxModules;
    uint8_t totalModules;
    uint8_t diffModules;
    uint8_t CLKC;
    uint8_t state;
    reportstate reportState;
    uint16_t timeout;
    char responseBuffer[80];
  } S88_State_t;

typedef struct {
    S88_Config_t Config;
    S88_State_t State;
  } S88_t;

enum sendtypes {
  STRING = -1,
  NONE = 0,
  ONEBYTEDATA,
  TWOBYTEDATA,    
  THREEBYTEDATA
};

void SetupS88Hardware(void);
void InitForTest(S88_t* S88);
void SetClock(S88_t* S88, uint16_t* clk, int store);
uint16_t GetClock(S88_t* S88);
 
void cmdDispatcher(S88_t* S88, char cmd[8] );
void SwapAndClearS88Data(S88_t* S88);
int SendableResponse(S88_t* S88);
void StartS88Read(volatile S88_t* S88, reportstate full);
int8_t IsReady(S88_t* S88);

void S88Reset(S88_t* S88);

#ifdef __cplusplus
}
#endif
#endif


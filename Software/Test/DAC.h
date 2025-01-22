/*
  Default configuration of teensy pins
*/
#ifndef DAC_h
#define DAC_h

#include "Arduino.h"

//NOTE: It seems that in this compiler lists longer than 4 need to be built in CPP while shorter lists need to be built in header with constexpr

class DAC
{
  public:
    DAC();
    static void initiazize();
    static void setSingleCurrent(uint8_t board_id, uint16_t intensity);
    static void setSyncedCurrent(uint16_t *intensity);
    static void setAllCurrent(uint16_t intensity);
    static void allOff();
    static void setSinglePWM(uint8_t board_id, uint16_t intensity);
    static void setAllPWM(uint16_t intensity);
    static void externalSignal();
    static void setExternalImpedance(uint8_t impedance);

  private:
    static void init(); //Initialize reference variables
    static void sendCommand(); 
    const static uint8_t SET_NO_UPDATE = B00000000;
    const static uint8_t SET_WITH_UPDATE = B00010000;
    const static uint8_t SET_ALL_WITH_UPDATE = B00100000;
    const static uint8_t POWER_DOWN_ALL = B00110000;
};
#endif

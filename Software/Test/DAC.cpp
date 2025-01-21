/*
  Default configuration of teensy pins
*/

#include "Arduino.h"
#include "pinSetup.h"
#include "SPI.h"
#include "DAC.h"

pinSetup pins;
SPISettings spiSettings(24000000, MSBFIRST, SPI_MODE0); //https://forum.pjrc.com/index.php?threads/lpspi-speed-limit-and-qspi.72055/#post-319847 - up to 40 MHz for DAC

union BYTE16UNION
{
 uint16_t bytes_var;
 uint8_t bytes[2];
}command;

DAC::DAC(){
}

void DAC::init(){
}

void DAC::initiazize(){
  //Initialize all DACs to 0
  command.bytes[0] = SET_ALL_WITH_UPDATE;
  command.bytes[1] = 0;
}

void DAC::setSingleCurrent(uint8_t board_id, uint16_t intensity){
  command.bytes[0] = SET_WITH_UPDATE;
  command.bytes[0] += board_id << 6;
  command.bytes_var += intensity >> 4;
  sendCommand();
}

void DAC::sendCommand(){
  SPI1.beginTransaction(spiSettings);
  digitalWrite(pins.CS,LOW);
  SPI1.transfer16(command.bytes_var);
  digitalWrite(pins.CS,HIGH);
  SPI1.endTransaction();
  pinMode(pins.FAN_PWM[1], OUTPUT); //Reset pin 1 to output, since it is also MISO1 pin*************************************
}
    // static void setSingleCurrent(uint8_t board_id, uint16_t intensity);
    // static void setSyncedCurrent(uint16_t *intensity);
    // static void setAllCurrent(uint16_t intensity);
    // static void allOff();
    // static void setSinglePWM(uint8_t board_id, uint16_t intensity);
    // static void setSyncedPWM(uint16_t *intensity);
    // static void setAllPWM(uint16_t intensity);
    // static void externalSignal();
    // static void setExternalImpedance(uint8_t impedance);
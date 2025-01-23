/*
  Default configuration of teensy pins
*/

#include "Arduino.h"
#include "pinSetup.h"
#include "SPI.h"
#include "DAC.h"

pinSetup pins;
SPISettings spiSettings(35e6, MSBFIRST, SPI_MODE0); //https://forum.pjrc.com/index.php?threads/lpspi-speed-limit-and-qspi.72055/#post-319847 - up to 40 MHz for DAC

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
  SPI1.begin();
  delay(1);
  //Initialize all DACs to 0
  allOff();
}

void DAC::setSingleCurrent(uint8_t board_id, uint16_t intensity){
  command.bytes_var = 0;
  command.bytes[1] = SET_WITH_UPDATE;
  command.bytes[1] += board_id << 6;
  command.bytes_var += intensity >> 4;
  sendCommand();
}

void DAC::setSyncedCurrent(uint16_t *intensity){
  for(uint8_t i = 0; i<3; i++){
    command.bytes_var = 0;
    if(i == 2) command.bytes[1] = SET_WITH_UPDATE;
    else command.bytes[1] = SET_NO_UPDATE;
    command.bytes[1] += i << 6;
    command.bytes_var += intensity[i] >> 4;
    sendCommand();
  }
}

void DAC::setAllCurrent(uint16_t intensity){
  command.bytes_var = 0;
  command.bytes[1] = SET_ALL_WITH_UPDATE;
  intensity >>= 4;
  command.bytes_var += intensity;
  Serial.println(command.bytes_var);
  sendCommand();
}

void DAC::allOff(){
  setAllCurrent(0);
  for(uint8_t i = 0; i<3; i++){
    pinMode(pins.INTERLINE[i], OUTPUT);
    digitalWriteFast(pins.INTERLINE[i], LOW);
  }
}

void DAC::externalSignal(){
  command.bytes[1] = POWER_DOWN_ALL;
  sendCommand();
}

void DAC::setExternalImpedance(uint8_t impedance){
  command.bytes[1] = POWER_DOWN_ALL;
  command.bytes[1] += impedance << 6;
  sendCommand();
}

void DAC::setSinglePWM(uint8_t board_id, uint16_t intensity){
  if(intensity) analogWrite(pins.INTERLINE[board_id], intensity);
  else{
    pinMode(pins.INTERLINE[board_id], OUTPUT);
    digitalWriteFast(pins.INTERLINE[board_id], LOW); 
  }
}

void DAC::setAllPWM(uint16_t intensity){
  for(uint8_t i = 0; i<3; i++){
    if(intensity) analogWrite(pins.INTERLINE[i], intensity);
    else{
      pinMode(pins.INTERLINE[i], OUTPUT);
      digitalWriteFast(pins.INTERLINE[i], LOW); 
    }
  }
}

void DAC::sendCommand(){
  SPI1.beginTransaction(spiSettings);
  digitalWrite(pins.CS,LOW);
  SPI1.transfer16(command.bytes_var);
  digitalWrite(pins.CS,HIGH);
  SPI1.endTransaction();
  pinMode(pins.FAN_PWM[1], OUTPUT); //Reset pin 1 to output, since it is also MISO1 pin*************************************
}

    // static void setSinglePWM(uint8_t board_id, uint16_t intensity);
    // static void setSyncedPWM(uint16_t *intensity);
    // static void setAllPWM(uint16_t intensity);
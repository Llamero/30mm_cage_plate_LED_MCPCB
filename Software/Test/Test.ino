#include <SPI.h>
#include "pinSetup.h"

int a;
int b;
int c;
int intensity = 16;

pinSetup pin;

void setup() {
  //Set pin configurations
  pin.configurePins();
  Serial.begin(9500);
  SPI1.begin();
  delay(1);

  SPI1.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pin.CS,LOW);
  SPI1.transfer(47);
  SPI1.transfer(255);
  digitalWrite(pin.CS,HIGH);
  // release control of the SPI port
  SPI1.endTransaction();
  pinMode(pin.FAN_PWM[1], OUTPUT); //Reset pin 1 to output, since it is also MISO1 pin*************************************

  // b=0;
  for(a=0; a<3; a++) analogWrite(pin.INTERLINE[a], intensity);
}

void loop() {
  for(c=0; c<3; c++){
    for(a=0; a<4; a++){
      if(a==b) digitalWriteFast(pin.RELAY[c][a], HIGH);
      else digitalWriteFast(pin.RELAY[c][a], LOW);
    }
    delay(100);
    if(digitalReadFast(pin.PUSHBUTTON[0])){
      for(a=0; a<3; a++) analogWrite(pin.INTERLINE[a], intensity);
    }
    else{
      for(a=0; a<3; a++) analogWrite(pin.INTERLINE[a], 65535);
    }
    
    for(a=0; a<3; a++) digitalWrite(pin.FAN_PWM[a], LOW);
    if(!digitalReadFast(pin.PUSHBUTTON[1]) && digitalReadFast(pin.PUSHBUTTON[2])){
      digitalWrite(pin.FAN_PWM[0], HIGH);
    }
    else if(digitalReadFast(pin.PUSHBUTTON[1]) && !digitalReadFast(pin.PUSHBUTTON[2])){
      digitalWrite(pin.FAN_PWM[1], HIGH);
    }
    else if(!digitalReadFast(pin.PUSHBUTTON[1]) && !digitalReadFast(pin.PUSHBUTTON[2])){
      digitalWrite(pin.FAN_PWM[2], HIGH);
    }
    else for(a=0; a<3; a++) digitalWrite(pin.FAN_PWM[a], LOW);
    b++;
    if(b>3) b = 0;
  }
  int adc_temp;
  float float_temp;
  for(a=0; a<3; a++){
    adc_temp = pin.boardTemp(a);
    float_temp = pin.adcToTemp(adc_temp);
    Serial.print(float_temp);
    Serial.print(" ");
  }
  Serial.println();
}

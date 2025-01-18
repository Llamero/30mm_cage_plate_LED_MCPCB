/*
  Default configuration of teensy pins
*/

#include "Arduino.h"
#include "pinSetup.h"
#include "ADC.h"
#include "ADC_util.h"
#include "Wire.h"
#include "eFlexPwm.h"

ADC *adc = new ADC(); // adc object;

// avoid systematically prefixing objects with the namespace
using namespace eFlex;

// My eFlexPWM submodules (Hardware > PWM2: SM[0], SM[2], SM[3])
SubModule Sm13 (8, 7);
SubModule Sm22 (6, 9);
SubModule Sm42 (2, 3);
Timer &Tm1 = Sm13.timer();
Timer &Tm2 = Sm22.timer();
Timer &Tm4 = Sm42.timer();

uint8_t dutyCyclePercent = 0;

//NOTE: It seems that in this compiler lists longer than 4 need to be built in CPP while shorter lists need to be built in header with constexpr
const int pinSetup::RELAY[][4] = {{17,16, 15, 14}, {13, 41, 40, 39}, {38, 37, 36, 35}}; //SSR relays for changing LED channel
const int pinSetup::INTERLINE[] = {10, 11, 12}; //Switch between analog input and gnd to turn off LED
const int pinSetup::ALARM[] = {24, 25}; //Audible alarm
const int pinSetup::BOARD_TEMP[] = {20, 19, 18}; //NTC thermistor monitoring LED board temperature(s)
const int pinSetup::FAN_PWM[] = {0, 1, 4}; //5V PWM to control internal fan speed
const int pinSetup::PUSHBUTTON[] = {5, 29, 30}; //Four pushbutton inputs 
const int pinSetup::LED[][2] = {{2, 3}, {6, 9}, {8, 7}}; //Indicator LEDs on pushbuttons - red then green
const int pinSetup::POT[] = {23, 22, 21}; //Input voltage from potentiometer
const int pinSetup::INPUTS[] = {33, 34, 32, 31}; //4-channel analog/digital inputs
//const uint8_t pinSetup::color_list[] = {0, 13, 50, 100}; //4 color settings for pushbutton
const uint8_t pinSetup::color_list[] = {100, 90, 50, 0}; //4 color settings for pushbutton

pinSetup::pinSetup()
{
}

void pinSetup::init(){
}


void pinSetup::configurePins(){
  unsigned int a; //Loop counter
  unsigned int b; //Loop counter
  analogWriteResolution(16);
  
  ///// ADC0 ////
  // reference can be ADC_REFERENCE::REF_3V3, ADC_REFERENCE::REF_1V2 (not for Teensy LC) or ADC_REFERENCE::REF_EXT.
  adc->adc0->setReference(ADC_REFERENCE::REF_3V3); // change all 3.3 to 1.2 if you change the reference to 1V2
  adc->adc0->setAveraging(adc_averaging); // set number of averages
  adc->adc0->setResolution(adc_resolution); // set bits of resolution

  // it can be any of the ADC_CONVERSION_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED_16BITS, HIGH_SPEED or VERY_HIGH_SPEED
  // see the documentation for more information
  // additionally the conversion speed can also be ADACK_2_4, ADACK_4_0, ADACK_5_2 and ADACK_6_2,
  // where the numbers are the frequency of the ADC clock in MHz and are independent on the bus speed.
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::LOW_SPEED); // change the conversion speed
  // it can be any of the ADC_MED_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED or VERY_HIGH_SPEED
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::LOW_SPEED); // change the sampling speed

  ////// ADC1 /////
  adc->adc1->setReference(ADC_REFERENCE::REF_3V3);
  adc->adc1->setAveraging(adc_averaging); // set number of averages
  adc->adc1->setResolution(adc_resolution); // set bits of resolution
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::LOW_SPEED); // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::LOW_SPEED); // change the sampling speed
  
  ////// INPUT /////
  for(a=0; a<sizeof(PUSHBUTTON)/sizeof(PUSHBUTTON[0]); a++) pinMode(PUSHBUTTON[a], INPUT_PULLUP);

  ////// OUTPUT /////
  for(a=0; a<sizeof(RELAY)/sizeof(RELAY[0]); a++){
    for(b=0; b<sizeof(RELAY[0])/sizeof(RELAY[0][0]); b++){
      pinMode(RELAY[a][b], OUTPUT);
      digitalWriteFast(RELAY[a][b], !RELAY_CLOSE);
    } 
  } 
  for(a=0; a<sizeof(INTERLINE)/sizeof(INTERLINE[0]); a++){
    pinMode(INTERLINE[a], OUTPUT);
    digitalWriteFast(INTERLINE[a], LOW);
    analogWriteFrequency((uint8_t) INTERLINE[a], LED_FREQ); //Set output PWM freq for LEDs https://www.pjrc.com/teensy/td_pulse.html
  } 
  for(a=0; a<sizeof(ALARM)/sizeof(ALARM[0]); a++){
    pinMode(ALARM[a], OUTPUT);
    digitalWriteFast(ALARM[a], LOW);
  }
  for(a=0; a<sizeof(FAN_PWM)/sizeof(FAN_PWM[0]); a++){
    pinMode(FAN_PWM[a], OUTPUT);
    digitalWriteFast(FAN_PWM[a], LOW);
    analogWriteFrequency(FAN_PWM[a], FAN_FREQ); //Set output PWM freq to optimal CPU fan freq, also sets analog_select PWM freq (on same timer): https://www.pjrc.com/teensy/td_pulse.html
  } 
  for(a=0; a<sizeof(LED)/sizeof(LED[0]); a++){
    for(b=0; b<sizeof(LED[0])/sizeof(LED[0][0]); b++){
      pinMode(LED[a][b], OUTPUT);
      digitalWriteFast(LED[a][b], LOW);
    } 
  }
  pinMode(CS, OUTPUT);
  digitalWriteFast(CS, HIGH);  
  
  ////// DISABLE /////
  for(a=0; a<sizeof(INPUTS)/sizeof(INPUTS[0]); a++) pinMode(INPUTS[a], INPUT_DISABLE);
  
  ////// I2C /////
  // Wire.begin();
  // Wire.setClock(3400000);
  // Wire.setSDA(SDA0);
  // Wire.setSCL(SCL0);

  ////// eFlexPWM /////
  Config myConfig;

  /* Use full cycle reload */
  myConfig.setReloadLogic (kPWM_ReloadPwmFullCycle);
  /* PWM A & PWM B form a complementary PWM pair */
  myConfig.setPairOperation (kPWM_ComplementaryPwmA);
  myConfig.setPwmFreqHz (BUTTON_LED_FREQ);

  SubModule Sm13 (LED[2][0], LED[2][1]);
  SubModule Sm22 (LED[1][0], LED[1][1]);
  SubModule Sm42 (LED[0][0], LED[0][1]);

  /* Initialize submodule 0 */
  if (Sm13.configure (myConfig) != true) {
    Serial.println ("Submodule 13 initialization failed");
    exit (EXIT_FAILURE);
  }

  if (Sm22.configure (myConfig) != true) {
    Serial.println ("Submodule 22 initialization failed");
    exit (EXIT_FAILURE);
  }

  /* Initialize submodule 3 the same way as submodule 2 */
  if (Sm42.configure (myConfig) != true) {
    Serial.println ("Submodule 42 initialization failed");
    exit (EXIT_FAILURE);
  }

  //Tm1.setupDeadtime (3000000, 1);
  // Tm2.setupDeadtime (3000, 1);
  // Tm4.setupDeadtime (3000, 1);

  // synchronize registers and start all submodules
  if (Tm1.begin() != true) {
    Serial.println ("Failed to start module 1");
    exit (EXIT_FAILURE);
  }
  else if (Tm2.begin() != true) {
    Serial.println ("Failed to start module 2");
    exit (EXIT_FAILURE);
  }
  else if (Tm4.begin() != true) {
    Serial.println ("Failed to start module 4");
    exit (EXIT_FAILURE);
  }
}

//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC//////////////ADC

int pinSetup::adcMax(){
  return (uint16_t) adc->adc0->getMaxValue();
}

//Pins 24, 25 go to ADC0 only, pins 26, 27, 38, 39 go to ADC1 only, and 14-23, 40, 41 can go to either channel - https://forum.pjrc.com/index.php?threads/teensy-4-1-adc-channels.72373/post-322241
uint16_t pinSetup::boardTemp(uint8_t a){
  return adc->adc0->analogRead(BOARD_TEMP[a]);
}

uint16_t pinSetup::potValue(uint8_t a){
  uint16_t pot = 0;
  uint8_t index = 16;
  while(index--) pot += adcMax()-adc->adc0->analogRead(POT[a]);
  if(pot < POT_OFFSET) pot = 0;
  else pot -= POT_OFFSET;
  return pot;
}

//Requires 14 µs to complete calculation
float pinSetup::adcToTemp(uint16_t adc){
  float steinhart;
  float raw = (float) adc;
  raw = adcMax() / raw - 1;
  raw = SERIES_RESISTOR / raw;
  steinhart = raw / PCB_THERMISTOR_NOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= PCB_B_COEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (25 + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;   
  return steinhart;
}

//Requires 14 µs to complete calculation
float pinSetup::adcToTemp(uint16_t adc, int therm_nominal, int b_coefficient){
  float steinhart;
  float raw = (float) adc;
  raw = adcMax() / raw - 1;
  raw = SERIES_RESISTOR / raw;
  steinhart = raw / therm_nominal;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= b_coefficient;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (25 + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;   
  return steinhart;
}

uint16_t pinSetup::tempToAdc(float temperature, int therm_nominal = PCB_THERMISTOR_NOMINAL, int b_coefficient = PCB_B_COEFFICIENT){
  float steinhart = temperature;
  float raw;
  steinhart += 273.15;  
  steinhart = 1.0 / steinhart;  
  steinhart -= 1.0 / (25 + 273.15); // + (1/To); 
  steinhart *= b_coefficient;
  steinhart = exp(steinhart);
  raw = steinhart * therm_nominal; 
  raw = SERIES_RESISTOR/raw;
  raw = adcMax()/(raw+1); 
  return (uint16_t) round(raw);
}

//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM//////////////eFlexPWM

void pinSetup::setButtonColor(uint8_t id, uint8_t color_index){
  if(color_index >= sizeof(color_list)) toggleButtonLED(id, false); //Turn LED off if invalid index
  else{
    toggleButtonLED(id, true); //Turn LED off if invalid index
    if(id == 0){
      Sm42.updateDutyCyclePercent (color_list[color_index], ChanA);
      Tm4.setPwmLdok();
    }
    else if(id == 1){
      Sm22.updateDutyCyclePercent (color_list[color_index], ChanA);
      Tm2.setPwmLdok();
    }
    else{
      Sm13.updateDutyCyclePercent (color_list[color_index], ChanA);
      Tm1.setPwmLdok();
    }
  }
}

void pinSetup::toggleButtonLED(uint8_t id, bool state){
  if(id == 0) Sm42.enable(state);
  else if(id == 1) Sm22.enable(state);
  else Sm13.enable(state);
  if(!state) Serial.println(id);
}
  
  





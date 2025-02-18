/*
  Default configuration of teensy pins
*/
#ifndef pinSetup_h
#define pinSetup_h

#include "Arduino.h"

//NOTE: It seems that in this compiler lists longer than 4 need to be built in CPP while shorter lists need to be built in header with constexpr

class pinSetup
{
  public:
    pinSetup();
    static void configurePins();
    static int adcMax(); //Returns the maximum value for the ADC
    static uint16_t boardTemp(uint8_t a); //Returns the current temperature of the specified board thermistor
    static uint16_t boardTempFast(uint8_t a); //Read board temp on fast DAC
    static uint16_t potValue(uint8_t a); //Returns the current temperature of the specified board thermistor
    static float adcToTemp(uint16_t adc); //Convert raw ADC value to temperature in °C
    static float adcToTemp(uint16_t adc, int therm_nominal, int b_coefficient); //Convert raw ADC value to temperature in °C
    static uint16_t tempToAdc(float temperature, int therm_nominal, int b_coefficient); //Convert temperature in °C to equivalent ADC value
    static uint16_t tempToAdc(float temperature); //Convert temperature in °C to equivalent ADC value
    static void setButtonColor(uint8_t id, uint8_t intensity); //Set color of pushbutton LED - 0 = red, 100 = green
    static void toggleButtonLED(uint8_t id, bool state); //Set intensity of pushbutton LED - 0 = off, 255 = full

    const static int RELAY[][4]; //SSR relays for changing LED channel
    const static int INTERLINE[3]; //Switch between analog input and gnd to turn off LED
    const static int ALARM[2]; //Audible alarm
    const static uint16_t DEBOUNCE = 200; //ms to wait for switch to stop bouncing
    
    const static int BOARD_TEMP[3]; //NTC thermistor monitoring LED board temperature(s)
    const static int SDA2 = 25; //I2C SDA pin for optional peripheral comunication
    const static int SCL2 = 24; //I2C SCL pin for optional peripheral comunication
    const static int CS = 28;
    const static int FAN_PWM[3]; //5V PWM to control internal fan speed
    
    const static int PUSHBUTTON[3]; //Three pushbutton inputs 
    const static int LED[][2]; //Indicator LEDs on pushbuttons
    
    const static int POT[3]; //Input voltage from potentiometer
    const static int INPUTS[4]; //4-channel analog/digital inputs

    const static int LED_FREQ = 4577; //LED driver PWM freq - default to 2^n multiple to have optimal dynamic range while staying outside the auditory range - https://www.pjrc.com/teensy/td_pulse.html
    const static int FAN_FREQ = 25000; //5V output PWM frequency (in Hz) - 25kHz is optimal for driving CPU fans
    const static bool RELAY_CLOSE = true; //Polarity of relay inputs to close relay - False - 0=closed, 1=open, True - 0=open, 1=closed

  private:
    static void init(); //Initialize reference variables
    static void convertToAdc(); //Convert reference temperatures to ADC values
   
    //ADC setup
    const static int adc_averaging = 16; //Number of times to average adc recording before returning value
    const static int adc_resolution = 16; //Number of significant bits to return per adc recording
    const static uint16_t POT_OFFSET = 150;
    
    //Termistor setup
    const static int SERIES_RESISTOR = 3600; //Value of series resistor to the thermistor on the PCB
    const static int PCB_THERMISTOR_NOMINAL = 4700; //Value of thermistor resistor on PCB at nominal temp (25°C)
    const static int PCB_B_COEFFICIENT = 3500; //Beta value for the PCB thermistor

    //eFlexPWM setup
    const static uint32_t BUTTON_LED_FREQ = 18000;
    const static uint8_t color_list[4];

    static uint16_t buffer_size; //Variable for storing the size of the calibration stream packet to be sent back to GUI
};
   
#endif

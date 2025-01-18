#include <SPI.h>
#include "pinSetup.h"
#include "PacketSerial.h"
#include "SDcard.h"
#include <EEPROM.h>
#include <TimeLib.h> //Set RTC time and get time strings
#include <eFlexPwm.h>

//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT

#pragma pack(1) //Remove alignment padding bytes in structs - https://forum.pjrc.com/threads/50536-problem-with-union-in-Teensy-3-5
struct configurationStruct{ //259 bytes
  uint8_t prefix;
  char driver_name[16]; //Name of LED driver: "default name"
  char led_names[3][4][16]; //Name of each LED channel
  boolean led_active[3][4]; //Whether LED channel is in use: {false, false, false, false}
  uint16_t current_limit[3][4]; //Current limit for each channel on DAC values: {0,0,0,0}
  uint16_t warn_temp; //Warn temp for three LED boards respectively in ADC units: {0,0,0}
  uint16_t fault_temp; //Fault temp for transistor, resistor, and external respectively in ADC units: {0,0,0}
  uint16_t driver_fan[2]; //Driver fan min and max temperatures in ADC units: {65535, 65535}
  uint8_t audio_volume[2]; //Status and alarm volumes for transducer: {10, 100}
  bool pushbutton_intensity; //LED intensity - on/off
  uint8_t pushbutton_mode; //LED illumination mode when alarm is active
  uint8_t checksum; //Checksum to confirm that configuration is valid
};

const struct defaultConfigurationStruct{ //259 bytes
  uint8_t prefix = 2;
  char driver_name[16] = "Unnamed driver "; //Name of LED driver: "default name"
  char led_names[3][4][16] = {{"LED #1         ", "LED #2         ", "LED #3         ", "LED #4         "}, //Name of each LED channel
                              {"LED #5         ", "LED #6         ", "LED #7         ", "LED #8         "},
                              {"LED #9         ", "LED #10        ", "LED #11        ", "LED #12        "}};
  boolean led_active[3][4] = {{false, false, false, false}, //Whether LED channel is in use: {false, false, false, false}
                              {false, false, false, false},
                              {false, false, false, false}};
  uint16_t current_limit[3][4] = {{65535,65535,65535,65535}, //Current limit for each channel on DAC values: {0,0,0,0}
                                  {65535,65535,65535,65535},
                                  {65535,65535,65535,65535}};
  uint16_t warn_temp = 14604; //Warn at 60°C
  uint16_t fault_temp = 8891; //Fault at 80°C
  uint16_t driver_fan[2] = {29565, 14604}; //Fan on at 30°C, fan max at 60°C
  uint8_t audio_volume[2] = {10, 100}; //Status and alarm volumes for transducer: {10, 100}
  uint16_t pushbutton_intensity = 65535; //LED intensity at full intensity
  uint8_t pushbutton_mode = 0; //LED illumination mode when alarm is active
  uint8_t checksum = 124; //Checksum to confirm that configuration is valid
} defaultConfig;

struct syncStruct{ //158 bytes
  uint8_t prefix;
  uint8_t mode; //Type of sync - digital, analog, confocal, etc.
  uint8_t sync_output_channel; //Channel to output sync signal
  
  uint8_t digital_channel; //The input channel for the sync signal
  uint8_t digital_mode[2]; //The digital sync mode  in the LOW and HIGH trigger states respectively
  uint8_t digital_led[2]; //The active LED channel in the LOW and HIGH trigger states respectively
  uint16_t digital_pwm[2]; //The PWM value in the LOW and HIGH trigger states respectively
  uint16_t digital_current[2]; //The DAC value in the LOW and HIGH trigger states respectively
  uint32_t digital_duration[2]; //The maximum number of milliseconds to hold LED state

  uint8_t analog_channel; //The input channel for the sync signal
  uint8_t analog_led; //The active LED channel
  uint8_t analog_mode; //The analog sync mode
  uint16_t analog_pwm; //ADC averages per PWM update
  uint16_t analog_current; //ADC averages per DAC update
  
  boolean shutter_polarity; //Shutter polarity when scan is active
  uint8_t confocal_channel; //The input channel for the line sync signal
  boolean confocal_sync_mode; //Whether the line sync is digital (true) or analog (false)
  boolean confocal_sync_polarity[2]; //Sync polarity for digital and analog sync inputs
  uint16_t confocal_threshold; //Threshold for analog sync trigger
  boolean confocal_scan_mode; //Whether scan is unidirectional (true) or bidrectional (false)
  uint32_t confocal_mirror_period; //Time in clock cycles for the scanning mirror to complete one cycle
  uint32_t confocal_delay[3]; //Delay in clock cycles for each sync delay
 
  uint8_t confocal_mode[2]; //The digital sync mode  in the image and flyback states respectively
  uint8_t confocal_led[2]; //The active LED channel in the image and flyback states respectively
  uint16_t confocal_pwm[2]; //The PWM value in the image and flyback states respectively
  uint16_t confocal_current[2]; //The DAC value in the image and flyback states respectively
  uint32_t confocal_duration[2]; //The maximum number of milliseconds to hold LED state
  
  uint8_t checksum; //Checksum to confirm that configuration is valid
};

const struct defaultSyncStruct{ //158 bytes
  uint8_t prefix = 4;
  uint8_t mode = 0; //Type of sync - digital, analog, confocal, etc.
  uint8_t sync_output_channel = 0; //Channel to output sync signal
  
  uint8_t digital_channel = 0; //The input channel for the sync signal
  uint8_t digital_mode[2] = {0,0}; //The digital sync mode  in the LOW and HIGH trigger states respectively
  uint8_t digital_led[2] = {0,0}; //The active LED channel in the LOW and HIGH trigger states respectively
  uint16_t digital_pwm[2] = {0,0}; //The PWM value in the LOW and HIGH trigger states respectively
  uint16_t digital_current[2] = {0,0}; //The DAC value in the LOW and HIGH trigger states respectively
  uint32_t digital_duration[2] = {0,0}; //The maximum number of milliseconds to hold LED state
  
  uint8_t analog_channel = 0; //The input channel for the sync signal
  uint8_t analog_led = 0; //The active LED channel
  uint8_t analog_mode = 0; //The analog sync mode
  uint16_t analog_pwm = 1; //ADC averages per PWM update
  uint16_t analog_current = 1; //ADC averages per DAC update

  boolean shutter_polarity = true; //Shutter polarity when scan is active
  uint8_t confocal_channel = 0; //The input channel for the line sync signal
  boolean confocal_sync_mode = false; //Whether the line sync is digital (false) or analog (true)
  boolean confocal_sync_polarity[2] = {true, true}; //Sync polarity for digital and analog sync inputs
  uint16_t confocal_threshold = 2000; //Threshold for analog sync trigger
  boolean confocal_scan_mode = true; //Whether scan is unidirectional (false) or bidrectional (true)
  uint32_t confocal_mirror_period = 180000; //Time in µs for the scanning mirror to complete one cycle
  uint32_t confocal_delay[3] = {0,36000,0}; //Delay in clock cycles for each sync delay
  
  uint8_t confocal_mode[2] = {0,0}; //The digital sync mode  in the image and flyback states respectively
  uint8_t confocal_led[2] = {0,0}; //The active LED channel in the image and flyback states respectively
  uint16_t confocal_pwm[2] = {0,0}; //The PWM value (in clock cycles) in the image and flyback states respectively
  uint16_t confocal_current[2] = {0,0}; //The DAC value in the image and flyback states respectively
  uint32_t confocal_duration[2] = {0,0}; //The maximum number of milliseconds to hold LED state

  uint8_t checksum = 18; //Checksum to confirm that configuration is valid
} defaultSync;

struct sequenceHeaderStruct{ //6 bytes
  uint8_t prefix;
  uint8_t file_index;
  uint32_t buffer_size;
};

struct sequenceStruct{ //9 bytes
  uint8_t led_id; //LED channel
  uint16_t led_pwm; //LED PWM (analog out units)
  uint16_t led_current; //LED current (DAC units)
  uint32_t led_duration; //LED duration (microseconds)
};

struct statusStruct{
  uint8_t led_channel[3]; //Active LED channel
  uint16_t led_pwm[3]; //PWM value for internal and external fan respectively
  uint16_t led_current[3]; //DAC value for active LED
  uint8_t mode; //0=Sync, 1=PWM, 2=Current, 3=Off
  boolean state; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  boolean driver_control; //True = driver controls itself, False = GUI controls driver
  uint16_t temp[3]; //ADC temp reading of mosfet, resistor, and external respectively
  uint16_t fan_speed[3]; //PWM value for internal and external fan respectively
};

const struct defaultStatusStruct{
  uint8_t led_channel[3] = {0, 0, 0}; //Active LED channel
  uint16_t led_pwm[3] = {0, 0, 0}; //PWM value for internal and external fan respectively
  uint16_t led_current[3] = {0, 0, 0}; //DAC value for active LED
  uint8_t mode = 3; //0=Sync, 1=PWM, 2=Current, 3=Off
  boolean state = 0; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  boolean driver_control = true; //True = driver controls itself, False = GUI controls driver
  uint16_t temp[3] = {0, 0, 0}; //ADC temp reading of mosfet, resistor, and external respectively
  uint16_t fan_speed[3] = {0, 0, 0}; //PWM value for internal and external fan respectively
} defaultStatus;

const struct prefixStruct{
  uint8_t message = 0; //Send error, warning, and notification messages to be diplayed as pop-up in GUI
  uint8_t connection = 1; //Recv magic number at connection start and confirm with magic reply
  uint8_t send_config = 2; //Send active configuration byte file
  uint8_t recv_config = 3; //Recv and apply new configuration byte file
  uint8_t send_sync = 4; //Send active configuration byte file
  uint8_t recv_sync = 5; //Recv and apply new configuration byte file
  uint8_t send_seq = 6; //Send specified seq byte file from SD card if available
  uint8_t recv_seq = 7; //Recv specified seq byte file and save on SD card if available
  uint8_t send_id = 8; //Send the driver ID only
  uint8_t recv_time = 9; //Receive the unix time of the GUI to sync driver RTC
  uint8_t recv_stream = 10; //Signal the driver is ready to receive stream that is queued
  uint8_t send_stream = 11; //If recv - signals that ready for next packet
  uint8_t status_update = 12; //Status update packet for driver or GUI
  uint8_t calibration = 13; //Send a plot of LED square wave test output for calibrating op-amp compensation trimpots
  uint8_t gui_disconnect = 14; //GUI is disconnecting form LED driver
  uint8_t measure_period = 15; //Measure the mirror period - return as 
  uint8_t test_current = 16; //Measure the output for each channel - return as 4x uint16_t - disable channels with output below 51633 (0.7 V diff) to protect opamp.
  uint8_t test_volume = 17; //Test status or indicator volume
  uint8_t long_off = 148; //Prefix sent when computer has been off for a while
} prefix;

//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION

//Convert between byte list and float
union FLOATUNION
{
 float float_var;
 uint8_t bytes[4];
}floatUnion;

//Convert between byte list and int
union INTUNION
{
 int int_var;
 uint8_t bytes[4];
}intUnion;

//Convert between byte list and int
union BYTE16UNION
{
 uint16_t bytes_var;
 uint8_t bytes[2];
}uint16Union;

union BYTE32UNION
{
 uint32_t bytes_var;
 uint8_t bytes[4];
}uint32Union;

//From: https://forum.arduino.cc/index.php?topic=263107.0
union CONFIGUNION //Convert binary buffer <-> config setup
{
   configurationStruct c;
   byte byte_buffer[sizeof(defaultConfigurationStruct)];
} conf;

union SYNCUNION //Convert binary buffer <-> sync setup
{
   syncStruct s;
   byte byte_buffer[sizeof(defaultSyncStruct)];
} sync;

union SEQHEADERUNION //Convert binary buffer <-> sync setup
{
   sequenceHeaderStruct s;
   byte byte_buffer[sizeof(sequenceHeaderStruct)];
} seq_header;

union SEQUNION //Convert binary buffer <-> sync setup
{
   sequenceStruct s;
   byte byte_buffer[sizeof(sequenceStruct)];
}seq;

union STATUSUNION //Convert binary buffer <-> sync setup
{
   statusStruct s;
   byte byte_buffer[sizeof(defaultStatusStruct)];
} current_status;

//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF

//https://forum.arduino.cc/index.php?topic=171069.0
typedef void (* GenericFP)(const uint8_t*, size_t); //function pointer prototype to a function which takes an 'int' an returns 'void'
GenericFP function_router[256]; //create an array of 'GenericFP' function pointers. Notice the '&' operator

//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE

uint32_t a;
int b;
int c;
int intensity = 60;
uint8_t color_index[3];
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


  uint8_t sum = 0;
  uint8_t *buffer_ptr = (uint8_t *)&defaultConfig;
  for(a=0; a<sizeof(conf.byte_buffer); a++) sum += *buffer_ptr++;
  while(!Serial);
  Serial.println(sizeof(conf.byte_buffer));
}

void loop() {
  // for(a = 0; a < 3; a++){
  //   pot[a] = pin.potValue(a);
  //   pot[a] /= (float) pin.adcMax()/100;
  //   Serial.print(pot[a]);
  //   Serial.print(" ");
  //   pin.setButtonColor(a, (uint8_t) pot[a]);
  // }
  for(a=0; a<3; a++){
    if(!digitalReadFast(pin.PUSHBUTTON[a])){
      pin.setButtonColor(a, color_index[a]);
      for(c=0; c<4; c++){
        if(c==color_index[a]) digitalWriteFast(pin.RELAY[a][c], HIGH);
        else digitalWriteFast(pin.RELAY[a][c], LOW);
      }
      delay(50);  
      while(!digitalReadFast(pin.PUSHBUTTON[a]));
      delay(50);
      color_index[a]++;
      if(color_index[a] > 4){
        for(c=0; c<4; c++) digitalWriteFast(pin.RELAY[a][c], LOW);
        color_index[a] = 0;
      } 
    }
    c=0;
    for(b=0; b<16; b++) c+=pin.potValue(a);
    analogWrite(pin.INTERLINE[a], c);
  }

//delay(50);


  // for(c=0; c<3; c++){
  //   for(a=0; a<4; a++){

  //   }
  //   delay(100);
  //   if(digitalReadFast(pin.PUSHBUTTON[0])){
  //     for(a=0; a<3; a++) analogWrite(pin.INTERLINE[a], intensity);
  //   }
  //   else{
  //     for(a=0; a<3; a++) analogWrite(pin.INTERLINE[a], 65535);
  //   }
    
  //   for(a=0; a<3; a++) digitalWrite(pin.FAN_PWM[a], LOW);
  //   if(!digitalReadFast(pin.PUSHBUTTON[1]) && digitalReadFast(pin.PUSHBUTTON[2])){
  //     digitalWrite(pin.FAN_PWM[0], HIGH);
  //   }
  //   else if(digitalReadFast(pin.PUSHBUTTON[1]) && !digitalReadFast(pin.PUSHBUTTON[2])){
  //     digitalWrite(pin.FAN_PWM[1], HIGH);
  //   }
  //   else if(!digitalReadFast(pin.PUSHBUTTON[1]) && !digitalReadFast(pin.PUSHBUTTON[2])){
  //     digitalWrite(pin.FAN_PWM[2], HIGH);
  //   }
  //   else for(a=0; a<3; a++) digitalWrite(pin.FAN_PWM[a], LOW);
  //   b++;
  //   if(b>3) b = 0;
  // }
  // int adc_temp;
  // float float_temp;
  // for(a=0; a<3; a++){
  //   adc_temp = pin.boardTemp(a);
  //   float_temp = pin.adcToTemp(adc_temp);
  //   Serial.print(float_temp);
  //   Serial.print(" ");
  // }
  // Serial.println();
}
